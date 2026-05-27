#include <SD.h>
#include <SPI.h>
#include <TMRpcm.h>
#include <EEPROM.h>
#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/wdt.h>
#include "logic.h"

const uint8_t  SD_CS_PIN              = 4;
const uint8_t  SPEAKER_PIN            = 9;
const uint8_t  IGNITION_SENSE_PIN     = 2;
const int      NOISE_PIN              = A0;   // intentionally unconnected — reads electrical noise for random seed

// Startup EEPROM layout (identical to v0)
const uint8_t  EEPROM_SEQ_INDEX_ADDR        = 0;
const uint8_t  EEPROM_LAST_PLAYED_ADDR      = 1;
const uint8_t  EEPROM_SHUFFLE_MASK_ADDR     = (uint8_t)(EEPROM_LAST_PLAYED_ADDR + TRACK_NAME_LEN);
const uint8_t  EEPROM_WDT_CRASH_ADDR        = (uint8_t)(EEPROM_SHUFFLE_MASK_ADDR + 2);

// Shutdown EEPROM layout (new in v1, starts after WDT crash flag)
const uint8_t  EEPROM_SDOWN_SEQ_ADDR        = (uint8_t)(EEPROM_WDT_CRASH_ADDR + 1);
const uint8_t  EEPROM_SDOWN_LAST_ADDR       = (uint8_t)(EEPROM_SDOWN_SEQ_ADDR + 1);
const uint8_t  EEPROM_SDOWN_MASK_ADDR       = (uint8_t)(EEPROM_SDOWN_LAST_ADDR + TRACK_NAME_LEN);

const uint8_t  WDT_CRASH_MAGIC             = 0xAB;
const char     SHUTDOWN_DIR[]              = "/SHUTDOWN";
const uint8_t  SHUTDOWN_PATH_LEN           = 24; // "/SHUTDOWN/" (10) + 8.3 name (12) + null

const uint8_t       SD_INIT_RETRIES        = 3;
const unsigned long SD_RETRY_DELAY_MS      = 500;
const unsigned long PLAYBACK_WATCHDOG_MS   = 2000;
const unsigned long SHUTDOWN_WATCHDOG_MS   = 15000; // supercap budget
const unsigned long FADE_STEP_MS           = 100;
const unsigned long BLINK_DURATION_MS      = 200;
const unsigned long BLINK_PAUSE_MS         = 800;
const uint8_t       ERROR_BLINK_CYCLES     = 3;
const uint8_t       CONFIG_LINE_LEN        = 32;

enum ErrorCode {
  SD_INIT_FAILED   = 2,
  NO_WAV_FILES     = 3,
  ROOT_DIR_FAILED  = 4,
  PLAYBACK_TIMEOUT = 5
};

enum DeviceState : uint8_t {
  STATE_STARTUP,
  STATE_IDLE,
  STATE_SHUTDOWN
};

TMRpcm       player;
DeviceState   state            = STATE_STARTUP;
Config        gCfg;
bool          wasPlaying       = false;
unsigned long playbackDeadline = 0;
uint8_t       playsDone        = 0;
uint8_t       playsTarget      = 1;
char          startupTrack[TRACK_NAME_LEN]  = "";
char          shutdownTrack[TRACK_NAME_LEN] = "";

void enterPowerDownSleep() __attribute__((noreturn));
void haltWithErrorCode(ErrorCode code) __attribute__((noreturn));

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------

void setup() {
  wdt_disable();
  initStatusLed();
  pinMode(IGNITION_SENSE_PIN, INPUT);

  if (EEPROM.read(EEPROM_WDT_CRASH_ADDR) == WDT_CRASH_MAGIC) {
    EEPROM.update(EEPROM_WDT_CRASH_ADDR, 0);
    haltWithErrorCode(SD_INIT_FAILED);
  }

  seedRandom();
  EEPROM.update(EEPROM_WDT_CRASH_ADDR, WDT_CRASH_MAGIC);
  wdt_enable(WDTO_4S);
  initSD();
  wdt_disable();
  EEPROM.update(EEPROM_WDT_CRASH_ADDR, 0);

  gCfg = loadConfig();
  playsTarget = gCfg.playCount;

  if (gCfg.delaySeconds > 0) delay((unsigned long)gCfg.delaySeconds * 1000UL);

  char trackName[TRACK_NAME_LEN];
  pickStartupWav(trackName, gCfg);
  validateWavFile(trackName);
  copyTrackName(startupTrack, trackName);

  configureAndPlay(startupTrack, gCfg.volume);
}

// ---------------------------------------------------------------------------
// Loop — state machine
// ---------------------------------------------------------------------------

void loop() {
  switch (state) {
    case STATE_STARTUP:  runStartupLoop();  break;
    case STATE_IDLE:     runIdleLoop();     break;
    case STATE_SHUTDOWN: runShutdownLoop(); break;
  }
}

void runStartupLoop() {
  if (player.isPlaying()) {
    wasPlaying = true;
    digitalWrite(LED_BUILTIN, (millis() % 1000UL < 100UL) ? HIGH : LOW);
    return;
  }

  digitalWrite(LED_BUILTIN, LOW);

  if (wasPlaying) {
    wasPlaying = false;
    playsDone++;
    if (playsDone >= playsTarget) {
      state = STATE_IDLE;
      return;
    }
    player.play(startupTrack);
    playbackDeadline = millis() + PLAYBACK_WATCHDOG_MS;
    return;
  }

  if (millis() > playbackDeadline) haltWithErrorCode(PLAYBACK_TIMEOUT);
}

void runIdleLoop() {
  if (digitalRead(IGNITION_SENSE_PIN) == LOW) {
    beginShutdown();
    return;
  }
  delay(10);
}

void beginShutdown() {
  char trackName[TRACK_NAME_LEN];
  if (!pickShutdownWav(trackName, gCfg)) { enterPowerDownSleep(); }

  char path[SHUTDOWN_PATH_LEN];
  buildShutdownPath(path, trackName);
  if (!validateShutdownWavFile(path))    { enterPowerDownSleep(); }

  copyTrackName(shutdownTrack, trackName);
  player.play(path);
  playbackDeadline = millis() + SHUTDOWN_WATCHDOG_MS;
  wasPlaying = false;
  state = STATE_SHUTDOWN;
}

void runShutdownLoop() {
  if (player.isPlaying()) {
    wasPlaying = true;
    digitalWrite(LED_BUILTIN, (millis() % 1000UL < 100UL) ? HIGH : LOW);
    return;
  }

  digitalWrite(LED_BUILTIN, LOW);

  if (wasPlaying) { enterPowerDownSleep(); }

  // Track never started — drain risk, sleep immediately without blinking
  if (millis() > playbackDeadline) { enterPowerDownSleep(); }
}

// ---------------------------------------------------------------------------
// Hardware helpers
// ---------------------------------------------------------------------------

void initStatusLed() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
}

void seedRandom() {
  randomSeed(analogRead(NOISE_PIN));
}

void initSD() {
  for (uint8_t attempt = 0; attempt < SD_INIT_RETRIES; attempt++) {
    if (SD.begin(SD_CS_PIN)) return;
    delay(SD_RETRY_DELAY_MS);
  }
  haltWithErrorCode(SD_INIT_FAILED);
}

void buildShutdownPath(char* path, const char* filename) {
  strcpy(path, "/SHUTDOWN/");
  strncat(path, filename, SHUTDOWN_PATH_LEN - 11);
}

Config loadConfig() {
  Config cfg = DEFAULT_CONFIG;
  File f = SD.open("CONFIG.TXT");
  if (!f) return cfg;
  char line[CONFIG_LINE_LEN];
  uint8_t pos = 0;
  auto flush = [&]() { if (pos > 0) { line[pos] = '\0'; applyConfigLine(cfg, line); pos = 0; } };
  while (f.available()) {
    char c = (char)f.read();
    if (c == '\n' || c == '\r') flush();
    else if (pos < CONFIG_LINE_LEN - 1) line[pos++] = c;
  }
  flush();
  f.close();
  return cfg;
}

void validateWavFile(const char* trackName) {
  File f = SD.open(trackName);
  if (!f) haltWithErrorCode(NO_WAV_FILES);
  uint8_t header[WAV_HEADER_MIN_SIZE];
  bool valid = (f.read(header, WAV_HEADER_MIN_SIZE) == (int)WAV_HEADER_MIN_SIZE)
               && isValidWavHeader(header);
  f.close();
  if (!valid) haltWithErrorCode(NO_WAV_FILES);
}

bool validateShutdownWavFile(const char* path) {
  File f = SD.open(path);
  if (!f) return false;
  uint8_t header[WAV_HEADER_MIN_SIZE];
  bool valid = (f.read(header, WAV_HEADER_MIN_SIZE) == (int)WAV_HEADER_MIN_SIZE)
               && isValidWavHeader(header);
  f.close();
  return valid;
}

void configureAndPlay(const char* trackName, uint8_t volume) {
  player.speakerPin = SPEAKER_PIN;
  player.setVolume(0);
  player.play(trackName);
  for (uint8_t v = 1; v <= volume; v++) {
    delay(FADE_STEP_MS);
    player.setVolume(v);
  }
  playbackDeadline = millis() + PLAYBACK_WATCHDOG_MS;
}

void enterPowerDownSleep() {
  player.disable();
  digitalWrite(SPEAKER_PIN, LOW);
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  power_all_disable();
  sleep_mode();
  while (true);
}

void haltWithErrorCode(ErrorCode code) {
  wdt_disable();
  EEPROM.update(EEPROM_WDT_CRASH_ADDR, 0);
  for (uint8_t cycle = 0; cycle < ERROR_BLINK_CYCLES; cycle++) {
    for (uint8_t i = 0; i < (uint8_t)code; i++) {
      digitalWrite(LED_BUILTIN, HIGH);
      delay(BLINK_DURATION_MS);
      digitalWrite(LED_BUILTIN, LOW);
      delay(BLINK_DURATION_MS);
    }
    delay(BLINK_PAUSE_MS);
  }
  enterPowerDownSleep();
}

// ---------------------------------------------------------------------------
// EEPROM helpers
// ---------------------------------------------------------------------------

void readLastPlayed(char* name, uint8_t addr) {
  for (uint8_t i = 0; i < TRACK_NAME_LEN; i++)
    name[i] = (char)EEPROM.read(addr + i);
  name[TRACK_NAME_LEN - 1] = '\0';
}

void writeLastPlayed(const char* name, uint8_t addr) {
  for (uint8_t i = 0; i < TRACK_NAME_LEN; i++)
    EEPROM.update(addr + i, (uint8_t)name[i]);
}

// ---------------------------------------------------------------------------
// SD scanning — return bool, accept directory path
// ---------------------------------------------------------------------------

bool isEligibleFile(File& entry, uint8_t minSizeKb) {
  return !entry.isDirectory() && isWav(entry.name()) && meetsMinSize(entry.size(), minSizeKb);
}

bool pickRandomWavFrom(char* trackName, const char* dir, const char* lastPlayed, uint8_t minSizeKb) {
  trackName[0] = '\0';
  uint8_t candidateCount = 0;
  char firstEligible[TRACK_NAME_LEN] = "";

  File d = SD.open(dir);
  if (!d) return false;
  while (true) {
    File entry = d.openNextFile();
    if (!entry) break;
    if (isEligibleFile(entry, minSizeKb)) {
      if (firstEligible[0] == '\0') copyTrackName(firstEligible, entry.name());
      if (!shouldSkipForAntiRepeat(entry.name(), lastPlayed)) {
        candidateCount++;
        if (reservoirShouldReplace(candidateCount, random)) copyTrackName(trackName, entry.name());
      }
    }
    entry.close();
  }
  d.close();

  if (firstEligible[0] == '\0') return false;
  if (candidateCount == 0) copyTrackName(trackName, firstEligible);
  return true;
}

bool countWavFilesIn(const char* dir, uint8_t minSizeKb, uint8_t& count) {
  File d = SD.open(dir);
  if (!d) { count = 0; return false; }
  count = 0;
  while (true) {
    File entry = d.openNextFile();
    if (!entry) break;
    if (isEligibleFile(entry, minSizeKb)) count++;
    entry.close();
  }
  d.close();
  return true;
}

bool fetchFileAtIndexIn(char* trackName, const char* dir, uint8_t idx, uint8_t minSizeKb) {
  trackName[0] = '\0';
  File d = SD.open(dir);
  if (!d) return false;
  uint8_t position = 0;
  while (true) {
    File entry = d.openNextFile();
    if (!entry) break;
    if (isEligibleFile(entry, minSizeKb)) {
      if (position == idx) {
        copyTrackName(trackName, entry.name());
        entry.close();
        break;
      }
      position++;
    }
    entry.close();
  }
  d.close();
  return trackName[0] != '\0';
}

bool pickSequentialWavFrom(char* trackName, const char* dir, uint8_t seqAddr, uint8_t minSizeKb) {
  uint8_t stored = EEPROM.read(seqAddr);
  uint8_t total = 0;
  char firstFile[TRACK_NAME_LEN] = "";
  trackName[0] = '\0';

  File d = SD.open(dir);
  if (!d) return false;
  while (true) {
    File entry = d.openNextFile();
    if (!entry) break;
    if (isEligibleFile(entry, minSizeKb)) {
      if (total == 0) copyTrackName(firstFile, entry.name());
      if (total == stored) copyTrackName(trackName, entry.name());
      total++;
    }
    entry.close();
  }
  d.close();

  if (total == 0) return false;
  uint8_t idx = resolveSequentialIndex(stored, total);
  if (trackName[0] == '\0') copyTrackName(trackName, firstFile);
  EEPROM.update(seqAddr, nextSequentialIndex(idx, total));
  return true;
}

bool pickShuffleWavFrom(char* trackName, const char* dir, uint8_t seqAddr, uint8_t maskAddr, uint8_t minSizeKb) {
  uint8_t total = 0;
  if (!countWavFilesIn(dir, minSizeKb, total) || total == 0) return false;

  if (total > SHUFFLE_MAX_TRACKS) {
    char lastPlayed[TRACK_NAME_LEN];
    readLastPlayed(lastPlayed, seqAddr); // reuse seqAddr slot for last-played in fallback
    bool ok = pickRandomWavFrom(trackName, dir, lastPlayed, minSizeKb);
    if (ok) writeLastPlayed(trackName, seqAddr);
    return ok;
  }

  uint16_t mask = (uint16_t)EEPROM.read(maskAddr) |
                  ((uint16_t)EEPROM.read(maskAddr + 1) << 8);
  if (shuffleAllPlayed(mask, total)) mask = 0;

  uint8_t candidateCount = 0;
  uint8_t selectedIdx    = 0;
  for (uint8_t i = 0; i < total; i++) {
    if (!shufflePlayed(mask, i)) {
      candidateCount++;
      if (reservoirShouldReplace(candidateCount, random)) selectedIdx = i;
    }
  }

  if (!fetchFileAtIndexIn(trackName, dir, selectedIdx, minSizeKb)) return false;
  uint16_t newMask = shuffleMarkPlayed(mask, selectedIdx);
  EEPROM.update(maskAddr,     (uint8_t)(newMask & 0xFF));
  EEPROM.update(maskAddr + 1, (uint8_t)(newMask >> 8));
  return true;
}

// ---------------------------------------------------------------------------
// Track pickers
// ---------------------------------------------------------------------------

void pickStartupWav(char* trackName, const Config& cfg) {
  bool ok = false;
  switch (cfg.mode) {
    case MODE_RANDOM: {
      char lastPlayed[TRACK_NAME_LEN];
      readLastPlayed(lastPlayed, EEPROM_LAST_PLAYED_ADDR);
      ok = pickRandomWavFrom(trackName, "/", lastPlayed, cfg.minSizeKb);
      if (ok) writeLastPlayed(trackName, EEPROM_LAST_PLAYED_ADDR);
      break;
    }
    case MODE_SEQUENTIAL:
      ok = pickSequentialWavFrom(trackName, "/", EEPROM_SEQ_INDEX_ADDR, cfg.minSizeKb);
      break;
    case MODE_SINGLE:
      if (cfg.singleTrack[0] != '\0') { copyTrackName(trackName, cfg.singleTrack); ok = true; }
      break;
    case MODE_SHUFFLE:
      ok = pickShuffleWavFrom(trackName, "/", EEPROM_SEQ_INDEX_ADDR, EEPROM_SHUFFLE_MASK_ADDR, cfg.minSizeKb);
      break;
  }
  if (!ok) haltWithErrorCode(NO_WAV_FILES);
}

bool pickShutdownWav(char* trackName, const Config& cfg) {
  switch (cfg.shutdownMode) {
    case MODE_RANDOM: {
      char lastPlayed[TRACK_NAME_LEN];
      readLastPlayed(lastPlayed, EEPROM_SDOWN_LAST_ADDR);
      bool ok = pickRandomWavFrom(trackName, SHUTDOWN_DIR, lastPlayed, cfg.minSizeKb);
      if (ok) writeLastPlayed(trackName, EEPROM_SDOWN_LAST_ADDR);
      return ok;
    }
    case MODE_SEQUENTIAL:
      return pickSequentialWavFrom(trackName, SHUTDOWN_DIR, EEPROM_SDOWN_SEQ_ADDR, cfg.minSizeKb);
    case MODE_SINGLE:
      if (cfg.shutdownTrack[0] == '\0') return false;
      copyTrackName(trackName, cfg.shutdownTrack);
      return true;
    case MODE_SHUFFLE:
      return pickShuffleWavFrom(trackName, SHUTDOWN_DIR, EEPROM_SDOWN_SEQ_ADDR, EEPROM_SDOWN_MASK_ADDR, cfg.minSizeKb);
  }
  return false;
}
