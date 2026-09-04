#pragma once

#include <stdint.h>

/*
 * The one deterministic random source in TobyTank.
 *
 * Pure C, no hardware or ESP-IDF dependency, so the host tests and the preview
 * tool use the same generator the firmware does. Nothing here ever reads the
 * clock or the system entropy pool: every stream is a pure function of the seed
 * material it was given, which is what makes a fish reproducible from its
 * identity alone.
 */

typedef struct {
    uint64_t state;
} tobytank_prng_t;

/*
 * Independent streams derived from the same identity. Splitting them means
 * adding a colour trait later cannot shift the anatomy of every fish already
 * recorded in the Fishbook.
 */
typedef enum {
    TOBYTANK_PRNG_STREAM_ANATOMY = 1,
    TOBYTANK_PRNG_STREAM_COLOR = 2,
    TOBYTANK_PRNG_STREAM_PATTERN = 3,
    TOBYTANK_PRNG_STREAM_BEHAVIOR = 4,
    TOBYTANK_PRNG_STREAM_MOTION = 5,
    TOBYTANK_PRNG_STREAM_SOUND = 6,
    TOBYTANK_PRNG_STREAM_ENVIRONMENT = 7,
} tobytank_prng_stream_t;

/* Mixing step used for seeding; exposed because fingerprints use it too. */
uint64_t tobytank_splitmix64(uint64_t *state);

/* Seeds directly from arbitrary material. Used where a caller owns its own
   namespace, such as the aquarium environment. */
void tobytank_prng_seed_raw(tobytank_prng_t *rng, uint64_t seed_material);

/*
 * Seeds one stream for one identity. `variant` separates regeneration attempts
 * so a rejected genome is retried deterministically rather than randomly.
 */
void tobytank_prng_seed(tobytank_prng_t *rng, uint64_t identity,
                        tobytank_prng_stream_t stream, uint32_t variant);

uint64_t tobytank_prng_next_u64(tobytank_prng_t *rng);
uint32_t tobytank_prng_next_u32(tobytank_prng_t *rng);

/* Uniform in [0, 1). */
float tobytank_prng_unit(tobytank_prng_t *rng);

/* Uniform in [low, high). Returns low when high <= low. */
float tobytank_prng_range(tobytank_prng_t *rng, float low, float high);

/* Uniform in [low, high_inclusive]. Returns low when the span is empty. */
int tobytank_prng_int(tobytank_prng_t *rng, int low, int high_inclusive);

/* True with the given probability, clamped to [0, 1]. */
int tobytank_prng_chance(tobytank_prng_t *rng, float probability);
