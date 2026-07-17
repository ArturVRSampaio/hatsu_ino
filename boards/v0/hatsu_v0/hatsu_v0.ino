#include <SD.h>
#include <SPI.h>
#include <TMRpcm.h>
#include <EEPROM.h>
#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/wdt.h>
#include "logic.h"

const uint8_t  SD_CS_PIN               = 10;
const uint8_t  SPEAKER_PIN             = 9;
const int      NOISE_PIN               = A0;   // intentionally unconnected — reads electrical noise for random seed
const uint8_t  EEPROM_SEQ_INDEX_ADDR   = 0;    // sequential mode: play index (1 byte)
const uint8_t  EEPROM_LAST_PLAYED_ADDR = 1;    // random mode: last played filename (TRACK_NAME_LEN bytes)
const uint8_t  EEPROM_SHUFFLE_MASK_ADDR = (uint8_t)(EEPROM_LAST_PLAYED_ADDR + TRACK_NAME_LEN); // shuffle mode: bitmask (2 bytes, addr 14–15)
const uint8_t  EEPROM_WDT_CRASH_ADDR   = (uint8_t)(EEPROM_SHUFFLE_MASK_ADDR + 2); // WDT crash flag (1 byte, addr 16)

const uint8_t  WDT_CRASH_MAGIC         = 0xAB;

const uint8_t      SD_INIT_RETRIES       = 3;
const unsigned long SD_RETRY_DELAY_MS    = 500;
const unsigned long PLAYBACK_WATCHDOG_MS = 2000;
const unsigned long FADE_STEP_MS         = 100;
const unsigned long BLINK_DURATION_MS    = 200;
const unsigned long BLINK_PAUSE_MS       = 800;
const uint8_t      ERROR_BLINK_CYCLES    = 3;
const uint8_t      CONFIG_LINE_LEN       = 32;

const unsigned int ERROR_TONE_FREQ_HZ    = 1000;

// Defaults to the DEFAULT_CONFIG value; errors before loadConfig() (e.g. SD init failure)
// can't read CONFIG.TXT yet, so they use this default instead of the configured value.
bool playToneOnError = true;

enum ErrorCode {
  SD_INIT_FAILED   = 2,
  NO_WAV_FILES     = 3,
  ROOT_DIR_FAILED  = 4,
  PLAYBACK_TIMEOUT = 5
};

TMRpcm player;
bool          wasPlaying = false;
unsigned long playbackDeadline = 0;
uint8_t       playsDone      = 0;
uint8_t       playsTarget     = 1;
char          currentTrack[TRACK_NAME_LEN] = "";

void enterPowerDownSleep() __attribute__((noreturn));
void haltWithErrorCode(ErrorCode code) __attribute__((noreturn));

void setup() {
  wdt_disable();
  initStatusLed();

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

  Config cfg = loadConfig();
  playsTarget = cfg.playCount;
  playToneOnError = cfg.playToneOnError;

  if (cfg.delaySeconds > 0) delay((unsigned long)cfg.delaySeconds * 1000UL);

  char trackName[TRACK_NAME_LEN];
  pickWav(trackName, cfg);
  validateWavFile(trackName);
  copyTrackName(currentTrack, trackName);

  configureAndPlay(trackName, cfg.volume);
}

void loop() {
  if (player.isPlaying()) {
    wasPlaying = true;
    digitalWrite(LED_BUILTIN, (millis() % 1000UL < 100UL) ? HIGH : LOW);
    return;
  }

  digitalWrite(LED_BUILTIN, LOW);

  if (wasPlaying) {
    wasPlaying = false;
    playsDone++;
    if (playsDone >= playsTarget) enterPowerDownSleep();
    player.play(currentTrack);
    playbackDeadline = millis() + PLAYBACK_WATCHDOG_MS;
    return;
  }

  if (millis() > playbackDeadline) haltWithErrorCode(PLAYBACK_TIMEOUT);
}

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

bool isEligibleFile(File& entry, uint8_t minSizeKb) {
  return !entry.isDirectory() && isWav(entry.name()) && meetsMinSize(entry.size(), minSizeKb);
}

void fetchFileAtIndex(char* trackName, uint8_t idx, uint8_t minSizeKb) {
  trackName[0] = '\0';
  File rootDir = SD.open("/");
  if (!rootDir) haltWithErrorCode(ROOT_DIR_FAILED);
  uint8_t position = 0;
  while (true) {
    File entry = rootDir.openNextFile();
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
  rootDir.close();
  if (trackName[0] == '\0') haltWithErrorCode(NO_WAV_FILES);
}

void pickWav(char* trackName, const Config& cfg) {
  switch (cfg.mode) {
    case MODE_RANDOM: {
      char lastPlayed[TRACK_NAME_LEN];
      readLastPlayed(lastPlayed);
      pickRandomWav(trackName, lastPlayed, cfg.minSizeKb);
      writeLastPlayed(trackName);
      break;
    }
    case MODE_SEQUENTIAL:
      pickSequentialWav(trackName, cfg.minSizeKb);
      break;
    case MODE_SINGLE:
      pickSingleWav(trackName, cfg.singleTrack);
      break;
    case MODE_SHUFFLE:
      pickShuffleWav(trackName, cfg.minSizeKb);
      break;
    default:
      haltWithErrorCode(NO_WAV_FILES);
  }
}

void readLastPlayed(char* name) {
  for (uint8_t i = 0; i < TRACK_NAME_LEN; i++)
    name[i] = (char)EEPROM.read(EEPROM_LAST_PLAYED_ADDR + i);
  name[TRACK_NAME_LEN - 1] = '\0';
}

void writeLastPlayed(const char* name) {
  for (uint8_t i = 0; i < TRACK_NAME_LEN; i++)
    EEPROM.update(EEPROM_LAST_PLAYED_ADDR + i, (uint8_t)name[i]);
}

void pickRandomWav(char* trackName, const char* lastPlayed, uint8_t minSizeKb) {
  trackName[0] = '\0';
  uint8_t candidateCount = 0;
  char firstEligible[TRACK_NAME_LEN] = "";

  File rootDir = SD.open("/");
  if (!rootDir) haltWithErrorCode(ROOT_DIR_FAILED);
  while (true) {
    File entry = rootDir.openNextFile();
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
  rootDir.close();

  if (firstEligible[0] == '\0') haltWithErrorCode(NO_WAV_FILES);
  // Only one qualifying file and it was the last played — no alternative, repeat it
  if (candidateCount == 0) copyTrackName(trackName, firstEligible);
}

uint8_t countWavFiles(uint8_t minSizeKb) {
  File rootDir = SD.open("/");
  if (!rootDir) haltWithErrorCode(ROOT_DIR_FAILED);
  uint8_t count = 0;
  while (true) {
    File entry = rootDir.openNextFile();
    if (!entry) break;
    if (isEligibleFile(entry, minSizeKb)) count++;
    entry.close();
  }
  rootDir.close();
  return count;
}

void pickSequentialWav(char* trackName, uint8_t minSizeKb) {
  uint8_t stored = EEPROM.read(EEPROM_SEQ_INDEX_ADDR);
  uint8_t total = 0;
  char firstFile[TRACK_NAME_LEN] = "";
  trackName[0] = '\0';

  File rootDir = SD.open("/");
  if (!rootDir) haltWithErrorCode(ROOT_DIR_FAILED);
  while (true) {
    File entry = rootDir.openNextFile();
    if (!entry) break;
    if (isEligibleFile(entry, minSizeKb)) {
      if (total == 0) copyTrackName(firstFile, entry.name());
      if (total == stored) copyTrackName(trackName, entry.name());
      total++;
    }
    entry.close();
  }
  rootDir.close();

  if (total == 0) haltWithErrorCode(NO_WAV_FILES);
  uint8_t idx = resolveSequentialIndex(stored, total);
  if (trackName[0] == '\0') copyTrackName(trackName, firstFile);
  EEPROM.update(EEPROM_SEQ_INDEX_ADDR, nextSequentialIndex(idx, total));
}

void pickShuffleWav(char* trackName, uint8_t minSizeKb) {
  // total needed before sampling: SHUFFLE_MAX_TRACKS check and mask-reset both require it upfront
  uint8_t total = countWavFiles(minSizeKb);
  if (total == 0) haltWithErrorCode(NO_WAV_FILES);

  // More than 16 tracks: fall back to anti-repeat random
  if (total > SHUFFLE_MAX_TRACKS) {
    char lastPlayed[TRACK_NAME_LEN];
    readLastPlayed(lastPlayed);
    pickRandomWav(trackName, lastPlayed, minSizeKb);
    writeLastPlayed(trackName);
    return;
  }

  uint16_t mask = (uint16_t)EEPROM.read(EEPROM_SHUFFLE_MASK_ADDR) |
                  ((uint16_t)EEPROM.read(EEPROM_SHUFFLE_MASK_ADDR + 1) << 8);
  if (shuffleAllPlayed(mask, total)) mask = 0;

  uint8_t candidateCount = 0;
  uint8_t selectedIdx    = 0;
  for (uint8_t i = 0; i < total; i++) {
    if (!shufflePlayed(mask, i)) {
      candidateCount++;
      if (reservoirShouldReplace(candidateCount, random)) selectedIdx = i;
    }
  }

  fetchFileAtIndex(trackName, selectedIdx, minSizeKb);
  uint16_t newMask = shuffleMarkPlayed(mask, selectedIdx);
  EEPROM.update(EEPROM_SHUFFLE_MASK_ADDR,     (uint8_t)(newMask & 0xFF));
  EEPROM.update(EEPROM_SHUFFLE_MASK_ADDR + 1, (uint8_t)(newMask >> 8));
}

void pickSingleWav(char* trackName, const char* singleTrack) {
  if (singleTrack[0] == '\0') haltWithErrorCode(NO_WAV_FILES);
  copyTrackName(trackName, singleTrack);
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

// ~20mA active → ~0.1µA in power-down. Wakes on next ignition power cycle.
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
      if (playToneOnError) tone(SPEAKER_PIN, ERROR_TONE_FREQ_HZ);
      delay(BLINK_DURATION_MS);
      digitalWrite(LED_BUILTIN, LOW);
      if (playToneOnError) noTone(SPEAKER_PIN);
      delay(BLINK_DURATION_MS);
    }
    delay(BLINK_PAUSE_MS);
  }
  enterPowerDownSleep();
}
