# TobyTank Development Notes

## Current Milestone

Milestone 1, procedural aquarium environment and host preview, is complete. Milestone 0 (ESP-IDF foundation and display proof) is complete and superseded by it.

The firmware boots, generates a seeded aquarium, simulates it at a fixed 60 Hz, and renders it at 19.9 FPS on the panel with no visible UI and no reboot loop. Host tests pass, the ESP-IDF build is clean, and a host preview tool renders the same scene to image files without a board.

Milestone 2 (persistent unique identity and fish genome) is the next milestone and is not started.

## Architecture

Hardware, deterministic simulation, rendering, persistence, input, audio, and optional connectivity stay separate:

- `main/hardware/`: board initialization, display boundary, and managed BSP interactions. Implemented.
- `main/aquarium/`: seeded environment generation and fixed-timestep simulation. Implemented for the empty tank; visitor lifecycle and time ecology come later.
- `main/render/`: RGB565 canvas plus background, effects, and particle rasterizers. Implemented for the empty tank; fish rasterization comes later.
- `main/diagnostics/`: boot and periodic health reporting over serial. Implemented.
- `tests/`: host-runnable deterministic tests for pure logic and repository contracts. Implemented.
- `tools/preview/`: host preview rendering to a dependency-free image format. Implemented.
- `main/fish/`, `main/memory/`, `main/input/`, `main/audio/`, `main/net/`, `main/storage/`: planned, not implemented.

Simulation runs at a fixed timestep and publishes an immutable snapshot. Rendering consumes the snapshot and never mutates simulation state. Everything under `main/aquarium/` and `main/render/` is plain C with no ESP-IDF, FreeRTOS, or BSP dependency, which is what lets the host tests and the preview tool compile the real firmware sources unchanged. A contract test enforces that.

Large framebuffers live in PSRAM; DMA transfer bands live in internal DMA-capable memory. Nothing is allocated in the frame loop.

## Milestone 0 Implementation

- `CMakeLists.txt`, `main/CMakeLists.txt`, `main/idf_component.yml`, `sdkconfig.defaults`, `partitions.csv`: minimal standalone ESP-IDF project for `esp32s3` with the managed BSP dependency `waveshare/esp32_s3_touch_amoled_1_8` (`^2.0.3`, resolved to 2.0.3).
- `main/hardware/display.c`: brings the panel up through the BSP only. It calls `bsp_touch_new()` before `bsp_display_new()` because the BSP probes the touch controller to select the board revision and resets a GPIO shared with the LCD, then sets brightness with `bsp_display_brightness_set()`. No GPIOs, panel commands, controller selection, or panel offsets appear in TobyTank code; the BSP owns all of it, including the `esp_lcd_panel_disp_on_off()` call.
- `main/hardware/board.c`: the board boundary that startup initializes and logs.
- `main/render/canvas.c` and `canvas.h`: RGB565 primitives with clipping. Packing and per-pixel blending are inline in the header because the rasterizers touch hundreds of thousands of pixels per frame.
- `main/diagnostics/boot_diagnostics.c`: boot report of app/IDF version, chip model/revision/cores/features, flash size, PSRAM presence and size, BSP versus application display geometry, and buffer sizes; plus a periodic heap health line.
- `main/app_main.c`: boot diagnostics, board init, renderer init, and the frame loop, halting with an explicit error log rather than crash-looping if init fails.

Colour byte order matches the proven Stickman pattern: `tobytank_rgb565()` builds the native RGB565 value and then swaps bytes, because the panel receives big-endian pixel data.

The Milestone 0 display-validation scene was a gradient, a colour-stripe bar, a three-frame edge ruler, a crosshair, and two bars. It confirmed geometry, colour order, and edge coverage on hardware and has been replaced by the aquarium.

## Milestone 1 Implementation

- `main/aquarium/environment.c` and `.h`: seeded generation and fixed-timestep simulation of the empty tank. splitmix64 seeds an xorshift64* stream; the same seed and canvas size always produce the same scene. Generated content is the water and substrate palettes, a twelve-point substrate dune profile, 3-8 stones, 4-10 plants, 2-4 light shafts, 40 bubbles, and 96 drifting motes. `tobytank_environment_snapshot()` copies the whole scene so renderers cannot touch live state.
- `main/render/background.c`: water gradient, substrate, stones, and swaying plants. Exposed as three separately timed passes.
- `main/render/effects.c`: drifting light shafts, caustic ripples near the surface, and surface shimmer.
- `main/render/particles.c`: motes and bubbles, rasterized from the snapshot. Particle motion belongs to the simulation, not here.
- `main/render/renderer.c`: the ESP-side glue. It accumulates real elapsed time, runs whole 1/60 s simulation steps (at most six per frame, then drops the backlog rather than spiralling), snapshots, draws the three layers, and submits.
- `tools/preview/`: a host program that renders the real scene at 368x448 for four seeds and three moments, plus a PPM encoder. PPM was chosen because it needs no image library, so preview output stays dependency-free.

The scene is deliberately dark and mostly still: AMOLED-friendly blacks, a blue-teal ramp that sinks toward black at the bottom, and a darkened border. The border darkening is the current burn-in mitigation and is folded into the background write, so it costs no readback.

Two visual details came out of looking at preview output rather than reasoning: the water gradient banded badly until a 4x4 ordered dither was added, because RGB565 has only 32 blue levels; and light shafts and stone highlights drawn as blend rectangles read as pasted rectangles until they were feathered.

## Toolchain Notes

The documented activation command is:

```powershell
. 'C:\Espressif\tools\Microsoft.v5.5.5.PowerShell_profile.ps1'
```

On this machine that profile alone is not sufficient, for two reasons:

1. It sets `IDF_PYTHON_ENV_PATH` to `C:\Espressif\tools\python\v5.5.5\venv`, which does not exist. The real environment is `C:\Espressif\tools\python_env\idf5.5_py3.11_env`.
2. Its tab-completion helper leaves `_IDF.PY_COMPLETE=powershell_source` set, which makes every later `idf.py` invocation exit silently with code 1.

Both are repaired after sourcing the profile:

```powershell
. 'C:\Espressif\tools\Microsoft.v5.5.5.PowerShell_profile.ps1'
Remove-Item 'Env:_IDF.PY_COMPLETE' -ErrorAction SilentlyContinue
$env:IDF_PYTHON_ENV_PATH = 'C:\Espressif\tools\python_env\idf5.5_py3.11_env'
$env:PATH = "$env:IDF_PYTHON_ENV_PATH\Scripts;$env:PATH"
& "$env:IDF_PYTHON_ENV_PATH\Scripts\python.exe" "$env:IDF_PATH\tools\idf.py" set-target esp32s3
ninja -C build -j4 all
```

The toolchain paths the profile adds to `PATH` are correct (`xtensa-esp-elf` `esp-14.2.0_20260121`, CMake 3.30.2, Ninja 1.12.1); only the Python environment and the completion variable need fixing.

Note that `sdkconfig.defaults` only applies when `sdkconfig` is created. After changing defaults, delete `sdkconfig` and run `idf.py reconfigure`.

## Host Validation

Run everything with:

```powershell
powershell -ExecutionPolicy Bypass -File tests\run_host_tests.ps1
```

Three suites exist:

- `tests/test_project_contract.py` (12 tests, Python `unittest`): project identity and BSP dependency pinning, board memory/flash defaults, partition layout fitting 16 MB, display geometry and buffer sizes, the required BSP call order and the absence of hardcoded GPIO/panel-command/panel-offset symbols in hardware code, the required diagnostics facts, that the renderer drives a fixed-timestep simulation and never allocates in the frame loop, that the simulation and rasterizers stay free of ESP-IDF dependencies, that environment generation is seeded rather than ambient, that the Milestone 1 sources are registered with CMake, that preview output is dependency-free, generated-file ignore rules, and documentation presence.
- `tests/canvas_host_test.c`: RGB565 byte order, canvas clear, pixel clipping, rectangle clipping, line clipping, and frame rectangles.
- `tests/environment_host_test.c`: determinism of generation and of 900 simulation steps for a seed, that different seeds differ, that every generated parameter stays in its documented range across 40 seeds, that bubbles and motes stay bounded and their pools stay full over 4000 steps, that a snapshot is a copy rather than a view, that substrate lookup is clamped outside the tank, that 240 frames of the full scene never write outside a guarded canvas and cover every pixel, that null and undersized inputs are safe, and that PPM encoding round-trips colours and rejects short buffers.

`tests/host_cc.ps1` finds the compiler. It prefers `gcc` or `clang` on `PATH`; neither is installed here, so it locates Visual Studio 2022 with `vswhere` and compiles with `cl /W4 /WX` from a temporary directory, keeping object files out of the repository.

Result on 2026-09-02: all three suites pass.

## Hardware Validation

Flashed on 2026-09-02 with explicit authorization, over `COM3` (native USB CDC), using `idf.py -p COM3 flash`. Only the bootloader, partition table, and app partition were written. No full-flash erase, no eFuse change, no boot-security change.

Observed from serial:

- Chip revision v0.2, two cores, features 0x12; flash detected as 16,777,216 bytes; PSRAM initialized at 8,388,608 bytes (octal, 80 MHz).
- Partition table matched `partitions.csv`.
- The BSP revision probe worked: `Touch CST816S 0x15 found`, then the CO5300 panel driver 2.1.0 initialized over QSPI.
- BSP and application display geometry agreed: `BSP=368x448 RGB16 app=368x448 RGB16`.
- Buffers allocated as planned: two 329,728-byte PSRAM frames plus two 11,776-byte DMA bands.
- Brightness at the configured 60 percent.
- Steady 19.9 FPS with 265,171 bytes free internal and 7,725,548 bytes free PSRAM, unchanged over minutes. No frame failures and no reboot loop.

Two boot warnings appear. Both come from ESP-IDF and the BSP, not from TobyTank code:

- `i2c.master: Please check pull-up resistances whether be connected properly` — the standard ESP-IDF I2C notice.
- `co5300_spi: The 3Ah command has been used and will be overwritten by external initialization sequence` — the panel driver noting that the BSP init sequence sets the pixel format itself.

Confirmed by eye during Milestone 0: full panel coverage with no margin, offset, or edge wrap; correct (not byte-swapped) colour order; smooth gradient; centre alignment. A single-pixel border proved too fine to judge on this panel, so the validation scene used a three-frame edge ruler instead.

Milestone 1 hardware checkpoint, confirmed by eye: the empty aquarium animates smoothly, has no menu or debug UI, and keeps its bright areas small and its edges dark.

## Performance Measurements

Measured on hardware on 2026-09-02, from the once-per-second telemetry line:

- 19.9 FPS sustained, flat over minutes, against the 20 FPS Milestone 1 target.
- Simulation locked at 60 fixed steps per second regardless of render rate.
- Frame budget, averaged: draw 49 ms total, split water 17 ms, terrain 12 ms, plants 6 ms, effects 11 ms, particles under 1 ms. Panel submission no longer appears in the frame time at all.
- Free internal memory 265,171 bytes; free PSRAM 7,725,548 bytes. Neither drifts, which matches the no-allocation-in-loop rule.

Getting there took four measured changes, in order of what they were worth:

1. `CONFIG_SPIRAM_FETCH_INSTRUCTIONS` and `CONFIG_SPIRAM_RODATA` were on, so code and constants were fetched from PSRAM while the framebuffer was being written to the same octal bus. Turning both off, and giving the caches 32 KB instruction and 64 KB data, was the single largest win.
2. Panel submission moved to its own task pinned to core 1, fed by a queue, with the two PSRAM framebuffers as the handoff. Drawing frame N+1 now overlaps the transfer of frame N, which removed 28 ms per frame from the critical path. Within a transfer, two internal DMA bands also overlap the PSRAM-to-internal copy with the transfer of the previous band.
3. The water and substrate passes were rewritten from float to 8.8 fixed point with lookup tables for the dither bias and the edge darkening, and the interior of each water row is written as 32-bit pairs.
4. Plant sway sampled a nine-point sine curve per blade and interpolated, instead of calling `sinf` per pixel step.

Measured but rejected: skipping water pixels that the substrate would later overwrite. The per-pixel branch cost more than the writes it saved (19.5 FPS versus 19.9).

Remaining headroom, for the Milestone 11 performance pass:

- The water pass is close to PSRAM write bandwidth: 330 KB per frame in 17 ms is about 19 MB/s. Reducing how much of the frame is repainted every frame, rather than making the writes cheaper, is the next real lever.
- Effects still cost 11 ms, nearly all of it the light shafts.
- The BSP fixes the QSPI clock, so a full-frame transfer takes about 28 ms. That is the ceiling on frame rate once drawing is fully overlapped, or about 35 FPS.

Static build results (`idf.py size` after Milestone 1): `tobytank.bin` is 0x486c0 bytes, leaving 96 percent of the 8 MB app partition free.

Targets still open for later milestones:

- Hold at least 20 FPS once fish rasterization is added on top of the aquarium.
- Keep deterministic simulation at a fixed timestep, which it now is.
- Avoid per-frame persistence writes and allocation in steady-state loops.

## Known Issues

- Frame rate sits right at the 20 FPS target with no fish drawn yet. Milestone 3 and 4 will add rasterization on top of a budget that is already full, so either the aquarium passes get cheaper or the target gets revisited.
- The BSP is a public dependency that pulls in LVGL 9.5 and `esp_lvgl_port`. TobyTank does not use LVGL, but it costs most of the build time.
- The touch handle returned by `bsp_touch_new()` is deliberately discarded; touch input arrives in Milestone 5.
- The environment seed is a fixed constant, so every device generates the same tank. Milestone 2 introduces persistent identity and can derive a per-device seed.
- `main/render/background.c` keeps a 512-entry substrate cache in static storage and skips the substrate entirely for canvases wider than that. The panel is 368 wide, so this only constrains host experiments.
- Fish identity persistence, genome generation, visitor lifecycle, Fishbook, input, RTC, audio, Wi-Fi, and microSD modules are planned but unimplemented.
- Product decisions remain open around default lifecycle timing, internal Fishbook capacity, portrait storage format, Fishbook access gesture, audio default mode, and optional connectivity UX.

## Exact Next Checkpoint

Implement Milestone 2 only: persistent unique identity and fish genome.

1. Add `main/fish/identity.*`, `main/fish/prng.*`, `main/fish/genome.*`, `main/fish/genome_validate.*`, and `main/memory/identity_store.*`.
2. Reserve identity blocks in NVS so identities are never reused across ordinary resets or power loss.
3. Derive deterministic PRNG streams from identity, and fold the environment's local splitmix64/xorshift stream into that shared PRNG module rather than leaving two implementations.
4. Add host tests before the production behaviour: same identity yields the same genome, consecutive identities differ across a full fingerprint, thousands of genomes validate, rejection and regeneration are deterministic, and a simulated reboot skips unused IDs from a reserved block.
5. Run host tests, then build with `ninja -C build -j4 all`.
6. Do not flash unless the hardware checkpoint is explicitly authorized.
