#include <AUnit.h>
#include "logic.h"

test(isWav_valid_uppercase)    { assertTrue(isWav("SONG.WAV")); }
test(isWav_valid_lowercase)    { assertTrue(isWav("song.wav")); }
test(isWav_valid_mixedcase)    { assertTrue(isWav("Song.Wav")); }
test(isWav_valid_min_name)     { assertTrue(isWav("A.WAV")); }      // shortest valid: 1 char name
test(isWav_valid_8char_name)   { assertTrue(isWav("ABCDEFGH.WAV")); } // max 8.3 name length

test(isWav_invalid_no_ext)       { assertFalse(isWav("MELODY")); }
test(isWav_invalid_wrong_ext)    { assertFalse(isWav("SONG.MP3")); }
test(isWav_invalid_long_ext)     { assertFalse(isWav("SONG.WAVE")); } // 4-char extension
test(isWav_invalid_empty)        { assertFalse(isWav("")); }
test(isWav_invalid_only_ext)     { assertFalse(isWav(".WAV")); }      // no name part (len == 4, not > 4)
test(isWav_invalid_dot_only)     { assertFalse(isWav(".")); }

void setup() {
  Serial.begin(115200);
  while (!Serial);
}

void loop() {
  aunit::TestRunner::run();
}
