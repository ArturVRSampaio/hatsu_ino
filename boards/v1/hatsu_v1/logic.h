#pragma once
#include <stdint.h>

const uint8_t SHUFFLE_MAX_TRACKS = 16; // bitmask fits two EEPROM bytes

// P(replace)=1/n for uniform selection; injectable fn enables deterministic testing.
using RandomFn = long (*)(long);

inline bool reservoirShouldReplace(uint8_t n, RandomFn randFn) {
  return randFn(n) == 0;
}

inline uint8_t resolveSequentialIndex(uint8_t stored, uint8_t total) {
  if (total == 0) return 0;
  return stored >= total ? 0 : stored;
}

inline uint8_t nextSequentialIndex(uint8_t current, uint8_t total) {
  if (total == 0) return 0;
  return (uint8_t)((current + 1) % total);
}

// --- Shuffle helpers (EEPROM bitmask, max SHUFFLE_MAX_TRACKS tracks) ---

inline bool shufflePlayed(uint16_t mask, uint8_t index) {
  return (mask & (uint16_t)((uint32_t)1 << index)) != 0;
}

inline uint16_t shuffleMarkPlayed(uint16_t mask, uint8_t index) {
  return mask | (uint16_t)((uint32_t)1 << index);
}

inline bool shuffleAllPlayed(uint16_t mask, uint8_t total) {
  if (total == 0 || total > SHUFFLE_MAX_TRACKS) return false;
  uint16_t fullMask = (uint16_t)(((uint32_t)1 << total) - 1);
  return (mask & fullMask) == fullMask;
}

enum PlayMode : uint8_t {
  MODE_RANDOM,
  MODE_SEQUENTIAL,
  MODE_SHUFFLE,
  MODE_SINGLE
};
