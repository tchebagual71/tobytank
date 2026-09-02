# TobyTank Roadmap

Development proceeds in small, independently testable milestones. Mandatory offline aquarium behavior comes before optional connectivity and archival features. Only the current approved milestone should be implemented at any time.

## Milestone 0: ESP-IDF Foundation and Display Proof

- Scope: create the standalone ESP-IDF project skeleton, managed BSP dependency, display boundary, RGB565 frame submission path, basic frame loop, diagnostics, and placeholder static aquarium color proof.
- Expected files/modules: `CMakeLists.txt`, `main/CMakeLists.txt`, `main/idf_component.yml`, `main/app_main.c`, `main/hardware/board.*`, `main/hardware/display.*`, `main/render/canvas.*`, `main/render/renderer.*`, `sdkconfig.defaults`, `partitions.csv`, and `tests/` project contract checks.
- Host-test requirements: verify repository contract, dimensions, generated-file ignore rules, RGB565 helpers, canvas clipping, and that hardware-facing code uses the BSP boundary rather than hardcoded board revision logic.
- Build requirements: activate ESP-IDF v5.5.5, run `idf.py set-target esp32s3`, then `ninja -C build -j4 all`.
- Hardware checkpoint: only with explicit authorization, flash a display proof and confirm full-screen RGB565 output, brightness control, no crash loop, and low-rate serial diagnostics.
- Definition of done: host tests pass, ESP-IDF build succeeds, docs are current, no generated files are tracked, and hardware validation status is recorded.

## Milestone 1: Procedural Aquarium Environment and Host Preview

- Scope: implement deterministic offline aquarium background and ambient animation without fish.
- Expected files/modules: `main/aquarium/environment.*`, `main/render/background.*`, `main/render/particles.*`, `main/render/effects.*`, `tools/preview/`, and host preview output helpers.
- Host-test requirements: seeded environment generation is deterministic, generated scene parameters stay in range, particles/bubbles remain bounded, raster primitives clip safely, and preview frames render to a dependency-free image format.
- Build requirements: host tests pass and ESP-IDF display build remains successful with bounded parallelism.
- Hardware checkpoint: confirm empty aquarium animates smoothly, has no visible UI, avoids static bright burn-in risks, and maintains acceptable low-rate telemetry.
- Definition of done: empty-tank presentation is pleasant, deterministic, previewable on host, and physically validated on display.

## Milestone 2: Persistent Unique Identity and Fish Genome

- Scope: add identity allocation, NVS block reservation, deterministic PRNG streams, coherent genome generation, genome validation, and genome fingerprints.
- Expected files/modules: `main/fish/identity.*`, `main/fish/prng.*`, `main/fish/genome.*`, `main/fish/genome_validate.*`, `main/memory/identity_store.*`, and host tests.
- Host-test requirements: same identity yields the same genome, consecutive identities yield different complete fingerprints, thousands of genomes validate, rejection/regeneration is deterministic, and simulated reboot skips unused IDs from a reserved block.
- Build requirements: ESP-IDF build succeeds with NVS dependency and explicit allocation/error handling.
- Hardware checkpoint: optional serial-only validation that identity block reservation works across ordinary reset, if explicitly authorized.
- Definition of done: identity reuse is prevented under the documented guarantee, genome generation is coherent and deterministic, and failure modes are explicit.

## Milestone 3: Detailed Procedural Fish Rasterizer

- Scope: draw generated fish anatomy and markings into cached masks/textures and composite them safely into RGB565 frames.
- Expected files/modules: `main/render/fish_rasterizer.*`, `main/render/fish_cache.*`, `main/render/dither.*`, `main/render/composite.*`, `main/fish/portrait.*`, and preview contact-sheet tooling.
- Host-test requirements: extreme valid genomes render inside guarded buffers, clipping holds at all edges, portrait generation is deterministic, and contact sheets show visibly coherent variety.
- Build requirements: firmware build succeeds without steady-state allocation in render loops and with PSRAM use isolated to large caches/buffers.
- Hardware checkpoint: inspect several generated fish on the AMOLED for readability, shimmer, color banding, and frame time.
- Definition of done: fish are visibly unique beyond palette swaps and rasterization is safe for every accepted genome.

## Milestone 4: Organic Swimming and Visitor Lifecycle

- Scope: implement the one-fish lifecycle scheduler, entry/exploration/exit states, steering, depth, tail/fin animation timing, and live fish snapshots.
- Expected files/modules: `main/aquarium/lifecycle.*`, `main/fish/motion.*`, `main/fish/behavior.*`, `main/sim/snapshot.*`, and telemetry updates.
- Host-test requirements: scheduler never allows more than one fish, durations respect bounds, entry starts fully offscreen, movement has no discontinuities, every fish exits within bounded timeout, and seeded replay is deterministic.
- Build requirements: ESP-IDF build succeeds with fixed simulation timestep independent of render FPS.
- Hardware checkpoint: confirm natural entry, exploration, farewell, empty intervals, and at least 20 FPS target behavior on display.
- Definition of done: the central one-time visitor loop works offline and is documented with measured hardware observations.

## Milestone 5: Touch and IMU Physical Interactions

- Scope: add normalized touch and IMU input layers, gentle touch ripples/currents, fish temperament reactions, and tilt-based parallax/water cues.
- Expected files/modules: `main/input/touch.*`, `main/input/imu.*`, `main/input/motion_filter.*`, `main/aquarium/interactions.*`, and related tests.
- Host-test requirements: gesture recognition and motion filtering are deterministic from recorded samples, interactions are bounded, and lifecycle state cannot be corrupted by input.
- Build requirements: ESP-IDF build succeeds with managed BSP/sensor APIs and graceful fallback when input hardware is unavailable.
- Hardware checkpoint: validate touch coordinates, IMU orientation mapping, subtle parallax, no harsh motion behavior, and no accidental menu activation.
- Definition of done: interactions add presence while preserving calmness, determinism, and one-fish lifecycle rules.

## Milestone 6: Fishbook and Persistent Encounter Records

- Scope: persist compact encounter records, generated names, local field notes, portrait references, favorites, and a minimal Fishbook UI reached from the aquarium.
- Expected files/modules: `main/fishbook/records.*`, `main/fishbook/names.*`, `main/fishbook/field_notes.*`, `main/fishbook/ui.*`, `main/memory/fishbook_store.*`, and tests.
- Host-test requirements: record serialization round-trips, bounded storage eviction/favorites work, field-note generation is deterministic, and every completed visitor produces one memory.
- Build requirements: ESP-IDF build succeeds with NVS/storage limits documented and no per-frame persistence writes.
- Hardware checkpoint: confirm long-press Fishbook access, readable portrait cards, automatic save after farewell, and graceful behavior when storage is full.
- Definition of done: every encounter becomes a durable memory without user pressure or identity reuse.

## Milestone 7: RTC Lighting and Time-Dependent Ecology

- Scope: add RTC boundary, time validity handling, day/night lighting, visitor-family time preferences, nocturnal traits, and quiet nighttime behavior.
- Expected files/modules: `main/hardware/rtc.*`, `main/aquarium/time_ecology.*`, `main/render/lighting.*`, and tests.
- Host-test requirements: lighting phase selection is deterministic, invalid RTC data falls back safely, visitor probabilities remain bounded, and nighttime sound/display rules are enforced.
- Build requirements: ESP-IDF build succeeds with managed `waveshare/pcf85063a` dependency where needed.
- Hardware checkpoint: confirm RTC detection/read, lighting phase changes, and logged fallback when RTC is unavailable or unset.
- Definition of done: time enriches the offline aquarium without requiring Wi-Fi.

## Milestone 8: Speaker and Microphone Interactions

- Scope: add audio ownership, quiet procedural sound playback, arrival/farewell motifs, touch event sounds, clap/tap detection, and conservative ambient classification.
- Expected files/modules: `main/audio/audio.*`, `main/audio/synth.*`, `main/audio/mic.*`, `main/audio/events.*`, and tests for pure audio/event logic.
- Host-test requirements: event-to-sound mapping is deterministic, generated motif parameters stay in safe ranges, and microphone classifiers reject ordinary noise samples conservatively.
- Build requirements: ESP-IDF build succeeds with ES8311/I2S setup and explicit codec failure handling.
- Hardware checkpoint: validate speaker volume, silence mode, no audio boot loops, tap/clap sensitivity, and safe alternation between playback and capture.
- Definition of done: audio is tasteful, optional at runtime, and never required for core fish visits.

## Milestone 9: Optional Wi-Fi, NTP, and Local Gallery

- Scope: add optional Wi-Fi provisioning, NTP sync, and a local LAN Fishbook gallery without cloud dependency.
- Expected files/modules: `main/net/wifi.*`, `main/net/ntp.*`, `main/net/gallery_server.*`, `main/fishbook/export.*`, and tests for pure serialization/routes where practical.
- Host-test requirements: gallery export is deterministic, missing credentials leave core behavior offline, and network failures do not block aquarium simulation.
- Build requirements: ESP-IDF build succeeds with feature flags and documented memory impact.
- Hardware checkpoint: validate provisioning, NTP update, local gallery access, offline fallback, and no dependency on external accounts.
- Definition of done: connectivity enhances the Fishbook while the aquarium remains complete offline.

## Milestone 10: Optional microSD Archive

- Scope: add optional microSD archive for full portraits, encounter metadata, local HTML export, and optional imported themes/sound packs.
- Expected files/modules: `main/storage/sdcard.*`, `main/fishbook/archive.*`, `main/fishbook/html_export.*`, and tests for archive manifests.
- Host-test requirements: archive manifest serialization round-trips, missing card is non-fatal, writes are bounded, and malformed imported assets are rejected.
- Build requirements: ESP-IDF build succeeds using BSP SD mounting APIs with no generated archive files tracked.
- Hardware checkpoint: validate FAT-formatted card mount, write/read/unmount behavior, removal fallback, and serial logging of failures.
- Definition of done: microSD extends retention but removal never breaks core aquarium or internal Fishbook.

## Milestone 11: Performance, Burn-In Protection, and Final Polish

- Scope: optimize frame time, memory use, telemetry, visual polish, burn-in mitigation, failure handling, and final documentation.
- Expected files/modules: cross-cutting updates to `main/render/`, `main/aquarium/`, `main/fish/`, `main/hardware/`, `main/diagnostics/`, `README.md`, `PRODUCT_SPEC.md`, `ROADMAP.md`, and `DEVELOPMENT.md`.
- Host-test requirements: full host test suite passes, deterministic replay remains stable, rendering guards still pass, and performance-sensitive code has regression coverage where practical.
- Build requirements: clean ESP-IDF build with `ninja -C build -j4 all`, no warnings attributable to TobyTank, and generated files ignored.
- Hardware checkpoint: record on-device FPS, render time, free internal memory, free PSRAM, visual stability, temperature/brightness comfort, burn-in behavior, and graceful peripheral failure modes.
- Definition of done: the product meets the user-facing acceptance criteria and has documented host, build, and physical validation results.
