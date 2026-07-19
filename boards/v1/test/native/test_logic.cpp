#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>
#include "logic.h"

// --- resolveSequentialIndex / nextSequentialIndex ---

TEST_CASE("resolveSequentialIndex returns stored index when in range", "[sequential]") {
    REQUIRE(resolveSequentialIndex(0, 5) == 0);
    REQUIRE(resolveSequentialIndex(3, 5) == 3);
    REQUIRE(resolveSequentialIndex(4, 5) == 4);
}

TEST_CASE("resolveSequentialIndex wraps to 0 when stored is out of range", "[sequential]") {
    REQUIRE(resolveSequentialIndex(5, 5) == 0);
    REQUIRE(resolveSequentialIndex(255, 5) == 0);
}

TEST_CASE("resolveSequentialIndex returns 0 when total is 0", "[sequential]") {
    REQUIRE(resolveSequentialIndex(0, 0) == 0);
    REQUIRE(resolveSequentialIndex(3, 0) == 0);
}

TEST_CASE("nextSequentialIndex advances by one", "[sequential]") {
    REQUIRE(nextSequentialIndex(0, 5) == 1);
    REQUIRE(nextSequentialIndex(3, 5) == 4);
}

TEST_CASE("nextSequentialIndex wraps around at the end", "[sequential]") {
    REQUIRE(nextSequentialIndex(4, 5) == 0);
}

TEST_CASE("nextSequentialIndex returns 0 when total is 0", "[sequential]") {
    REQUIRE(nextSequentialIndex(0, 0) == 0);
    REQUIRE(nextSequentialIndex(3, 0) == 0);
}

// --- Shuffle helpers ---

TEST_CASE("shufflePlayed reflects the bit at the given index", "[shuffle]") {
    uint16_t mask = 0;
    REQUIRE_FALSE(shufflePlayed(mask, 0));
    mask = shuffleMarkPlayed(mask, 0);
    REQUIRE(shufflePlayed(mask, 0));
    REQUIRE_FALSE(shufflePlayed(mask, 1));
}

TEST_CASE("shuffleMarkPlayed sets only the targeted bit", "[shuffle]") {
    uint16_t mask = shuffleMarkPlayed(0, 3);
    REQUIRE(mask == 0b1000);
    mask = shuffleMarkPlayed(mask, 5);
    REQUIRE(mask == 0b101000);
}

TEST_CASE("shuffleAllPlayed is false until every track's bit is set", "[shuffle]") {
    uint16_t mask = 0;
    REQUIRE_FALSE(shuffleAllPlayed(mask, 3));
    mask = shuffleMarkPlayed(mask, 0);
    mask = shuffleMarkPlayed(mask, 1);
    REQUIRE_FALSE(shuffleAllPlayed(mask, 3));
    mask = shuffleMarkPlayed(mask, 2);
    REQUIRE(shuffleAllPlayed(mask, 3));
}

TEST_CASE("shuffleAllPlayed ignores bits beyond total", "[shuffle]") {
    // all of tracks 0-2 played, but bit 5 (out of range for total=3) also happens to be set
    uint16_t mask = 0b100111;
    REQUIRE(shuffleAllPlayed(mask, 3));
}

TEST_CASE("shuffleAllPlayed is false for total of 0 or more than SHUFFLE_MAX_TRACKS", "[shuffle]") {
    REQUIRE_FALSE(shuffleAllPlayed(0xFFFF, 0));
    REQUIRE_FALSE(shuffleAllPlayed(0xFFFF, SHUFFLE_MAX_TRACKS + 1));
}

// --- reservoirShouldReplace ---

TEST_CASE("reservoirShouldReplace always replaces on the first candidate", "[reservoir]") {
    REQUIRE(reservoirShouldReplace(1, [](long) -> long { return 0; }));
}

TEST_CASE("reservoirShouldReplace follows the injected random function", "[reservoir]") {
    REQUIRE(reservoirShouldReplace(4, [](long n) -> long { return n - 1; }) == false);
    REQUIRE(reservoirShouldReplace(1, [](long n) -> long { return n - 1; }) == true);
}
