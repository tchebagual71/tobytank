#include "hardware/display.h"

#include <stdbool.h>
#include <string.h>

#include "bsp/esp-bsp.h"
#include "bsp/touch.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "hardware/board.h"

static const char *TAG = "tobytank_display";

static esp_lcd_panel_handle_t s_panel;
static esp_lcd_panel_io_handle_t s_io;
static SemaphoreHandle_t s_free_buffers;
static SemaphoreHandle_t s_transfer_done;
static uint16_t *s_frames[TOBYTANK_DISPLAY_BUFFER_COUNT];
static uint16_t *s_transfer_buffers[TOBYTANK_DISPLAY_BAND_BUFFERS];
static QueueHandle_t s_pending_frames;
static TaskHandle_t s_display_task;
static volatile esp_err_t s_last_transfer_error;
static unsigned s_next_frame;
static bool s_initialized;

static void display_task(void *argument);

static bool on_color_trans_done(esp_lcd_panel_io_handle_t io,
                                esp_lcd_panel_io_event_data_t *event,
                                void *user_ctx)
{
    (void)io;
    (void)event;
    (void)user_ctx;

    BaseType_t higher_priority_woken = pdFALSE;
    xSemaphoreGiveFromISR(s_transfer_done, &higher_priority_woken);
    return higher_priority_woken == pdTRUE;
}

esp_err_t tobytank_display_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    /*
     * The managed BSP owns board revision probing and touch-controller
     * selection. Calling bsp_touch_new before bsp_display_new follows the
     * proven reference pattern that lets the BSP choose the right display gap
     * and reset sequence.
     */
    esp_lcd_touch_handle_t touch = NULL;
    esp_err_t ret = bsp_touch_new(NULL, &touch);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "BSP touch/revision probe failed: %s", esp_err_to_name(ret));
        return ret;
    }

    const bsp_display_config_t config = {
        .max_transfer_sz = TOBYTANK_DISPLAY_FRAME_BYTES,
    };
    ret = bsp_display_new(&config, &s_panel, &s_io);
    if (ret != ESP_OK || s_panel == NULL || s_io == NULL) {
        ESP_LOGE(TAG, "BSP display initialization failed: %s",
                 esp_err_to_name(ret == ESP_OK ? ESP_FAIL : ret));
        return ret == ESP_OK ? ESP_FAIL : ret;
    }

    ret = bsp_display_brightness_set(TOBYTANK_DISPLAY_BRIGHTNESS_PERCENT);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Brightness setup failed: %s", esp_err_to_name(ret));
        return ret;
    }

    s_free_buffers = xSemaphoreCreateCounting(TOBYTANK_DISPLAY_BUFFER_COUNT,
                                              TOBYTANK_DISPLAY_BUFFER_COUNT);
    if (s_free_buffers == NULL) {
        ESP_LOGE(TAG, "Could not create framebuffer semaphore");
        return ESP_ERR_NO_MEM;
    }

    s_transfer_done = xSemaphoreCreateBinary();
    if (s_transfer_done == NULL) {
        ESP_LOGE(TAG, "Could not create transfer-completion semaphore");
        return ESP_ERR_NO_MEM;
    }

    for (int i = 0; i < TOBYTANK_DISPLAY_BUFFER_COUNT; ++i) {
        s_frames[i] = heap_caps_aligned_alloc(TOBYTANK_DISPLAY_BUFFER_ALIGNMENT,
                                              TOBYTANK_DISPLAY_FRAME_BYTES,
                                              MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (s_frames[i] == NULL) {
            ESP_LOGE(TAG, "Could not allocate PSRAM framebuffer %d (%u bytes)",
                     i, (unsigned)TOBYTANK_DISPLAY_FRAME_BYTES);
            return ESP_ERR_NO_MEM;
        }
    }

    for (int i = 0; i < TOBYTANK_DISPLAY_BAND_BUFFERS; ++i) {
        s_transfer_buffers[i] = heap_caps_aligned_alloc(TOBYTANK_DISPLAY_BUFFER_ALIGNMENT,
                                                        TOBYTANK_DISPLAY_BAND_BYTES,
                                                        MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
        if (s_transfer_buffers[i] == NULL) {
            ESP_LOGE(TAG, "Could not allocate internal DMA transfer band %d (%u bytes)",
                     i, (unsigned)TOBYTANK_DISPLAY_BAND_BYTES);
            return ESP_ERR_NO_MEM;
        }
    }

    const esp_lcd_panel_io_callbacks_t callbacks = {
        .on_color_trans_done = on_color_trans_done,
    };
    ret = esp_lcd_panel_io_register_event_callbacks(s_io, &callbacks, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Could not register LCD transfer callback: %s", esp_err_to_name(ret));
        return ret;
    }

    s_pending_frames = xQueueCreate(TOBYTANK_DISPLAY_BUFFER_COUNT, sizeof(uint16_t *));
    if (s_pending_frames == NULL) {
        ESP_LOGE(TAG, "Could not create the display submission queue");
        return ESP_ERR_NO_MEM;
    }

    /* Pinned to the second core so panel transfer runs alongside drawing. */
    if (xTaskCreatePinnedToCore(display_task, "tobytank_lcd", 4096, NULL, 5,
                                &s_display_task, 1) != pdPASS) {
        ESP_LOGE(TAG, "Could not start the display task");
        return ESP_ERR_NO_MEM;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "RGB565 buffers: %d PSRAM frames x %u bytes, %d DMA bands x %u bytes",
             TOBYTANK_DISPLAY_BUFFER_COUNT,
             (unsigned)TOBYTANK_DISPLAY_FRAME_BYTES,
             TOBYTANK_DISPLAY_BAND_BUFFERS,
             (unsigned)TOBYTANK_DISPLAY_BAND_BYTES);
    return ESP_OK;
}

uint16_t *tobytank_display_acquire_frame(void)
{
    if (!s_initialized || xSemaphoreTake(s_free_buffers, portMAX_DELAY) != pdTRUE) {
        return NULL;
    }
    return s_frames[s_next_frame++ % TOBYTANK_DISPLAY_BUFFER_COUNT];
}

static int band_rows_at(int y)
{
    return (y + TOBYTANK_DISPLAY_BAND_ROWS <= TOBYTANK_DISPLAY_HEIGHT)
               ? TOBYTANK_DISPLAY_BAND_ROWS
               : TOBYTANK_DISPLAY_HEIGHT - y;
}

static size_t band_row_bytes(int y)
{
    return (size_t)TOBYTANK_DISPLAY_WIDTH * (size_t)band_rows_at(y) * sizeof(uint16_t);
}

static esp_err_t transfer_frame(const uint16_t *frame)
{
    esp_err_t ret = ESP_OK;
    int index = 0;

    /* Prime the first band, then copy each following band while the previous
       one is still on the wire, so the PSRAM copy overlaps the DMA transfer. */
    memcpy(s_transfer_buffers[index], frame, band_row_bytes(0));

    for (int y = 0; y < TOBYTANK_DISPLAY_HEIGHT; y += TOBYTANK_DISPLAY_BAND_ROWS) {
        const int rows = band_rows_at(y);
        ret = esp_lcd_panel_draw_bitmap(s_panel, 0, y, TOBYTANK_DISPLAY_WIDTH, y + rows,
                                        s_transfer_buffers[index]);
        if (ret != ESP_OK) {
            break;
        }

        const int next_y = y + TOBYTANK_DISPLAY_BAND_ROWS;
        if (next_y < TOBYTANK_DISPLAY_HEIGHT) {
            memcpy(s_transfer_buffers[index ^ 1],
                   frame + (size_t)next_y * TOBYTANK_DISPLAY_WIDTH,
                   band_row_bytes(next_y));
        }

        if (xSemaphoreTake(s_transfer_done, portMAX_DELAY) != pdTRUE) {
            ret = ESP_FAIL;
            break;
        }
        index ^= 1;
    }

    return ret;
}

static void display_task(void *argument)
{
    (void)argument;
    for (;;) {
        const uint16_t *frame = NULL;
        if (xQueueReceive(s_pending_frames, &frame, portMAX_DELAY) != pdTRUE || frame == NULL) {
            continue;
        }
        const esp_err_t ret = transfer_frame(frame);
        if (ret != ESP_OK) {
            s_last_transfer_error = ret;
            ESP_LOGE(TAG, "Panel transfer failed: %s", esp_err_to_name(ret));
        }
        xSemaphoreGive(s_free_buffers);
    }
}

esp_err_t tobytank_display_submit_frame(const uint16_t *frame)
{
    if (!s_initialized || frame == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    const esp_err_t pending = s_last_transfer_error;
    s_last_transfer_error = ESP_OK;

    if (xQueueSend(s_pending_frames, &frame, portMAX_DELAY) != pdTRUE) {
        xSemaphoreGive(s_free_buffers);
        return ESP_FAIL;
    }
    return pending;
}

size_t tobytank_display_frame_bytes(void)
{
    return TOBYTANK_DISPLAY_FRAME_BYTES;
}

size_t tobytank_display_band_bytes(void)
{
    return TOBYTANK_DISPLAY_BAND_BYTES;
}
