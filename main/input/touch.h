#pragma once

#include "esp_err.h"
#include "input/motion_filter.h"

esp_err_t tobytank_touch_init(int width, int height);
void tobytank_touch_poll(float dt_seconds);
const tobytank_touch_filter_t *tobytank_touch_state(void);
int tobytank_touch_available(void);

