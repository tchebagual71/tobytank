#include "aquarium/lifecycle.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#include "fish/genome_validate.h"
#include "fish/prng.h"

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

static void clear_fish(tobytank_lifecycle_t *lifecycle)
{
    memset(&lifecycle->genome, 0, sizeof(lifecycle->genome));
    memset(&lifecycle->motion, 0, sizeof(lifecycle->motion));
    memset(&lifecycle->plan, 0, sizeof(lifecycle->plan));
    lifecycle->has_fish = 0;
}

static void enter_empty(tobytank_lifecycle_t *lifecycle)
{
    clear_fish(lifecycle);
    lifecycle->state = TOBYTANK_VISITOR_EMPTY;
    lifecycle->state_age = 0.0f;
    lifecycle->state_duration = tobytank_prng_range(&lifecycle->rng, 4.0f, 11.0f);
}

static int admit_next(tobytank_lifecycle_t *lifecycle)
{
    if (lifecycle->identity_source == NULL) {
        return 0;
    }

    for (int attempt = 0; attempt < 4; ++attempt) {
        tobytank_identity_t identity = TOBYTANK_IDENTITY_INVALID;
        if (!lifecycle->identity_source(lifecycle->identity_context, &identity)) {
            return 0;
        }
        if (!tobytank_genome_generate(&lifecycle->genome, identity)) {
            continue;
        }
        if (tobytank_genome_validate(&lifecycle->genome) == TOBYTANK_GENOME_OK) {
            tobytank_behavior_plan(&lifecycle->plan, &lifecycle->genome,
                                   lifecycle->width, lifecycle->height,
                                   &lifecycle->rng);
            tobytank_motion_start_entry(&lifecycle->motion, &lifecycle->genome,
                                        &lifecycle->plan, lifecycle->width);
            lifecycle->state = TOBYTANK_VISITOR_ENTERING;
            lifecycle->state_age = 0.0f;
            lifecycle->state_duration = lifecycle->plan.entry_seconds;
            lifecycle->has_fish = 1;
            return 1;
        }
    }
    return 0;
}

static void start_exploring(tobytank_lifecycle_t *lifecycle)
{
    lifecycle->state = TOBYTANK_VISITOR_EXPLORING;
    lifecycle->state_age = 0.0f;
    lifecycle->state_duration = lifecycle->plan.explore_seconds;
    lifecycle->motion.target_x = (float)lifecycle->width * 0.52f;
    lifecycle->motion.target_y = lifecycle->plan.cruise_y;
}

static void start_exit(tobytank_lifecycle_t *lifecycle)
{
    lifecycle->state = TOBYTANK_VISITOR_EXITING;
    lifecycle->state_age = 0.0f;
    lifecycle->state_duration = lifecycle->plan.exit_seconds;
    tobytank_motion_start_exit(&lifecycle->motion, &lifecycle->genome,
                               &lifecycle->plan, lifecycle->width);
}

static void apply_interactions(tobytank_lifecycle_t *lifecycle,
                               const tobytank_interactions_t *interactions,
                               float dt_seconds)
{
    if (interactions == NULL || !lifecycle->has_fish ||
        interactions->attention_strength <= 0.01f ||
        lifecycle->state == TOBYTANK_VISITOR_EMPTY ||
        lifecycle->state == TOBYTANK_VISITOR_EXITING) {
        return;
    }

    const float dx = interactions->attention_x - lifecycle->motion.x;
    const float dy = interactions->attention_y - lifecycle->motion.y;
    const float distance2 = dx * dx + dy * dy;
    if (distance2 < 1.0f) {
        return;
    }
    const float distance = sqrtf(distance2);
    const float temperament = 0.25f + lifecycle->genome.curiosity * 0.50f -
                              lifecycle->genome.hover_tendency * 0.18f;
    const float falloff = 1.0f / (1.0f + distance / 110.0f);
    const float impulse = lifecycle->genome.preferred_speed * temperament *
                          interactions->attention_strength * falloff * dt_seconds;
    lifecycle->motion.velocity_x += dx / distance * impulse;
    lifecycle->motion.velocity_y += dy / distance * impulse * 0.55f;
    lifecycle->motion.velocity_x = clampf(lifecycle->motion.velocity_x,
                                          -lifecycle->genome.preferred_speed,
                                          lifecycle->genome.preferred_speed);
    lifecycle->motion.velocity_y = clampf(lifecycle->motion.velocity_y,
                                          -lifecycle->genome.preferred_speed * 0.55f,
                                          lifecycle->genome.preferred_speed * 0.55f);
}

int tobytank_lifecycle_init(tobytank_lifecycle_t *lifecycle,
                            uint64_t seed,
                            int width,
                            int height,
                            tobytank_identity_source_fn identity_source,
                            void *identity_context)
{
    if (lifecycle == NULL || width < 64 || height < 64) {
        return 0;
    }
    memset(lifecycle, 0, sizeof(*lifecycle));
    lifecycle->width = width;
    lifecycle->height = height;
    lifecycle->seed = seed;
    lifecycle->identity_source = identity_source;
    lifecycle->identity_context = identity_context;
    tobytank_prng_seed_raw(&lifecycle->rng, seed ^ 0x4C4946454359434CULL);
    enter_empty(lifecycle);
    return 1;
}

void tobytank_lifecycle_update(tobytank_lifecycle_t *lifecycle, float dt_seconds)
{
    tobytank_lifecycle_update_with_interactions(lifecycle, dt_seconds, NULL);
}

void tobytank_lifecycle_update_with_interactions(tobytank_lifecycle_t *lifecycle,
                                                 float dt_seconds,
                                                 const tobytank_interactions_t *interactions)
{
    if (lifecycle == NULL || dt_seconds <= 0.0f || lifecycle->width < 64 ||
        lifecycle->height < 64) {
        return;
    }

    lifecycle->state_age += dt_seconds;
    switch (lifecycle->state) {
    case TOBYTANK_VISITOR_EMPTY:
        if (lifecycle->state_age >= lifecycle->state_duration) {
            if (!admit_next(lifecycle)) {
                enter_empty(lifecycle);
            }
        }
        break;
    case TOBYTANK_VISITOR_ENTERING:
        tobytank_motion_update(&lifecycle->motion, &lifecycle->genome, lifecycle->state,
                               &lifecycle->plan, dt_seconds,
                               lifecycle->width, lifecycle->height);
        apply_interactions(lifecycle, interactions, dt_seconds);
        if (lifecycle->state_age >= lifecycle->state_duration ||
            lifecycle->motion.x >= lifecycle->motion.target_x - 2.0f) {
            start_exploring(lifecycle);
        }
        break;
    case TOBYTANK_VISITOR_EXPLORING:
        tobytank_motion_update(&lifecycle->motion, &lifecycle->genome, lifecycle->state,
                               &lifecycle->plan, dt_seconds,
                               lifecycle->width, lifecycle->height);
        apply_interactions(lifecycle, interactions, dt_seconds);
        if (lifecycle->state_age >= lifecycle->state_duration) {
            start_exit(lifecycle);
        }
        break;
    case TOBYTANK_VISITOR_EXITING:
        tobytank_motion_update(&lifecycle->motion, &lifecycle->genome, lifecycle->state,
                               &lifecycle->plan, dt_seconds,
                               lifecycle->width, lifecycle->height);
        if (lifecycle->state_age >= lifecycle->state_duration ||
            lifecycle->motion.x > (float)lifecycle->width + 96.0f) {
            enter_empty(lifecycle);
        }
        break;
    default:
        enter_empty(lifecycle);
        break;
    }
}

void tobytank_lifecycle_snapshot(const tobytank_lifecycle_t *lifecycle,
                                 tobytank_fish_snapshot_t *snapshot)
{
    if (lifecycle == NULL || snapshot == NULL) {
        return;
    }
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->state = lifecycle->state;
    snapshot->has_fish = lifecycle->has_fish;
    snapshot->identity = lifecycle->has_fish ? lifecycle->genome.identity :
                                               TOBYTANK_IDENTITY_INVALID;
    if (lifecycle->has_fish) {
        snapshot->genome = lifecycle->genome;
        snapshot->x = lifecycle->motion.x;
        snapshot->y = lifecycle->motion.y;
        snapshot->facing = lifecycle->motion.velocity_x < -0.1f ? -1.0f : 1.0f;
        snapshot->tail_phase = lifecycle->motion.tail_phase;
        snapshot->fin_phase = lifecycle->motion.fin_phase;
    }
    snapshot->state_age = lifecycle->state_age;
    snapshot->state_time_remaining = lifecycle->state_duration > lifecycle->state_age ?
                                     lifecycle->state_duration - lifecycle->state_age : 0.0f;
}
