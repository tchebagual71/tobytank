#include "input/touch.h"

#include "esp_lcd_touch.h"
#include "esp_log.h"
#include "hardware/display.h"

static const char *TAG = "tobytank_touch";

static esp_lcd_touch_handle_t s_touch;
static tobytank_touch_filter_t s_filter;
static int s_available;

esp_err_t tobytank_touch_init(int width, int height)
{
    tobytank_touch_filter_init(&s_filter, width, height);
    s_touch = tobytank_display_touch_handle();
    if (s_touch == NULL) {
        s_available = 0;
        ESP_LOGW(TAG, "Touch unavailable; interactions disabled");
        return ESP_OK;
    }
    s_available = 1;
    ESP_LOGI(TAG, "Touch input ready: %dx%d", width, height);
    return ESP_OK;
}

void tobytank_touch_poll(float dt_seconds)
{
    tobytank_touch_sample_t sample = {0};
    if (s_available && s_touch != NULL) {
        esp_lcd_touch_point_data_t point[1] = {0};
        uint8_t point_count = 0;
        if (esp_lcd_touch_read_data(s_touch) == ESP_OK &&
            esp_lcd_touch_get_data(s_touch, point, &point_count, 1) == ESP_OK &&
            point_count > 0) {
            sample.pressed = 1;
            sample.x = point[0].x;
            sample.y = point[0].y;
            sample.strength = point[0].strength;
        }
    }
    tobytank_touch_filter_update(&s_filter, &sample, dt_seconds);
}

const tobytank_touch_filter_t *tobytank_touch_state(void)
{
    return &s_filter;
}

int tobytank_touch_available(void)
{
    return s_available;
}
