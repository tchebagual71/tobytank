#pragma once

#include <stdint.h>

/*
 * Cached fish sprite storage.
 *
 * Pixels use the same byte-swapped RGB565 format as the display canvas. Alpha is
 * stored separately so a cached fish can be composited over any aquarium frame.
 */
typedef struct {
    uint16_t *pixels;
    uint8_t *alpha;
    int width;
    int height;
    int origin_x;
    int origin_y;
} tobytank_fish_sprite_t;

