# TobyTank Product Specification

## Product Vision

TobyTank is a calm desk object that makes the Waveshare ESP32-S3-Touch-AMOLED-1.8 feel like a small, living aquarium behind glass. It should reward quiet glances, curiosity, and gentle interaction without becoming a demanding virtual pet.

The emotional premise is simple and strict: exactly one unique visitor fish may be present at a time, every fish appears only once, and every encounter becomes a memory. A visitor is not a sprite selected from a catalog. It is a one-time procedural creature with a persistent identity, coherent anatomy, behavior, and encounter record.

TobyTank must work as a fully offline product. Connectivity may improve time synchronization, gallery access, or sharing later, but the aquarium, visitor lifecycle, uniqueness guarantee, and Fishbook must not depend on Wi-Fi, cloud services, accounts, or downloaded runtime assets.

## Core Experience Loop

The product loop is:

anticipation -> arrival -> discovery -> interaction -> attachment -> farewell -> keepsake

- Anticipation: the tank is empty but alive, with subtle water motion, particles, lighting, plants, and bubbles.
- Arrival: environmental cues foreshadow an incoming visitor before it enters fully from offscreen.
- Discovery: the fish reveals its silhouette, color, markings, movement, temperament, and unusual traits over time.
- Interaction: touch, tilt, room sound, and restrained audio responses let the owner influence the moment without controlling the fish like a toy cursor.
- Attachment: the fish reacts according to personality and encounter history, creating a brief relationship.
- Farewell: the fish naturally leaves the tank, fully exiting before it is retired.
- Keepsake: a compact memory is saved to the Fishbook so the encounter remains even though the fish never returns.

## Target Hardware

- Board: Waveshare ESP32-S3-Touch-AMOLED-1.8.
- MCU/module class: ESP32-S3R8.
- Memory: 8 MB octal PSRAM and 16 MB flash.
- Display: 368x448 RGB565 AMOLED.
- Framework: ESP-IDF v5.5.5, C/C++, and FreeRTOS.
- Board support: managed BSP `waveshare/esp32_s3_touch_amoled_1_8`.

Application code must rely on the managed BSP for board revision probing, display/touch controller selection, panel offsets, pins, power setup, and low-level initialization. The application must not invent GPIOs, panel controller registers, or board-revision rules.

## Core Requirements

### AMOLED Presentation

- Render a full-screen aquarium with no ordinary menu chrome, labels, borders, debug overlays, or visible UI during idle aquarium mode.
- Use AMOLED-friendly blacks, controlled brightness, dark teal/blue depth, a darker lower region, soft haze, and restrained highlights.
- Include procedural background elements such as substrate, stones, plants, driftwood, particles, bubbles, caustics, surface shimmer, and parallax layers.
- Keep empty-tank time visually meaningful; the aquarium must feel alive even when no fish is present.
- Include burn-in protection from the start of final polish: dimming, small long-period shifts, and avoidance of static bright elements.

### Visitor Lifecycle

- Enforce one visitor fish at a time.
- Use an explicit lifecycle similar to `EMPTY_WAIT`, `ENTERING`, `EXPLORING`, `EXITING`, and return to `EMPTY_WAIT`.
- Spend visible randomized time empty between visitors.
- Generate a fish only when the next visitor is needed.
- Start each visitor fully offscreen and enter smoothly.
- Keep each visitor visible for a bounded randomized stay.
- Exit naturally and destroy or retire the live instance only after it is fully offscreen.
- Never reuse a completed fish identity during ordinary resets or power loss.

### Procedural Fish Generation

- Assign every visitor a unique persistent identity of at least 64 bits.
- Derive deterministic PRNG streams from the identity for anatomy, color, pattern, behavior, motion, and sound.
- Use a coherent procedural genome rather than finite premade fish sprites, palettes, or definitions.
- Make uniqueness perceptible through silhouette, face, fins, color, markings, movement, temperament, arrival behavior, sound cues, and encounter history.
- Validate generated genomes for safe geometry, readable contrast, reasonable animation ranges, and rasterizer safety.
- Reject malformed genomes deterministically.

The genome should cover at least body dimensions, taper, curves, eyes, mouth, gills, tail peduncle, caudal fin type, dorsal/anal/pelvic/pectoral fins, palette, iridescence, scales, markings, fin membranes/rays, accent marks, swimming cadence, preferred speed, turn response, curiosity, depth preference, and hover behavior.

### Behavior

- Use fixed-timestep deterministic simulation independent of rendering and hardware access.
- Publish immutable rendering snapshots from simulation state.
- Make motion organic: wandering, depth changes, turns, hovering, edge avoidance, acceleration limits, body undulation, tail beats, and coordinated fin motion.
- Correlate behavior with anatomy and temperament; differences should feel intentional, not arbitrary noise.
- Keep all interactions restrained so the object remains calm on a desk.

### Fishbook

- Save a compact record for every completed encounter.
- Include identity, generated name, arrival date/time when known, observed duration, temperament discoveries, favorite depth, interaction highlights, rarity traits, and a generated local field note.
- Save a portrait or reproducible portrait seed for each encounter.
- Preserve records automatically; the user must not be pressured to act before a fish leaves.
- Use internal storage for a bounded core Fishbook and optional microSD for a fuller archive.

### Touch

- Treat touch as contact with the aquarium glass, not as a conventional button layer.
- Support gestures such as tap ripples, drag currents, holding still near a curious fish, double-tap feeding if enabled, bubble popping, and long-press access to the Fishbook.
- Fish reactions must depend on temperament and current behavior.
- Touch must not break the lifecycle scheduler or allow multiple fish.

### IMU

- Use the IMU for subtle physical presence: parallax, plant sway, particle drift, light direction changes, and gentle water-surge cues.
- Avoid making the fish fall under gravity or turning the aquarium into an aggressive motion game.
- Startle or curiosity responses must never punish the user or erase progress.

### Microphone

- Use the microphone conservatively for ambient reactions such as sharp tap/clap detection, quiet-room conditions, rhythmic secrets, and gentle influence from sustained room sound.
- Avoid constant reactions to normal conversation.
- Coordinate microphone capture and speaker playback through a single audio ownership model so shared codec resources are handled safely.

### Speaker

- Default sound should be quiet and sparse.
- Provide silent, ambient, and expressive sound modes when audio settings exist.
- Include restrained water ambience, bubbles, arrival signatures, touch/feeding sounds, and farewell motifs.
- Derive procedural audio characteristics from the fish genome where practical.
- Respect nighttime behavior once RTC lighting exists.

### RTC

- Use RTC time for offline lighting phases and time-dependent ecology when valid time is available.
- Support morning, daylight, sunset, night, and deep-night palettes.
- Let visitor families and rare conditions depend on local time without requiring internet access.
- If the RTC is unavailable or unset, run with a deterministic fallback schedule and log the limitation.

### Ethical Reward Design

- Encourage delight through variable arrival timing, anticipation cues, staged reveals, coherent rare traits, interaction responses, secrets, and permanent memories.
- Do not implement punishment, sickness, missed-day penalties, streak pressure, aggressive notifications, currency, grinding, forced feeding, or fake scarcity countdowns.
- Consider a hidden wonder guarantee so the experience does not become monotonous, while keeping the mechanism invisible and non-exploitative.

## Optional Future Enhancements

These are explicitly outside the mandatory offline core until their roadmap milestones are approved:

- Wi-Fi provisioning for NTP time synchronization.
- Local web Fishbook gallery on the LAN.
- Optional local weather influence.
- Local sharing of compact genome postcards between devices.
- Firmware-defined seasonal events that do not download premade fish.
- Optional microSD archive with full-resolution portraits, encounter metadata, HTML gallery export, and imported themes or sound packs.
- Habitat evolution based on encounter history.
- Top-down view or hidden orientation-dependent scenes if IMU behavior remains calm and coherent.

## User-Facing Acceptance Criteria

- On power-up, TobyTank shows a full-screen aquarium on the 368x448 AMOLED with no menu or debug UI.
- The aquarium remains visually alive when empty.
- At most one visitor fish is ever visible or alive in simulation.
- Every visitor enters from fully offscreen, explores, and exits fully offscreen without popping.
- Empty and occupied durations vary within documented bounds.
- No visitor identity repeats across ordinary resets or power loss.
- Consecutive visitors are visibly different in coherent ways, including more than color swaps.
- Touch and motion interactions feel like physical interaction with water and glass, not direct avatar control.
- Ignoring the device never causes punishment, sickness, lost progress, or pressure.
- Every completed encounter creates a Fishbook memory.
- Core aquarium behavior works without Wi-Fi, microSD, accounts, cloud services, or downloaded assets.
- Audio, microphone, RTC, Wi-Fi, and microSD failures degrade gracefully and are logged when those features exist.
- The firmware milestone claiming completion has passing host tests, a successful ESP-IDF build, and documented hardware observations.
