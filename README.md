# TobyTank

TobyTank is planned as standalone ESP-IDF firmware for the Waveshare ESP32-S3-Touch-AMOLED-1.8. The product turns the 368x448 RGB565 AMOLED into a calm procedural aquarium where exactly one unique visitor fish appears at a time, visits once, leaves forever, and becomes a saved Fishbook memory.

This repository is pre-Milestone-0. No TobyTank firmware has been implemented, built, flashed, or physically validated yet. `plan.txt` is retained as historical source material; `PRODUCT_SPEC.md`, `ROADMAP.md`, and `DEVELOPMENT.md` are the durable planning documents.

## Target Hardware

- Waveshare ESP32-S3-Touch-AMOLED-1.8.
- ESP32-S3R8 with 8 MB PSRAM and 16 MB flash.
- 368x448 RGB565 AMOLED.
- Managed BSP: `waveshare/esp32_s3_touch_amoled_1_8`.
- ESP-IDF v5.5.5, C/C++, and FreeRTOS. Arduino is not used.

## Status

- Current milestone: Milestone 0, ESP-IDF foundation and display proof.
- Firmware status: not implemented.
- Host tests: not implemented.
- ESP-IDF build: not run for TobyTank.
- Hardware validation: not performed for TobyTank.

## Architecture

The intended architecture keeps hardware access, deterministic simulation, rendering, persistence, input, audio, and optional connectivity separate:

- `main/hardware/` for BSP-backed board/display/RTC boundaries.
- `main/aquarium/` for environment simulation and visitor lifecycle.
- `main/fish/` for identity, genome, behavior, motion, and portraits.
- `main/render/` for RGB565 drawing, aquarium effects, fish rasterization, and compositing.
- `main/memory/` for NVS-backed identity and compact Fishbook persistence.
- `main/input/` for touch and IMU normalization.
- `main/audio/` for speaker and microphone ownership.
- `main/net/` and `main/storage/` for optional Wi-Fi/NTP/gallery and microSD archive milestones.
- `tests/` and `tools/preview/` for host validation and rendering previews.

## Build

Placeholder until Milestone 0 creates the ESP-IDF project.

Expected PowerShell setup:

```powershell
. 'C:\Espressif\tools\Microsoft.v5.5.5.PowerShell_profile.ps1'
idf.py set-target esp32s3
ninja -C build -j4 all
```

## Flash

Do not flash TobyTank until a roadmap hardware checkpoint is explicitly authorized.

Future manual command placeholder:

```powershell
idf.py -p COM3 flash monitor
```

Never erase full flash, alter eFuses, or change boot security.

## Test

Placeholder until Milestone 0 adds host tests.

Expected direction:

```powershell
# Run host tests from the tests/ directory once they exist.
```

Host tests should cover deterministic pure logic before production behavior changes, and an ESP-IDF build must pass before a milestone is declared done.
