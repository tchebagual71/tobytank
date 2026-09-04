#pragma once

#include "fish/behavior.h"
#include "sim/snapshot.h"

typedef struct {
    float x;
    float y;
    float velocity_x;
    float velocity_y;
    float target_x;
    float target_y;
    float tail_phase;
    float fin_phase;
} tobytank_motion_t;

void tobytank_motion_start_entry(tobytank_motion_t *motion,
                                 const tobytank_genome_t *genome,
                                 const tobytank_behavior_plan_t *plan,
                                 int tank_width);

void tobytank_motion_start_exit(tobytank_motion_t *motion,
                                const tobytank_genome_t *genome,
                                const tobytank_behavior_plan_t *plan,
                                int tank_width);

void tobytank_motion_update(tobytank_motion_t *motion,
                            const tobytank_genome_t *genome,
                            tobytank_visitor_state_t state,
                            const tobytank_behavior_plan_t *plan,
                            float dt_seconds,
                            int tank_width,
                            int tank_height);

