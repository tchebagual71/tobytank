#include "render/composite.h"

static void composite_sprite(const tobytank_canvas_t *canvas,
                             const tobytank_fish_sprite_t *sprite,
                             int x, int y,
                             int flip_x)
{
    if (canvas == NULL || canvas->pixels == NULL || sprite == NULL ||
        sprite->pixels == NULL || sprite->alpha == NULL ||
        canvas->width <= 0 || canvas->height <= 0 ||
        sprite->width <= 0 || sprite->height <= 0) {
        return;
    }

    int src_x0 = 0;
    int src_y0 = 0;
    int dst_x0 = x - sprite->origin_x;
    int dst_y0 = y - sprite->origin_y;
    int copy_w = sprite->width;
    int copy_h = sprite->height;

    if (dst_x0 < 0) {
        src_x0 = -dst_x0;
        copy_w += dst_x0;
        dst_x0 = 0;
    }
    if (dst_y0 < 0) {
        src_y0 = -dst_y0;
        copy_h += dst_y0;
        dst_y0 = 0;
    }
    if (dst_x0 + copy_w > canvas->width) {
        copy_w = canvas->width - dst_x0;
    }
    if (dst_y0 + copy_h > canvas->height) {
        copy_h = canvas->height - dst_y0;
    }
    if (copy_w <= 0 || copy_h <= 0) {
        return;
    }

    for (int row = 0; row < copy_h; ++row) {
        const int src_y = src_y0 + row;
        uint16_t *dst = &canvas->pixels[(dst_y0 + row) * canvas->width + dst_x0];
        for (int col = 0; col < copy_w; ++col) {
            const int src_x = flip_x ? sprite->width - 1 - (src_x0 + col) :
                                      src_x0 + col;
            const int src_index = src_y * sprite->width + src_x;
            const uint8_t alpha = sprite->alpha[src_index];
            if (alpha == 0) {
                continue;
            }
            const uint16_t stored = sprite->pixels[src_index];
            const uint16_t native = (uint16_t)((stored >> 8) | (stored << 8));
            const uint8_t r5 = (uint8_t)((native >> 11) & 0x1Fu);
            const uint8_t g6 = (uint8_t)((native >> 5) & 0x3Fu);
            const uint8_t b5 = (uint8_t)(native & 0x1Fu);
            tobytank_canvas_blend_at(&dst[col],
                                     (uint8_t)((r5 << 3) | (r5 >> 2)),
                                     (uint8_t)((g6 << 2) | (g6 >> 4)),
                                     (uint8_t)((b5 << 3) | (b5 >> 2)),
                                     alpha);
        }
    }
}

void tobytank_composite_sprite(const tobytank_canvas_t *canvas,
                               const tobytank_fish_sprite_t *sprite,
                               int x, int y)
{
    composite_sprite(canvas, sprite, x, y, 0);
}

void tobytank_composite_sprite_facing(const tobytank_canvas_t *canvas,
                                      const tobytank_fish_sprite_t *sprite,
                                      int x, int y,
                                      float facing)
{
    composite_sprite(canvas, sprite, x, y, facing > 0.0f);
}
