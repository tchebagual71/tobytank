#include "render/background.h"

#include <math.h>

/*
 * The water and substrate passes together cover every pixel on the panel, so
 * they run on 8.8 fixed-point maths and lookup tables. An earlier float version
 * of the same picture cost about 100 ms per frame on the board.
 */

/* RGB565 has only 32 blue levels, so a smooth full-height water ramp bands
   badly. A 4x4 ordered dither hides it for the cost of a table lookup. */
static const int kBayer4[4][4] = {
    {0, 8, 2, 10},
    {12, 4, 14, 6},
    {3, 11, 1, 9},
    {15, 7, 13, 5},
};

/* Width of the darkened border, in pixels, on every edge. */
#define VIGNETTE_EXTENT 40

/* Widest canvas the substrate cache supports; wider canvases take a slow path. */
#define BACKGROUND_MAX_WIDTH 512

/* One 5-bit step is 8/255, one 6-bit step is 4/255, both in 8.8 fixed point. */
static int s_bias_five[16];
static int s_bias_six[16];
static int s_edge_scale[VIGNETTE_EXTENT];
static int s_tables_ready;
static int s_surface_row[BACKGROUND_MAX_WIDTH];

static void ensure_tables(void)
{
    if (s_tables_ready) {
        return;
    }
    for (int threshold = 0; threshold < 16; ++threshold) {
        const float offset = ((float)threshold - 7.5f) / 16.0f;
        s_bias_five[threshold] = (int)(offset * 8.0f * 256.0f);
        s_bias_six[threshold] = (int)(offset * 4.0f * 256.0f);
    }
    for (int distance = 0; distance < VIGNETTE_EXTENT; ++distance) {
        /* Eased edge darkening. Besides framing the tank it is the cheapest
           burn-in mitigation available on an AMOLED, and folding it into the
           background write means no pixel has to be read back and blended. */
        const float along = (float)distance / (float)VIGNETTE_EXTENT;
        s_edge_scale[distance] = (int)((0.52f + 0.48f * (along * (2.0f - along))) * 256.0f);
    }
    s_tables_ready = 1;
}

static inline int edge_scale(int position, int span)
{
    const int mirrored = span - 1 - position;
    const int distance = position < mirrored ? position : mirrored;
    if (distance >= VIGNETTE_EXTENT || distance < 0) {
        return 256;
    }
    return s_edge_scale[distance];
}

static inline uint8_t dither_channel(int value_q8, int bias_q8)
{
    const int level = (value_q8 + bias_q8 + 128) >> 8;
    if (level <= 0) {
        return 0;
    }
    if (level >= 255) {
        return 255;
    }
    return (uint8_t)level;
}

static inline uint16_t dithered_color(int red_q8, int green_q8, int blue_q8, int threshold)
{
    return tobytank_rgb565_pack(dither_channel(red_q8, s_bias_five[threshold]),
                                dither_channel(green_q8, s_bias_six[threshold]),
                                dither_channel(blue_q8, s_bias_five[threshold]));
}

/*
 * Fills s_surface_row with the substrate height per column and returns the
 * highest of them. Water and terrain share it so that between them every pixel
 * is written exactly once.
 */
static int compute_surface(const tobytank_canvas_t *canvas,
                           const tobytank_env_snapshot_t *snapshot)
{
    if (canvas->width > BACKGROUND_MAX_WIDTH) {
        return canvas->height;
    }

    int first_row = canvas->height;
    for (int x = 0; x < canvas->width; ++x) {
        int surface = (int)(tobytank_environment_substrate_at(snapshot, (float)x) + 0.5f);
        if (surface < 0) {
            surface = 0;
        }
        if (surface > canvas->height) {
            surface = canvas->height;
        }
        s_surface_row[x] = surface;
        if (surface < first_row) {
            first_row = surface;
        }
    }
    return first_row;
}

static void draw_water(const tobytank_canvas_t *canvas,
                       const tobytank_env_snapshot_t *snapshot)
{
    const int last_row = canvas->height - 1;
    int edge = VIGNETTE_EXTENT;
    if (edge > canvas->width / 2) {
        edge = canvas->width / 2;
    }

    for (int y = 0; y < canvas->height; ++y) {
        /* Squaring the ramp keeps the upper water open and sinks the lower
           third toward black, which is both prettier and easier on an AMOLED. */
        const int linear_q8 = last_row > 0 ? (y * 256) / last_row : 0;
        const int weight_q8 = (linear_q8 * linear_q8) >> 8;
        const int row_scale = edge_scale(y, canvas->height);

        int red = (int)snapshot->water_top[0] * 256 +
                  ((int)snapshot->water_bottom[0] - (int)snapshot->water_top[0]) * weight_q8;
        int green = (int)snapshot->water_top[1] * 256 +
                    ((int)snapshot->water_bottom[1] - (int)snapshot->water_top[1]) * weight_q8;
        int blue = (int)snapshot->water_top[2] * 256 +
                   ((int)snapshot->water_bottom[2] - (int)snapshot->water_top[2]) * weight_q8;
        red = (red * row_scale) >> 8;
        green = (green * row_scale) >> 8;
        blue = (blue * row_scale) >> 8;

        const int *bayer_row = kBayer4[y & 3];
        uint16_t pattern[4];
        for (int i = 0; i < 4; ++i) {
            pattern[i] = dithered_color(red, green, blue, bayer_row[i]);
        }

        uint16_t *row = &canvas->pixels[y * canvas->width];
        for (int x = 0; x < edge; ++x) {
            const int scale = edge_scale(x, canvas->width);
            row[x] = dithered_color((red * scale) >> 8, (green * scale) >> 8,
                                    (blue * scale) >> 8, bayer_row[x & 3]);
        }
        for (int x = canvas->width - edge; x < canvas->width; ++x) {
            const int scale = edge_scale(x, canvas->width);
            row[x] = dithered_color((red * scale) >> 8, (green * scale) >> 8,
                                    (blue * scale) >> 8, bayer_row[x & 3]);
        }
        /* The interior repeats every four pixels, so write it as 32-bit pairs. */
        const int interior_start = edge;
        const int interior_end = canvas->width - edge;
        int x = interior_start;
        if ((x & 1) != 0 && x < interior_end) {
            row[x] = pattern[x & 3];
            ++x;
        }
        uint32_t pair_even;
        uint32_t pair_odd;
        {
            const uint32_t p0 = pattern[0];
            const uint32_t p1 = pattern[1];
            const uint32_t p2 = pattern[2];
            const uint32_t p3 = pattern[3];
            pair_even = p0 | (p1 << 16);
            pair_odd = p2 | (p3 << 16);
        }
        uint32_t *pairs = (uint32_t *)(void *)&row[x];
        int pair_index = x >> 1;
        const int pair_end = interior_end >> 1;
        if ((pair_index & 1) != 0 && pair_index < pair_end) {
            *pairs++ = pair_odd;
            ++pair_index;
        }
        while (pair_index + 1 < pair_end) {
            pairs[0] = pair_even;
            pairs[1] = pair_odd;
            pairs += 2;
            pair_index += 2;
        }
        while (pair_index < pair_end) {
            *pairs++ = (pair_index & 1) ? pair_odd : pair_even;
            ++pair_index;
        }
        for (x = pair_end << 1; x < interior_end; ++x) {
            row[x] = pattern[x & 3];
        }
    }
}

static void draw_substrate(const tobytank_canvas_t *canvas,
                           const tobytank_env_snapshot_t *snapshot)
{
    const int first_row = compute_surface(canvas, snapshot);
    if (first_row >= canvas->height) {
        return;
    }

    /* Row-major so the framebuffer is written in cache-line order. */
    for (int y = first_row; y < canvas->height; ++y) {
        uint16_t *row = &canvas->pixels[y * canvas->width];
        const int row_scale = edge_scale(y, canvas->height);
        const int *bayer_row = kBayer4[y & 3];
        /* The dune line moves slowly across the row, so the depth-dependent
           colour only has to be recomputed when the surface height changes. */
        int cached_surface = -1;
        int red = 0;
        int green = 0;
        int blue = 0;
        for (int x = 0; x < canvas->width; ++x) {
            const int surface = s_surface_row[x];
            if (y < surface) {
                continue;
            }
            if (surface != cached_surface) {
                cached_surface = surface;
                const int depth = canvas->height - surface;
                const int weight_q8 = depth > 1 ? ((y - surface) * 256) / (depth - 1) : 0;
                red = (int)snapshot->substrate_top[0] * 256 +
                      ((int)snapshot->substrate_bottom[0] -
                       (int)snapshot->substrate_top[0]) * weight_q8;
                green = (int)snapshot->substrate_top[1] * 256 +
                        ((int)snapshot->substrate_bottom[1] -
                         (int)snapshot->substrate_top[1]) * weight_q8;
                blue = (int)snapshot->substrate_top[2] * 256 +
                       ((int)snapshot->substrate_bottom[2] -
                        (int)snapshot->substrate_top[2]) * weight_q8;
            }
            const int scale = (row_scale * edge_scale(x, canvas->width)) >> 8;
            row[x] = dithered_color((red * scale) >> 8, (green * scale) >> 8,
                                    (blue * scale) >> 8, bayer_row[x & 3]);
            if (y == surface) {
                /* A single lighter crest line reads as sand catching the light. */
                tobytank_canvas_blend_at(&row[x], snapshot->haze[0], snapshot->haze[1],
                                         snapshot->haze[2], 46);
            }
        }
    }
}

static void draw_stones(const tobytank_canvas_t *canvas,
                        const tobytank_env_snapshot_t *snapshot)
{
    for (int i = 0; i < snapshot->stone_count; ++i) {
        const tobytank_stone_t *stone = &snapshot->stones[i];
        const int center_x = (int)(stone->center_x + 0.5f);
        const int center_y = (int)(stone->center_y + 0.5f);
        const int radius_x = (int)(stone->radius_x + 0.5f);
        const int radius_y = (int)(stone->radius_y + 0.5f);
        const uint8_t tone = stone->tone;
        tobytank_canvas_fill_ellipse(canvas, center_x, center_y, radius_x, radius_y,
                                     tobytank_rgb565((uint8_t)(tone * 3 / 4),
                                                     (uint8_t)(tone * 9 / 10),
                                                     tone));
        /* Two soft ellipse caps toward the top so stones read as rounded rather
           than as flat blobs. A rectangle here looked like a pasted patch. */
        for (int pass = 0; pass < 2; ++pass) {
            const int shrink = 2 + pass * 2;
            const int lift = radius_y / 3 + pass;
            const int cap_x = radius_x - shrink * 3;
            const int cap_y = radius_y - shrink;
            if (cap_x <= 0 || cap_y <= 0) {
                continue;
            }
            for (int dy = -cap_y; dy <= 0; ++dy) {
                const float normalized = (float)dy / (float)cap_y;
                const int span = (int)((float)cap_x * (1.0f - normalized * normalized));
                if (span <= 0) {
                    continue;
                }
                tobytank_canvas_blend_rect(canvas, center_x - span, center_y - lift + dy,
                                           2 * span, 1,
                                           snapshot->haze[0], snapshot->haze[1],
                                           snapshot->haze[2], 12);
            }
        }
    }
}

static void draw_plants(const tobytank_canvas_t *canvas,
                        const tobytank_env_snapshot_t *snapshot)
{
    for (int i = 0; i < snapshot->plant_count; ++i) {
        const tobytank_plant_t *plant = &snapshot->plants[i];
        const float root_y = tobytank_environment_substrate_at(snapshot, plant->base_x);
        for (int blade = 0; blade < (int)plant->blades; ++blade) {
            const float blade_offset = ((float)blade - (float)plant->blades * 0.5f) * 3.0f;
            const float blade_height = plant->height *
                                       (0.62f + 0.38f * ((float)((blade * 7) % 5) / 4.0f));
            const float phase = plant->sway_phase + (float)blade * 0.42f;
            const int steps = (int)blade_height;
            /* Sample the sway curve nine times and interpolate. A sine per step
               cost more than every other part of the plant put together. */
            float curve[9];
            for (int k = 0; k <= 8; ++k) {
                curve[k] = sinf(phase + (float)k * (2.1f / 8.0f));
            }
            const float inverse_span = 1.0f / (float)(steps > 1 ? steps - 1 : 1);
            for (int step = 0; step < steps; ++step) {
                const float along = (float)step * inverse_span;
                const float sampled = along * 8.0f;
                int index = (int)sampled;
                if (index > 7) {
                    index = 7;
                }
                const float fraction = sampled - (float)index;
                const float wave = curve[index] + (curve[index + 1] - curve[index]) * fraction;
                const float sway = wave * plant->sway_amount * along * along;
                const int x = (int)(plant->base_x + blade_offset + sway + 0.5f);
                const int y = (int)(root_y - (float)step + 0.5f);
                /* Blades brighten toward the tip and stay translucent so the
                   water behind them still shows through. */
                const uint8_t alpha = (uint8_t)(120.0f + 70.0f * along);
                tobytank_canvas_blend(canvas, x, y,
                                      (uint8_t)(plant->green / 5),
                                      plant->green, plant->blue, alpha);
                tobytank_canvas_blend(canvas, x + 1, y,
                                      (uint8_t)(plant->green / 6),
                                      (uint8_t)(plant->green * 3 / 4),
                                      (uint8_t)(plant->blue * 3 / 4),
                                      (uint8_t)(alpha / 2));
            }
        }
    }
}

static int usable(const tobytank_canvas_t *canvas, const tobytank_env_snapshot_t *snapshot)
{
    return canvas != NULL && canvas->pixels != NULL && snapshot != NULL &&
           canvas->width > 0 && canvas->height > 0;
}

void tobytank_background_draw_water(const tobytank_canvas_t *canvas,
                                    const tobytank_env_snapshot_t *snapshot)
{
    if (!usable(canvas, snapshot)) {
        return;
    }
    ensure_tables();
    draw_water(canvas, snapshot);
}

void tobytank_background_draw_terrain(const tobytank_canvas_t *canvas,
                                      const tobytank_env_snapshot_t *snapshot)
{
    if (!usable(canvas, snapshot)) {
        return;
    }
    ensure_tables();
    draw_substrate(canvas, snapshot);
    draw_stones(canvas, snapshot);
}

void tobytank_background_draw_plants(const tobytank_canvas_t *canvas,
                                     const tobytank_env_snapshot_t *snapshot)
{
    if (!usable(canvas, snapshot)) {
        return;
    }
    ensure_tables();
    draw_plants(canvas, snapshot);
}

void tobytank_background_draw(const tobytank_canvas_t *canvas,
                              const tobytank_env_snapshot_t *snapshot)
{
    tobytank_background_draw_water(canvas, snapshot);
    tobytank_background_draw_terrain(canvas, snapshot);
    tobytank_background_draw_plants(canvas, snapshot);
}
