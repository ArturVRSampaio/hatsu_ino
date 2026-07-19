#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h>
#include <EEPROM.h>
#include <avr/sleep.h>
#include <avr/power.h>
#include "logic.h"

// ── USER CONFIG — edit these before compiling/uploading ─────────────
const PlayMode CONFIG_MODE          = MODE_RANDOM; // MODE_RANDOM, MODE_SEQUENTIAL, MODE_SHUFFLE, MODE_SINGLE
const uint8_t  CONFIG_SINGLE_TRACK  = 1;   // track number to play in MODE_SINGLE (1-based, DFPlayer numbering)
const uint8_t  CONFIG_PLAY_COUNT    = 1;   // how many times to play the chosen track before sleeping (1-255)
const uint8_t  CONFIG_DELAY_SECONDS = 0;   // seconds to wait after power-up before playing (0-255)

const uint8_t DFPLAYER_RX_PIN   = 2;  // Nano RX <- DFPlayer TX
const uint8_t DFPLAYER_TX_PIN   = 3;  // Nano TX -> DFPlayer RX (through a 1kΩ series resistor)
const uint8_t BUSY_PIN          = 4;  // DFPlayer BUSY: LOW while playing, HIGH when idle
const uint8_t DFPLAYER_GATE_PIN = 5;  // gates the DFPlayer's GND return through an N-channel MOSFET
const int     NOISE_PIN         = A0; // intentionally unconnected — reads electrical noise for random seed
const uint8_t DEFAULT_VOLUME    = 20; // 0-30

const uint8_t EEPROM_SEQ_INDEX_ADDR    = 0; // sequential mode: play index (1 byte)
const uint8_t EEPROM_SHUFFLE_MASK_ADDR = 1; // shuffle mode: bitmask (2 bytes, addr 1-2)

const unsigned long GATE_SETTLE_DELAY_MS    = 50;   // let the DFPlayer's rail actually rise before booting it
const unsigned long DFPLAYER_BOOT_DELAY_MS  = 1000; // let the module finish booting before sending commands
const unsigned long PLAYBACK_START_DELAY_MS = 500;  // give playback a moment to actually start before polling BUSY
const unsigned long PLAYBACK_WATCHDOG_MS    = 8000; // max time to wait for a single play-through to finish
const unsigned long BUSY_POLL_MS            = 100;
const unsigned long BLINK_DURATION_MS       = 200;
const unsigned long BLINK_PAUSE_MS          = 800;
const uint8_t       ERROR_BLINK_CYCLES      = 3;

SoftwareSerial dfSerial(DFPLAYER_RX_PIN, DFPLAYER_TX_PIN);
DFRobotDFPlayerMini player;

void enterPowerDownSleep() __attribute__((noreturn));
void haltWithError() __attribute__((noreturn));

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  pinMode(BUSY_PIN, INPUT_PULLUP);

  pinMode(DFPLAYER_GATE_PIN, OUTPUT);
  digitalWrite(DFPLAYER_GATE_PIN, HIGH); // power the DFPlayer Mini on
  delay(GATE_SETTLE_DELAY_MS);

  randomSeed(analogRead(NOISE_PIN));

  dfSerial.begin(9600);
  delay(DFPLAYER_BOOT_DELAY_MS);

  if (!player.begin(dfSerial)) haltWithError();

  player.volume(DEFAULT_VOLUME);

  if (CONFIG_DELAY_SECONDS > 0) delay((unsigned long)CONFIG_DELAY_SECONDS * 1000UL);

  uint16_t trackNumber = pickTrack();

  for (uint8_t i = 0; i < CONFIG_PLAY_COUNT; i++) {
    if (trackNumber == 0) {
      player.randomAll();
      delay(PLAYBACK_START_DELAY_MS);
      trackNumber = player.readCurrentFileNumber(); // remember it so repeats replay the same track
    } else {
      player.play(trackNumber);
      delay(PLAYBACK_START_DELAY_MS);
    }
    waitForPlaybackToFinish();
  }

  enterPowerDownSleep();
}

void loop() {}

// Returns the DFPlayer track number to play, or 0 as a sentinel meaning
// "use player.randomAll() instead" (DFPlayer's own random logic doesn't
// require knowing a track number up front, and there's no way to control
// or inspect its selection algorithm from the Nano side).
uint16_t pickTrack() {
  switch (CONFIG_MODE) {
    case MODE_SEQUENTIAL: {
      int total = player.readFileCounts();
      if (total <= 0) haltWithError();
      uint8_t stored = EEPROM.read(EEPROM_SEQ_INDEX_ADDR);
      uint8_t idx = resolveSequentialIndex(stored, (uint8_t)total);
      EEPROM.update(EEPROM_SEQ_INDEX_ADDR, nextSequentialIndex(idx, (uint8_t)total));
      return idx + 1; // DFPlayer track numbers are 1-based
    }
    case MODE_SHUFFLE: {
      int total = player.readFileCounts();
      if (total <= 0) haltWithError();
      if (total > SHUFFLE_MAX_TRACKS) return 0; // too many tracks to bitmask — fall back to DFPlayer's own random

      uint16_t mask = (uint16_t)EEPROM.read(EEPROM_SHUFFLE_MASK_ADDR) |
                      ((uint16_t)EEPROM.read(EEPROM_SHUFFLE_MASK_ADDR + 1) << 8);
      if (shuffleAllPlayed(mask, (uint8_t)total)) mask = 0;

      uint8_t candidateCount = 0;
      uint8_t selectedIdx = 0;
      for (uint8_t i = 0; i < (uint8_t)total; i++) {
        if (!shufflePlayed(mask, i)) {
          candidateCount++;
          if (reservoirShouldReplace(candidateCount, random)) selectedIdx = i;
        }
      }

      uint16_t newMask = shuffleMarkPlayed(mask, selectedIdx);
      EEPROM.update(EEPROM_SHUFFLE_MASK_ADDR,     (uint8_t)(newMask & 0xFF));
      EEPROM.update(EEPROM_SHUFFLE_MASK_ADDR + 1, (uint8_t)(newMask >> 8));
      return selectedIdx + 1;
    }
    case MODE_SINGLE:
      return CONFIG_SINGLE_TRACK;
    case MODE_RANDOM:
    default:
      return 0;
  }
}

void waitForPlaybackToFinish() {
  unsigned long deadline = millis() + PLAYBACK_WATCHDOG_MS;
  while (digitalRead(BUSY_PIN) == LOW) {
    digitalWrite(LED_BUILTIN, (millis() % 1000UL < 100UL) ? HIGH : LOW);
    if (millis() > deadline) haltWithError();
    delay(BUSY_POLL_MS);
  }
  digitalWrite(LED_BUILTIN, LOW);
}

// ~20mA active → ~0.1µA in power-down. Wakes on next ignition power cycle.
// Cuts the DFPlayer Mini's power via DFPLAYER_GATE_PIN first — without this,
// the DFPlayer keeps drawing its own idle current the whole time the Nano
// sleeps, which would undo most of the point of sleeping at all.
void enterPowerDownSleep() {
  digitalWrite(DFPLAYER_GATE_PIN, LOW);
  digitalWrite(LED_BUILTIN, LOW);
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  power_all_disable();
  sleep_mode();
  while (true);
}

// No DFPlayer detected, failed to initialize, no tracks found, or a
// play-through never finished within the watchdog: blink forever.
void haltWithError() {
  while (true) {
    for (uint8_t cycle = 0; cycle < ERROR_BLINK_CYCLES; cycle++) {
      digitalWrite(LED_BUILTIN, HIGH);
      delay(BLINK_DURATION_MS);
      digitalWrite(LED_BUILTIN, LOW);
      delay(BLINK_DURATION_MS);
    }
    delay(BLINK_PAUSE_MS);
  }
}
