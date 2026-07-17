// Hardware smoke test: plays a 1kHz tone through the speaker for 5s, then stops.
// Verifies the D9 -> capacitor -> PAM8403 -> speaker audio path independent of the SD card.

const uint8_t      SPEAKER_PIN     = 9;
const unsigned int  TONE_FREQ_HZ    = 1000;
const unsigned long TONE_DURATION_MS = 5000;

void setup() {
  tone(SPEAKER_PIN, TONE_FREQ_HZ, TONE_DURATION_MS);
  delay(TONE_DURATION_MS);
  noTone(SPEAKER_PIN);
}

void loop() {}
