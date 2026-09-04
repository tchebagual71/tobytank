#pragma once

#include <stdint.h>

#include "aquarium/interactions.h"
#include "fish/prng.h"

/*
 * Deterministic aquarium environment.
 *
 * This module is pure C with no hardware, ESP-IDF, or rendering dependencies so
 * it can run unchanged on the host. Simulation advances on a fixed timestep and
 * publishes an immutable snapshot; renderers read the snapshot and never touch
 * the live state.
 */

#define TOBYTANK_ENV_MAX_PLANTS 10
#define TOBYTANK_ENV_MAX_STONES 8
#define TOBYTANK_ENV_MAX_SHAFTS 4
#define TOBYTANK_ENV_MAX_BUBBLES 40
#define TOBYTANK_ENV_MAX_MOTES 96
#define TOBYTANK_ENV_SUBSTRATE_POINTS 12

/* Fixed simulation timestep, independent of render rate. */
#define TOBYTANK_ENV_TIMESTEP_SECONDS (1.0f / 60.0f)

/* Bounds the tests and the renderers rely on. */
#define TOBYTANK_ENV_BUBBLE_MIN_RADIUS 1.0f
#define TOBYTANK_ENV_BUBBLE_MAX_RADIUS 4.0f
#define TOBYTANK_ENV_MARGIN 24.0f

typedef struct {
    float x;
    float y;
    float radius;
    float rise_speed;
    float wobble_phase;
    float wobble_speed;
    float wobble_amount;
    uint8_t alpha;
} tobytank_bubble_t;

typedef struct {
    float x;
    float y;
    float drift_x;
    float drift_y;
    float radius;
    uint8_t alpha;
} tobytank_mote_t;

typedef struct {
    float base_x;
    float height;
    float sway_phase;
    float sway_speed;
    float sway_amount;
    uint8_t blades;
    uint8_t green;
    uint8_t blue;
} tobytank_plant_t;

typedef struct {
    float center_x;
    float center_y;
    float radius_x;
    float radius_y;
    uint8_t tone;
} tobytank_stone_t;

typedef struct {
    float x;
    float width;
    float slant;
    float drift_speed;
    float reach;
    uint8_t intensity;
} tobytank_shaft_t;

/* Immutable rendering snapshot. */
typedef struct {
    int width;
    int height;
    float time_seconds;

    uint8_t water_top[3];
    uint8_t water_bottom[3];
    uint8_t haze[3];
    uint8_t substrate_top[3];
    uint8_t substrate_bottom[3];

    float substrate_height[TOBYTANK_ENV_SUBSTRATE_POINTS];

    int plant_count;
    int stone_count;
    int shaft_count;
    int bubble_count;
    int mote_count;

    tobytank_plant_t plants[TOBYTANK_ENV_MAX_PLANTS];
    tobytank_stone_t stones[TOBYTANK_ENV_MAX_STONES];
    tobytank_shaft_t shafts[TOBYTANK_ENV_MAX_SHAFTS];
    tobytank_bubble_t bubbles[TOBYTANK_ENV_MAX_BUBBLES];
    tobytank_mote_t motes[TOBYTANK_ENV_MAX_MOTES];
} tobytank_env_snapshot_t;

typedef struct {
    uint64_t seed;
    tobytank_prng_t rng;
    tobytank_env_snapshot_t scene;
} tobytank_environment_t;

/*
 * Generate an environment for the given seed and canvas size. The same seed and
 * size always produce the same scene. Sizes below 64x64 are rejected.
 */
void tobytank_environment_init(tobytank_environment_t *environment,
                               uint64_t seed, int width, int height);

/* Advance one fixed timestep. dt_seconds should be TOBYTANK_ENV_TIMESTEP_SECONDS. */
void tobytank_environment_update(tobytank_environment_t *environment, float dt_seconds);

void tobytank_environment_update_with_interactions(
    tobytank_environment_t *environment,
    float dt_seconds,
    const tobytank_interactions_t *interactions);

/* Copy the current state into an immutable snapshot for rendering. */
void tobytank_environment_snapshot(const tobytank_environment_t *environment,
                                   tobytank_env_snapshot_t *snapshot);

/* Substrate surface height in pixels at a given column, interpolated. */
float tobytank_environment_substrate_at(const tobytank_env_snapshot_t *snapshot, float x);
