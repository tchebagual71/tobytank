#include "render/particles.h"

#include <math.h>

static void blend_disc(const tobytank_canvas_t *canvas, int center_x, int center_y,
                       int radius, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha)
{
    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            if (dx * dx + dy * dy > radius * radius) {
                continue;
            }
            tobytank_canvas_blend(canvas, center_x + dx, center_y + dy,
                                        red, green, blue, alpha);
        }
    }
}

static void draw_motes(const tobytank_canvas_t *canvas,
                       const tobytank_env_snapshot_t *snapshot)
{
    for (int i = 0; i < snapshot->mote_count; ++i) {
        const tobytank_mote_t *mote = &snapshot->motes[i];
        const int x = (int)(mote->x + 0.5f);
        const int y = (int)(mote->y + 0.5f);
        tobytank_canvas_blend(canvas, x, y,
                                    snapshot->haze[0], snapshot->haze[1],
                                    snapshot->haze[2], mote->alpha);
        if (mote->radius > 1.2f) {
            const uint8_t faint = (uint8_t)(mote->alpha / 2);
            tobytank_canvas_blend(canvas, x + 1, y, snapshot->haze[0],
                                        snapshot->haze[1], snapshot->haze[2], faint);
            tobytank_canvas_blend(canvas, x, y + 1, snapshot->haze[0],
                                        snapshot->haze[1], snapshot->haze[2], faint);
        }
    }
}

static void draw_bubbles(const tobytank_canvas_t *canvas,
                         const tobytank_env_snapshot_t *snapshot)
{
    for (int i = 0; i < snapshot->bubble_count; ++i) {
        const tobytank_bubble_t *bubble = &snapshot->bubbles[i];
        const float wobble = sinf(bubble->wobble_phase) * bubble->wobble_amount;
        const int x = (int)(bubble->x + wobble + 0.5f);
        const int y = (int)(bubble->y + 0.5f);
        const int radius = (int)(bubble->radius + 0.5f);
        if (radius <= 0) {
            continue;
        }

        /* A soft body with a brighter rim and a small highlight reads as a
           bubble at these sizes without needing anti-aliasing. */
        blend_disc(canvas, x, y, radius, snapshot->haze[0], snapshot->haze[1],
                   snapshot->haze[2], (uint8_t)(bubble->alpha / 3));
        for (int dy = -radius; dy <= radius; ++dy) {
            for (int dx = -radius; dx <= radius; ++dx) {
                const int distance = dx * dx + dy * dy;
                if (distance <= radius * radius && distance > (radius - 1) * (radius - 1)) {
                    tobytank_canvas_blend(canvas, x + dx, y + dy,
                                                snapshot->haze[0], snapshot->haze[1],
                                                snapshot->haze[2], bubble->alpha);
                }
            }
        }
        if (radius >= 2) {
            tobytank_canvas_blend(canvas, x - radius / 2, y - radius / 2,
                                        255, 255, 255, (uint8_t)(bubble->alpha / 2));
        }
    }
}

void tobytank_particles_draw(const tobytank_canvas_t *canvas,
                             const tobytank_env_snapshot_t *snapshot)
{
    if (canvas == NULL || canvas->pixels == NULL || snapshot == NULL ||
        canvas->width <= 0 || canvas->height <= 0) {
        return;
    }

    draw_motes(canvas, snapshot);
    draw_bubbles(canvas, snapshot);
}
