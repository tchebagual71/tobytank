#pragma once

#include "fish/genome.h"

/*
 * Genome validation.
 *
 * Generation is free to explore; this decides what is safe to draw and worth
 * looking at. Rejection is deterministic and returns a specific reason, so a
 * rejected fish is retried with the next variant rather than silently patched
 * into range, and so failures can be logged and counted.
 */
typedef enum {
    TOBYTANK_GENOME_OK = 0,
    TOBYTANK_GENOME_REJECT_NULL,
    TOBYTANK_GENOME_REJECT_IDENTITY,
    TOBYTANK_GENOME_REJECT_BODY_SIZE,
    TOBYTANK_GENOME_REJECT_BODY_SHAPE,
    TOBYTANK_GENOME_REJECT_FACE,
    TOBYTANK_GENOME_REJECT_FINS,
    TOBYTANK_GENOME_REJECT_EXTENT,
    TOBYTANK_GENOME_REJECT_CONTRAST,
    TOBYTANK_GENOME_REJECT_PALETTE,
    TOBYTANK_GENOME_REJECT_MOTION,
} tobytank_genome_verdict_t;

tobytank_genome_verdict_t tobytank_genome_validate(const tobytank_genome_t *genome);

/* Stable short name for logs and tests. */
const char *tobytank_genome_verdict_name(tobytank_genome_verdict_t verdict);
