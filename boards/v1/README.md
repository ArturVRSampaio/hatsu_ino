# hatsu_ino v1

> **hatsu** (初, first sound) + **ino** (Arduino) — the first sound your car makes.

> **Confirmed working on real hardware** (Nano + DFPlayer Mini clone + 3W/4Ω speaker): boots, reads the SD card, plays a random track, and powers the DFPlayer back off before sleeping. The 3D-printed case dimensions are still unverified against the real parts — see the case section below.

A JDM car melody box that plays an MP3 file when you start your car — built around a DFPlayer Mini module instead of driving audio from the Nano itself.

## What it does

When your car's ignition turns on, the Nano wakes the DFPlayer Mini, picks a track according to the configured mode (see Configuration below), waits for playback to finish, then enters deep sleep until the next ignition cycle.

Before waking the DFPlayer, the Nano checks its own 5V rail using the ATmega328's internal 1.1V bandgap reference — no extra hardware needed. If the rail reads below `LOW_VOLTAGE_THRESHOLD_MV` (default 4500mV, set in `hatsu_v1.ino`), it halts with the standard error blink instead of booting the DFPlayer on a sagging supply, which is more likely to produce garbled audio or an unreliable boot than a clean, visible failure.

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
| `CONFIG_EQ` | `DFPLAYER_EQ_NORMAL` / `_POP` / `_ROCK` / `_JAZZ` / `_CLASSIC` / `_BASS` | `DFPLAYER_EQ_NORMAL` | Equalizer preset applied to the DFPlayer's built-in amp |

**MODE_RANDOM** — the Nano reads the SD card's track count and picks a random track number itself, then calls `play(N)`. This used to rely on the DFPlayer's own `randomAll()` command instead, but that proved unreliable on real hardware — see "Known hardware quirks" below.

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
      ┌────────────────────┐        │                                 │  ││
      │         Q1 MOSFET  │        │                                 │  ││
      │                    │        │                                 │  ││
      │  G       D     S   │        │                                 │  ││
      └──┼───────┼─────┼───┘        │                                 │  ││
         │       │   GND rail       │                                 │  ││
         │       │                  │                                 │  ││
         │       └──────────────────┼─────────────────────────────────┘  ││
         └──────────────────────────┘                                    ││
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

The DFPlayer has its own serial standby command (`0x0A`, wrapped by the library as `sleep()`), but it doesn't cut the module's own power — it only switches its internal state, and these modules (YX5200/GD3200B-based) still pull roughly 15-20mA in that state. Left connected like that on a parked car, that alone could flatten a typical 50Ah battery in about 3.5-4.6 months on its own, and likely much sooner in practice once stacked on the car's existing baseline parasitic draw (ECU memory, alarm, radio presets, etc.). Physically gating the DFPlayer's ground return instead drops its contribution to roughly the same ~0.1µA as the sleeping Nano — at that level our circuit's own leakage is irrelevant next to the battery's normal chemical self-discharge.

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

`MODE_RANDOM` reads the card's total file count and picks a track number itself (see "Known hardware quirks" below for why). File naming isn't as strict as v0's 8.3 requirement, but numbered names (`0001.mp3`, `0002.mp3`, etc.) are the most reliable convention across DFPlayer Mini clones — some cheap modules are picky about file ordering/naming.

## Known hardware quirks

Found during bring-up on a real DFPlayer Mini clone — worth knowing if you hit similar symptoms:

- **`randomAll()` doesn't stop.** The DFPlayer's own random-play command kept auto-advancing through every track on the card indefinitely, regardless of anything the Nano did afterward — the firmware's watchdog would fire (since it never saw BUSY go idle) while the module kept right on playing. `hatsu_v1.ino` no longer uses it: `MODE_RANDOM` instead reads the track count and calls `play(N)` on a Nano-picked random track, which behaves like any other single-track play.
- **Loop-all can be on by default.** Some clones boot with loop-all playback enabled. `hatsu_v1.ino` calls `player.disableLoopAll()` right after `player.begin()` as a precaution.
- **Query commands are flakier than playback commands.** Fire-and-forget commands like `play()` were reliable, but commands that wait for a response (`readFileCounts()` in particular) failed intermittently on the first attempt. `hatsu_v1.ino` retries these up to 5 times (`QUERY_RETRY_COUNT`) before giving up.
- **Tracks are longer than you'd guess from a "chime."** `PLAYBACK_WATCHDOG_MS` originally defaulted to 8 seconds (sized for a short startup jingle) and falsely tripped on real songs. It's now 60 seconds — adjust it in `hatsu_v1.ino` if your longest track runs past that.

## 3D-printable case

A parametric OpenSCAD enclosure lives at `case/case.scad`, following the same two-piece screw-together design as v0: a flat back for VHB tape mounting, a speaker grille cut into the lid, and a microSD slot + USB cutout sharing one end wall (the Nano and DFPlayer Mini both mount flush against that wall so their connectors line up with the cutouts). No amp box needed — the DFPlayer Mini's built-in amp drives the speaker directly, so the layout is simpler than v0's. Same lid-logo/floor-signature setup as v0 too, via the `lid_label_text` / `sig_text` variables.

**The dimensions in the file are estimates, not measurements — verify before printing.** Unlike v0 (whose case dimensions were confirmed against real hardware), v1's case was designed before the DFPlayer Mini module and speaker arrived. The DFPlayer Mini's footprint (assumed ~20×20mm) and microSD slot position especially vary between clones more than most breakout boards, and the speaker size (assumed ~50mm/3W/4Ω) hasn't been confirmed against an actual part yet. Measure both with calipers and adjust the variables at the top of `case.scad` once you have them in hand — the whole design was checked for internal consistency (no overlapping parts, wall cutouts land where expected) but not against real components.

**Render and export STL** (requires [OpenSCAD](https://openscad.org/)):
```bash
openscad -o base.stl -D 'part="base"' case/case.scad
openscad -o lid.stl  -D 'part="lid"'  case/case.scad
```

**Suggested print settings:** PLA or PETG, 0.2mm layer height, 3 perimeters, 20% infill, no supports needed for the base or lid as designed.

## Shield PCB (optional)

Instead of wiring the Nano, DFPlayer Mini, R1/R2, and Q1 together with loose jumper wires, `pcb/generate_board.py` generates a small shield PCB: female headers on one side plug directly onto the Nano's own pins, header sockets on the other side accept the DFPlayer Mini, and R1/R2/Q1/the speaker terminal are built into the board itself. It's optional — the loose-wire build documented above works fine — this just gets rid of the wiring for anyone who'd rather have a compact, solderable board.

The board is generated (not hand-drawn) via [KiCad](https://www.kicad.org/)'s `pcbnew` Python API, from the same pin mappings and wiring documented in this README, cross-checked against Arduino's official Nano pinout and the DFPlayer Mini's datasheet pin table rather than guessed. It's DRC-clean (0 errors) — verified with `kicad-cli pcb ... ` reports since this KiCad install doesn't have the `pcb drc` CLI subcommand, so DRC was run via `pcbnew.WriteDRCReport()` instead.

**Regenerate the board:**
```bash
cd pcb
python3 generate_board.py    # writes hatsu_v1_shield.kicad_pcb
```
Open `hatsu_v1_shield.kicad_pcb` in KiCad to inspect, tweak, or export Gerbers for fabrication.

**Known limitation:** this hasn't been fabricated or tested on real hardware — it's a from-scratch layout verified only via DRC and a geometric routing checker (`check_routes.py`), not by actually populating and powering a board. Confirm continuity with a multimeter before trusting it with real hardware, and re-check the DFPlayer Mini header footprint's 2.0mm-vs-2.54mm pin pitch against your actual module (see "Known hardware quirks" — this was inferred from a breadboard fit test, not a caliper measurement). If you build this shield, the case dimensions above will also need rechecking — they were designed around the loose-wire build's component footprint, not this board's.

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
