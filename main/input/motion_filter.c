#include "input/motion_filter.h"

#include <math.h>
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

static int clampi(int value, int low, int high)
{
    if (value < low) {
        return low;
    }
    if (value > high) {
        return high;
    }
    return value;
}

void tobytank_touch_filter_init(tobytank_touch_filter_t *filter,
                                int width, int height)
{
    if (filter == NULL) {
        return;
    }
    memset(filter, 0, sizeof(*filter));
    filter->width = width;
    filter->height = height;
    filter->idle_seconds = 999.0f;
}

void tobytank_touch_filter_update(tobytank_touch_filter_t *filter,
                                  const tobytank_touch_sample_t *sample,
                                  float dt_seconds)
{
    if (filter == NULL || sample == NULL || dt_seconds <= 0.0f ||
        filter->width <= 0 || filter->height <= 0) {
        return;
    }

    filter->event = TOBYTANK_TOUCH_EVENT_NONE;
    filter->dx = 0.0f;
    filter->dy = 0.0f;

    const int pressed = sample->pressed ? 1 : 0;
    const float next_x = (float)clampi(sample->x, 0, filter->width - 1);
    const float next_y = (float)clampi(sample->y, 0, filter->height - 1);

    if (pressed) {
        if (!filter->pressed) {
            filter->event = TOBYTANK_TOUCH_EVENT_DOWN;
            filter->x = next_x;
            filter->y = next_y;
            filter->down_seconds = 0.0f;
        } else {
            filter->dx = next_x - filter->x;
            filter->dy = next_y - filter->y;
            filter->event = (filter->dx * filter->dx + filter->dy * filter->dy > 4.0f)
                                ? TOBYTANK_TOUCH_EVENT_DRAG
                                : TOBYTANK_TOUCH_EVENT_NONE;
            filter->x = next_x;
            filter->y = next_y;
            filter->down_seconds += dt_seconds;
        }
        filter->idle_seconds = 0.0f;
    } else {
        if (filter->pressed) {
            filter->event = filter->down_seconds <= 0.32f ?
                            TOBYTANK_TOUCH_EVENT_TAP : TOBYTANK_TOUCH_EVENT_UP;
        }
        filter->idle_seconds += dt_seconds;
        filter->down_seconds = 0.0f;
    }

    filter->pressed = pressed;
}

void tobytank_imu_filter_init(tobytank_imu_filter_t *filter)
{
    if (filter == NULL) {
        return;
    }
    memset(filter, 0, sizeof(*filter));
}

void tobytank_imu_filter_update(tobytank_imu_filter_t *filter,
                                const tobytank_imu_sample_t *sample,
                                float dt_seconds)
{
    if (filter == NULL || sample == NULL || dt_seconds <= 0.0f) {
        return;
    }
    if (!sample->available) {
        filter->available = 0;
        filter->tilt_x *= 0.92f;
        filter->tilt_y *= 0.92f;
        filter->shake *= 0.86f;
        return;
    }

    const float next_x = clampf(sample->accel_x, -1.0f, 1.0f);
    const float next_y = clampf(sample->accel_y, -1.0f, 1.0f);
    const float alpha = clampf(dt_seconds * 8.0f, 0.0f, 1.0f);
    const float old_x = filter->tilt_x;
    const float old_y = filter->tilt_y;
    filter->available = 1;
    filter->tilt_x += (next_x - filter->tilt_x) * alpha;
    filter->tilt_y += (next_y - filter->tilt_y) * alpha;
    const float impulse = fabsf(filter->tilt_x - old_x) + fabsf(filter->tilt_y - old_y);
    filter->shake = clampf(filter->shake * (1.0f - alpha * 0.55f) + impulse * 0.50f,
                           0.0f, 1.0f);
}

