#include "render/canvas.h"

#include <stdlib.h>

uint16_t tobytank_rgb565(uint8_t red, uint8_t green, uint8_t blue)
{
    return tobytank_rgb565_pack(red, green, blue);
}

void tobytank_canvas_clear(const tobytank_canvas_t *canvas, uint16_t color)
{
    if (canvas == NULL || canvas->pixels == NULL || canvas->width <= 0 || canvas->height <= 0) {
        return;
    }

    for (int y = 0; y < canvas->height; ++y) {
        for (int x = 0; x < canvas->width; ++x) {
            canvas->pixels[y * canvas->width + x] = color;
        }
    }
}

void tobytank_canvas_pixel(const tobytank_canvas_t *canvas, int x, int y, uint16_t color)
{
    if (canvas == NULL || canvas->pixels == NULL ||
        x < 0 || y < 0 || x >= canvas->width || y >= canvas->height) {
        return;
    }

    canvas->pixels[y * canvas->width + x] = color;
}

void tobytank_canvas_fill_rect(const tobytank_canvas_t *canvas,
                               int x, int y, int width, int height, uint16_t color)
{
    if (canvas == NULL || canvas->pixels == NULL ||
        canvas->width <= 0 || canvas->height <= 0 || width <= 0 || height <= 0) {
        return;
    }

    int x0 = x;
    int y0 = y;
    int x1 = x + width;
    int y1 = y + height;

    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > canvas->width) x1 = canvas->width;
    if (y1 > canvas->height) y1 = canvas->height;
    if (x0 >= x1 || y0 >= y1) {
        return;
    }

    for (int row = y0; row < y1; ++row) {
        for (int col = x0; col < x1; ++col) {
            canvas->pixels[row * canvas->width + col] = color;
        }
    }
}

void tobytank_canvas_frame_rect(const tobytank_canvas_t *canvas,
                                int x, int y, int width, int height,
                                int thickness, uint16_t color)
{
    if (canvas == NULL || canvas->pixels == NULL || width <= 0 || height <= 0 || thickness <= 0) {
        return;
    }

    /* A frame thicker than half the rectangle collapses into a filled rectangle. */
    if (thickness * 2 >= width || thickness * 2 >= height) {
        tobytank_canvas_fill_rect(canvas, x, y, width, height, color);
        return;
    }

    tobytank_canvas_fill_rect(canvas, x, y, width, thickness, color);
    tobytank_canvas_fill_rect(canvas, x, y + height - thickness, width, thickness, color);
    tobytank_canvas_fill_rect(canvas, x, y + thickness, thickness, height - 2 * thickness, color);
    tobytank_canvas_fill_rect(canvas, x + width - thickness, y + thickness, thickness,
                              height - 2 * thickness, color);
}

void tobytank_canvas_fill_ellipse(const tobytank_canvas_t *canvas,
                                  int center_x, int center_y,
                                  int radius_x, int radius_y, uint16_t color)
{
    if (canvas == NULL || canvas->pixels == NULL || radius_x <= 0 || radius_y <= 0) {
        return;
    }

    for (int dy = -radius_y; dy <= radius_y; ++dy) {
        const int y = center_y + dy;
        if (y < 0 || y >= canvas->height) {
            continue;
        }
        /* Solve the ellipse equation for the half width of this row. */
        const long numerator = (long)(radius_y * radius_y - dy * dy);
        if (numerator < 0) {
            continue;
        }
        const long span_squared = numerator * (long)(radius_x * radius_x) /
                                  (long)(radius_y * radius_y);
        int span = 0;
        while ((long)(span + 1) * (long)(span + 1) <= span_squared) {
            ++span;
        }
        tobytank_canvas_fill_rect(canvas, center_x - span, y, 2 * span + 1, 1, color);
    }
}

void tobytank_canvas_blend_pixel(const tobytank_canvas_t *canvas, int x, int y,
                                 uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha)
{
    tobytank_canvas_blend(canvas, x, y, red, green, blue, alpha);
}

void tobytank_canvas_blend_rect(const tobytank_canvas_t *canvas,
                                int x, int y, int width, int height,
                                uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha)
{
    if (canvas == NULL || canvas->pixels == NULL || width <= 0 || height <= 0 || alpha == 0) {
        return;
    }

    if (alpha == 255) {
        tobytank_canvas_fill_rect(canvas, x, y, width, height,
                                  tobytank_rgb565(red, green, blue));
        return;
    }

    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + width;
    int y1 = y + height;
    if (x1 > canvas->width) x1 = canvas->width;
    if (y1 > canvas->height) y1 = canvas->height;

    for (int row = y0; row < y1; ++row) {
        uint16_t *line = &canvas->pixels[row * canvas->width];
        for (int col = x0; col < x1; ++col) {
            tobytank_canvas_blend_at(&line[col], red, green, blue, alpha);
        }
    }
}

void tobytank_canvas_line(const tobytank_canvas_t *canvas,
                          int x0, int y0, int x1, int y1, uint16_t color)
{
    if (canvas == NULL || canvas->pixels == NULL || canvas->width <= 0 || canvas->height <= 0) {
        return;
    }

    const int dx = abs(x1 - x0);
    const int sx = x0 < x1 ? 1 : -1;
    const int dy = -abs(y1 - y0);
    const int sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;

    for (;;) {
        tobytank_canvas_pixel(canvas, x0, y0, color);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        const int twice_error = 2 * error;
        if (twice_error >= dy) {
            error += dy;
            x0 += sx;
        }
        if (twice_error <= dx) {
            error += dx;
            y0 += sy;
        }
    }
}
