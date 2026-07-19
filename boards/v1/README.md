# hatsu_ino v1

> **hatsu** (初, first sound) + **ino** (Arduino) — the first sound your car makes.

> **Work in progress.** The firmware here compiles but hasn't been tested on real hardware yet — the DFPlayer Mini module is still on order. Wiring and behavior may change once it's actually been built and tested. [v0](../v0/README.md) is the confirmed-working board in the meantime.

A JDM car melody box that plays an MP3 file when you start your car — built around a DFPlayer Mini module instead of driving audio from the Nano itself.

## What it does

When your car's ignition turns on, the Nano wakes the DFPlayer Mini, tells it to play a random track from its SD card, waits for playback to finish, then enters deep sleep until the next ignition cycle.

## Why v1 is different from v0

[v0](../v0/README.md) drives audio directly from the Nano — reading WAV files itself, generating PWM audio, and amplifying it externally. That works, but chasing audio-quality and power-delivery bugs through that whole chain was the majority of the effort in building it.

The DFPlayer Mini owns its own SD card, its own audio decoder, its own proper DAC, and its own built-in amplifier — all on one small, purpose-built module. The Nano's job shrinks down to sending simple commands over a 2-wire serial connection ("play a random track", "set volume") instead of doing file I/O and PWM audio generation itself. This sidesteps WAV header parsing, PWM filtering, and amp-input signal levels entirely — categories of bugs v0 spent a lot of time on.

**v1 intentionally starts minimal**: play one random track on power-up, then sleep. No `CONFIG.TXT`, no track-selection modes, no LED error codes yet — those can be added later the same way v0 grew them, once the basic build is confirmed working on real hardware.

## Components

| Component | Description | Code |
|---|---|---|
| Arduino Nano Compatible V3 ATmega328 CH340 | Main microcontroller | ATmega328P |
| DFPlayer Mini MP3 module | Reads MP3s from its own SD card, decodes, and amplifies | DFPlayer Mini |
| Mini Speaker 3W 4Ω | Audio output, driven directly by the DFPlayer's built-in amp | — |
| LM2596 Adjustable Step-Down Buck Converter 3A | Steps 12V car power down to 5V *(optional — only needed if powering from the 12V car line)* | LM2596 |
| Resistor 1kΩ | In series on the Nano TX → DFPlayer RX line (protects the DFPlayer's RX pin) | — |
| microSD card | Stores MP3 files (FAT32 formatted) | — |

### Extra parts needed

| Component | Description |
|---|---|
| Jumper wires / proto board | Connecting components |
| Soldering iron | For permanent connections |
| USB Type-A to Mini-B cable | Uploading firmware to the Nano |
| Car wiring tap or splice connector | Connecting to the ignition-switched power line |

## Code structure

| File | Role |
|---|---|
| `hatsu_v1.ino` | Arduino entry point — the entire firmware for now, since v1 has no board-specific pure logic yet. |

## A hard-won lesson from v0, carried forward

**Power the Nano through its 5V and GND pins directly from your external supply — not through USB.** v0's entire final debugging saga traced back to powering the board over USB, which is current-limited in ways that caused intermittent failures once real current draw was involved. Feed 5V/GND directly to the pins on the underside of the board from your actual power source (LM2596 output or a 5V accessory line), and only use USB for uploading firmware, never as the power source for a working install.

## Wiring

### Power

| From | To |
|---|---|
| 5V source + | Arduino Nano **5V** pin |
| 5V source + | DFPlayer Mini **VCC** |
| 5V source − | Arduino Nano **GND** |
| 5V source − | DFPlayer Mini **GND** |

> As above: feed the Nano's 5V/GND pins directly from your actual power source, not through its USB port.

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

## Uploading the firmware

Same process as v0:

1. Install the [Arduino IDE](https://www.arduino.cc/en/software) (version 2.x recommended)
2. Install the CH340 driver if needed (Windows/macOS — see [v0's README](../v0/README.md#2--install-the-ch340-driver) for details; Linux needs no action)
3. Arduino IDE → **Sketch → Include Library → Manage Libraries** → search `DFRobotDFPlayerMini` → install
4. File → Open → select `hatsu_v1.ino`
5. Board: **Arduino Nano**, Processor: **ATmega328P** (or **ATmega328P (Old Bootloader)** if upload fails with a sync error — try the other option)
6. Click **Upload**

## SD card

Format as FAT32, ≤32GB. Copy MP3 files to the root directory.

DFPlayer Mini's `randomAll()` command plays a random file from across the whole card. File naming isn't as strict as v0's 8.3 requirement, but numbered names (`0001.mp3`, `0002.mp3`, etc.) are the most reliable convention across DFPlayer Mini clones — some cheap modules are picky about file ordering/naming, so if random playback misbehaves, numbered filenames are the first thing to try.

## Testing

No native test suite yet — v1 currently has no board-specific pure logic (`logic.h`) to test, since track selection is handled by the DFPlayer module itself rather than Nano-side code. A native Catch2 suite will be added here once real logic (config parsing, track-selection modes, etc.) gets built, following the same pattern as [v0's test suite](../v0/test/native/).
