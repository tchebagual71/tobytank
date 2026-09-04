#include "diagnostics/boot_diagnostics.h"

#include "bsp/esp-bsp.h"
#include "esp_app_desc.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_psram.h"
#include "esp_system.h"
#include "fish/genome.h"
#include "hardware/display.h"
#include "memory/identity_store.h"
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

/* A fixed identity used only to prove genome generation works on the device.
   It is outside any reserved block, so no real visitor is ever spent on it. */
#define TOBYTANK_GENOME_SELF_CHECK_IDENTITY 0xFFFFFFFFFFFFFF01ULL

void tobytank_boot_diagnostics_log_identity(void)
{
    ESP_LOGI(TAG, "identity block: next=%llu remaining=%llu counter=%llu history_lost=%d",
             (unsigned long long)tobytank_identity_store_peek(),
             (unsigned long long)tobytank_identity_store_remaining(),
             (unsigned long long)tobytank_identity_store_counter(),
             tobytank_identity_store_history_lost());

    tobytank_genome_t genome;
    if (!tobytank_genome_generate(&genome, TOBYTANK_GENOME_SELF_CHECK_IDENTITY)) {
        ESP_LOGE(TAG, "genome self-check failed: no variant passed validation");
        return;
    }

    float width = 0.0f;
    float height = 0.0f;
    tobytank_genome_extent(&genome, &width, &height);
    ESP_LOGI(TAG, "genome self-check: fingerprint=0x%016llx variant=%u body=%.1fx%.1f "
                  "extent=%.0fx%.0f caudal=%u pattern=%u speed=%.1f cadence=%.2f",
             (unsigned long long)tobytank_genome_fingerprint(&genome),
             (unsigned)genome.variant,
             (double)genome.body_length, (double)genome.body_depth,
             (double)width, (double)height,
             (unsigned)genome.caudal_type, (unsigned)genome.pattern_type,
             (double)genome.preferred_speed, (double)genome.swim_cadence);
}

void tobytank_boot_diagnostics_log_health(void)
{
    ESP_LOGI(TAG, "memory free_heap=%lu free_internal=%u free_psram=%u min_free_heap=%lu",
             (unsigned long)esp_get_free_heap_size(),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
             (unsigned long)esp_get_minimum_free_heap_size());
}
