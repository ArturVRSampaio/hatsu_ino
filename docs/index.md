---
title: Home
nav_order: 1
---

# hatsu_ino

> **hatsu** (初, first sound) + **ino** (Arduino) — the first sound your car makes.

An Arduino-based JDM car melody box that plays a WAV audio file when you start your car. When the ignition turns on, it picks a track from an SD card, plays it through an amplifier, and enters deep sleep — drawing ~0.1µA until the next start.

## Boards

| Version | Description |
|---|---|
| [v0](boards/v0/) | Arduino Nano + PAM8403 amplifier, first production run |
| [v1](boards/v1/) | v0 + shutdown sound via supercapacitor hold-up |

## Source

[github.com/ArturVRSampaio/hatsu_ino](https://github.com/ArturVRSampaio/hatsu_ino)
