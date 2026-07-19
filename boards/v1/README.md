# hatsu_ino v1

> **hatsu** (初, first sound) + **ino** (Arduino) — the first sound your car makes.

> **Work in progress.** The firmware here compiles but hasn't been tested on real hardware yet — the DFPlayer Mini module is still on order. Wiring and behavior may change once it's actually been built and tested. [v0](../v0/README.md) is the confirmed-working board in the meantime.

A JDM car melody box that plays an MP3 file when you start your car — built around a DFPlayer Mini module instead of driving audio from the Nano itself.

## What it does

When your car's ignition turns on, the Nano wakes the DFPlayer Mini, picks a track according to the configured mode (see Configuration below), waits for playback to finish, then enters deep sleep until the next ignition cycle.

## Why v1 is different from v0

[v0](../v0/README.md) drives audio directly from the Nano — reading WAV files itself, generating PWM audio, and amplifying it externally. That works, but chasing audio-quality and power-delivery bugs through that whole chain was the majority of the effort in building it.

The DFPlayer Mini owns its own SD card, its own audio decoder, its own proper DAC, and its own built-in amplifier — all on one small, purpose-built module. The Nano's job shrinks down to sending simple commands over a 2-wire serial connection ("play track N", "set volume") instead of doing file I/O and PWM audio generation itself. This sidesteps WAV header parsing, PWM filtering, and amp-input signal levels entirely — categories of bugs v0 spent a lot of time on.

**No `CONFIG.TXT` support** — and this isn't a "not yet," it's architectural: the Nano has no direct filesystem access to the DFPlayer's SD card (only the 2-wire serial link), so it can't read a config file off it the way v0 could. Settings here are compile-time constants instead — see Configuration below. LED error codes still TODO.

## Components

| Component | Description | Code |
|---|---|---|
| Arduino Nano Compatible V3 ATmega328 CH340 | Main microcontroller | ATmega328P |
| DFPlayer Mini MP3 module | Reads MP3s from its own SD card, decodes, and amplifies — see [DFRobot's official docs](https://wiki.dfrobot.com/DFPlayer_Mini_SKU_DFR0299) (pinout, command protocol, specs) | DFPlayer Mini |
| Mini Speaker 3W 4Ω | Audio output, driven directly by the DFPlayer's built-in amp | — |
| LM2596 Adjustable Step-Down Buck Converter 3A | Steps 12V car power down to 5V *(optional — only needed if powering from the 12V car line)* | LM2596 |
| Resistor 1kΩ | In series on the Nano TX → DFPlayer RX line (protects the DFPlayer's RX pin) | — |
| N-channel logic-level MOSFET | Gates the DFPlayer Mini's power off during sleep, so it doesn't keep drawing current with the Nano powered down | IRLZ44N |
| Resistor 100-220Ω | In series on the MOSFET gate pin | — |
| microSD card | Stores MP3 files (FAT32 formatted) | — |

### Extra parts needed

| Component | Description |
|---|---|
| Jumper wires / proto board | Connecting components |
| Soldering iron | For permanent connections |
| USB Type-A to Mini-B cable | Uploading firmware and powering the Nano (via a car USB charger or accessory line for the final install) |
| Car wiring tap or splice connector | Connecting to the ignition-switched power line |

## Code structure

| File | Role |
|---|---|
| `hatsu_v1.ino` | Arduino entry point — hardware setup, DFPlayer control, power gating, sleep. |
| `logic.h` | Testable pure logic — sequential/shuffle index math (tested natively, no hardware dependency). Track numbers only, no filenames, since the Nano has no filesystem access on this board. |
| `test/native/test_logic.cpp` | Host-side Catch2 tests for everything in `logic.h`. |

## Configuration

v1 has no `CONFIG.TXT` (see "Why v1 is different from v0" above for why) — settings are compile-time constants at the top of `hatsu_v1.ino`, edited before uploading:

| Constant | Values | Default | Description |
|---|---|---|---|
| `CONFIG_MODE` | `MODE_RANDOM` / `MODE_SEQUENTIAL` / `MODE_SHUFFLE` / `MODE_SINGLE` | `MODE_RANDOM` | Track selection mode |
| `CONFIG_SINGLE_TRACK` | `1` – `255` | `1` | Track number to play in `MODE_SINGLE` (DFPlayer track numbers are 1-based) |
| `CONFIG_PLAY_COUNT` | `1` – `255` | `1` | How many times to play the chosen track before sleeping |
| `CONFIG_DELAY_SECONDS` | `0` – `255` | `0` | Seconds to wait after power-up before playing |

**MODE_RANDOM** — uses the DFPlayer's own `randomAll()` command. Its exact selection algorithm (whether it avoids repeats, etc.) is opaque from the Nano's side — no visibility or control beyond "play something."

**MODE_SEQUENTIAL** — plays tracks in numeric order (1, 2, 3, ...), advancing one per ignition cycle. Current position is stored in the Nano's EEPROM, same persistence approach as v0.

**MODE_SHUFFLE** — plays every track exactly once before any repeats, via an EEPROM bitmask (same logic as v0, just resolving to track numbers instead of filenames). Supports up to 16 tracks; if the DFPlayer reports more than that, falls back to `MODE_RANDOM`'s behavior for that cycle.

**MODE_SINGLE** — always plays `CONFIG_SINGLE_TRACK`.

## Wiring

### Circuit overview

```
   ┌─────────────────────── Option A — 12V car line (optional) ─────────────────────┐
   │                   ┌──────────────┐                                             │
   │   CAR 12V+ ──────►│    LM2596    ├──── 5V  ────────────────────── (5V rail)    │
   │   CAR GND  ──────►│  BUCK CONV   ├──── GND ────────────────────── (GND rail)   │
   │                   └──────────────┘                                             │
   └────────────────────────────────────────────────────────────────────────────────┘

                                           ┌───────────────────────┐
   ┌──────────────────────────────┐        │      DFPLAYER MINI    │
   │        ARDUINO NANO          │        │                       │
   │                              │        │  VCC  ◄──── 5V rail   │
   │  5V  ◄──── 5V rail or USB    │        │  GND  ───────────────────┐
   │  GND ◄──── GND rail          │        │                       │  │
   │  D2  ◄────────────────────────────────── TX                   │  │
   │  D3  ──[R1 1kΩ]────────────────────────► RX                   │  │
   │  D4  ◄────────────────────────────────── BUSY                 │  │
   │  D5  ──[R2 220Ω]───────────────┐       │                      │  │
   │                              │ │       │  SPK_1 ─────────────────┼──┐
   │  (USB) ◄── power             │ │       │  SPK_2 ─────────────────┼──┼┐
   └──────────────────────────────┘ │       └──────────────────────┘  │  ││
                                    │                                 │  ││
                                ┌───┼────────────────┐                │  ││
                                │ gate    Q1 MOSFET  │                │  ││
                                │              drain │◄───────────────┘  ││
                                │             source │                   ││
                                └──────────┬─────────┘                   ││
                                           ▼                             ││
                                       GND rail                          ││
                                                                         ││
                                            ┌──────────────────┐         ││
                                            │     SPEAKER      │         ││
                                            │                  │         ││
                                            │  POSITIVE ◄────────────────┘│
                                            │  NEGATIVE ◄─────────────────┘
                                            └──────────────────┘
```

> **Option A** (shown above) is the 12V car line via LM2596 — matches v0's setup if you're reusing that converter. **Option B (recommended for v1)**: skip the LM2596 entirely and power the Nano through its USB port instead (car USB charger or switched accessory line), letting the Nano's onboard regulator supply the 5V rail for the DFPlayer Mini. See the Power section below for the reasoning.
>
> **Q1** (N-channel MOSFET, e.g. IRLZ44N) gates the DFPlayer Mini's GND return — its drain connects to the DFPlayer's GND, its source connects to the actual GND rail, and its gate is driven by Nano D5 through resistor R2. This is what actually cuts the DFPlayer's power during sleep — see the "Power gate" section below for why it's needed.
>
> The 1kΩ resistor (R1) on the Nano's TX → DFPlayer RX line protects the DFPlayer's RX pin from the Nano's 5V logic level.

### Power

**v1 is designed to run entirely off USB power.** Unlike v0 — whose separate SD module and external amplifier pushed combined current draw high enough that USB power caused real problems — v1's total load (just the Nano plus the DFPlayer Mini, no separate amp or SD module) is low enough to comfortably power through the Nano's USB port.

| From | To |
|---|---|
| Nano **5V** pin (output from its onboard regulator, powered via USB) | DFPlayer Mini **VCC** |
| Nano **GND** | DFPlayer Mini **GND** |

For the car install, power the Nano's USB port from a car USB charger or switched USB accessory line. If you'd rather feed a clean external 5V directly instead (e.g. from the LM2596, bypassing the Nano's onboard regulator entirely), that still works too — just wire 5V/GND straight to the Nano's 5V/GND pins instead of through USB, same as v0's approach.

### DFPlayer Mini ↔ Nano (serial control)

| DFPlayer Mini pin | Arduino Nano pin |
|---|---|
| TX | D2 |
| RX | D3 *(through a 1kΩ resistor in series)* |
| BUSY | D4 |

> The 1kΩ resistor on the RX line protects the DFPlayer's RX pin from the Nano's 5V logic level. TX doesn't need one — the DFPlayer's 3.3V-ish TX output reads fine as a HIGH on the Nano's 5V-tolerant input.

### Speaker

| DFPlayer Mini pin | To |
|---|---|
| SPK_1 | Speaker **(+)** |
| SPK_2 | Speaker **(−)** |

The DFPlayer's built-in amp drives the speaker directly — no external amplifier needed.

### Power gate (DFPlayer sleep cutoff)

Without this, the DFPlayer Mini keeps drawing its own idle current the whole time the car's parked, even while the Nano is correctly asleep at ~0.1µA — undoing most of the point of sleeping at all.

| From | To |
|---|---|
| Nano **D5** | MOSFET **gate**, through a 100-220Ω resistor |
| DFPlayer Mini **GND** | MOSFET **drain** |
| MOSFET **source** | GND rail |

The Nano's own GND stays wired directly to the rail — only the DFPlayer Mini's ground return is gated. `hatsu_v1.ino` drives D5 high at boot (powering the DFPlayer on) and low right before sleeping (cutting it off), waking it again on the next ignition cycle.

## Uploading the firmware

Same process as v0:

1. Install the [Arduino IDE](https://www.arduino.cc/en/software) (version 2.x recommended)
2. Install the CH340 driver if needed (Windows/macOS — see [v0's README](../v0/README.md#2--install-the-ch340-driver) for details; Linux needs no action)
3. Arduino IDE → **Sketch → Include Library → Manage Libraries** → search [`DFRobotDFPlayerMini`](https://github.com/DFRobot/DFRobotDFPlayerMini) → install
4. File → Open → select `hatsu_v1.ino`
5. Board: **Arduino Nano**, Processor: **ATmega328P** (or **ATmega328P (Old Bootloader)** if upload fails with a sync error — try the other option)
6. Click **Upload**

## SD card

Format as FAT32, ≤32GB. Copy MP3 files to the root directory.

DFPlayer Mini's `randomAll()` command plays a random file from across the whole card. File naming isn't as strict as v0's 8.3 requirement, but numbered names (`0001.mp3`, `0002.mp3`, etc.) are the most reliable convention across DFPlayer Mini clones — some cheap modules are picky about file ordering/naming, so if random playback misbehaves, numbered filenames are the first thing to try.

## 3D-printable case

A parametric OpenSCAD enclosure lives at `case/case.scad`, following the same two-piece screw-together design as v0: a flat back for VHB tape mounting, a speaker grille cut into the lid, and a microSD slot + USB cutout sharing one end wall (the Nano and DFPlayer Mini both mount flush against that wall so their connectors line up with the cutouts). No amp box needed — the DFPlayer Mini's built-in amp drives the speaker directly, so the layout is simpler than v0's. Same lid-logo/floor-signature setup as v0 too, via the `lid_label_text` / `sig_text` variables.

**The dimensions in the file are estimates, not measurements — verify before printing.** Unlike v0 (whose case dimensions were confirmed against real hardware), v1's case was designed before the DFPlayer Mini module and speaker arrived. The DFPlayer Mini's footprint (assumed ~20×20mm) and microSD slot position especially vary between clones more than most breakout boards, and the speaker size (assumed ~50mm/3W/4Ω) hasn't been confirmed against an actual part yet. Measure both with calipers and adjust the variables at the top of `case.scad` once you have them in hand — the whole design was checked for internal consistency (no overlapping parts, wall cutouts land where expected) but not against real components.

**Render and export STL** (requires [OpenSCAD](https://openscad.org/)):
```bash
openscad -o base.stl -D 'part="base"' case/case.scad
openscad -o lid.stl  -D 'part="lid"'  case/case.scad
```

**Suggested print settings:** PLA or PETG, 0.2mm layer height, 3 perimeters, 20% infill, no supports needed for the base or lid as designed.

## Testing

### Native tests (CI)

Tests for the sequential/shuffle index math in `logic.h`, compiled and run on the host with [Catch2](https://github.com/catchorg/Catch2), same pattern as [v0's test suite](../v0/test/native/).

To run locally:
```bash
cmake -S test/native -B test/native/build
cmake --build test/native/build
./test/native/build/tests
```

Requires CMake ≥ 3.14 and a C++17 compiler. Catch2 is fetched automatically.
