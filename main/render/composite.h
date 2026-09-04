#pragma once

#include "render/canvas.h"
#include "render/fish_sprite.h"

/* Blends a cached sprite onto the destination canvas at its origin point. */
void tobytank_composite_sprite(const tobytank_canvas_t *canvas,
                               const tobytank_fish_sprite_t *sprite,
                               int x, int y);

/* Positive facing mirrors the cached left-facing sprite so the nose leads. */
void tobytank_composite_sprite_facing(const tobytank_canvas_t *canvas,
                                      const tobytank_fish_sprite_t *sprite,
                                      int x, int y,
                                      float facing);
