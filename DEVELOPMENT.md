# TobyTank Development Notes

## Current Milestone

Milestone 6, Fishbook and persistent encounter records, is the current approved implementation milestone. Milestones 0 through 5 are complete.

The firmware boots, generates a seeded aquarium, simulates it at a fixed 60 Hz, reserves persistent identity blocks in NVS, admits exactly one generated visitor at a time, moves that visitor through empty, entry, exploration, and exit states, renders the current visitor from immutable snapshots, samples touch through the BSP-provided touch handle, and applies bounded touch ripple/current interaction state. Host tests pass, the ESP-IDF build is clean, host preview tools render both the live aquarium lifecycle and generated fish contact sheets without a board, and Milestone 5 has been hardware-validated on the AMOLED.

Milestone 3 has been flashed for testing. Serial validation showed clean boot, NVS identity block reservation, fish cache initialization, stable memory, and no reboot loop; later lifecycle hardware viewing superseded the static sample-fish checkpoint.

Milestone 5 has been flashed and monitored on hardware. Serial validation confirms BSP touch initialization, touch/ripple telemetry, IMU fallback logging, lifecycle stability across multiple identities, stable memory, and measured FPS on the panel. First visual feedback confirmed the ripples worked correctly but were very small and hard to notice; the ripple renderer was then made modestly brighter and wider, reflashed, and confirmed by eye as working great.

## Architecture

Hardware, deterministic simulation, rendering, persistence, input, audio, and optional connectivity stay separate:

- `main/hardware/`: board initialization, display boundary, and managed BSP interactions. Implemented.
- `main/aquarium/`: seeded environment generation, fixed-timestep simulation, and one-fish visitor lifecycle. Implemented; time ecology comes later.
- `main/render/`: RGB565 canvas plus background, effects, particle rasterizers, fish sprite rasterizer, sprite cache, and compositor. Implemented.
- `main/fish/`: identity allocation, deterministic PRNG streams, genome generation, validation, fingerprints, behavior planning, motion, and portrait entry point. Implemented.
- `main/memory/`: NVS-backed identity block reservation. Implemented; compact Fishbook persistence comes later.
- `main/input/`: touch polling, deterministic gesture filtering, and IMU fallback. Implemented.
- `main/sim/`: immutable live fish snapshot shared between lifecycle and renderer. Implemented.
- `main/diagnostics/`: boot and periodic health reporting over serial. Implemented.
- `tests/`: host-runnable deterministic tests for pure logic and repository contracts. Implemented.
- `tools/preview/`: host preview rendering to a dependency-free image format. Implemented.
- `main/audio/`, `main/net/`, `main/storage/`: planned, not implemented.

Simulation runs at a fixed timestep and publishes an immutable snapshot. Rendering consumes the snapshot and never mutates simulation state. Everything under `main/aquarium/`, `main/fish/`, and `main/render/` is plain C with no ESP-IDF, FreeRTOS, or BSP dependency, which is what lets the host tests and the preview tool compile the real firmware sources unchanged. A contract test enforces that.

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

## Milestone 2 Implementation

- `main/fish/prng.c` and `.h`: the shared deterministic generator. `tobytank_splitmix64()` seeds an xorshift64* stream, `tobytank_prng_seed_raw()` supports non-fish namespaces such as the aquarium, and `tobytank_prng_seed()` derives independent streams for anatomy, colour, markings, behaviour, motion, sound, and environment.
- `main/aquarium/environment.*`: refactored to use the shared PRNG without changing preview output. The preview hashes before and after the refactor matched.
- `main/fish/identity.c` and `.h`: pure block-reservation allocator. Identity 0 is invalid. Each boot reserves 64 identities, persists the new counter before use, then hands identities out from RAM. A reset skips unused IDs from the prior block. Near `UINT64_MAX`, reservation fails closed instead of wrapping.
- `main/memory/identity_store.c` and `.h`: NVS-backed counter storage. It initializes NVS, reserves and commits a block before any identity can be returned, logs when NVS recovery erases identity history, and never erases full flash.
- `main/fish/genome.c` and `.h`: deterministic genome generation as a pure function of identity and variant. Anatomy, palette, markings, behaviour, motion, and future sound traits use separate streams so later additions to one group do not shift the others.
- `main/fish/genome_validate.c` and `.h`: explicit validation with stable verdict names. Invalid identities, impossible body shapes, face placement, fin bounds, sprite extent, contrast, palette, and motion ranges are rejected. `tobytank_genome_generate()` retries deterministic variants up to `TOBYTANK_GENOME_MAX_ATTEMPTS`.
- `main/diagnostics/boot_diagnostics.c`: serial identity report after store initialization, plus a fixed self-check genome fingerprint that does not consume a real visitor identity.

Milestone 2 initialized identity storage so resets could be observed on serial before any visitor consumed identities. Milestone 4 now consumes those identities only at real visitor admission.

## Milestone 3 Implementation

- `main/render/fish_sprite.h`: the cached sprite contract. A fish sprite stores byte-swapped RGB565 pixels plus a separate alpha plane so it can be blended over any aquarium frame.
- `main/render/fish_rasterizer.c` and `.h`: deterministic anatomy rasterization from a validated genome. It draws fins, caudal variants, tapered body shape, face, gill curve, scale/lighting variation, and pattern types into a bounded `168x128` sprite.
- `main/render/dither.c` and `.h`: small ordered-dither channel biases used by procedural rasterizers so fish gradients and value shifts do not collapse into harsh RGB565 bands.
- `main/render/composite.c` and `.h`: clipped alpha compositing from a cached sprite to the destination canvas. Host tests exercise every edge.
- `main/render/fish_cache.c` and `.h`: caller-owned cache metadata. The firmware allocates the sprite planes once in PSRAM during renderer initialization and only rerasterizes when the identity/fingerprint changes.
- `main/fish/portrait.c` and `.h`: current portrait entry point. For now it renders the full-body fish sprite; Fishbook-specific crops and metadata arrive later.
- `main/render/renderer.c`: composites one deterministic non-consumed display fish at the centre of the aquarium. This proves the rasterizer path in firmware without implementing the visitor lifecycle or spending an NVS identity.
- `tools/preview/contact_sheet_main.c` and `run_contact_sheet.ps1`: host contact-sheet rendering for 16 deterministic fish. The generated `tools/preview/out/fish_contact_sheet.ppm` was visually inspected by converting a temporary copy to BMP outside the repo.
- `tools/preview/preview_main.c`: the normal aquarium preview now composites the same sample fish as firmware.

The contact sheet shows varied silhouettes, fins, body colours, and markings. The aquarium preview shows the sample fish readable against the dark water with particles and plants still visible.

## Milestone 4 Implementation

- `main/aquarium/lifecycle.c` and `.h`: the one-fish visitor scheduler. It starts empty, waits a seeded duration, requests one persistent identity from an injected source, generates and validates that visitor's genome, and advances through entering, exploring, exiting, and empty states. The lifecycle never admits a second fish while one is live.
- `main/fish/behavior.c` and `.h`: seeded behavior planning from genome traits. It chooses empty, entry, exploration, and exit durations plus cruise, entry, and exit depths from temperament and motion traits.
- `main/fish/motion.c` and `.h`: smooth entry from fully offscreen, exploration steering within the water column, bounded exit to offscreen, and tail/fin phase timing.
- `main/sim/snapshot.c` and `.h`: immutable live-fish snapshot types and state names used by rendering and telemetry.
- `main/render/renderer.c`: updates environment and lifecycle inside the same fixed 60 Hz simulation loop, snapshots both, prepares the cached sprite for the current visitor only when identity/fingerprint changes, composites it at the lifecycle position, and logs visitor state, identity, and remaining state time in render telemetry.
- `tools/preview/preview_main.c`: now runs the same lifecycle path with a deterministic host identity source, so preview frames show empty and active visitor moments without NVS or a board.

The firmware initializes NVS identity storage before renderer startup. If identity storage fails, rendering still starts and the lifecycle remains empty rather than inventing identities or reusing old ones.

After first hardware viewing, the live fish entered tail-first because the cached sprite art is left-facing while entry motion starts from the left. `tobytank_composite_sprite_facing()` now mirrors the cached sprite for positive/rightward facing, and a host regression test checks the mirrored pixel order.

Second hardware viewing showed occasional green rectangular patches around patterned fish. The cause was `draw_markings()` painting into the rectangular body scan bounds even where the fish alpha mask was transparent. Markings now blend only over existing fish pixels, and a host regression test verifies that markings never expand the sprite alpha mask.

## Milestone 5 Implementation

- `main/input/touch.c` and `.h`: ESP-side touch polling. It reuses the touch handle created by `bsp_touch_new()` during display initialization, reads with `esp_lcd_touch_read_data()` and `esp_lcd_touch_get_data()`, and feeds the host-portable filter. No GPIOs, touch-controller choices, or panel offsets are owned by TobyTank code.
- `main/input/imu.c` and `.h`: ESP-side IMU boundary. The managed BSP currently reports `BSP_CAPS_IMU` as `0`, so Milestone 5 logs that IMU tilt is unavailable and feeds a neutral sample. It does not invent QMI registers, I2C addresses, or pins.
- `main/input/motion_filter.c` and `.h`: host-portable touch gesture and IMU smoothing logic. Touch samples produce down, drag, up, and tap events with clamped coordinates and deltas. IMU samples are low-pass filtered and bounded, with neutral fallback when unavailable.
- `main/aquarium/interactions.c` and `.h`: deterministic interaction state for touch ripples, touch/tilt current, parallax cues, and fish attention strength. Outputs are bounded and decay over time.
- `main/aquarium/environment.c`: adds `tobytank_environment_update_with_interactions()` so current can gently move bubbles and motes without replacing the original deterministic update entry point.
- `main/aquarium/lifecycle.c`: adds `tobytank_lifecycle_update_with_interactions()` so touch attention can nudge the current visitor without changing identity allocation, state timing, or the one-fish invariant.
- `main/render/effects.c`: adds `tobytank_effects_draw_interactions()` for clipped touch ripple rendering.
- `tools/preview/preview_main.c`: injects a deterministic short touch near the first active preview moment so ripple/current behavior can be inspected without hardware.

The IMU part is intentionally a graceful fallback with the current BSP. Hardware validation should confirm the log line and absence of harsh motion, then a later BSP or driver addition can fill in real tilt samples without changing the pure filtering/interactions contracts.

After first Milestone 5 hardware feedback, touch ripples were confirmed to work but were too small to notice easily. The interaction start radius/strength and render band/peak were raised slightly, and `tests/interactions_host_test.c` now checks that a representative ripple changes a visible number of pixels while preserving clipping.

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

Seven suites exist:

- `tests/test_project_contract.py` (18 tests, Python `unittest`): project identity and BSP dependency pinning, board memory/flash defaults, partition layout fitting 16 MB, display geometry and buffer sizes, the required BSP call order and the absence of hardcoded GPIO/panel-command/panel-offset symbols in hardware code, the required diagnostics facts, that the renderer drives a fixed-timestep simulation and never allocates in the frame loop, that the simulation, rasterizers, lifecycle, interaction, and fish genome code stay free of ESP-IDF dependencies, that environment generation is seeded rather than ambient, that Milestone 1 through 5 sources are registered with CMake, that there is one shared deterministic generator, that genome traits cover the required groups, that identity uniqueness is block-reserved, that the fish rasterizer is cached and clipped, that the visitor lifecycle is deterministic and one-fish, that touch/IMU/interaction behavior is bounded, that preview output is dependency-free, generated-file ignore rules, and documentation presence.
- `tests/canvas_host_test.c`: RGB565 byte order, canvas clear, pixel clipping, rectangle clipping, line clipping, and frame rectangles.
- `tests/environment_host_test.c`: determinism of generation and of 900 simulation steps for a seed, that different seeds differ, that every generated parameter stays in its documented range across 40 seeds, that bubbles and motes stay bounded and their pools stay full over 4000 steps, that a snapshot is a copy rather than a view, that substrate lookup is clamped outside the tank, that 240 frames of the full scene never write outside a guarded canvas and cover every pixel, that null and undersized inputs are safe, and that PPM encoding round-trips colours and rejects short buffers.
- `tests/genome_host_test.c`: same identity produces the same genome and fingerprint, neighbouring identities differ across trait groups, 4000 consecutive identities all validate and have unique fingerprints, rejected first attempts regenerate deterministically, every accepted fish fits the reserved sprite box, invalid inputs are rejected, PRNG streams are repeatable and independent, block-reserved identity allocation skips unused IDs across simulated reboots, and near-overflow counters fail closed.
- `tests/fish_rasterizer_host_test.c`: invalid inputs are safe, 600 generated genomes rasterize deterministically with plausible alpha coverage, compositing clips at all canvas edges without touching guard regions, cache reuse and invalidation follow identity/fingerprint, and 80 large/deep accepted genomes fit the sprite.
- `tests/lifecycle_host_test.c`: lifecycle starts empty without consuming an identity, admission consumes exactly one identity, replay with the same seed and identity stream is deterministic, no second visitor appears before the first clears, empty intervals occur between visitors, exits are bounded, entry starts fully offscreen, motion has no frame-to-frame discontinuities, identity-source failure keeps the tank empty, and invalid inputs are safe.
- `tests/interactions_host_test.c`: recorded touch streams are deterministic and clamped, IMU samples are bounded and fall back to neutral, interaction outputs stay inside documented current/parallax/attention limits, environment currents keep particles bounded, touch attention cannot admit a second fish, and ripple rendering clips at canvas edges.

`tests/host_cc.ps1` finds the compiler. It prefers `gcc` or `clang` on `PATH`; neither is installed here, so it locates Visual Studio 2022 with `vswhere` and compiles with `cl /W4 /WX` from a temporary directory, keeping object files out of the repository.

Result on 2026-09-04: all seven suites pass.

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

Milestone 3 flashed on 2026-09-04 with explicit authorization, over `COM3` (native USB CDC), using `idf.py -p COM3 flash`. Only the bootloader, partition table, and app partition were written. No full-flash erase, no eFuse change, no boot-security change.

Observed from serial after a USB UART reset:

- Identity storage reserved block `129..192` with counter `192` and `history_lost=0`, proving the block-reservation counter survived earlier boots and skipped unused IDs.
- Genome self-check succeeded without consuming a real visitor: fingerprint `0xbcdbc4e88a9e9329`, variant `0`, body `63.0x15.9`, extent `90x28`, caudal `4`, pattern `6`, speed `49.3`, cadence `1.94`.
- Fish cache initialized for the non-consumed display sample: identity `18446744073709548289`, fingerprint `0xcad7aca8fc150056`, sprite `168x128`.
- Render telemetry stabilized at 18.8-19.5 FPS, draw 50-52 ms, split water 17 ms, terrain 12 ms, plants 6 ms, effects 11-13 ms, fish 1 ms, particles 0-1 ms, wait 0 ms.
- Free internal memory held at 260,771 bytes and free PSRAM at 7,661,028 bytes with no visible drift in the captured window.

Milestone 5 flashed on 2026-09-04 with explicit authorization, over `COM3` (native USB CDC), using `idf.py -p COM3 flash monitor`. Only the bootloader, partition table, and app partition were written. No full-flash erase, no eFuse change, no boot-security change.

Observed from serial:

- Flash wrote and verified the normal bootloader, app, and partition table regions only. `tobytank.bin` was `0x51d30` bytes, leaving 96 percent of the 8 MB app partition free.
- Boot remained clean on ESP-IDF v5.5.5 with chip revision v0.2, 16 MB flash, and 8 MB PSRAM.
- The BSP revision probe and touch path initialized: `Touch CST816S 0x15 found`, `Touch input ready: 368x448`, and `Interactions ready: touch=1 imu=0`.
- The managed BSP reported no IMU capability, and the firmware logged the expected graceful fallback: `IMU unavailable in managed BSP; tilt interactions disabled`.
- Identity storage reserved block `641..704` with counter `704` and `history_lost=0`.
- Touch input was observed in render telemetry as `touch=1`, and ripple activity was observed as `ripple=1`, without corrupting lifecycle state.
- The visitor lifecycle ran `empty -> entering -> exploring -> exiting -> empty`, then admitted a new visitor identity `642`, proving identity progression and the one-fish scheduler on hardware.
- Render telemetry ranged from 17.7-20.0 FPS. Empty-tank frames were roughly 19.0-20.0 FPS; active fish frames were roughly 17.7-18.4 FPS, with draw time 48-54 ms.
- Free PSRAM held at 7,661,028 bytes. Free internal memory held at 260,219 bytes for most of the capture and later reported 260,155 bytes while the second visitor was exiting; no reboot loop or progressive PSRAM drift was observed.

Final human visual feedback after the adjusted ripple build: the aquarium is working great, and the adjusted ripples are working correctly. The original issue was that ripples were very small and hard to notice; the brighter/wider ripple tuning addressed that without changing lifecycle behavior.

## Performance Measurements

Measured on hardware on 2026-09-02, from the once-per-second telemetry line:

- 19.9 FPS sustained, flat over minutes, against the 20 FPS Milestone 1 target.
- Simulation locked at 60 fixed steps per second regardless of render rate.
- Frame budget, averaged: draw 49 ms total, split water 17 ms, terrain 12 ms, plants 6 ms, effects 11 ms, particles under 1 ms. Panel submission no longer appears in the frame time at all.
- Free internal memory 265,171 bytes; free PSRAM 7,725,548 bytes. Neither drifts, which matches the no-allocation-in-loop rule.

Measured on hardware after the Milestone 3 flash on 2026-09-04:

- 18.8-19.5 FPS in the captured serial window.
- Fish compositing costs about 1 ms per frame because the sprite is already cached.
- Free internal memory dropped from the Milestone 1 value to 260,771 bytes; free PSRAM dropped to 7,661,028 bytes, mainly from the fish sprite pixel and alpha cache.

Measured on hardware after the Milestone 5 flash on 2026-09-04:

- Empty-tank telemetry ranged from 19.0-20.0 FPS.
- Active visitor telemetry ranged from 17.7-18.4 FPS during the captured entry, exploration, and exit states.
- Draw time ranged from 48-54 ms: water 17 ms, terrain 12 ms, plants 6 ms, effects 11-14 ms, fish 0-2 ms, particles 0-1 ms.
- Free internal memory was 260,219 bytes for most of the run and 260,155 bytes late in the capture; free PSRAM held at 7,661,028 bytes.

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

Static build results (`ninja -C build -j4 all` after Milestone 2): `tobytank.bin` is 0x4e4d0 bytes, leaving 96 percent of the 8 MB app partition free.

Static build results (`ninja -C build -j4 all` after Milestone 3): `tobytank.bin` is 0x500d0 bytes, leaving 96 percent of the 8 MB app partition free.

Static build results (`ninja -C build -j4 all` after Milestone 4 marking-mask fix): `tobytank.bin` is 0x50ac0 bytes, leaving 96 percent of the 8 MB app partition free.

Static build results (`ninja -C build -j4 all` after Milestone 5): `tobytank.bin` is 0x51d30 bytes, leaving 96 percent of the 8 MB app partition free.

Static build results (`ninja -C build -j4 all` after adjusted Milestone 5 ripple tuning): `tobytank.bin` is 0x51d30 bytes, leaving 96 percent of the 8 MB app partition free.

Targets still open for later milestones:

- Hold at least 20 FPS once fish rasterization is added on top of the aquarium.
- Keep deterministic simulation at a fixed timestep, which it now is.
- Avoid per-frame persistence writes and allocation in steady-state loops.

## Known Issues

- Frame rate sits below the 20 FPS target with active visitor fish on hardware: 17.7-18.4 FPS in the captured Milestone 5 serial window. Empty intervals still sit around 19.0-20.0 FPS. The presentation is acceptable for Milestone 5, but this remains a Milestone 11 optimization target.
- IMU tilt is a graceful fallback in Milestone 5 because the managed BSP reports `BSP_CAPS_IMU` as `0`. Touch works through the BSP touch handle; real tilt needs a future supported BSP API or explicitly approved driver boundary.
- The BSP is a public dependency that pulls in LVGL 9.5 and `esp_lvgl_port`. TobyTank does not use LVGL, but it costs most of the build time.
- The environment seed is still a fixed constant, so every device generates the same tank. Milestone 2 introduced persistent identity, but the environment seed has not been switched to a per-device derivation.
- `main/render/background.c` keeps a 512-entry substrate cache in static storage and skips the substrate entirely for canvases wider than that. The panel is 368 wide, so this only constrains host experiments.
- Fishbook, RTC, audio, Wi-Fi, and microSD modules are planned but unimplemented.
- Product decisions remain open around default lifecycle timing, internal Fishbook capacity, portrait storage format, Fishbook access gesture, audio default mode, and optional connectivity UX.

## Exact Next Checkpoint

Begin Milestone 6 only: Fishbook and persistent encounter records.

1. Design a compact encounter record that fits the internal storage budget.
2. Add deterministic generated names and local field-note text.
3. Persist one record when a visitor fully exits, without writing every frame.
4. Add a minimal Fishbook UI reached from the aquarium, likely by long press.
5. Add host tests for record serialization, bounded storage eviction/favorites, deterministic field notes, and one memory per completed visitor.
6. Run host tests and `ninja -C build -j4 all` before any Milestone 6 hardware checkpoint.
