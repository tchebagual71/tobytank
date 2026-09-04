#pragma once

#include <stddef.h>
#include <stdint.h>

#include "fish/genome.h"
#include "render/fish_sprite.h"

typedef struct {
    tobytank_fish_sprite_t sprite;
    tobytank_identity_t identity;
    uint64_t fingerprint;
    int valid;
} tobytank_fish_cache_t;

size_t tobytank_fish_cache_pixel_bytes(void);
size_t tobytank_fish_cache_alpha_bytes(void);

void tobytank_fish_cache_init(tobytank_fish_cache_t *cache,
                              uint16_t *pixels, uint8_t *alpha);

/* Renders only when the cached fingerprint differs. */
int tobytank_fish_cache_prepare(tobytank_fish_cache_t *cache,
                                const tobytank_genome_t *genome);
