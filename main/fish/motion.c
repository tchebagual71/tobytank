#include "fish/motion.h"

#include <math.h>
#include <stddef.h>

#define OFFSCREEN_MARGIN 96.0f

static float clampf(float value, float low, float high)
{
    if (value < low) {
        return low;
    }
    if (value > high) {
        return high;
    }
    return value;
}

static float approach(float current, float target, float max_delta)
{
    const float delta = target - current;
    if (delta > max_delta) {
        return current + max_delta;
    }
    if (delta < -max_delta) {
        return current - max_delta;
    }
    return target;
}

void tobytank_motion_start_entry(tobytank_motion_t *motion,
                                 const tobytank_genome_t *genome,
                                 const tobytank_behavior_plan_t *plan,
                                 int tank_width)
{
    if (motion == NULL || genome == NULL || plan == NULL) {
        return;
    }
    motion->x = -OFFSCREEN_MARGIN;
    motion->y = plan->entry_y;
    motion->target_x = (float)tank_width * (0.36f + 0.18f * genome->curiosity);
    motion->target_y = plan->cruise_y;
    motion->velocity_x = genome->preferred_speed * 0.70f;
    motion->velocity_y = 0.0f;
    motion->tail_phase = 0.0f;
    motion->fin_phase = 0.0f;
}

void tobytank_motion_start_exit(tobytank_motion_t *motion,
                                const tobytank_genome_t *genome,
                                const tobytank_behavior_plan_t *plan,
                                int tank_width)
{
    if (motion == NULL || genome == NULL || plan == NULL) {
        return;
    }
    motion->target_x = (float)tank_width + OFFSCREEN_MARGIN;
    motion->target_y = plan->exit_y;
    motion->velocity_x = genome->preferred_speed * 0.80f;
}

void tobytank_motion_update(tobytank_motion_t *motion,
                            const tobytank_genome_t *genome,
                            tobytank_visitor_state_t state,
                            const tobytank_behavior_plan_t *plan,
                            float dt_seconds,
                            int tank_width,
                            int tank_height)
{
    if (motion == NULL || genome == NULL || plan == NULL || dt_seconds <= 0.0f) {
        return;
    }

    float target_x = motion->target_x;
    float target_y = motion->target_y;
    if (state == TOBYTANK_VISITOR_EXPLORING) {
        const float wave_a = sinf(motion->fin_phase * 0.41f + genome->curiosity * 6.0f);
        const float wave_b = sinf(motion->fin_phase * 0.17f + genome->boldness * 4.0f);
        target_x = (float)tank_width * (0.22f + 0.56f * (0.5f + 0.5f * wave_a));
        target_y = plan->cruise_y + wave_b * (18.0f + 22.0f * genome->curiosity);
    }

    target_y = clampf(target_y, (float)tank_height * 0.14f, (float)tank_height * 0.76f);
    const float speed = genome->preferred_speed *
                        (state == TOBYTANK_VISITOR_EXPLORING ? 0.55f : 0.90f);
    const float response = 0.50f + genome->turn_response;
    motion->velocity_x = approach(motion->velocity_x, (target_x - motion->x) * response,
                                  speed * dt_seconds * 2.2f);
    motion->velocity_y = approach(motion->velocity_y, (target_y - motion->y) * response,
                                  speed * dt_seconds * 1.5f);
    motion->velocity_x = clampf(motion->velocity_x, -speed, speed);
    motion->velocity_y = clampf(motion->velocity_y, -speed * 0.55f, speed * 0.55f);
    motion->x += motion->velocity_x * dt_seconds;
    motion->y += motion->velocity_y * dt_seconds;
    motion->tail_phase += genome->swim_cadence * dt_seconds;
    motion->fin_phase += dt_seconds;
    if (motion->tail_phase >= 1.0f) {
        motion->tail_phase -= floorf(motion->tail_phase);
    }
}

