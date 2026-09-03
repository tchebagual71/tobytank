#pragma once

#include "aquarium/environment.h"
#include "render/canvas.h"

/*
 * Draws the drifting motes and rising bubbles held in the environment snapshot.
 * Particle motion lives in the simulation; this module only rasterizes it.
 */
void tobytank_particles_draw(const tobytank_canvas_t *canvas,
                             const tobytank_env_snapshot_t *snapshot);
