#include "fish/prng.h"

#include <stddef.h>

#define GOLDEN_GAMMA 0x9E3779B97F4A7C15ULL

uint64_t tobytank_splitmix64(uint64_t *state)
{
    uint64_t z = (*state += GOLDEN_GAMMA);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

void tobytank_prng_seed_raw(tobytank_prng_t *rng, uint64_t seed_material)
{
    if (rng == NULL) {
        return;
    }
    uint64_t seeder = seed_material;
    rng->state = tobytank_splitmix64(&seeder);
    if (rng->state == 0) {
        /* xorshift64* is stuck at zero, so never start there. */
        rng->state = GOLDEN_GAMMA;
    }
}

void tobytank_prng_seed(tobytank_prng_t *rng, uint64_t identity,
                        tobytank_prng_stream_t stream, uint32_t variant)
{
    uint64_t material = identity;
    material ^= ((uint64_t)stream << 56) ^ ((uint64_t)stream * 0xD6E8FEB86659FD93ULL);
    material ^= ((uint64_t)variant + 1u) * 0xA24BAED4963EE407ULL;
    tobytank_prng_seed_raw(rng, material);
}

uint64_t tobytank_prng_next_u64(tobytank_prng_t *rng)
{
    if (rng == NULL) {
        return 0;
    }
    uint64_t x = rng->state;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    rng->state = x;
    return x * 0x2545F4914F6CDD1DULL;
}

uint32_t tobytank_prng_next_u32(tobytank_prng_t *rng)
{
    return (uint32_t)(tobytank_prng_next_u64(rng) >> 32);
}

float tobytank_prng_unit(tobytank_prng_t *rng)
{
    /* Top 24 bits give a clean [0,1) float with no rounding to 1.0. */
    return (float)(tobytank_prng_next_u64(rng) >> 40) * (1.0f / 16777216.0f);
}

float tobytank_prng_range(tobytank_prng_t *rng, float low, float high)
{
    if (high <= low) {
        return low;
    }
    return low + (high - low) * tobytank_prng_unit(rng);
}

int tobytank_prng_int(tobytank_prng_t *rng, int low, int high_inclusive)
{
    if (high_inclusive <= low) {
        return low;
    }
    const uint64_t span = (uint64_t)(high_inclusive - low) + 1u;
    return low + (int)(tobytank_prng_next_u64(rng) % span);
}

int tobytank_prng_chance(tobytank_prng_t *rng, float probability)
{
    if (probability <= 0.0f) {
        return 0;
    }
    if (probability >= 1.0f) {
        return 1;
    }
    return tobytank_prng_unit(rng) < probability ? 1 : 0;
}
