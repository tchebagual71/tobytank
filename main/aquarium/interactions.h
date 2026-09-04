#pragma once

#include "input/motion_filter.h"

typedef struct {
    int width;
    int height;
    int ripple_active;
    float ripple_x;
    float ripple_y;
    float ripple_age;
    float ripple_radius;
    float ripple_strength;
    float current_x;
    float current_y;
    float parallax_x;
    float parallax_y;
    float attention_x;
    float attention_y;
    float attention_strength;
} tobytank_interactions_t;

void tobytank_interactions_init(tobytank_interactions_t *interactions,
                                int width, int height);

void tobytank_interactions_update(tobytank_interactions_t *interactions,
                                  const tobytank_touch_filter_t *touch,
                                  const tobytank_imu_filter_t *imu,
                                  float dt_seconds);

