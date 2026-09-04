#include "fish/behavior.h"

#include <stddef.h>

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

void tobytank_behavior_plan(tobytank_behavior_plan_t *plan,
                            const tobytank_genome_t *genome,
                            int tank_width, int tank_height,
                            tobytank_prng_t *rng)
{
    if (plan == NULL || genome == NULL || rng == NULL ||
        tank_width <= 0 || tank_height <= 0) {
        return;
    }

    const float calm = clampf(genome->hover_tendency, 0.0f, 1.0f);
    const float bold = clampf(genome->boldness, 0.0f, 1.0f);
    plan->empty_seconds = tobytank_prng_range(rng, 4.0f, 11.0f);
    plan->entry_seconds = tobytank_prng_range(rng, 6.0f, 12.0f) + calm * 4.0f;
    plan->explore_seconds = tobytank_prng_range(rng, 24.0f, 52.0f) + bold * 16.0f;
    plan->exit_seconds = tobytank_prng_range(rng, 7.0f, 14.0f) + calm * 3.0f;

    const float top = (float)tank_height * 0.18f;
    const float bottom = (float)tank_height * 0.72f;
    plan->cruise_y = top + (bottom - top) * clampf(genome->depth_preference, 0.0f, 1.0f);
    plan->cruise_y += tobytank_prng_range(rng, -18.0f, 18.0f);
    plan->cruise_y = clampf(plan->cruise_y, top, bottom);
    plan->entry_y = clampf(plan->cruise_y + tobytank_prng_range(rng, -28.0f, 22.0f),
                           top, bottom);
    plan->exit_y = clampf(plan->cruise_y + tobytank_prng_range(rng, -22.0f, 28.0f),
                          top, bottom);
    (void)tank_width;
}

