#pragma once

#include "fish/genome.h"
#include "fish/prng.h"

typedef struct {
    float empty_seconds;
    float entry_seconds;
    float explore_seconds;
    float exit_seconds;
    float cruise_y;
    float entry_y;
    float exit_y;
} tobytank_behavior_plan_t;

void tobytank_behavior_plan(tobytank_behavior_plan_t *plan,
                            const tobytank_genome_t *genome,
                            int tank_width, int tank_height,
                            tobytank_prng_t *rng);

