# hatsu_ino v1

[![Static Analysis](https://github.com/ArturVRSampaio/hatsu_ino/actions/workflows/cppcheck.yml/badge.svg)](https://github.com/ArturVRSampaio/hatsu_ino/actions/workflows/cppcheck.yml)
[![Unit Tests](https://github.com/ArturVRSampaio/hatsu_ino/actions/workflows/native-tests.yml/badge.svg)](https://github.com/ArturVRSampaio/hatsu_ino/actions/workflows/native-tests.yml)
[![Arduino Build](https://github.com/ArturVRSampaio/hatsu_ino/actions/workflows/arduino-build.yml/badge.svg)](https://github.com/ArturVRSampaio/hatsu_ino/actions/workflows/arduino-build.yml)
[![Coverage](https://codecov.io/gh/ArturVRSampaio/hatsu_ino/branch/master/graph/badge.svg)](https://codecov.io/gh/ArturVRSampaio/hatsu_ino)
[![Deploy Docs](https://github.com/ArturVRSampaio/hatsu_ino/actions/workflows/pages.yml/badge.svg)](https://github.com/ArturVRSampaio/hatsu_ino/actions/workflows/pages.yml)

> **Work in progress — do not use until officially released.**

> **hatsu** (初, first sound) + **ino** (Arduino) — the first and last sound your car makes.

Extends [v0](../v0/README.md) with a shutdown sound: when the ignition turns off, the device detects the power loss via a supercapacitor hold-up circuit, plays a farewell track, then enters deep sleep.

## What it does

| Event | Behaviour |
|---|---|
| Ignition ON | Plays a startup WAV from the SD card (same modes as v0: random, sequential, shuffle, single) |
| Ignition OFF | Detects voltage drop on the sense pin, plays a shutdown WAV from the `SHUTDOWN/` folder on the SD card, then sleeps |

The supercapacitor keeps the board alive long enough to finish playback after the ignition line cuts. Once the track ends (or if no shutdown track is found), the device enters deep sleep drawing ~0.1µA.

## Components

### Same as v0

| Component | Description |
|---|---|
| Arduino Nano Compatible V3 ATmega328 CH340 | Main microcontroller |
| Micro SD Card Reader Module for Arduino | Reads WAV files via SPI |
| PAM8403 Mini Digital Amplifier 2x3W 5V | Drives the speaker |
| Mini Speaker 0.5W 8Ω 40mm | Audio output |
| LM2596 Adjustable Step-Down Buck Converter 3A | Steps 12V car power down to 5V |
| Electrolytic Capacitor 10µF 50V 105°C | Audio coupling between D9 and PAM8403 IN+ |
| Micro SD card | Stores WAV files (FAT32 formatted) |

### New in v1

| Component | Description |
|---|---|
| Supercapacitor 1F 5.5V | Hold-up power after ignition cuts — keeps the board alive during shutdown playback |
| Schottky diode 1N5819 | Isolates LM2596 from supercapacitor; prevents back-feed when ignition drops |
| Resistor 47kΩ | Voltage divider upper leg — ignition sense input |
| Resistor 22kΩ | Voltage divider lower leg — ignition sense input |

### Extra parts

| Component | Description |
|---|---|
| Jumper wires / proto board | Connecting components |
| Soldering iron | For permanent connections |
| USB Type-A to Mini-B cable | Uploading firmware to the Nano |
| Car wiring tap or splice connector | Connecting to the ignition-switched power line |

## Code structure

| File | Role |
|---|---|
| `hatsu_v1.ino` | Arduino entry point — hardware setup, SD init, ignition sense, sleep, LED error codes |
| `logic.h` | All testable domain logic — config parsing, track selection, WAV validation, EEPROM index math. Pure C++; no Arduino dependencies. |
| `test/native/test_logic.cpp` | Host-side Catch2 tests for everything in `logic.h` |

## How it works

1. Ignition ON → 12V → LM2596 → 5V rail → Arduino powers up
2. LM2596 also charges the supercapacitor through the Schottky diode
3. Arduino reads the ignition sense pin (D2) — it reads HIGH while the car is running
4. Startup track plays (same behaviour as v0)
5. Ignition OFF → 12V line drops → sense pin goes LOW → Arduino detects shutdown
6. Supercapacitor takes over, powering the board for several seconds
7. Shutdown track plays from the `SHUTDOWN/` folder on the SD card
8. Board enters deep sleep (~0.1µA) until the next ignition cycle

> The supercapacitor hold-up time depends on the track length and current draw. A 1F capacitor provides roughly 5–10 seconds of playback at typical load. Keep shutdown tracks short.

## Assembly

### Circuit overview

```
   CAR 12V+ ──┬──────────────────────────────────────────────────────────────────────
              │                                             │
              │   ┌──────────────┐                       [47kΩ]
              └──►│    LM2596    ├── [1N5819 ►|] ──┬── (5V rail)    Arduino D2 (sense)
   CAR GND  ────►│  BUCK CONV   ├─────────────────┼── (GND rail)         │
                  └──────────────┘          [1F SUPERCAP]              [22kΩ]
                                                   │                      │
                                                  GND                    GND

                                          ┌──────────────────────────┐
   ┌──────────────────────────────┐        │       SD MODULE          │
   │        ARDUINO NANO          │        │                          │
   │                              │        │  VCC  ◄──── 5V rail      │
   │  5V  ◄──── 5V rail           │        │  GND  ◄──── GND rail     │
   │  GND ◄──── GND rail          │        │                          │
   │  D2  ◄──── ignition sense    │        │                          │
   │  D4  ──────────────────────────────────► CS                      │
   │  D11 ──────────────────────────────────► MOSI                    │
   │  D12 ◄─────────────────────────────────  MISO                    │
   │  D13 ──────────────────────────────────► SCK                     │
   │                              │        └──────────────────────────┘
   │  D9  ──── [+ 10µF ─] ─────────────────────────────────────────────────────┐
   │  GND ──────────────────────────────────────────────────────────────────┐  │
   └──────────────────────────────┘                                         │  │
                                                                            │  │
   ┌──────────────────────────────┐                                         │  │
   │           PAM8403            │                                         │  │
   │                              │                                         │  │
   │  5V+  ◄──── 5V rail          │                                         │  │
   │  GND  ◄──── GND rail         │                                         │  │
   │                              │                                         │  │
   │  R   ◄──────┐ (optional)     │                                         │  │
   │  L   ◄──────┴──────────────────────────────────────────────────────────┘  │
   │  5V─ ◄────────────────────────────────────── GND rail ────────────────────┘
   │                              │                                ┌──────────────────┐
   │                              │                                │     SPEAKER      │
   │                              │                                │                  │
   │  L+  ──────────────────────────────────────────────────────────────► POSITIVE    │
   │  L─  ──────────────────────────────────────────────────────────────► NEGATIVE    │
   │                              │      ┌──────────────────┐      └──────────────────┘
   │  (optional)                  │      │     SPEAKER      │
   │  R─  ────────────────────────────────────► POSITIVE    │
   │  R+  ────────────────────────────────────► NEGATIVE    │
   │                              │      └──────────────────┘
   └──────────────────────────────┘
```

> The Schottky diode (1N5819) sits between the LM2596 output and the 5V rail. When ignition cuts, the diode prevents the supercapacitor from back-feeding into the regulator. The supercapacitor then powers the 5V rail alone until the board sleeps.
>
> The voltage divider (47kΩ / 22kΩ) brings the 12V ignition line down to ~3.8V — safe for the Arduino input pin and readable as HIGH. When ignition cuts, the pin reads LOW.

---

### Before you start

Set the LM2596 output to exactly **5.0V** before connecting anything else — same procedure as v0. See the [v0 assembly guide](../v0/README.md#before-you-start--choose-your-power-source) for step-by-step instructions.

---

### Step 1 — Power

| From | To |
|---|---|
| Car 12V+ | LM2596 IN+ |
| Car GND | LM2596 IN− |
| LM2596 OUT+ | 1N5819 anode |
| 1N5819 cathode | 5V rail |
| 1N5819 cathode | Supercapacitor **+** |
| Supercapacitor **−** | GND rail |
| 5V rail | Arduino Nano **5V** pin |
| 5V rail | SD module **VCC** |
| 5V rail | PAM8403 **VCC** |
| GND rail | Arduino Nano **GND** |
| GND rail | SD module **GND** |
| GND rail | PAM8403 **GND** |

> Powering the Nano through the **5V pin** (not VIN) bypasses the onboard regulator — correct when supplying a clean 5V externally.

---

### Step 2 — Ignition sense

| From | To |
|---|---|
| Car 12V+ | 47kΩ resistor (one leg) |
| 47kΩ resistor (other leg) | Arduino **D2** |
| Arduino **D2** | 22kΩ resistor (one leg) |
| 22kΩ resistor (other leg) | GND rail |

---

### Step 3 — SD card module (SPI)

Same as v0:

| SD module pin | Arduino Nano pin |
|---|---|
| VCC | 5V rail |
| GND | GND rail |
| CS | D4 |
| MOSI | D11 |
| MISO | D12 |
| SCK | D13 |

---

### Step 4 — Audio

Same as v0:

| From | To |
|---|---|
| Nano **D9** | Capacitor **+** leg |
| Capacitor **−** leg | PAM8403 **L** |
| Nano **GND** | PAM8403 **⏚** |
| Nano **GND** | PAM8403 **5V−** |
| PAM8403 **5V+** | 5V rail |
| PAM8403 **L+** | Speaker **(+)** |
| PAM8403 **L−** | Speaker **(−)** |

---

### All connections at a glance

| Wire | From | To |
|---|---|---|
| 12V power | Car 12V+ | LM2596 IN+ |
| GND | Car GND | LM2596 IN− |
| 5V rail | LM2596 OUT+ → 1N5819 → | Nano 5V, SD VCC, PAM8403 5V+, Supercap + |
| GND rail | LM2596 OUT− | Nano GND, SD GND, PAM8403 5V−, Supercap − |
| Ignition sense upper | Car 12V+ | 47kΩ → Nano D2 |
| Ignition sense lower | Nano D2 | 22kΩ → GND |
| SPI CS | Nano D4 | SD CS |
| SPI MOSI | Nano D11 | SD MOSI |
| SPI MISO | Nano D12 | SD MISO |
| SPI SCK | Nano D13 | SD SCK |
| Audio | Nano D9 | 10µF cap (+) → cap (−) → PAM8403 L |
| Audio GND | Nano GND | PAM8403 ⏚ and PAM8403 5V− |
| Speaker | PAM8403 L+ / L− | Speaker + / − |

## Uploading the firmware

Same procedure as v0 — see the [v0 firmware upload guide](../v0/README.md#uploading-the-firmware).

Select **Arduino Nano** / **ATmega328P (Old Bootloader)** and open `hatsu_v1.ino`.

## SD card

### Setup

1. Format as FAT32
2. Copy startup `.wav` files to the **root** of the SD card
3. Create a `SHUTDOWN/` folder and copy shutdown `.wav` files into it
4. Optionally create a `CONFIG.TXT` to customise behaviour

If no `SHUTDOWN/` folder exists or it is empty, the device skips the shutdown sound and sleeps immediately.

### WAV file requirements

Same as v0 — PCM WAV, 8-bit, mono, 8000–32000 Hz. Keep shutdown tracks short (under 10 seconds) to stay within supercapacitor hold-up time.

### File naming

Same 8.3 rules as v0 apply to both root and `SHUTDOWN/` files.

### CONFIG.TXT

All v0 keys apply to startup behaviour. New keys for shutdown:

| Key | Values | Default | Description |
|---|---|---|---|
| `VOLUME` | `0` – `4` | `4` | Playback volume for both startup and shutdown |
| `MODE` | `RANDOM` / `SEQUENTIAL` / `SINGLE` / `SHUFFLE` | `RANDOM` | Startup track selection mode |
| `DELAY` | `0` – `255` | `0` | Seconds to wait after power-up before playing startup track |
| `MIN_SIZE` | `0` – `255` | `0` | Skip WAV files smaller than N kilobytes |
| `PLAY_COUNT` | `1` – `255` | `1` | How many times to repeat the startup track |
| `TRACK` | any valid WAV filename | *(none)* | Startup file in `SINGLE` mode |
| `SHUTDOWN_MODE` | `RANDOM` / `SEQUENTIAL` / `SINGLE` / `SHUFFLE` | `RANDOM` | Shutdown track selection mode (picks from `SHUTDOWN/` folder) |
| `SHUTDOWN_TRACK` | any valid WAV filename | *(none)* | Shutdown file in `SINGLE` mode (must be in `SHUTDOWN/` folder) |

## Error codes

Same LED blink codes as v0, plus:

| Blinks | Meaning |
|---|---|
| 2 | SD card failed to initialize |
| 3 | No WAV files found, file missing, or invalid WAV format |
| 4 | SD root directory failed to open |
| 5 | Playback watchdog — track selected but never started playing |

Shutdown errors blink once then the device sleeps immediately to avoid draining the supercapacitor.

## Testing

### Native tests

```bash
cmake -S boards/v1/test/native -B boards/v1/test/native/build
cmake --build boards/v1/test/native/build
./boards/v1/test/native/build/tests
```

### On-hardware tests

Open `boards/v1/test/test_hatsu_v1/test_hatsu_v1.ino`, upload to the Nano, open Serial Monitor at **115200 baud**.
