#include "render/fish_cache.h"

#include "fish/portrait.h"

size_t tobytank_fish_cache_pixel_bytes(void)
{
    return (size_t)TOBYTANK_FISH_MAX_WIDTH * (size_t)TOBYTANK_FISH_MAX_HEIGHT *
           sizeof(uint16_t);
}

size_t tobytank_fish_cache_alpha_bytes(void)
{
    return (size_t)TOBYTANK_FISH_MAX_WIDTH * (size_t)TOBYTANK_FISH_MAX_HEIGHT *
           sizeof(uint8_t);
}

void tobytank_fish_cache_init(tobytank_fish_cache_t *cache,
                              uint16_t *pixels, uint8_t *alpha)
{
    if (cache == NULL) {
        return;
    }
    cache->sprite.pixels = pixels;
    cache->sprite.alpha = alpha;
    cache->sprite.width = TOBYTANK_FISH_MAX_WIDTH;
    cache->sprite.height = TOBYTANK_FISH_MAX_HEIGHT;
    cache->sprite.origin_x = TOBYTANK_FISH_MAX_WIDTH / 2;
    cache->sprite.origin_y = TOBYTANK_FISH_MAX_HEIGHT / 2;
    cache->identity = TOBYTANK_IDENTITY_INVALID;
    cache->fingerprint = 0;
    cache->valid = 0;
}

int tobytank_fish_cache_prepare(tobytank_fish_cache_t *cache,
                                const tobytank_genome_t *genome)
{
    if (cache == NULL || genome == NULL ||
        cache->sprite.pixels == NULL || cache->sprite.alpha == NULL) {
        return 0;
    }

    const uint64_t fingerprint = tobytank_genome_fingerprint(genome);
    if (cache->valid && cache->identity == genome->identity &&
        cache->fingerprint == fingerprint) {
        return 1;
    }

    if (!tobytank_portrait_render(genome, &cache->sprite)) {
        cache->valid = 0;
        return 0;
    }
    cache->identity = genome->identity;
    cache->fingerprint = fingerprint;
    cache->valid = 1;
    return 1;
}

