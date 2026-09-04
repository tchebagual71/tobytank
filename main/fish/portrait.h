#pragma once

#include "fish/genome.h"
#include "render/fish_sprite.h"

/*
 * Portrait generation currently means a deterministic full-body fish sprite.
 * Fishbook-specific crops and metadata arrive with the Fishbook milestone.
 */
int tobytank_portrait_render(const tobytank_genome_t *genome,
                             tobytank_fish_sprite_t *sprite);

