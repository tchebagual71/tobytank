#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#define TOBYTANK_DISPLAY_WIDTH 368
#define TOBYTANK_DISPLAY_HEIGHT 448
#define TOBYTANK_DISPLAY_BITS_PER_PIXEL 16
#define TOBYTANK_DISPLAY_BAND_ROWS 16
#define TOBYTANK_DISPLAY_BUFFER_COUNT 2
/* Two DMA bands so the next band is copied while the current one is on the wire. */
#define TOBYTANK_DISPLAY_BAND_BUFFERS 2
#define TOBYTANK_DISPLAY_BUFFER_ALIGNMENT 64
#define TOBYTANK_DISPLAY_FRAME_PIXELS \
    (TOBYTANK_DISPLAY_WIDTH * TOBYTANK_DISPLAY_HEIGHT)
#define TOBYTANK_DISPLAY_FRAME_BYTES \
    (TOBYTANK_DISPLAY_FRAME_PIXELS * sizeof(uint16_t))
#define TOBYTANK_DISPLAY_BAND_BYTES \
    (TOBYTANK_DISPLAY_WIDTH * TOBYTANK_DISPLAY_BAND_ROWS * sizeof(uint16_t))

esp_err_t tobytank_display_init(void);

/*
 * Blocks until one of the PSRAM framebuffers is free, then returns it. With
 * submission running on its own task this is where the renderer waits for the
 * panel, so drawing the next frame overlaps the current transfer.
 */
uint16_t *tobytank_display_acquire_frame(void);

/*
 * Hands the frame to the display task and returns immediately. The buffer stays
 * owned by the display until its transfer completes. A failure from a previous
 * transfer is reported here, since the caller has no other way to see it.
 */
esp_err_t tobytank_display_submit_frame(const uint16_t *frame);
size_t tobytank_display_frame_bytes(void);
size_t tobytank_display_band_bytes(void);
