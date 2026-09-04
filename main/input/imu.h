#pragma once

#include "esp_err.h"
#include "input/motion_filter.h"

esp_err_t tobytank_imu_init(void);
void tobytank_imu_poll(float dt_seconds);
const tobytank_imu_filter_t *tobytank_imu_state(void);
int tobytank_imu_available(void);

