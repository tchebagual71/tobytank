#pragma once

#include <stdint.h>

#include "fish/identity.h"

/*
 * The procedural genome of one visitor.
 *
 * A genome is a pure function of a 64-bit identity: the same identity always
 * produces the same fish, and no premade sprites, palettes, or species tables
 * exist anywhere in the firmware. Traits are generated in correlated groups so
 * that a fish reads as one coherent animal rather than as independent noise;
 * for example a deep-bodied fish is slower and hovers more, while an elongated
 * one is faster and carries a forked or lunate tail.
 *
 * Pure C with no hardware dependency so host tests can generate and check
 * thousands of genomes.
 */

/* The rasterizer in a later milestone must fit any accepted fish in this box. */
#define TOBYTANK_FISH_MAX_WIDTH 168
#define TOBYTANK_FISH_MAX_HEIGHT 128

/* Bounded regeneration: a rejected genome is retried with the next variant. */
#define TOBYTANK_GENOME_MAX_ATTEMPTS 8

typedef enum {
    TOBYTANK_CAUDAL_ROUNDED = 0,
    TOBYTANK_CAUDAL_TRUNCATE,
    TOBYTANK_CAUDAL_FORKED,
    TOBYTANK_CAUDAL_LUNATE,
    TOBYTANK_CAUDAL_LANCEOLATE,
    TOBYTANK_CAUDAL_COUNT,
} tobytank_caudal_t;

typedef enum {
    TOBYTANK_PATTERN_NONE = 0,
    TOBYTANK_PATTERN_VERTICAL_BARS,
    TOBYTANK_PATTERN_HORIZONTAL_STRIPES,
    TOBYTANK_PATTERN_SPOTS,
    TOBYTANK_PATTERN_BLOTCHES,
    TOBYTANK_PATTERN_RETICULATED,
    TOBYTANK_PATTERN_SADDLE,
    TOBYTANK_PATTERN_COUNT,
} tobytank_pattern_t;

typedef struct {
    /* Silhouette, in pixels unless noted. */
    float body_length;
    float body_depth;
    float depth_ratio;      /* body_depth / body_length, the shape driver */
    float front_taper;      /* 0 blunt .. 1 pointed snout */
    float rear_taper;       /* 0 blunt .. 1 drawn-out peduncle */
    float back_curve;       /* dorsal profile bulge, fraction of depth */
    float belly_curve;      /* ventral profile bulge, fraction of depth */
    float peduncle_depth;   /* fraction of body_depth at the tail base */

    /* Face. */
    float eye_size;
    float eye_offset_x;     /* fraction of body_length from the snout */
    float eye_offset_y;     /* fraction of body_depth from the midline */
    float mouth_size;
    float mouth_angle;      /* radians, negative is upturned */
    float gill_offset;      /* fraction of body_length from the snout */
    float gill_curve;

    /* Fins. */
    uint8_t caudal_type;    /* tobytank_caudal_t */
    float caudal_span;      /* vertical reach */
    float caudal_length;
    float dorsal_position;  /* fraction of body_length */
    float dorsal_length;
    float dorsal_height;
    float anal_length;
    float anal_height;
    float pelvic_size;
    float pectoral_size;
    float pectoral_angle;   /* radians */
    uint8_t fin_rays;
    float membrane_softness; /* 0 stiff .. 1 flowing */

    /* Palette. Hues are turns in [0,1); saturation and value are [0,1]. */
    float base_hue;
    float base_saturation;
    float base_value;
    float belly_lightness;  /* added to base_value on the underside */
    float back_darkness;    /* subtracted from base_value on the back */
    float accent_hue_offset;
    float accent_saturation;
    float fin_hue_offset;
    float iridescence;
    float scale_contrast;

    /* Markings. */
    uint8_t pattern_type;   /* tobytank_pattern_t */
    float pattern_density;
    float pattern_scale;
    float pattern_contrast;
    uint8_t accent_marks;
    float accent_mark_size;

    /* Behaviour and motion. */
    float swim_cadence;      /* tail beats per second */
    float preferred_speed;   /* pixels per second */
    float turn_response;     /* 0 ponderous .. 1 darting */
    float curiosity;
    float boldness;
    float depth_preference;  /* 0 surface .. 1 substrate */
    float hover_tendency;
    float tail_beat_amplitude;
    float body_undulation;

    /* Reserved for the audio milestone; generated now so the identity fixes it. */
    float sound_pitch;
    float sound_timbre;

    /* Provenance. */
    tobytank_identity_t identity;
    uint8_t variant;         /* which regeneration attempt was accepted */
} tobytank_genome_t;

/*
 * Generates the genome for an identity, retrying deterministically with the
 * next variant if validation rejects it. Returns 1 on success. Returns 0, and
 * leaves the genome zeroed, if the identity is invalid or every attempt was
 * rejected; callers must handle that rather than shipping a broken fish.
 */
int tobytank_genome_generate(tobytank_genome_t *genome, tobytank_identity_t identity);

/* Generates one specific attempt without validating it. Used by tests. */
void tobytank_genome_generate_variant(tobytank_genome_t *genome,
                                      tobytank_identity_t identity, uint32_t variant);

/*
 * A 64-bit digest over every trait, not over the struct's bytes, so padding
 * cannot make two different fish look identical or one fish look unstable.
 */
uint64_t tobytank_genome_fingerprint(const tobytank_genome_t *genome);

/* Widest and tallest the fish can draw, including fins. */
void tobytank_genome_extent(const tobytank_genome_t *genome, float *width, float *height);
