/*
 * Host preview for the TobyTank aquarium.
 *
 * Renders the same simulation and rasterizers the firmware uses, at the real
 * panel size, and writes binary PPM frames so the tank can be inspected
 * without flashing a board.
 *
 * Usage: preview [output_directory]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aquarium/environment.h"
#include "aquarium/interactions.h"
#include "aquarium/lifecycle.h"
#include "fish/genome.h"
#include "fish/identity.h"
#include "input/motion_filter.h"
#include "ppm.h"
#include "render/background.h"
#include "render/canvas.h"
#include "render/composite.h"
#include "render/effects.h"
#include "render/fish_cache.h"
#include "render/particles.h"

#define PREVIEW_WIDTH 368
#define PREVIEW_HEIGHT 448
#define PREVIEW_LIFECYCLE_SEED 0x56697369746F7231ULL

static const uint64_t kSeeds[] = {
    0x546F627954616E6BULL,
    0x0000000000000001ULL,
    0x9E3779B97F4A7C15ULL,
    0xDEADBEEFCAFEF00DULL,
};

static const float kMoments[] = {0.0f, 8.0f, 24.0f};

typedef struct {
    tobytank_identity_t next_identity;
} preview_identity_source_t;

static int next_preview_identity(void *context, tobytank_identity_t *out_identity)
{
    preview_identity_source_t *source = (preview_identity_source_t *)context;
    if (source == NULL || out_identity == NULL) {
        return 0;
    }
    *out_identity = source->next_identity++;
    return 1;
}

static int write_frame(const char *path, const tobytank_canvas_t *canvas,
                       unsigned char *buffer, size_t capacity)
{
    size_t written = 0;
    if (!tobytank_ppm_encode(canvas, buffer, capacity, &written)) {
        fprintf(stderr, "failed to encode %s\n", path);
        return 0;
    }

    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        fprintf(stderr, "failed to open %s for writing\n", path);
        return 0;
    }
    const size_t stored = fwrite(buffer, 1, written, file);
    fclose(file);
    if (stored != written) {
        fprintf(stderr, "short write for %s\n", path);
        return 0;
    }
    printf("wrote %s (%zu bytes)\n", path, written);
    return 1;
}

int main(int argc, char **argv)
{
    const char *out_dir = (argc > 1) ? argv[1] : "out";

    const size_t pixel_count = (size_t)PREVIEW_WIDTH * (size_t)PREVIEW_HEIGHT;
    uint16_t *pixels = (uint16_t *)malloc(pixel_count * sizeof(uint16_t));
    const size_t ppm_capacity = tobytank_ppm_size(PREVIEW_WIDTH, PREVIEW_HEIGHT);
    unsigned char *ppm_buffer = (unsigned char *)malloc(ppm_capacity);
    tobytank_environment_t *environment =
        (tobytank_environment_t *)malloc(sizeof(tobytank_environment_t));
    tobytank_env_snapshot_t *snapshot =
        (tobytank_env_snapshot_t *)malloc(sizeof(tobytank_env_snapshot_t));
    tobytank_lifecycle_t *lifecycle =
        (tobytank_lifecycle_t *)malloc(sizeof(tobytank_lifecycle_t));
    tobytank_fish_snapshot_t *fish_snapshot =
        (tobytank_fish_snapshot_t *)malloc(sizeof(tobytank_fish_snapshot_t));
    tobytank_interactions_t *interactions =
        (tobytank_interactions_t *)malloc(sizeof(tobytank_interactions_t));
    uint16_t *fish_pixels = (uint16_t *)malloc(tobytank_fish_cache_pixel_bytes());
    uint8_t *fish_alpha = (uint8_t *)malloc(tobytank_fish_cache_alpha_bytes());

    if (pixels == NULL || ppm_buffer == NULL || environment == NULL || snapshot == NULL ||
        lifecycle == NULL || fish_snapshot == NULL ||
        interactions == NULL ||
        fish_pixels == NULL || fish_alpha == NULL) {
        fprintf(stderr, "out of memory\n");
        free(pixels);
        free(ppm_buffer);
        free(environment);
        free(snapshot);
        free(lifecycle);
        free(fish_snapshot);
        free(interactions);
        free(fish_pixels);
        free(fish_alpha);
        return 1;
    }

    const tobytank_canvas_t canvas = {
        .pixels = pixels,
        .width = PREVIEW_WIDTH,
        .height = PREVIEW_HEIGHT,
    };
    tobytank_fish_cache_t fish_cache;
    tobytank_fish_cache_init(&fish_cache, fish_pixels, fish_alpha);

    int failures = 0;
    const int seed_count = (int)(sizeof(kSeeds) / sizeof(kSeeds[0]));
    const int moment_count = (int)(sizeof(kMoments) / sizeof(kMoments[0]));

    for (int s = 0; s < seed_count; ++s) {
        preview_identity_source_t identity_source = {
            .next_identity = (tobytank_identity_t)(1 + s * 64),
        };
        tobytank_environment_init(environment, kSeeds[s], PREVIEW_WIDTH, PREVIEW_HEIGHT);
        if (!tobytank_lifecycle_init(lifecycle, PREVIEW_LIFECYCLE_SEED ^ kSeeds[s],
                                     PREVIEW_WIDTH, PREVIEW_HEIGHT,
                                     next_preview_identity, &identity_source)) {
            fprintf(stderr, "failed to initialize lifecycle for seed %d\n", s);
            ++failures;
            continue;
        }
        tobytank_interactions_init(interactions, PREVIEW_WIDTH, PREVIEW_HEIGHT);
        tobytank_touch_filter_t touch;
        tobytank_imu_filter_t imu;
        tobytank_touch_filter_init(&touch, PREVIEW_WIDTH, PREVIEW_HEIGHT);
        tobytank_imu_filter_init(&imu);
        float simulated = 0.0f;
        for (int m = 0; m < moment_count; ++m) {
            while (simulated + TOBYTANK_ENV_TIMESTEP_SECONDS <= kMoments[m]) {
                const float next_time = simulated + TOBYTANK_ENV_TIMESTEP_SECONDS;
                const tobytank_touch_sample_t touch_sample = {
                    .pressed = next_time >= 7.72f && next_time <= 8.08f,
                    .x = PREVIEW_WIDTH / 2,
                    .y = PREVIEW_HEIGHT / 3,
                    .strength = 90,
                };
                tobytank_touch_filter_update(&touch, &touch_sample,
                                             TOBYTANK_ENV_TIMESTEP_SECONDS);
                tobytank_imu_filter_update(&imu, &(tobytank_imu_sample_t){0},
                                           TOBYTANK_ENV_TIMESTEP_SECONDS);
                tobytank_interactions_update(interactions, &touch, &imu,
                                             TOBYTANK_ENV_TIMESTEP_SECONDS);
                tobytank_environment_update_with_interactions(
                    environment, TOBYTANK_ENV_TIMESTEP_SECONDS, interactions);
                tobytank_lifecycle_update_with_interactions(
                    lifecycle, TOBYTANK_ENV_TIMESTEP_SECONDS, interactions);
                simulated += TOBYTANK_ENV_TIMESTEP_SECONDS;
            }
            tobytank_environment_snapshot(environment, snapshot);
            tobytank_lifecycle_snapshot(lifecycle, fish_snapshot);

            memset(pixels, 0, pixel_count * sizeof(uint16_t));
            tobytank_background_draw(&canvas, snapshot);
            tobytank_effects_draw(&canvas, snapshot);
            tobytank_effects_draw_interactions(&canvas, snapshot, interactions);
            if (fish_snapshot->has_fish &&
                tobytank_fish_cache_prepare(&fish_cache, &fish_snapshot->genome)) {
                tobytank_composite_sprite_facing(&canvas, &fish_cache.sprite,
                                                 (int)(fish_snapshot->x + 0.5f),
                                                 (int)(fish_snapshot->y + 0.5f),
                                                 fish_snapshot->facing);
            }
            tobytank_particles_draw(&canvas, snapshot);

            char path[512];
            snprintf(path, sizeof(path), "%s/seed%d_t%02d.ppm", out_dir, s,
                     (int)kMoments[m]);
            if (!write_frame(path, &canvas, ppm_buffer, ppm_capacity)) {
                ++failures;
            }
        }
    }

    free(pixels);
    free(ppm_buffer);
    free(environment);
    free(snapshot);
    free(lifecycle);
    free(fish_snapshot);
    free(interactions);
    free(fish_pixels);
    free(fish_alpha);

    if (failures != 0) {
        fprintf(stderr, "%d preview frame(s) failed\n", failures);
        return 1;
    }
    printf("preview complete: %d frames\n", seed_count * moment_count);
    return 0;
}
