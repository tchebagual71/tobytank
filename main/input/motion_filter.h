#pragma once

#include <stdint.h>

typedef enum {
    TOBYTANK_TOUCH_EVENT_NONE = 0,
    TOBYTANK_TOUCH_EVENT_DOWN,
    TOBYTANK_TOUCH_EVENT_DRAG,
    TOBYTANK_TOUCH_EVENT_UP,
    TOBYTANK_TOUCH_EVENT_TAP,
} tobytank_touch_event_t;

typedef struct {
    int pressed;
    int x;
    int y;
    uint16_t strength;
} tobytank_touch_sample_t;

typedef struct {
    int width;
    int height;
    int pressed;
    float x;
    float y;
    float dx;
    float dy;
    float down_seconds;
    float idle_seconds;
    tobytank_touch_event_t event;
} tobytank_touch_filter_t;

typedef struct {
    int available;
    float accel_x;
    float accel_y;
    float accel_z;
} tobytank_imu_sample_t;

typedef struct {
    int available;
    float tilt_x;
    float tilt_y;
    float shake;
} tobytank_imu_filter_t;

void tobytank_touch_filter_init(tobytank_touch_filter_t *filter,
                                int width, int height);

void tobytank_touch_filter_update(tobytank_touch_filter_t *filter,
                                  const tobytank_touch_sample_t *sample,
                                  float dt_seconds);

void tobytank_imu_filter_init(tobytank_imu_filter_t *filter);

void tobytank_imu_filter_update(tobytank_imu_filter_t *filter,
                                const tobytank_imu_sample_t *sample,
                                float dt_seconds);

