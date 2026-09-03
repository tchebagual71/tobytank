#pragma once

#include "aquarium/environment.h"
#include "render/canvas.h"

/*
 * Draws the still parts of the tank from an immutable snapshot: the water
 * gradient, the substrate dune, stones, and swaying plants. Pure C, no
 * hardware dependencies, so the host preview and tests use it directly.
 */
void tobytank_background_draw(const tobytank_canvas_t *canvas,
                              const tobytank_env_snapshot_t *snapshot);

/*
 * The three passes are also exposed individually so the renderer can time them
 * separately; the water and terrain passes cover every pixel and dominate the
 * frame budget.
 */
void tobytank_background_draw_water(const tobytank_canvas_t *canvas,
                                    const tobytank_env_snapshot_t *snapshot);
void tobytank_background_draw_terrain(const tobytank_canvas_t *canvas,
                                      const tobytank_env_snapshot_t *snapshot);
void tobytank_background_draw_plants(const tobytank_canvas_t *canvas,
                                     const tobytank_env_snapshot_t *snapshot);
