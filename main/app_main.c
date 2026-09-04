#include "diagnostics/boot_diagnostics.h"
#include "hardware/board.h"
#include "memory/identity_store.h"
#include "render/renderer.h"

#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "tobytank";

void app_main(void)
{
    tobytank_boot_diagnostics_print();

    esp_err_t ret = tobytank_board_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Halted: board/display init failed: %s",
                 esp_err_to_name(ret));
        while (true) {
            vTaskDelay(pdMS_TO_TICKS(5000));
        }
    }

    /*
     * Identity persistence must be ready before the renderer's lifecycle can
     * admit a visitor. A failure here is not fatal to the aquarium: the tank
     * continues to render and the lifecycle simply stays empty.
     */
    ret = tobytank_identity_store_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Identity persistence unavailable: %s. No visitor can be "
                      "admitted until this is resolved.",
                 esp_err_to_name(ret));
    } else {
        tobytank_boot_diagnostics_log_identity();
    }

    ret = tobytank_renderer_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Halted: renderer init failed: %s",
                 esp_err_to_name(ret));
        while (true) {
            vTaskDelay(pdMS_TO_TICKS(5000));
        }
    }

    ESP_LOGI(TAG, "Aquarium frame loop started");
    int health_tick = 0;
    while (true) {
        ret = tobytank_renderer_frame();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Frame failed: %s", esp_err_to_name(ret));
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        if (++health_tick >= 500) {
            health_tick = 0;
            tobytank_boot_diagnostics_log_health();
        }
        vTaskDelay(1);
    }
}
