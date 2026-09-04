#pragma once

#include "fish/genome.h"
#include "render/fish_sprite.h"

/*
 * Rasterizes one accepted genome into a caller-owned sprite. Pure C and
 * host-testable. Returns 1 on success, 0 for invalid input, invalid genomes, or
 * undersized sprite storage.
 */
int tobytank_fish_rasterize(const tobytank_genome_t *genome,
                            tobytank_fish_sprite_t *sprite);

