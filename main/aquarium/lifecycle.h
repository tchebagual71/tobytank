#pragma once

#include "aquarium/interactions.h"
#include "fish/behavior.h"
#include "fish/motion.h"
#include "sim/snapshot.h"

typedef int (*tobytank_identity_source_fn)(void *context, tobytank_identity_t *out_identity);

typedef struct {
    int width;
    int height;
    uint64_t seed;
    tobytank_identity_source_fn identity_source;
    void *identity_context;
    tobytank_prng_t rng;
    tobytank_visitor_state_t state;
    float state_age;
    float state_duration;
    tobytank_genome_t genome;
    tobytank_behavior_plan_t plan;
    tobytank_motion_t motion;
    int has_fish;
} tobytank_lifecycle_t;

int tobytank_lifecycle_init(tobytank_lifecycle_t *lifecycle,
                            uint64_t seed,
                            int width,
                            int height,
                            tobytank_identity_source_fn identity_source,
                            void *identity_context);

void tobytank_lifecycle_update(tobytank_lifecycle_t *lifecycle, float dt_seconds);

void tobytank_lifecycle_update_with_interactions(tobytank_lifecycle_t *lifecycle,
                                                 float dt_seconds,
                                                 const tobytank_interactions_t *interactions);

void tobytank_lifecycle_snapshot(const tobytank_lifecycle_t *lifecycle,
                                 tobytank_fish_snapshot_t *snapshot);
