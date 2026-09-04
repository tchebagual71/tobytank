#include "render/fish_rasterizer.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#include "fish/genome_validate.h"
#include "render/canvas.h"
#include "render/dither.h"

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} rgb_t;

static float clampf(float value, float low, float high)
{
    if (value < low) {
        return low;
    }
    if (value > high) {
        return high;
    }
    return value;
}

static uint8_t clamp_u8(int value)
{
    if (value < 0) {
        return 0;
    }
    if (value > 255) {
        return 255;
    }
    return (uint8_t)value;
}

static float wrap_turn(float value)
{
    while (value < 0.0f) {
        value += 1.0f;
    }
    while (value >= 1.0f) {
        value -= 1.0f;
    }
    return value;
}

static rgb_t hsv(float hue, float saturation, float value)
{
    hue = wrap_turn(hue) * 6.0f;
    saturation = clampf(saturation, 0.0f, 1.0f);
    value = clampf(value, 0.0f, 1.0f);

    const int sector = (int)hue;
    const float fraction = hue - (float)sector;
    const float p = value * (1.0f - saturation);
    const float q = value * (1.0f - saturation * fraction);
    const float t = value * (1.0f - saturation * (1.0f - fraction));
    float r = value;
    float g = t;
    float b = p;

    switch (sector % 6) {
    case 0: r = value; g = t; b = p; break;
    case 1: r = q; g = value; b = p; break;
    case 2: r = p; g = value; b = t; break;
    case 3: r = p; g = q; b = value; break;
    case 4: r = t; g = p; b = value; break;
    default: r = value; g = p; b = q; break;
    }

    const rgb_t out = {
        .r = clamp_u8((int)(r * 255.0f + 0.5f)),
        .g = clamp_u8((int)(g * 255.0f + 0.5f)),
        .b = clamp_u8((int)(b * 255.0f + 0.5f)),
    };
    return out;
}

static void clear_sprite(tobytank_fish_sprite_t *sprite)
{
    const size_t count = (size_t)sprite->width * (size_t)sprite->height;
    memset(sprite->pixels, 0, count * sizeof(uint16_t));
    memset(sprite->alpha, 0, count * sizeof(uint8_t));
}

static void sprite_blend(tobytank_fish_sprite_t *sprite, int x, int y,
                         rgb_t color, uint8_t alpha)
{
    if (alpha == 0 || x < 0 || y < 0 || x >= sprite->width || y >= sprite->height) {
        return;
    }

    const size_t index = (size_t)y * (size_t)sprite->width + (size_t)x;
    const uint8_t dst_alpha = sprite->alpha[index];
    if (dst_alpha == 0 || alpha == 255) {
        sprite->pixels[index] = tobytank_rgb565_pack(color.r, color.g, color.b);
        sprite->alpha[index] = alpha;
        return;
    }

    const uint16_t stored = sprite->pixels[index];
    const uint16_t native = (uint16_t)((stored >> 8) | (stored << 8));
    const uint8_t r5 = (uint8_t)((native >> 11) & 0x1Fu);
    const uint8_t g6 = (uint8_t)((native >> 5) & 0x3Fu);
    const uint8_t b5 = (uint8_t)(native & 0x1Fu);
    const uint8_t dst_r = (uint8_t)((r5 << 3) | (r5 >> 2));
    const uint8_t dst_g = (uint8_t)((g6 << 2) | (g6 >> 4));
    const uint8_t dst_b = (uint8_t)((b5 << 3) | (b5 >> 2));
    const unsigned out_alpha = alpha + ((unsigned)dst_alpha * (255u - alpha) + 127u) / 255u;

    sprite->pixels[index] = tobytank_rgb565_pack(
        tobytank_mix_channel(color.r, dst_r, alpha),
        tobytank_mix_channel(color.g, dst_g, alpha),
        tobytank_mix_channel(color.b, dst_b, alpha));
    sprite->alpha[index] = (uint8_t)(out_alpha > 255u ? 255u : out_alpha);
}

static void sprite_blend_existing(tobytank_fish_sprite_t *sprite, int x, int y,
                                  rgb_t color, uint8_t alpha)
{
    if (alpha == 0 || x < 0 || y < 0 || x >= sprite->width || y >= sprite->height) {
        return;
    }

    const size_t index = (size_t)y * (size_t)sprite->width + (size_t)x;
    if (sprite->alpha[index] == 0) {
        return;
    }
    sprite_blend(sprite, x, y, color, alpha);
}

static rgb_t tint(rgb_t color, float scale, int x, int y)
{
    return (rgb_t){
        .r = clamp_u8((int)(color.r * scale) + tobytank_dither_bias_5bit(x, y)),
        .g = clamp_u8((int)(color.g * scale) + tobytank_dither_bias_6bit(x, y)),
        .b = clamp_u8((int)(color.b * scale) + tobytank_dither_bias_5bit(x, y)),
    };
}

static void fill_triangle(tobytank_fish_sprite_t *sprite,
                          float ax, float ay, float bx, float by, float cx, float cy,
                          rgb_t color, uint8_t alpha)
{
    int min_x = (int)floorf(fminf(ax, fminf(bx, cx)));
    int max_x = (int)ceilf(fmaxf(ax, fmaxf(bx, cx)));
    int min_y = (int)floorf(fminf(ay, fminf(by, cy)));
    int max_y = (int)ceilf(fmaxf(ay, fmaxf(by, cy)));
    if (min_x < 0) min_x = 0;
    if (min_y < 0) min_y = 0;
    if (max_x >= sprite->width) max_x = sprite->width - 1;
    if (max_y >= sprite->height) max_y = sprite->height - 1;

    const float area = (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
    if (fabsf(area) < 0.001f) {
        return;
    }
    for (int y = min_y; y <= max_y; ++y) {
        for (int x = min_x; x <= max_x; ++x) {
            const float px = (float)x + 0.5f;
            const float py = (float)y + 0.5f;
            const float w0 = (bx - ax) * (py - ay) - (by - ay) * (px - ax);
            const float w1 = (cx - bx) * (py - by) - (cy - by) * (px - bx);
            const float w2 = (ax - cx) * (py - cy) - (ay - cy) * (px - cx);
            if ((area > 0.0f && w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f) ||
                (area < 0.0f && w0 <= 0.0f && w1 <= 0.0f && w2 <= 0.0f)) {
                sprite_blend(sprite, x, y, color, alpha);
            }
        }
    }
}

static void sprite_line(tobytank_fish_sprite_t *sprite,
                        int x0, int y0, int x1, int y1,
                        rgb_t color, uint8_t alpha)
{
    int dx = x1 - x0;
    if (dx < 0) dx = -dx;
    const int sx = x0 < x1 ? 1 : -1;
    int dy = y1 - y0;
    if (dy > 0) dy = -dy;
    const int sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;

    for (;;) {
        sprite_blend(sprite, x0, y0, color, alpha);
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

static void draw_body(tobytank_fish_sprite_t *sprite, const tobytank_genome_t *genome,
                      float nose_x, float center_y, rgb_t base)
{
    const int x0 = (int)(nose_x - 2.0f);
    const int x1 = (int)(nose_x + genome->body_length + 2.0f);
    const float mid = nose_x + genome->body_length * 0.48f;

    for (int x = x0; x <= x1; ++x) {
        const float along = ((float)x - nose_x) / genome->body_length;
        if (along < 0.0f || along > 1.0f) {
            continue;
        }
        const float head = clampf(along / 0.32f, 0.0f, 1.0f);
        const float tail = clampf((1.0f - along) / 0.30f, 0.0f, 1.0f);
        const float taper = fminf(0.42f + genome->front_taper * 0.58f * head,
                                  0.38f + genome->rear_taper * 0.62f * tail);
        const float curve = sinf(along * 3.14159265f);
        const float top = genome->body_depth * 0.5f *
                          (taper + genome->back_curve * curve);
        const float bottom = genome->body_depth * 0.5f *
                             (taper + genome->belly_curve * curve);
        const int y0 = (int)floorf(center_y - top - 1.0f);
        const int y1 = (int)ceilf(center_y + bottom + 1.0f);
        for (int y = y0; y <= y1; ++y) {
            const float rel = ((float)y - center_y) / (((float)y < center_y) ? top : bottom);
            if (rel < -1.0f || rel > 1.0f) {
                continue;
            }
            const float edge = 1.0f - rel * rel;
            const float vertical = ((float)y - (center_y - top)) / (top + bottom);
            const float light = 0.72f + genome->belly_lightness * vertical +
                                genome->back_darkness * (1.0f - vertical) * -0.5f;
            const uint8_t alpha = (uint8_t)(180.0f + 75.0f * clampf(edge * 2.4f, 0.0f, 1.0f));
            rgb_t color = tint(base, clampf(light, 0.35f, 1.18f), x, y);
            if ((int)fabsf((float)x - mid) % 7 == 0 && genome->scale_contrast > 0.22f) {
                color = tint(color, 0.90f + 0.16f * genome->scale_contrast, x, y);
            }
            sprite_blend(sprite, x, y, color, alpha);
        }
    }
}

static void draw_fins(tobytank_fish_sprite_t *sprite, const tobytank_genome_t *genome,
                      float nose_x, float center_y, rgb_t fin)
{
    const float tail_x = nose_x + genome->body_length;
    const float dorsal_x = nose_x + genome->body_length * genome->dorsal_position;
    fill_triangle(sprite, dorsal_x, center_y - genome->body_depth * 0.38f,
                  dorsal_x + genome->dorsal_length, center_y - genome->body_depth * 0.26f,
                  dorsal_x + genome->dorsal_length * 0.38f,
                  center_y - genome->body_depth * 0.50f - genome->dorsal_height,
                  fin, 150);
    fill_triangle(sprite, nose_x + genome->body_length * 0.46f,
                  center_y + genome->body_depth * 0.34f,
                  nose_x + genome->body_length * 0.46f + genome->anal_length,
                  center_y + genome->body_depth * 0.24f,
                  nose_x + genome->body_length * 0.60f,
                  center_y + genome->body_depth * 0.50f + genome->anal_height,
                  fin, 135);
    fill_triangle(sprite, nose_x + genome->body_length * 0.34f,
                  center_y + genome->body_depth * 0.22f,
                  nose_x + genome->body_length * 0.38f + genome->pelvic_size,
                  center_y + genome->body_depth * 0.36f,
                  nose_x + genome->body_length * 0.30f,
                  center_y + genome->body_depth * 0.38f + genome->pelvic_size,
                  fin, 128);
    fill_triangle(sprite, nose_x + genome->body_length * 0.36f,
                  center_y + genome->body_depth * 0.02f,
                  nose_x + genome->body_length * 0.42f + genome->pectoral_size * 0.7f,
                  center_y + genome->pectoral_size * 0.18f,
                  nose_x + genome->body_length * 0.36f + genome->pectoral_size * 0.30f,
                  center_y + genome->pectoral_size,
                  fin, 120);

    const float span = genome->caudal_span * 0.5f;
    const float length = genome->caudal_length;
    if (genome->caudal_type == TOBYTANK_CAUDAL_FORKED ||
        genome->caudal_type == TOBYTANK_CAUDAL_LUNATE) {
        const float fork = genome->caudal_type == TOBYTANK_CAUDAL_LUNATE ? 0.36f : 0.22f;
        fill_triangle(sprite, tail_x - 1.0f, center_y,
                      tail_x + length, center_y - span,
                      tail_x + length * fork, center_y - span * 0.10f, fin, 158);
        fill_triangle(sprite, tail_x - 1.0f, center_y,
                      tail_x + length, center_y + span,
                      tail_x + length * fork, center_y + span * 0.10f, fin, 158);
    } else {
        fill_triangle(sprite, tail_x - 1.0f, center_y - genome->body_depth * 0.18f,
                      tail_x + length, center_y - span,
                      tail_x + length, center_y + span, fin, 152);
        fill_triangle(sprite, tail_x - 1.0f, center_y - genome->body_depth * 0.18f,
                      tail_x + length, center_y + span,
                      tail_x - 1.0f, center_y + genome->body_depth * 0.18f, fin, 152);
    }

    for (int ray = 0; ray < (int)genome->fin_rays; ++ray) {
        const float t = (float)ray / (float)(genome->fin_rays > 1 ? genome->fin_rays - 1 : 1);
        const int y = (int)(center_y - span + span * 2.0f * t + 0.5f);
        const int x0 = (int)(tail_x + 0.5f);
        const int x1 = (int)(tail_x + length * (0.75f + 0.25f * fabsf(0.5f - t)) + 0.5f);
        sprite_line(sprite, x0, (int)(center_y + 0.5f), x1, y, fin, 94);
    }
}

static int pattern_hit(const tobytank_genome_t *genome, float ax, float ay)
{
    const float scale = genome->pattern_scale < 0.04f ? 0.04f : genome->pattern_scale;
    switch ((tobytank_pattern_t)genome->pattern_type) {
    case TOBYTANK_PATTERN_VERTICAL_BARS:
        return ((int)(ax / scale) & 1) == 0;
    case TOBYTANK_PATTERN_HORIZONTAL_STRIPES:
        return ((int)((ay + 0.6f) / (scale * 0.55f)) & 1) == 0;
    case TOBYTANK_PATTERN_SPOTS: {
        const float gx = ax / scale;
        const float gy = ay / scale;
        const float fx = gx - floorf(gx) - 0.5f;
        const float fy = gy - floorf(gy) - 0.5f;
        return fx * fx + fy * fy < 0.055f + 0.12f * genome->pattern_density;
    }
    case TOBYTANK_PATTERN_BLOTCHES:
        return sinf(ax * 19.0f + ay * 7.0f) + sinf(ax * 5.0f - ay * 17.0f) > 0.65f;
    case TOBYTANK_PATTERN_RETICULATED:
        return (((int)(ax / scale) + (int)(ay / scale)) % 3) == 0;
    case TOBYTANK_PATTERN_SADDLE:
        return ay < -0.08f && ((int)(ax / (scale * 1.6f)) & 1) == 0;
    default:
        return 0;
    }
}

static void draw_markings(tobytank_fish_sprite_t *sprite, const tobytank_genome_t *genome,
                          float nose_x, float center_y, rgb_t accent)
{
    if (genome->pattern_type == TOBYTANK_PATTERN_NONE) {
        return;
    }

    const int x0 = (int)nose_x;
    const int x1 = (int)(nose_x + genome->body_length);
    const int y0 = (int)(center_y - genome->body_depth);
    const int y1 = (int)(center_y + genome->body_depth);
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            const float ax = ((float)x - nose_x) / genome->body_length;
            const float ay = ((float)y - center_y) / genome->body_depth;
            if (ax < 0.08f || ax > 0.94f || ay < -0.56f || ay > 0.56f) {
                continue;
            }
            if (pattern_hit(genome, ax, ay)) {
                const uint8_t alpha = (uint8_t)(38.0f + 105.0f * genome->pattern_contrast);
                sprite_blend_existing(sprite, x, y, accent, alpha);
            }
        }
    }

    for (int mark = 0; mark < (int)genome->accent_marks; ++mark) {
        const float ax = 0.18f + 0.13f * (float)mark;
        const int cx = (int)(nose_x + genome->body_length * ax + 0.5f);
        const int cy = (int)(center_y - genome->body_depth * (0.20f - 0.07f * (float)(mark & 1)));
        const int radius = (int)(genome->accent_mark_size + 0.5f);
        for (int dy = -radius; dy <= radius; ++dy) {
            for (int dx = -radius; dx <= radius; ++dx) {
                if (dx * dx + dy * dy <= radius * radius) {
                    sprite_blend_existing(sprite, cx + dx, cy + dy, accent, 120);
                }
            }
        }
    }
}

static void draw_face(tobytank_fish_sprite_t *sprite, const tobytank_genome_t *genome,
                      float nose_x, float center_y)
{
    const int eye_x = (int)(nose_x + genome->body_length * genome->eye_offset_x + 0.5f);
    const int eye_y = (int)(center_y + genome->body_depth * genome->eye_offset_y + 0.5f);
    const int eye_r = (int)(genome->eye_size + 0.5f);
    for (int dy = -eye_r; dy <= eye_r; ++dy) {
        for (int dx = -eye_r; dx <= eye_r; ++dx) {
            if (dx * dx + dy * dy <= eye_r * eye_r) {
                sprite_blend(sprite, eye_x + dx, eye_y + dy, (rgb_t){230, 238, 228}, 245);
            }
        }
    }
    const int pupil = eye_r > 2 ? eye_r / 2 : 1;
    for (int dy = -pupil; dy <= pupil; ++dy) {
        for (int dx = -pupil; dx <= pupil; ++dx) {
            if (dx * dx + dy * dy <= pupil * pupil) {
                sprite_blend(sprite, eye_x + dx + 1, eye_y + dy, (rgb_t){2, 8, 12}, 255);
            }
        }
    }

    const int mouth_x = (int)(nose_x + 1.0f);
    const int mouth_y = (int)(center_y + genome->body_depth * 0.08f);
    const int mouth_len = (int)(genome->mouth_size + 0.5f);
    sprite_line(sprite, mouth_x, mouth_y,
                mouth_x + mouth_len,
                mouth_y + (int)(sinf(genome->mouth_angle) * (float)mouth_len),
                (rgb_t){18, 24, 24}, 180);

    const int gill_x = (int)(nose_x + genome->body_length * genome->gill_offset + 0.5f);
    const int gill_top = (int)(center_y - genome->body_depth * 0.24f);
    const int gill_bottom = (int)(center_y + genome->body_depth * 0.30f);
    for (int y = gill_top; y <= gill_bottom; ++y) {
        const float t = (float)(y - gill_top) / (float)(gill_bottom - gill_top + 1);
        const int x = gill_x + (int)(sinf(t * 3.14159265f) * genome->gill_curve * 3.0f);
        sprite_blend(sprite, x, y, (rgb_t){20, 38, 42}, 110);
    }
}

int tobytank_fish_rasterize(const tobytank_genome_t *genome,
                            tobytank_fish_sprite_t *sprite)
{
    if (genome == NULL || sprite == NULL || sprite->pixels == NULL || sprite->alpha == NULL ||
        sprite->width < TOBYTANK_FISH_MAX_WIDTH ||
        sprite->height < TOBYTANK_FISH_MAX_HEIGHT ||
        tobytank_genome_validate(genome) != TOBYTANK_GENOME_OK) {
        return 0;
    }

    clear_sprite(sprite);
    float extent_w = 0.0f;
    float extent_h = 0.0f;
    tobytank_genome_extent(genome, &extent_w, &extent_h);

    sprite->width = TOBYTANK_FISH_MAX_WIDTH;
    sprite->height = TOBYTANK_FISH_MAX_HEIGHT;
    sprite->origin_x = sprite->width / 2;
    sprite->origin_y = sprite->height / 2;

    const float nose_x = (float)sprite->origin_x - extent_w * 0.45f;
    const float center_y = (float)sprite->origin_y;

    const rgb_t base = hsv(genome->base_hue, genome->base_saturation, genome->base_value);
    const rgb_t fin = hsv(genome->base_hue + genome->fin_hue_offset,
                          genome->base_saturation * 0.82f,
                          genome->base_value + 0.06f);
    const rgb_t accent = hsv(genome->base_hue + genome->accent_hue_offset,
                             genome->accent_saturation,
                             clampf(genome->base_value + 0.04f, 0.0f, 0.94f));

    draw_fins(sprite, genome, nose_x, center_y, fin);
    draw_body(sprite, genome, nose_x, center_y, base);
    draw_markings(sprite, genome, nose_x, center_y, accent);
    draw_face(sprite, genome, nose_x, center_y);
    return 1;
}
