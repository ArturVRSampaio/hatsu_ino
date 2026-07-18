# Contributing

Thanks for considering a contribution to hatsu_ino.

## Ways to help

- **New board versions** — if you build a variant with different hardware (different amp, different microcontroller, different power setup), it can live alongside `boards/v0/` as its own `boards/<version>/` directory. See `CLAUDE.md` for the expected layout.
- **Bug fixes** — SD card edge cases, config parsing quirks, wiring corrections in the docs.
- **Documentation** — clearer wiring instructions, better troubleshooting steps, photos/diagrams.
- **Show your build** — open a [Discussion](../../discussions) with photos or a clip of your own hatsu_ino. Seeing real builds helps everyone else.

## Before opening a PR

1. **Write tests first, or immediately after, any logic change.** Every line added to a board's `logic.h` needs coverage in that board's `test/native/test_logic.cpp`.
2. **Run the native test suite** and confirm everything passes:
   ```bash
   cmake -S boards/v0/test/native -B boards/v0/test/native/build
   cmake --build boards/v0/test/native/build
   ./boards/v0/test/native/build/tests
   ```
3. **Update the board's README.md and CONFIG.TXT** if your change affects wiring, hardware, behavior, or config keys.
4. Keep PRs focused — one change per PR is easier to review than a bundle of unrelated fixes.

## Reporting issues

Open a GitHub Issue with what you expected vs. what happened. For hardware issues, include your wiring (a photo helps) and your `CONFIG.TXT`.
