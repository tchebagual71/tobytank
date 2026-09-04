#include "aquarium/interactions.h"

#include <stddef.h>
#include <string.h>

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

static float approach_zero(float value, float delta)
{
    if (value > delta) {
        return value - delta;
    }
    if (value < -delta) {
        return value + delta;
    }
    return 0.0f;
}

void tobytank_interactions_init(tobytank_interactions_t *interactions,
                                int width, int height)
{
    if (interactions == NULL) {
        return;
    }
    memset(interactions, 0, sizeof(*interactions));
    interactions->width = width;
    interactions->height = height;
}

void tobytank_interactions_update(tobytank_interactions_t *interactions,
                                  const tobytank_touch_filter_t *touch,
                                  const tobytank_imu_filter_t *imu,
                                  float dt_seconds)
{
    if (interactions == NULL || dt_seconds <= 0.0f ||
        interactions->width <= 0 || interactions->height <= 0) {
        return;
    }

    const float decay = clampf(dt_seconds * 1.8f, 0.0f, 1.0f);
    interactions->current_x = approach_zero(interactions->current_x, 18.0f * dt_seconds);
    interactions->current_y = approach_zero(interactions->current_y, 18.0f * dt_seconds);
    interactions->attention_strength *= (1.0f - decay);

    if (interactions->ripple_active) {
        interactions->ripple_age += dt_seconds;
        interactions->ripple_radius += (42.0f + 16.0f * interactions->ripple_strength) *
                                       dt_seconds;
        interactions->ripple_strength = approach_zero(interactions->ripple_strength,
                                                      0.95f * dt_seconds);
        if (interactions->ripple_age > 1.6f || interactions->ripple_strength <= 0.02f) {
            interactions->ripple_active = 0;
            interactions->ripple_strength = 0.0f;
        }
    }

    if (touch != NULL && touch->pressed) {
        interactions->attention_x = touch->x;
        interactions->attention_y = touch->y;
        interactions->attention_strength = clampf(interactions->attention_strength +
                                                  1.4f * dt_seconds,
                                                  0.0f, 1.0f);
        interactions->current_x = clampf(interactions->current_x + touch->dx * 1.4f,
                                         -28.0f, 28.0f);
        interactions->current_y = clampf(interactions->current_y + touch->dy * 1.0f,
                                         -18.0f, 18.0f);
    }
    if (touch != NULL && (touch->event == TOBYTANK_TOUCH_EVENT_DOWN ||
                          touch->event == TOBYTANK_TOUCH_EVENT_TAP ||
                          touch->event == TOBYTANK_TOUCH_EVENT_DRAG)) {
        interactions->ripple_active = 1;
        interactions->ripple_x = touch->x;
        interactions->ripple_y = touch->y;
        interactions->ripple_age = 0.0f;
        interactions->ripple_radius = touch->event == TOBYTANK_TOUCH_EVENT_DRAG ? 12.0f : 6.0f;
        interactions->ripple_strength = touch->event == TOBYTANK_TOUCH_EVENT_DRAG ? 0.58f : 0.92f;
    }

    float target_px = 0.0f;
    float target_py = 0.0f;
    if (imu != NULL && imu->available) {
        target_px = clampf(imu->tilt_x * 9.0f, -9.0f, 9.0f);
        target_py = clampf(imu->tilt_y * 7.0f, -7.0f, 7.0f);
        interactions->current_x = clampf(interactions->current_x + imu->tilt_x * 2.8f *
                                         dt_seconds, -28.0f, 28.0f);
        interactions->current_y = clampf(interactions->current_y + imu->tilt_y * 1.8f *
                                         dt_seconds, -18.0f, 18.0f);
    }

    const float parallax_alpha = clampf(dt_seconds * 5.0f, 0.0f, 1.0f);
    interactions->parallax_x += (target_px - interactions->parallax_x) * parallax_alpha;
    interactions->parallax_y += (target_py - interactions->parallax_y) * parallax_alpha;
}
