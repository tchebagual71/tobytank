#include "fish/genome.h"

#include <string.h>

#include "fish/genome_validate.h"
#include "fish/prng.h"

static float clampf(float value, float low, float high)
{
    if (value < low) {
        return low;
    }
    if (value > high) {
        return high;
    }
    return value;
}

static float wrap_turn(float hue)
{
    while (hue < 0.0f) {
        hue += 1.0f;
    }
    while (hue >= 1.0f) {
        hue -= 1.0f;
    }
    return hue;
}

static void generate_anatomy(tobytank_genome_t *genome, tobytank_identity_t identity,
                             uint32_t variant)
{
    tobytank_prng_t rng;
    tobytank_prng_seed(&rng, identity, TOBYTANK_PRNG_STREAM_ANATOMY, variant);

    /* One shape driver first; everything else is expressed relative to it, so
       the animal stays coherent instead of being a bag of unrelated numbers. */
    genome->depth_ratio = tobytank_prng_range(&rng, 0.22f, 0.72f);
    const float deep = (genome->depth_ratio - 0.22f) / 0.50f; /* 0 slim .. 1 deep */

    /* Deep-bodied fish are also shorter, which keeps the silhouette inside the
       sprite box and matches how real deep-bodied fish are built. */
    genome->body_length = tobytank_prng_range(&rng, 46.0f, 96.0f - 26.0f * deep);
    genome->body_depth = genome->body_length * genome->depth_ratio;

    genome->front_taper = tobytank_prng_range(&rng, 0.30f, 0.95f - 0.25f * deep);
    genome->rear_taper = tobytank_prng_range(&rng, 0.35f, 0.90f);
    genome->back_curve = tobytank_prng_range(&rng, 0.10f, 0.24f + 0.22f * deep);
    genome->belly_curve = tobytank_prng_range(&rng, 0.08f, 0.20f + 0.26f * deep);
    /* The tail base must stay thick enough to draw, however slim the body. */
    const float peduncle_floor = 3.2f / genome->body_depth;
    genome->peduncle_depth = tobytank_prng_range(&rng,
                                                 peduncle_floor > 0.18f ? peduncle_floor : 0.18f,
                                                 0.44f - 0.06f * deep);

    genome->eye_size = clampf(genome->body_depth * tobytank_prng_range(&rng, 0.13f, 0.24f),
                              1.6f, genome->body_depth * 0.34f);
    /* The eye has to sit clear of the snout, so its position depends on how big
       it turned out rather than being drawn independently. */
    const float eye_floor = (genome->eye_size * 1.15f) / genome->body_length;
    genome->eye_offset_x = tobytank_prng_range(&rng,
                                               eye_floor > 0.10f ? eye_floor : 0.10f, 0.26f);
    genome->eye_offset_y = tobytank_prng_range(&rng, -0.20f, -0.02f);
    genome->mouth_size = genome->body_length * tobytank_prng_range(&rng, 0.04f, 0.11f);
    genome->mouth_angle = tobytank_prng_range(&rng, -0.45f, 0.35f);
    /* Gills sit behind the eye, so the gill line follows from where the eye
       landed rather than being drawn on its own. */
    genome->gill_offset = tobytank_prng_range(&rng, genome->eye_offset_x + 0.06f, 0.42f);
    genome->gill_curve = tobytank_prng_range(&rng, 0.2f, 0.8f);

    /* Slim bodies favour the tails that suit fast swimmers. */
    const float lunate_weight = 1.0f - deep;
    if (tobytank_prng_unit(&rng) < 0.55f * lunate_weight) {
        genome->caudal_type = (uint8_t)(tobytank_prng_chance(&rng, 0.5f)
                                            ? TOBYTANK_CAUDAL_LUNATE
                                            : TOBYTANK_CAUDAL_FORKED);
    } else {
        const int choice = tobytank_prng_int(&rng, 0, 2);
        static const uint8_t kSlowTails[3] = {
            TOBYTANK_CAUDAL_ROUNDED,
            TOBYTANK_CAUDAL_TRUNCATE,
            TOBYTANK_CAUDAL_LANCEOLATE,
        };
        genome->caudal_type = kSlowTails[choice];
    }

    genome->caudal_length = genome->body_length * tobytank_prng_range(&rng, 0.20f, 0.40f);
    genome->caudal_span = genome->body_depth * tobytank_prng_range(&rng, 0.70f, 1.35f);

    genome->dorsal_position = tobytank_prng_range(&rng, 0.30f, 0.58f);
    /* The dorsal fin has to end before the tail does. */
    const float dorsal_room = 0.92f - genome->dorsal_position;
    genome->dorsal_length = genome->body_length *
                            tobytank_prng_range(&rng, 0.18f,
                                                dorsal_room < 0.46f ? dorsal_room : 0.46f);
    genome->dorsal_height = genome->body_depth *
                            tobytank_prng_range(&rng, 0.18f, 0.42f + 0.15f * deep);
    genome->anal_length = genome->body_length * tobytank_prng_range(&rng, 0.12f, 0.30f);
    genome->anal_height = genome->body_depth * tobytank_prng_range(&rng, 0.12f, 0.32f);
    genome->pelvic_size = genome->body_depth * tobytank_prng_range(&rng, 0.10f, 0.26f);
    genome->pectoral_size = genome->body_depth * tobytank_prng_range(&rng, 0.16f, 0.38f);
    genome->pectoral_angle = tobytank_prng_range(&rng, -0.5f, 0.6f);
    genome->fin_rays = (uint8_t)tobytank_prng_int(&rng, 4, 12);
    genome->membrane_softness = tobytank_prng_unit(&rng);
}

static void generate_palette(tobytank_genome_t *genome, tobytank_identity_t identity,
                             uint32_t variant)
{
    tobytank_prng_t rng;
    tobytank_prng_seed(&rng, identity, TOBYTANK_PRNG_STREAM_COLOR, variant);

    genome->base_hue = tobytank_prng_unit(&rng);
    genome->base_saturation = tobytank_prng_range(&rng, 0.25f, 0.95f);
    /* Kept clear of both ends so the fish reads against dark water without
       becoming a bright patch on an AMOLED. */
    genome->base_value = tobytank_prng_range(&rng, 0.42f, 0.86f);

    /* The lit belly and shaded back are what make the fish read against dark
       water, so both are bounded by the base value they modify. */
    const float belly_room = 1.0f - genome->base_value;
    genome->belly_lightness = tobytank_prng_range(&rng, 0.10f,
                                                  belly_room < 0.32f ? belly_room : 0.32f);
    const float back_room = genome->base_value - 0.14f;
    genome->back_darkness = tobytank_prng_range(&rng, 0.10f,
                                                back_room < 0.30f ? back_room : 0.30f);

    /* Accents stay analogous or complementary rather than arbitrary, which is
       what keeps two-colour fish looking designed. */
    if (tobytank_prng_chance(&rng, 0.62f)) {
        genome->accent_hue_offset = tobytank_prng_range(&rng, 0.04f, 0.14f) *
                                    (tobytank_prng_chance(&rng, 0.5f) ? 1.0f : -1.0f);
    } else {
        genome->accent_hue_offset = tobytank_prng_range(&rng, 0.42f, 0.58f);
    }
    genome->accent_saturation = clampf(genome->base_saturation +
                                           tobytank_prng_range(&rng, -0.2f, 0.3f),
                                       0.2f, 1.0f);
    genome->fin_hue_offset = tobytank_prng_range(&rng, -0.08f, 0.08f);
    genome->iridescence = tobytank_prng_unit(&rng);
    genome->scale_contrast = clampf(tobytank_prng_range(&rng, 0.05f, 0.55f) +
                                        0.2f * genome->iridescence,
                                    0.0f, 0.7f);
}

static void generate_markings(tobytank_genome_t *genome, tobytank_identity_t identity,
                              uint32_t variant)
{
    tobytank_prng_t rng;
    tobytank_prng_seed(&rng, identity, TOBYTANK_PRNG_STREAM_PATTERN, variant);

    genome->pattern_type = (uint8_t)tobytank_prng_int(&rng, 0, TOBYTANK_PATTERN_COUNT - 1);
    genome->pattern_density = tobytank_prng_range(&rng, 0.15f, 0.85f);
    genome->pattern_scale = tobytank_prng_range(&rng, 0.06f, 0.34f);
    /* Patternless fish still need a defined contrast; validation checks the
       pair, so keep the floor above zero for patterned ones. */
    genome->pattern_contrast = (genome->pattern_type == TOBYTANK_PATTERN_NONE)
                                   ? 0.0f
                                   : tobytank_prng_range(&rng, 0.18f, 0.75f);
    genome->accent_marks = (uint8_t)tobytank_prng_int(&rng, 0, 4);
    genome->accent_mark_size = genome->body_depth * tobytank_prng_range(&rng, 0.06f, 0.20f);
}

static void generate_behaviour(tobytank_genome_t *genome, tobytank_identity_t identity,
                               uint32_t variant)
{
    tobytank_prng_t rng;
    tobytank_prng_seed(&rng, identity, TOBYTANK_PRNG_STREAM_BEHAVIOR, variant);

    const float deep = clampf((genome->depth_ratio - 0.22f) / 0.50f, 0.0f, 1.0f);
    const float slim = 1.0f - deep;

    /* Body shape sets the temperament envelope; the stream picks within it. */
    genome->preferred_speed = tobytank_prng_range(&rng, 8.0f + 14.0f * slim,
                                                  26.0f + 30.0f * slim);
    genome->swim_cadence = tobytank_prng_range(&rng, 0.8f + 0.7f * slim,
                                               2.0f + 1.8f * slim);
    genome->turn_response = clampf(tobytank_prng_range(&rng, 0.15f, 0.65f) + 0.3f * slim,
                                   0.05f, 1.0f);
    genome->hover_tendency = clampf(tobytank_prng_range(&rng, 0.05f, 0.55f) + 0.35f * deep,
                                    0.0f, 1.0f);
    genome->curiosity = tobytank_prng_unit(&rng);
    genome->boldness = clampf(0.25f * genome->curiosity + tobytank_prng_range(&rng, 0.0f, 0.75f),
                              0.0f, 1.0f);
    genome->depth_preference = tobytank_prng_unit(&rng);

    tobytank_prng_t motion;
    tobytank_prng_seed(&motion, identity, TOBYTANK_PRNG_STREAM_MOTION, variant);
    genome->tail_beat_amplitude = tobytank_prng_range(&motion, 0.10f, 0.34f);
    genome->body_undulation = clampf(tobytank_prng_range(&motion, 0.05f, 0.30f) + 0.15f * slim,
                                     0.0f, 0.5f);

    tobytank_prng_t sound;
    tobytank_prng_seed(&sound, identity, TOBYTANK_PRNG_STREAM_SOUND, variant);
    /* Bigger fish sound lower; the audio milestone consumes this. */
    genome->sound_pitch = tobytank_prng_range(&sound, 180.0f, 900.0f) *
                          (1.0f - 0.4f * (genome->body_length - 46.0f) / 50.0f);
    genome->sound_timbre = tobytank_prng_unit(&sound);
}

void tobytank_genome_generate_variant(tobytank_genome_t *genome,
                                      tobytank_identity_t identity, uint32_t variant)
{
    if (genome == NULL) {
        return;
    }

    memset(genome, 0, sizeof(*genome));
    genome->identity = identity;
    genome->variant = (uint8_t)variant;

    generate_anatomy(genome, identity, variant);
    generate_palette(genome, identity, variant);
    generate_markings(genome, identity, variant);
    generate_behaviour(genome, identity, variant);

    genome->base_hue = wrap_turn(genome->base_hue);
}

int tobytank_genome_generate(tobytank_genome_t *genome, tobytank_identity_t identity)
{
    if (genome == NULL) {
        return 0;
    }
    if (!tobytank_identity_is_valid(identity)) {
        memset(genome, 0, sizeof(*genome));
        return 0;
    }

    for (uint32_t variant = 0; variant < TOBYTANK_GENOME_MAX_ATTEMPTS; ++variant) {
        tobytank_genome_generate_variant(genome, identity, variant);
        if (tobytank_genome_validate(genome) == TOBYTANK_GENOME_OK) {
            return 1;
        }
    }

    /* Every attempt was rejected. Report it rather than drawing a broken fish;
       the caller is expected to skip this identity and log the failure. */
    memset(genome, 0, sizeof(*genome));
    return 0;
}

void tobytank_genome_extent(const tobytank_genome_t *genome, float *width, float *height)
{
    if (genome == NULL) {
        return;
    }

    const float body_half = genome->body_depth * 0.5f;
    const float above = body_half * (1.0f + genome->back_curve) + genome->dorsal_height;
    const float below = body_half * (1.0f + genome->belly_curve) +
                        (genome->anal_height > genome->pelvic_size ? genome->anal_height
                                                                   : genome->pelvic_size);
    const float span_half = genome->caudal_span * 0.5f;

    if (width != NULL) {
        *width = genome->body_length + genome->caudal_length + genome->mouth_size;
    }
    if (height != NULL) {
        const float tall = above > span_half ? above : span_half;
        const float deep = below > span_half ? below : span_half;
        *height = tall + deep;
    }
}

static void mix(uint64_t *accumulator, uint64_t value)
{
    *accumulator ^= value + 0x9E3779B97F4A7C15ULL + (*accumulator << 6) + (*accumulator >> 2);
}

static void mix_float(uint64_t *accumulator, float value)
{
    /* Quantized so that a bit of floating-point noise cannot change a
       fingerprint, while any perceptible trait change does. */
    mix(accumulator, (uint64_t)(int64_t)(value * 4096.0f));
}

uint64_t tobytank_genome_fingerprint(const tobytank_genome_t *genome)
{
    if (genome == NULL) {
        return 0;
    }

    uint64_t hash = 0xCBF29CE484222325ULL;
    mix(&hash, genome->identity);
    mix(&hash, genome->variant);

    mix_float(&hash, genome->body_length);
    mix_float(&hash, genome->body_depth);
    mix_float(&hash, genome->depth_ratio);
    mix_float(&hash, genome->front_taper);
    mix_float(&hash, genome->rear_taper);
    mix_float(&hash, genome->back_curve);
    mix_float(&hash, genome->belly_curve);
    mix_float(&hash, genome->peduncle_depth);

    mix_float(&hash, genome->eye_size);
    mix_float(&hash, genome->eye_offset_x);
    mix_float(&hash, genome->eye_offset_y);
    mix_float(&hash, genome->mouth_size);
    mix_float(&hash, genome->mouth_angle);
    mix_float(&hash, genome->gill_offset);
    mix_float(&hash, genome->gill_curve);

    mix(&hash, genome->caudal_type);
    mix_float(&hash, genome->caudal_span);
    mix_float(&hash, genome->caudal_length);
    mix_float(&hash, genome->dorsal_position);
    mix_float(&hash, genome->dorsal_length);
    mix_float(&hash, genome->dorsal_height);
    mix_float(&hash, genome->anal_length);
    mix_float(&hash, genome->anal_height);
    mix_float(&hash, genome->pelvic_size);
    mix_float(&hash, genome->pectoral_size);
    mix_float(&hash, genome->pectoral_angle);
    mix(&hash, genome->fin_rays);
    mix_float(&hash, genome->membrane_softness);

    mix_float(&hash, genome->base_hue);
    mix_float(&hash, genome->base_saturation);
    mix_float(&hash, genome->base_value);
    mix_float(&hash, genome->belly_lightness);
    mix_float(&hash, genome->back_darkness);
    mix_float(&hash, genome->accent_hue_offset);
    mix_float(&hash, genome->accent_saturation);
    mix_float(&hash, genome->fin_hue_offset);
    mix_float(&hash, genome->iridescence);
    mix_float(&hash, genome->scale_contrast);

    mix(&hash, genome->pattern_type);
    mix_float(&hash, genome->pattern_density);
    mix_float(&hash, genome->pattern_scale);
    mix_float(&hash, genome->pattern_contrast);
    mix(&hash, genome->accent_marks);
    mix_float(&hash, genome->accent_mark_size);

    mix_float(&hash, genome->swim_cadence);
    mix_float(&hash, genome->preferred_speed);
    mix_float(&hash, genome->turn_response);
    mix_float(&hash, genome->curiosity);
    mix_float(&hash, genome->boldness);
    mix_float(&hash, genome->depth_preference);
    mix_float(&hash, genome->hover_tendency);
    mix_float(&hash, genome->tail_beat_amplitude);
    mix_float(&hash, genome->body_undulation);

    mix_float(&hash, genome->sound_pitch);
    mix_float(&hash, genome->sound_timbre);

    uint64_t final_state = hash;
    return tobytank_splitmix64(&final_state);
}
