#include "fish/genome_validate.h"

#include <stddef.h>

static int in_range(float value, float low, float high)
{
    return value >= low && value <= high;
}

tobytank_genome_verdict_t tobytank_genome_validate(const tobytank_genome_t *genome)
{
    if (genome == NULL) {
        return TOBYTANK_GENOME_REJECT_NULL;
    }
    if (!tobytank_identity_is_valid(genome->identity)) {
        return TOBYTANK_GENOME_REJECT_IDENTITY;
    }

    if (!in_range(genome->body_length, 40.0f, 110.0f) ||
        !in_range(genome->body_depth, 8.0f, 80.0f) ||
        !in_range(genome->depth_ratio, 0.18f, 0.80f)) {
        return TOBYTANK_GENOME_REJECT_BODY_SIZE;
    }

    /* The outline must stay a closed, positive-area shape at the tail, or the
       rasterizer would be asked to draw a body that pinches through itself. */
    if (!in_range(genome->front_taper, 0.10f, 1.0f) ||
        !in_range(genome->rear_taper, 0.10f, 1.0f) ||
        !in_range(genome->peduncle_depth, 0.12f, 0.55f) ||
        !in_range(genome->back_curve, 0.0f, 0.60f) ||
        !in_range(genome->belly_curve, 0.0f, 0.60f)) {
        return TOBYTANK_GENOME_REJECT_BODY_SHAPE;
    }
    if (genome->peduncle_depth * genome->body_depth < 3.0f) {
        return TOBYTANK_GENOME_REJECT_BODY_SHAPE;
    }

    /* The eye has to sit inside the head, and the head is the tapered end. */
    if (genome->eye_size < 1.5f || genome->eye_size > genome->body_depth * 0.35f) {
        return TOBYTANK_GENOME_REJECT_FACE;
    }
    if (!in_range(genome->eye_offset_x, 0.05f, 0.30f) ||
        !in_range(genome->eye_offset_y, -0.35f, 0.15f)) {
        return TOBYTANK_GENOME_REJECT_FACE;
    }
    if (genome->eye_offset_x * genome->body_length < genome->eye_size) {
        return TOBYTANK_GENOME_REJECT_FACE;
    }
    if (!in_range(genome->gill_offset, 0.18f, 0.45f) ||
        genome->gill_offset <= genome->eye_offset_x) {
        return TOBYTANK_GENOME_REJECT_FACE;
    }

    if (genome->caudal_type >= TOBYTANK_CAUDAL_COUNT ||
        genome->pattern_type >= TOBYTANK_PATTERN_COUNT) {
        return TOBYTANK_GENOME_REJECT_FINS;
    }
    if (genome->fin_rays < 3 || genome->fin_rays > 16) {
        return TOBYTANK_GENOME_REJECT_FINS;
    }
    if (!in_range(genome->membrane_softness, 0.0f, 1.0f) ||
        !in_range(genome->dorsal_position, 0.20f, 0.70f)) {
        return TOBYTANK_GENOME_REJECT_FINS;
    }
    /* Fins are attached to a body, so they cannot be longer than the body they
       hang from, and the dorsal has to fit between its root and the tail. */
    if (genome->dorsal_length > genome->body_length * 0.60f ||
        genome->anal_length > genome->body_length * 0.45f ||
        genome->dorsal_position * genome->body_length + genome->dorsal_length >
            genome->body_length) {
        return TOBYTANK_GENOME_REJECT_FINS;
    }
    if (genome->caudal_length < 6.0f || genome->caudal_span < 4.0f) {
        return TOBYTANK_GENOME_REJECT_FINS;
    }

    float width = 0.0f;
    float height = 0.0f;
    tobytank_genome_extent(genome, &width, &height);
    if (width > (float)TOBYTANK_FISH_MAX_WIDTH || height > (float)TOBYTANK_FISH_MAX_HEIGHT ||
        width <= 0.0f || height <= 0.0f) {
        return TOBYTANK_GENOME_REJECT_EXTENT;
    }

    if (!in_range(genome->base_hue, 0.0f, 1.0f) ||
        !in_range(genome->base_saturation, 0.15f, 1.0f) ||
        !in_range(genome->base_value, 0.35f, 0.90f) ||
        !in_range(genome->accent_saturation, 0.0f, 1.0f) ||
        !in_range(genome->iridescence, 0.0f, 1.0f) ||
        !in_range(genome->scale_contrast, 0.0f, 0.8f)) {
        return TOBYTANK_GENOME_REJECT_PALETTE;
    }

    /* Readability, not taste: the back, belly, and markings have to separate
       from the body colour or the fish becomes a silhouette in dark water. */
    const float back = genome->base_value - genome->back_darkness;
    const float belly = genome->base_value + genome->belly_lightness;
    if (back < 0.12f || belly > 1.0f || (belly - back) < 0.18f) {
        return TOBYTANK_GENOME_REJECT_CONTRAST;
    }
    if (genome->pattern_type != TOBYTANK_PATTERN_NONE) {
        if (genome->pattern_contrast < 0.15f ||
            !in_range(genome->pattern_density, 0.05f, 0.95f) ||
            !in_range(genome->pattern_scale, 0.03f, 0.45f)) {
            return TOBYTANK_GENOME_REJECT_CONTRAST;
        }
    }

    /* Animation ranges the simulation and rasterizer are built for. */
    if (!in_range(genome->swim_cadence, 0.5f, 4.5f) ||
        !in_range(genome->preferred_speed, 5.0f, 70.0f) ||
        !in_range(genome->tail_beat_amplitude, 0.05f, 0.45f) ||
        !in_range(genome->body_undulation, 0.0f, 0.55f) ||
        !in_range(genome->turn_response, 0.05f, 1.0f) ||
        !in_range(genome->curiosity, 0.0f, 1.0f) ||
        !in_range(genome->boldness, 0.0f, 1.0f) ||
        !in_range(genome->depth_preference, 0.0f, 1.0f) ||
        !in_range(genome->hover_tendency, 0.0f, 1.0f)) {
        return TOBYTANK_GENOME_REJECT_MOTION;
    }

    return TOBYTANK_GENOME_OK;
}

const char *tobytank_genome_verdict_name(tobytank_genome_verdict_t verdict)
{
    switch (verdict) {
    case TOBYTANK_GENOME_OK: return "ok";
    case TOBYTANK_GENOME_REJECT_NULL: return "null";
    case TOBYTANK_GENOME_REJECT_IDENTITY: return "identity";
    case TOBYTANK_GENOME_REJECT_BODY_SIZE: return "body_size";
    case TOBYTANK_GENOME_REJECT_BODY_SHAPE: return "body_shape";
    case TOBYTANK_GENOME_REJECT_FACE: return "face";
    case TOBYTANK_GENOME_REJECT_FINS: return "fins";
    case TOBYTANK_GENOME_REJECT_EXTENT: return "extent";
    case TOBYTANK_GENOME_REJECT_CONTRAST: return "contrast";
    case TOBYTANK_GENOME_REJECT_PALETTE: return "palette";
    case TOBYTANK_GENOME_REJECT_MOTION: return "motion";
    default: return "unknown";
    }
}
