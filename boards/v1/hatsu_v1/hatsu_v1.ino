#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h>
#include <avr/sleep.h>
#include <avr/power.h>

const uint8_t DFPLAYER_RX_PIN = 2;  // Nano RX <- DFPlayer TX
const uint8_t DFPLAYER_TX_PIN = 3;  // Nano TX -> DFPlayer RX (through a 1kΩ series resistor)
const uint8_t BUSY_PIN        = 4;  // DFPlayer BUSY: LOW while playing, HIGH when idle
const uint8_t DEFAULT_VOLUME  = 20; // 0-30

const unsigned long DFPLAYER_BOOT_DELAY_MS  = 1000; // let the module finish booting before sending commands
const unsigned long PLAYBACK_START_DELAY_MS = 500;  // give playback a moment to actually start before polling BUSY
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

  dfSerial.begin(9600);
  delay(DFPLAYER_BOOT_DELAY_MS);

  if (!player.begin(dfSerial)) haltWithError();

  player.volume(DEFAULT_VOLUME);
  player.randomAll();
  delay(PLAYBACK_START_DELAY_MS);

  while (digitalRead(BUSY_PIN) == LOW) {
    digitalWrite(LED_BUILTIN, (millis() % 1000UL < 100UL) ? HIGH : LOW);
    delay(BUSY_POLL_MS);
  }

  enterPowerDownSleep();
}

void loop() {}

// ~20mA active → ~0.1µA in power-down. Wakes on next ignition power cycle.
void enterPowerDownSleep() {
  digitalWrite(LED_BUILTIN, LOW);
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  power_all_disable();
  sleep_mode();
  while (true);
}

// No DFPlayer detected / failed to initialize: blink forever.
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
