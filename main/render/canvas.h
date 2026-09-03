#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint16_t *pixels;
    int width;
    int height;
} tobytank_canvas_t;

/*
 * Pixels are stored byte-swapped because the panel expects big-endian RGB565.
 * The pack/unpack and per-pixel blend helpers live here as inline functions:
 * the rasterizers touch hundreds of thousands of pixels per frame, and a
 * cross-translation-unit call per pixel measurably costs frame rate.
 */
static inline uint16_t tobytank_rgb565_pack(uint8_t red, uint8_t green, uint8_t blue)
{
    const uint16_t native = (uint16_t)(((red & 0xF8u) << 8) |
                                       ((green & 0xFCu) << 3) |
                                       (blue >> 3));
    return (uint16_t)((native >> 8) | (native << 8));
}

static inline uint8_t tobytank_mix_channel(uint8_t source, uint8_t destination, uint8_t alpha)
{
    const unsigned mixed = (unsigned)source * alpha + (unsigned)destination * (255u - alpha);
    return (uint8_t)((mixed + 127u) / 255u);
}

/* Blends into a pixel already known to be inside the canvas. */
static inline void tobytank_canvas_blend_at(uint16_t *pixel,
                                            uint8_t red, uint8_t green, uint8_t blue,
                                            uint8_t alpha)
{
    if (alpha == 0) {
        return;
    }
    if (alpha == 255) {
        *pixel = tobytank_rgb565_pack(red, green, blue);
        return;
    }

    const uint16_t stored = *pixel;
    const uint16_t native = (uint16_t)((stored >> 8) | (stored << 8));
    const uint8_t five_red = (uint8_t)((native >> 11) & 0x1Fu);
    const uint8_t six_green = (uint8_t)((native >> 5) & 0x3Fu);
    const uint8_t five_blue = (uint8_t)(native & 0x1Fu);

    *pixel = tobytank_rgb565_pack(
        tobytank_mix_channel(red, (uint8_t)((five_red << 3) | (five_red >> 2)), alpha),
        tobytank_mix_channel(green, (uint8_t)((six_green << 2) | (six_green >> 4)), alpha),
        tobytank_mix_channel(blue, (uint8_t)((five_blue << 3) | (five_blue >> 2)), alpha));
}

/* Bounds-checked blend for callers that cannot pre-clip. */
static inline void tobytank_canvas_blend(const tobytank_canvas_t *canvas, int x, int y,
                                         uint8_t red, uint8_t green, uint8_t blue,
                                         uint8_t alpha)
{
    if (canvas == NULL || canvas->pixels == NULL ||
        x < 0 || y < 0 || x >= canvas->width || y >= canvas->height) {
        return;
    }
    tobytank_canvas_blend_at(&canvas->pixels[y * canvas->width + x], red, green, blue, alpha);
}

uint16_t tobytank_rgb565(uint8_t red, uint8_t green, uint8_t blue);
void tobytank_canvas_clear(const tobytank_canvas_t *canvas, uint16_t color);
void tobytank_canvas_pixel(const tobytank_canvas_t *canvas, int x, int y, uint16_t color);
void tobytank_canvas_fill_rect(const tobytank_canvas_t *canvas,
                               int x, int y, int width, int height, uint16_t color);
void tobytank_canvas_frame_rect(const tobytank_canvas_t *canvas,
                                int x, int y, int width, int height,
                                int thickness, uint16_t color);
void tobytank_canvas_fill_ellipse(const tobytank_canvas_t *canvas,
                                  int center_x, int center_y,
                                  int radius_x, int radius_y, uint16_t color);
void tobytank_canvas_line(const tobytank_canvas_t *canvas,
                          int x0, int y0, int x1, int y1, uint16_t color);
void tobytank_canvas_blend_pixel(const tobytank_canvas_t *canvas, int x, int y,
                                 uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha);
void tobytank_canvas_blend_rect(const tobytank_canvas_t *canvas,
                                int x, int y, int width, int height,
                                uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha);
