#include "aquarium/environment.h"

#include <string.h>

#include "fish/prng.h"

static uint8_t clamp_u8(int value)
{
    if (value < 0) {
        return 0;
    }
    if (value > 255) {
        return 255;
    }
    return (uint8_t)value;
}

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

static void reset_bubble(tobytank_environment_t *environment, tobytank_bubble_t *bubble,
                         int start_at_bottom)
{
    tobytank_env_snapshot_t *scene = &environment->scene;
    bubble->x = tobytank_prng_range(&environment->rng, 4.0f, (float)scene->width - 4.0f);
    bubble->radius = tobytank_prng_range(&environment->rng, TOBYTANK_ENV_BUBBLE_MIN_RADIUS,
                                         TOBYTANK_ENV_BUBBLE_MAX_RADIUS);
    bubble->rise_speed = tobytank_prng_range(&environment->rng, 6.0f, 26.0f);
    bubble->wobble_phase = tobytank_prng_range(&environment->rng, 0.0f, 6.2831853f);
    bubble->wobble_speed = tobytank_prng_range(&environment->rng, 0.6f, 2.2f);
    bubble->wobble_amount = tobytank_prng_range(&environment->rng, 0.6f, 3.0f);
    bubble->alpha = (uint8_t)tobytank_prng_int(&environment->rng, 40, 110);
    if (start_at_bottom) {
        bubble->y = (float)scene->height + tobytank_prng_range(&environment->rng, 0.0f, 12.0f);
    } else {
        bubble->y = tobytank_prng_range(&environment->rng, 0.0f, (float)scene->height);
    }
}

static void reset_mote(tobytank_environment_t *environment, tobytank_mote_t *mote,
                       int anywhere)
{
    tobytank_env_snapshot_t *scene = &environment->scene;
    mote->x = tobytank_prng_range(&environment->rng, 0.0f, (float)scene->width);
    mote->y = anywhere ? tobytank_prng_range(&environment->rng, 0.0f, (float)scene->height)
                       : tobytank_prng_range(&environment->rng, -8.0f, 0.0f);
    mote->drift_x = tobytank_prng_range(&environment->rng, -4.0f, 4.0f);
    mote->drift_y = tobytank_prng_range(&environment->rng, 1.5f, 7.0f);
    mote->radius = tobytank_prng_range(&environment->rng, 0.5f, 1.6f);
    mote->alpha = (uint8_t)tobytank_prng_int(&environment->rng, 18, 62);
}

void tobytank_environment_init(tobytank_environment_t *environment,
                               uint64_t seed, int width, int height)
{
    if (environment == NULL || width < 64 || height < 64) {
        return;
    }

    memset(environment, 0, sizeof(*environment));
    environment->seed = seed;

    /* The environment owns its own seed namespace rather than a fish identity
       stream, so it seeds the shared generator directly. */
    tobytank_prng_seed_raw(&environment->rng, seed ^ 0xA5A5F00DFACE1234ULL);

    tobytank_env_snapshot_t *scene = &environment->scene;
    scene->width = width;
    scene->height = height;
    scene->time_seconds = 0.0f;

    /* Water palette: a cool blue-teal at the surface fading to near-black at the
       bottom, which suits an AMOLED and keeps average brightness low. */
    scene->water_top[0] = clamp_u8(tobytank_prng_int(&environment->rng, 4, 14));
    scene->water_top[1] = clamp_u8(tobytank_prng_int(&environment->rng, 52, 84));
    scene->water_top[2] = clamp_u8(tobytank_prng_int(&environment->rng, 78, 116));
    scene->water_bottom[0] = clamp_u8(tobytank_prng_int(&environment->rng, 0, 4));
    scene->water_bottom[1] = clamp_u8(tobytank_prng_int(&environment->rng, 6, 16));
    scene->water_bottom[2] = clamp_u8(tobytank_prng_int(&environment->rng, 12, 26));
    scene->haze[0] = clamp_u8(tobytank_prng_int(&environment->rng, 20, 40));
    scene->haze[1] = clamp_u8(tobytank_prng_int(&environment->rng, 90, 130));
    scene->haze[2] = clamp_u8(tobytank_prng_int(&environment->rng, 120, 170));

    scene->substrate_top[0] = clamp_u8(tobytank_prng_int(&environment->rng, 26, 44));
    scene->substrate_top[1] = clamp_u8(tobytank_prng_int(&environment->rng, 34, 54));
    scene->substrate_top[2] = clamp_u8(tobytank_prng_int(&environment->rng, 38, 60));
    scene->substrate_bottom[0] = clamp_u8(tobytank_prng_int(&environment->rng, 6, 14));
    scene->substrate_bottom[1] = clamp_u8(tobytank_prng_int(&environment->rng, 8, 18));
    scene->substrate_bottom[2] = clamp_u8(tobytank_prng_int(&environment->rng, 10, 22));

    /* Substrate profile: a gentle dune line across the bottom of the tank. */
    const float base = (float)height * 0.86f;
    for (int i = 0; i < TOBYTANK_ENV_SUBSTRATE_POINTS; ++i) {
        const float jitter =
            tobytank_prng_range(&environment->rng, -0.045f, 0.045f) * (float)height;
        scene->substrate_height[i] = clampf(base + jitter,
                                            (float)height * 0.78f,
                                            (float)height * 0.95f);
    }

    scene->stone_count = tobytank_prng_int(&environment->rng, 3, TOBYTANK_ENV_MAX_STONES);
    for (int i = 0; i < scene->stone_count; ++i) {
        tobytank_stone_t *stone = &scene->stones[i];
        stone->center_x = tobytank_prng_range(&environment->rng, 8.0f, (float)width - 8.0f);
        stone->radius_x = tobytank_prng_range(&environment->rng, 9.0f, 34.0f);
        stone->radius_y = stone->radius_x * tobytank_prng_range(&environment->rng, 0.34f, 0.68f);
        const float surface = tobytank_environment_substrate_at(scene, stone->center_x);
        stone->center_y = surface + tobytank_prng_range(&environment->rng, -2.0f, 8.0f);
        stone->tone = (uint8_t)tobytank_prng_int(&environment->rng, 30, 74);
    }

    scene->plant_count = tobytank_prng_int(&environment->rng, 4, TOBYTANK_ENV_MAX_PLANTS);
    for (int i = 0; i < scene->plant_count; ++i) {
        tobytank_plant_t *plant = &scene->plants[i];
        plant->base_x = tobytank_prng_range(&environment->rng, 6.0f, (float)width - 6.0f);
        plant->height = tobytank_prng_range(&environment->rng, (float)height * 0.14f,
                                            (float)height * 0.42f);
        plant->sway_phase = tobytank_prng_range(&environment->rng, 0.0f, 6.2831853f);
        plant->sway_speed = tobytank_prng_range(&environment->rng, 0.18f, 0.52f);
        plant->sway_amount = tobytank_prng_range(&environment->rng, 3.0f, 11.0f);
        plant->blades = (uint8_t)tobytank_prng_int(&environment->rng, 3, 7);
        plant->green = clamp_u8(tobytank_prng_int(&environment->rng, 78, 148));
        plant->blue = clamp_u8(tobytank_prng_int(&environment->rng, 56, 104));
    }

    scene->shaft_count = tobytank_prng_int(&environment->rng, 2, TOBYTANK_ENV_MAX_SHAFTS);
    for (int i = 0; i < scene->shaft_count; ++i) {
        tobytank_shaft_t *shaft = &scene->shafts[i];
        shaft->x = tobytank_prng_range(&environment->rng, 0.0f, (float)width);
        shaft->width = tobytank_prng_range(&environment->rng, 16.0f, 54.0f);
        shaft->slant = tobytank_prng_range(&environment->rng, -0.34f, 0.34f);
        shaft->drift_speed = tobytank_prng_range(&environment->rng, -3.4f, 3.4f);
        shaft->reach = tobytank_prng_range(&environment->rng, (float)height * 0.42f,
                                           (float)height * 0.78f);
        shaft->intensity = (uint8_t)tobytank_prng_int(&environment->rng, 10, 26);
    }

    scene->bubble_count = TOBYTANK_ENV_MAX_BUBBLES;
    for (int i = 0; i < scene->bubble_count; ++i) {
        reset_bubble(environment, &scene->bubbles[i], 0);
    }

    scene->mote_count = TOBYTANK_ENV_MAX_MOTES;
    for (int i = 0; i < scene->mote_count; ++i) {
        reset_mote(environment, &scene->motes[i], 1);
    }
}

void tobytank_environment_update(tobytank_environment_t *environment, float dt_seconds)
{
    tobytank_environment_update_with_interactions(environment, dt_seconds, NULL);
}

void tobytank_environment_update_with_interactions(
    tobytank_environment_t *environment,
    float dt_seconds,
    const tobytank_interactions_t *interactions)
{
    if (environment == NULL || dt_seconds <= 0.0f) {
        return;
    }

    tobytank_env_snapshot_t *scene = &environment->scene;
    if (scene->width < 64 || scene->height < 64) {
        return;
    }

    scene->time_seconds += dt_seconds;

    for (int i = 0; i < scene->plant_count; ++i) {
        tobytank_plant_t *plant = &scene->plants[i];
        plant->sway_phase += plant->sway_speed * dt_seconds;
        if (plant->sway_phase > 6.2831853f) {
            plant->sway_phase -= 6.2831853f;
        }
    }

    for (int i = 0; i < scene->shaft_count; ++i) {
        tobytank_shaft_t *shaft = &scene->shafts[i];
        shaft->x += shaft->drift_speed * dt_seconds;
        const float span = (float)scene->width + TOBYTANK_ENV_MARGIN * 2.0f;
        if (shaft->x < -TOBYTANK_ENV_MARGIN) {
            shaft->x += span;
        } else if (shaft->x > (float)scene->width + TOBYTANK_ENV_MARGIN) {
            shaft->x -= span;
        }
    }

    for (int i = 0; i < scene->bubble_count; ++i) {
        tobytank_bubble_t *bubble = &scene->bubbles[i];
        bubble->y -= bubble->rise_speed * dt_seconds;
        if (interactions != NULL) {
            bubble->x += interactions->current_x * 0.16f * dt_seconds;
            bubble->y += interactions->current_y * 0.06f * dt_seconds;
        }
        bubble->wobble_phase += bubble->wobble_speed * dt_seconds;
        if (bubble->wobble_phase > 6.2831853f) {
            bubble->wobble_phase -= 6.2831853f;
        }
        if (bubble->y + bubble->radius < -TOBYTANK_ENV_MARGIN) {
            reset_bubble(environment, bubble, 1);
        }
        bubble->x = clampf(bubble->x, -TOBYTANK_ENV_MARGIN,
                           (float)scene->width + TOBYTANK_ENV_MARGIN);
    }

    for (int i = 0; i < scene->mote_count; ++i) {
        tobytank_mote_t *mote = &scene->motes[i];
        float current_x = 0.0f;
        float current_y = 0.0f;
        if (interactions != NULL) {
            current_x = interactions->current_x;
            current_y = interactions->current_y;
        }
        mote->x += (mote->drift_x + current_x * 0.32f) * dt_seconds;
        mote->y += (mote->drift_y + current_y * 0.18f) * dt_seconds;
        if (mote->y > (float)scene->height + TOBYTANK_ENV_MARGIN) {
            reset_mote(environment, mote, 0);
        }
        if (mote->x < -TOBYTANK_ENV_MARGIN) {
            mote->x = (float)scene->width + TOBYTANK_ENV_MARGIN;
        } else if (mote->x > (float)scene->width + TOBYTANK_ENV_MARGIN) {
            mote->x = -TOBYTANK_ENV_MARGIN;
        }
    }
}

void tobytank_environment_snapshot(const tobytank_environment_t *environment,
                                   tobytank_env_snapshot_t *snapshot)
{
    if (environment == NULL || snapshot == NULL) {
        return;
    }
    *snapshot = environment->scene;
}

float tobytank_environment_substrate_at(const tobytank_env_snapshot_t *snapshot, float x)
{
    if (snapshot == NULL || snapshot->width <= 0) {
        return 0.0f;
    }

    const float span = (float)snapshot->width / (float)(TOBYTANK_ENV_SUBSTRATE_POINTS - 1);
    const float position = clampf(x, 0.0f, (float)snapshot->width - 1.0f) / span;
    int index = (int)position;
    if (index >= TOBYTANK_ENV_SUBSTRATE_POINTS - 1) {
        index = TOBYTANK_ENV_SUBSTRATE_POINTS - 2;
    }
    const float fraction = position - (float)index;
    /* Smoothstep keeps the dune line soft instead of faceted. */
    const float weight = fraction * fraction * (3.0f - 2.0f * fraction);
    const float low = snapshot->substrate_height[index];
    const float high = snapshot->substrate_height[index + 1];
    return low + (high - low) * weight;
}
