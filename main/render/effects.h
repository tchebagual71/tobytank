#pragma once

#include "aquarium/environment.h"
#include "render/canvas.h"

/*
 * Light and water effects layered over the background: drifting light shafts,
 * caustic ripples near the surface, surface shimmer, and a soft vignette. The
 * vignette also serves burn-in protection by keeping the frame edges dark.
 */
void tobytank_effects_draw(const tobytank_canvas_t *canvas,
                           const tobytank_env_snapshot_t *snapshot);
