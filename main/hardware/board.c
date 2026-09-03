#include "hardware/board.h"

#include "hardware/display.h"

#include "esp_log.h"

static const char *TAG = "tobytank_board";

esp_err_t tobytank_board_init(void)
{
    ESP_LOGI(TAG, "Initializing Waveshare board through managed BSP");

    const esp_err_t ret = tobytank_display_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Display initialization failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Display ready: %dx%d RGB%d at %d%% brightness",
             TOBYTANK_DISPLAY_WIDTH,
             TOBYTANK_DISPLAY_HEIGHT,
             TOBYTANK_DISPLAY_BITS_PER_PIXEL,
             TOBYTANK_DISPLAY_BRIGHTNESS_PERCENT);
    ESP_LOGI(TAG, "Board revision, touch controller selection, pins, power, and panel offsets are owned by the managed BSP");
    return ESP_OK;
}
