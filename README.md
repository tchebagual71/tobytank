# TobyTank

TobyTank is standalone ESP-IDF firmware for the Waveshare ESP32-S3-Touch-AMOLED-1.8. The product turns the 368x448 RGB565 AMOLED into a calm procedural aquarium where exactly one unique visitor fish appears at a time, visits once, leaves forever, and becomes a saved Fishbook memory.

Milestones 0 through 5 are complete. The repository is a buildable ESP-IDF project that brings up the AMOLED through the managed Waveshare BSP, renders a deterministic animated aquarium, reserves persistent visitor identity blocks in NVS, generates deterministic fish genomes from those identities, rasterizes procedural fish into cached RGB565 sprites, runs a one-fish visitor lifecycle, and handles gentle touch/tilt interaction state. `plan.txt` is retained as historical source material; `PRODUCT_SPEC.md`, `ROADMAP.md`, and `DEVELOPMENT.md` are the durable planning documents.

## Target Hardware

- Waveshare ESP32-S3-Touch-AMOLED-1.8.
- ESP32-S3R8 with 8 MB PSRAM and 16 MB flash.
- 368x448 RGB565 AMOLED.
- Managed BSP: `waveshare/esp32_s3_touch_amoled_1_8` (2.0.3 resolved).
- ESP-IDF v5.5.5, C/C++, and FreeRTOS. Arduino is not used.

## Status

- Current milestone: Milestone 6, Fishbook and persistent encounter records.
- Firmware status: Milestone 5 is implemented, builds clean, flashed, and hardware-validated.
- Host tests: implemented and passing (repository contract checks, canvas, environment/rendering, genome/identity, fish rasterizer, visitor lifecycle, and interactions).
- ESP-IDF build: succeeds for `esp32s3` with no TobyTank warnings.
- Hardware validation: Milestone 5 boots cleanly, reserves NVS identity blocks, initializes BSP touch, logs the IMU fallback, reports touch/ripple telemetry, runs the visitor lifecycle across multiple unique identities, and sustains roughly 17.7-20.0 FPS with no reboot loop. After a small visibility adjustment, touch ripples are noticeable enough and working correctly.

## Architecture

Hardware access, deterministic simulation, rendering, persistence, input, audio, and optional connectivity stay separate.

Present today:

- `main/hardware/` for the BSP-backed board and display boundary, including the display task that overlaps panel transfer with drawing.
- `main/aquarium/` for the seeded, fixed-timestep environment simulation and one-fish visitor lifecycle.
- `main/fish/` for persistent visitor identity allocation, deterministic PRNG streams, genome generation, validation, fingerprints, behavior planning, motion, and portrait entry points.
- `main/memory/` for the NVS-backed identity block reservation boundary.
- `main/input/` for touch polling, touch gesture filtering, IMU fallback, and motion filtering.
- `main/render/` for the RGB565 canvas, background, effects, particle rasterizers, fish sprite rasterizer, sprite cache, and compositor.
- `main/sim/` for immutable cross-module simulation snapshots.
- `main/diagnostics/` for boot and health reporting over serial.
- `tests/` for host validation.
- `tools/preview/` for rendering aquarium frames on the host as PPM images.

Planned for later milestones:

- `main/memory/` for compact Fishbook persistence.
- `main/audio/` for speaker and microphone ownership.
- `main/net/` and `main/storage/` for optional Wi-Fi/NTP/gallery and microSD archive milestones.

## Build

```powershell
. 'C:\Espressif\tools\Microsoft.v5.5.5.PowerShell_profile.ps1'
idf.py set-target esp32s3
ninja -C build -j4 all
```

On this machine the installed profile needs two repairs before `idf.py` runs; see "Toolchain Notes" in `DEVELOPMENT.md`.

## Flash

Flash only when a roadmap hardware checkpoint is explicitly authorized. The Milestone 0, 1, 3, and 5 checkpoints were authorized and flashed over `COM3`:

```powershell
idf.py -p COM3 flash monitor
```

Never erase full flash, alter eFuses, or change boot security.

## Test

```powershell
powershell -ExecutionPolicy Bypass -File tests\run_host_tests.ps1
```

That runs all seven host suites. They can also be run individually:

```powershell
python -m unittest discover -s tests -p "test_*.py"
powershell -ExecutionPolicy Bypass -File tests\run_canvas_host_test.ps1
powershell -ExecutionPolicy Bypass -File tests\run_environment_host_test.ps1
powershell -ExecutionPolicy Bypass -File tests\run_genome_host_test.ps1
powershell -ExecutionPolicy Bypass -File tests\run_fish_rasterizer_host_test.ps1
powershell -ExecutionPolicy Bypass -File tests\run_lifecycle_host_test.ps1
powershell -ExecutionPolicy Bypass -File tests\run_interactions_host_test.ps1
```

The C suites compile the real firmware sources for the host with gcc, clang, or an auto-discovered Visual Studio MSVC toolchain. Host tests cover deterministic pure logic and repository contracts, and an ESP-IDF build must pass before a milestone is declared done.

## Preview

```powershell
powershell -ExecutionPolicy Bypass -File tools\preview\run_preview.ps1
```

Renders the aquarium at the real 368x448 panel size for four seeds and three moments in time, writing binary PPM frames to `tools/preview/out/`. It runs the same environment, lifecycle, interaction, fish cache, compositor, and rasterizers as the firmware, so the tank can be inspected without flashing.

```powershell
powershell -ExecutionPolicy Bypass -File tools\preview\run_contact_sheet.ps1
```

Renders a 4x4 contact sheet of deterministic generated fish to `tools/preview/out/fish_contact_sheet.ppm`.
