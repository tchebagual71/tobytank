#include "diagnostics/boot_diagnostics.h"

#include "bsp/esp-bsp.h"
#include "esp_app_desc.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_psram.h"
#include "esp_system.h"
#include "hardware/display.h"
#include "spi_flash_mmap.h"

static const char *TAG = "tobytank_diag";

void tobytank_boot_diagnostics_print(void)
{
    const esp_app_desc_t *app = esp_app_get_description();
    esp_chip_info_t chip = {0};
    esp_chip_info(&chip);

    uint32_t flash_size = 0;
    const esp_err_t flash_ret = esp_flash_get_size(NULL, &flash_size);
    const bool psram_initialized = esp_psram_is_initialized();
    const size_t psram_size = psram_initialized ? esp_psram_get_size() : 0;

    ESP_LOGI(TAG, "app=%s version=%s idf=%s",
             app != NULL ? app->project_name : "unknown",
             app != NULL ? app->version : "unknown",
             app != NULL ? app->idf_ver : "unknown");
    ESP_LOGI(TAG, "chip model=%d revision=%d cores=%d features=0x%lx",
             chip.model, chip.revision, chip.cores, (unsigned long)chip.features);
    ESP_LOGI(TAG, "flash=%s %lu bytes",
             flash_ret == ESP_OK ? "detected" : esp_err_to_name(flash_ret),
             (unsigned long)flash_size);
    ESP_LOGI(TAG, "psram=%s %u bytes", psram_initialized ? "initialized" : "unavailable",
             (unsigned)psram_size);
    ESP_LOGI(TAG, "display geometry BSP=%dx%d RGB%d app=%dx%d RGB%d",
             BSP_LCD_H_RES, BSP_LCD_V_RES, BSP_LCD_BITS_PER_PIXEL,
             TOBYTANK_DISPLAY_WIDTH, TOBYTANK_DISPLAY_HEIGHT,
             TOBYTANK_DISPLAY_BITS_PER_PIXEL);
    ESP_LOGI(TAG, "planned buffers: frame=%u bytes, psram_frame_count=%d, dma_band=%u bytes",
             (unsigned)TOBYTANK_DISPLAY_FRAME_BYTES,
             TOBYTANK_DISPLAY_BUFFER_COUNT,
             (unsigned)TOBYTANK_DISPLAY_BAND_BYTES);
}

void tobytank_boot_diagnostics_log_health(void)
{
    ESP_LOGI(TAG, "memory free_heap=%lu free_internal=%u free_psram=%u min_free_heap=%lu",
             (unsigned long)esp_get_free_heap_size(),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
             (unsigned long)esp_get_minimum_free_heap_size());
}
