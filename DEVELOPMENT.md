# TobyTank Development Notes

## Current Milestone

Current milestone: Milestone 0, ESP-IDF foundation and display proof.

No TobyTank firmware has been physically validated yet. This repository currently contains product, architecture, roadmap, and engineering-rule bootstrap documentation only.

## Architecture

Planned architecture separates hardware, deterministic simulation, rendering, persistence, input, audio, and optional connectivity:

- `main/hardware/`: board initialization, display boundary, RTC, and managed BSP interactions.
- `main/render/`: RGB565 canvas, aquarium background, fish rasterizer, compositing, dithering, particles, caustics, and display-frame preparation.
- `main/aquarium/`: environment simulation, visitor lifecycle scheduler, time ecology, and interaction effects.
- `main/fish/`: identity, PRNG streams, genome generation, validation, motion, behavior, and portraits.
- `main/memory/`: NVS-backed identity block reservation and compact Fishbook persistence.
- `main/input/`: normalized touch and IMU sampling/filtering.
- `main/audio/`: speaker playback, procedural sound, microphone capture, and audio-event ownership.
- `main/net/`: optional Wi-Fi, NTP, and local gallery after the offline core is complete.
- `main/storage/`: optional microSD archive after the offline core is complete.
- `tests/`: host-runnable deterministic tests for pure logic and repository contracts.
- `tools/preview/`: host preview/contact-sheet tools for rendering inspection without flashing.

Simulation must run at a fixed timestep and publish immutable snapshots. Rendering must consume snapshots without mutating simulation state. Large framebuffers and caches belong in PSRAM; DMA transfer buffers belong in internal DMA-capable memory. Steady-state animation loops should not allocate.

## Completed Work

- Read and preserved `plan.txt` as historical source material.
- Defined the product specification in `PRODUCT_SPEC.md`.
- Defined the milestone roadmap in `ROADMAP.md`.
- Added durable repository rules in `AGENTS.md`.
- Added generated-file ignore rules in `.gitignore`.
- Added a bootstrap `README.md`.
- Inspected read-only reference repositories for proven constraints:
  - `..\stickman-dev\stickman`
  - `..\stickman-dev\waveshare-board`
  - `..\stickman-dev\fluidbox-reference`

## Host Validation

No host tests exist yet in TobyTank.

For Milestone 0, the first host validation should cover repository contract checks, RGB565 helpers, canvas clipping, generated-file ignore policy, and build configuration sanity. Host tests should be added before production firmware behavior changes.

## Hardware Validation

No TobyTank firmware has been flashed or physically validated.

Known hardware facts come from the read-only references:

- The target board is Waveshare ESP32-S3-Touch-AMOLED-1.8 with 368x448 RGB565 AMOLED.
- The managed BSP owns board revision probing, display/touch controller selection, panel offsets, pins, power, and initialization.
- The Stickman reference has a proven pattern using BSP initialization, PSRAM RGB565 framebuffers, internal DMA transfer bands, and asynchronous panel transfer completion.
- The board reference documents onboard capacitive touch, PCF85063A RTC, QMI8658 IMU, ES8311 audio codec with microphone/speaker paths, and microSD over SDMMC.
- FluidBox demonstrates the value of fixed-timestep simulation, host preview, performance telemetry, and careful separation between simulation and rendering.

## Known Issues

- TobyTank has no ESP-IDF project files yet.
- No display proof has been built or flashed for TobyTank.
- No host-test harness exists yet.
- Fish identity persistence, genome generation, visitor lifecycle, Fishbook, input, RTC, audio, Wi-Fi, and microSD modules are planned but unimplemented.
- Product decisions remain open around default lifecycle timing, internal Fishbook capacity, portrait storage format, Fishbook access gesture, audio default mode, and optional connectivity UX.

## Performance Measurements

No TobyTank performance measurements exist yet.

Initial performance targets:

- Render at least 20 FPS on the AMOLED, with 20-25 FPS acceptable for the richer aquarium target.
- Keep deterministic simulation at a fixed timestep, targeting 60 Hz if feasible.
- Use low-rate serial telemetry for render FPS, lifecycle state, current fish identity, state time remaining, free internal memory, free PSRAM, and render timing.
- Avoid per-frame persistence writes and avoid allocation in steady-state animation loops.

Reference observations only:

- Stickman demonstrates the board can use PSRAM RGB565 framebuffers and internal DMA transfer bands through the managed BSP.
- FluidBox reports high display throughput with banded DMA rendering and lower simulation throughput for heavier physical simulation; TobyTank should measure its own behavior before claiming performance.

## Exact Next Checkpoint

Implement Milestone 0 only:

1. Create the minimal standalone ESP-IDF project skeleton for ESP32-S3.
2. Add managed BSP dependency `waveshare/esp32_s3_touch_amoled_1_8`.
3. Add display/board hardware boundary using BSP-owned revision handling.
4. Add RGB565 canvas and a static full-screen display proof.
5. Add host repository-contract and canvas tests.
6. Run host tests.
7. Activate ESP-IDF v5.5.5, set target `esp32s3`, and build with `ninja -C build -j4 all`.
8. Do not flash unless the hardware checkpoint is explicitly authorized.
