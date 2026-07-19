# hatsu_ino

[![Static Analysis](https://github.com/ArturVRSampaio/hatsu_ino/actions/workflows/cppcheck.yml/badge.svg)](https://github.com/ArturVRSampaio/hatsu_ino/actions/workflows/cppcheck.yml)
[![Unit Tests](https://github.com/ArturVRSampaio/hatsu_ino/actions/workflows/native-tests.yml/badge.svg)](https://github.com/ArturVRSampaio/hatsu_ino/actions/workflows/native-tests.yml)
[![Arduino Build](https://github.com/ArturVRSampaio/hatsu_ino/actions/workflows/arduino-build.yml/badge.svg)](https://github.com/ArturVRSampaio/hatsu_ino/actions/workflows/arduino-build.yml)
[![Coverage](https://codecov.io/gh/ArturVRSampaio/hatsu_ino/branch/master/graph/badge.svg)](https://codecov.io/gh/ArturVRSampaio/hatsu_ino)
[![Deploy Docs](https://github.com/ArturVRSampaio/hatsu_ino/actions/workflows/pages.yml/badge.svg)](https://github.com/ArturVRSampaio/hatsu_ino/actions/workflows/pages.yml)
[![License: MIT](https://img.shields.io/github/license/ArturVRSampaio/hatsu_ino)](LICENSE)

> **hatsu** (初, first sound) + **ino** (Arduino) — the first sound your car makes.

A JDM car melody box that plays a WAV audio file when you start your car.

If you build one (or fork it for your own board), a ⭐ helps other people find this project.

## Repository layout

```
boards/
  v0/
    hatsu_v0/    # Arduino sketch
      logic.h    # board logic (tested natively, no hardware dependency)
    test/        # native unit test suite (Catch2 + CMake)
    README.md    # wiring, components, and setup for v0
    CONFIG.TXT   # example SD card config file for v0
  v1/
    hatsu_v1/    # Arduino sketch
    README.md    # wiring, components, and setup for v1
```

## Boards

| Version | Description |
|---|---|
| [v0](boards/v0/README.md) | Arduino Nano + PAM8403 amplifier, first production run *(deprecated in favor of v1)* |
| [v1](boards/v1/README.md) | Arduino Nano + DFPlayer Mini (self-contained SD/decode/amp module) *(work in progress)* |

## Development

### Native tests

```bash
cmake -S boards/v0/test/native -B boards/v0/test/native/build
cmake --build boards/v0/test/native/build
./boards/v0/test/native/build/tests
```

### Arduino build

Compile for Arduino Nano (ATmega328P old bootloader):

```bash
arduino-cli compile --fqbn arduino:avr:nano:cpu=atmega328old boards/v0/hatsu_v0
arduino-cli compile --fqbn arduino:avr:nano:cpu=atmega328old boards/v1/hatsu_v1
```

## License

[MIT](LICENSE)
