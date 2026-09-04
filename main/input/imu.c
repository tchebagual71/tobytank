#include "input/imu.h"

#include "bsp/esp-bsp.h"
#include "esp_log.h"

static const char *TAG = "tobytank_imu";

static tobytank_imu_filter_t s_filter;
static int s_available;

esp_err_t tobytank_imu_init(void)
{
    tobytank_imu_filter_init(&s_filter);
#if BSP_CAPS_IMU
    s_available = 0;
    ESP_LOGW(TAG, "BSP reports IMU capability, but TobyTank has no IMU driver yet");
#else
    s_available = 0;
    ESP_LOGW(TAG, "IMU unavailable in managed BSP; tilt interactions disabled");
#endif
    return ESP_OK;
}

void tobytank_imu_poll(float dt_seconds)
{
    const tobytank_imu_sample_t sample = {0};
    tobytank_imu_filter_update(&s_filter, &sample, dt_seconds);
}

const tobytank_imu_filter_t *tobytank_imu_state(void)
{
    return &s_filter;
}

int tobytank_imu_available(void)
{
    return s_available;
}

