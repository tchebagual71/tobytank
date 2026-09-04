#include "fish/portrait.h"

#include "render/fish_rasterizer.h"

int tobytank_portrait_render(const tobytank_genome_t *genome,
                             tobytank_fish_sprite_t *sprite)
{
    return tobytank_fish_rasterize(genome, sprite);
}

