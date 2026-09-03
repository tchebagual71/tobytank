#include "render/effects.h"

#include <math.h>

/*
 * Light and water effects layered over the background. Every loop here pre
 * clips its own bounds and writes through the inline blend so the per-pixel
 * cost stays low; these passes touch tens of thousands of pixels per frame.
 *
 * The edge vignette lives in the background pass, where it costs no readback.
 */

static void draw_shafts(const tobytank_canvas_t *canvas,
                        const tobytank_env_snapshot_t *snapshot)
{
    for (int i = 0; i < snapshot->shaft_count; ++i) {
        const tobytank_shaft_t *shaft = &snapshot->shafts[i];
        int reach = (int)shaft->reach;
        if (reach <= 1) {
            continue;
        }
        if (reach > canvas->height) {
            reach = canvas->height;
        }
        for (int y = 0; y < reach; ++y) {
            const float along = (float)y / (float)reach;
            /* Shafts widen and fade as they sink. */
            const float width = shaft->width * (0.72f + 0.55f * along);
            const float center = shaft->x + shaft->slant * (float)y;
            const float fade = (1.0f - along) * (1.0f - along);
            const float peak = (float)shaft->intensity * fade;
            if (peak < 0.5f) {
                continue;
            }
            const float half = width * 0.5f;
            int left = (int)(center - half);
            int right = (int)(center + half);
            if (left < 0) {
                left = 0;
            }
            if (right >= canvas->width) {
                right = canvas->width - 1;
            }
            uint16_t *row = &canvas->pixels[y * canvas->width];
            /* Feather across the width; a flat run reads as a rectangle. The
               offset advances by a constant, so no divide is needed per pixel. */
            const float inverse_half = 1.0f / half;
            float offset = ((float)left - center) * inverse_half;
            for (int x = left; x <= right; ++x, offset += inverse_half) {
                const float falloff = 1.0f - offset * offset;
                if (falloff <= 0.0f) {
                    continue;
                }
                tobytank_canvas_blend_at(&row[x], snapshot->haze[0], snapshot->haze[1],
                                         snapshot->haze[2],
                                         (uint8_t)(peak * falloff * falloff));
            }
        }
    }
}

static void draw_caustics(const tobytank_canvas_t *canvas,
                          const tobytank_env_snapshot_t *snapshot)
{
    const int depth = canvas->height * 30 / 100;
    const float time = snapshot->time_seconds;
    for (int y = 0; y < depth; ++y) {
        const float along = (float)y / (float)depth;
        const float fade = (1.0f - along) * (1.0f - along);
        const float peak = 26.0f * fade;
        if (peak < 0.5f) {
            continue;
        }
        uint16_t *row = &canvas->pixels[y * canvas->width];
        for (int ripple = 0; ripple < 3; ++ripple) {
            const float phase = time * 0.55f + (float)y * 0.045f + (float)ripple * 2.1f;
            const float position = (0.5f + 0.5f * sinf(phase)) * (float)canvas->width;
            const int length = 9 + (int)(6.0f * sinf(phase * 1.7f));
            if (length <= 1) {
                continue;
            }
            const int start = (int)position;
            for (int i = 0; i < length; ++i) {
                const int x = start + i;
                if (x < 0 || x >= canvas->width) {
                    continue;
                }
                /* Taper each ripple so caustics look like light, not like dashes. */
                const float shape = sinf((float)i / (float)(length - 1) * 3.14159265f);
                tobytank_canvas_blend_at(&row[x], snapshot->haze[0], snapshot->haze[1],
                                         snapshot->haze[2], (uint8_t)(peak * shape));
            }
        }
    }
}

static void draw_surface_shimmer(const tobytank_canvas_t *canvas,
                                 const tobytank_env_snapshot_t *snapshot)
{
    int rows = 7;
    if (rows > canvas->height) {
        rows = canvas->height;
    }
    const float time = snapshot->time_seconds;
    /* x drives the wave, so walk columns on the outside and reuse the two sine
       evaluations across all rows instead of recomputing them per pixel. */
    for (int x = 0; x < canvas->width; ++x) {
        const float wave = sinf((float)x * 0.06f + time * 1.1f) *
                           sinf((float)x * 0.017f - time * 0.6f);
        const float lit = 0.5f + 0.5f * wave;
        for (int y = 0; y < rows; ++y) {
            const float row_fade = 1.0f - (float)y / (float)rows;
            tobytank_canvas_blend_at(&canvas->pixels[y * canvas->width + x],
                                     snapshot->haze[0], snapshot->haze[1], snapshot->haze[2],
                                     (uint8_t)(46.0f * lit * row_fade));
        }
    }
}

void tobytank_effects_draw(const tobytank_canvas_t *canvas,
                           const tobytank_env_snapshot_t *snapshot)
{
    if (canvas == NULL || canvas->pixels == NULL || snapshot == NULL ||
        canvas->width <= 0 || canvas->height <= 0) {
        return;
    }

    draw_shafts(canvas, snapshot);
    draw_caustics(canvas, snapshot);
    draw_surface_shimmer(canvas, snapshot);
}
