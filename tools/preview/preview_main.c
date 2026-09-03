/*
 * Host preview for the TobyTank aquarium.
 *
 * Renders the same simulation and rasterizers the firmware uses, at the real
 * panel size, and writes binary PPM frames so the empty tank can be inspected
 * without flashing a board.
 *
 * Usage: preview [output_directory]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aquarium/environment.h"
#include "ppm.h"
#include "render/background.h"
#include "render/canvas.h"
#include "render/effects.h"
#include "render/particles.h"

#define PREVIEW_WIDTH 368
#define PREVIEW_HEIGHT 448

static const uint64_t kSeeds[] = {
    0x546F627954616E6BULL,
    0x0000000000000001ULL,
    0x9E3779B97F4A7C15ULL,
    0xDEADBEEFCAFEF00DULL,
};

static const float kMoments[] = {0.0f, 4.0f, 12.0f};

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

    if (pixels == NULL || ppm_buffer == NULL || environment == NULL || snapshot == NULL) {
        fprintf(stderr, "out of memory\n");
        free(pixels);
        free(ppm_buffer);
        free(environment);
        free(snapshot);
        return 1;
    }

    const tobytank_canvas_t canvas = {
        .pixels = pixels,
        .width = PREVIEW_WIDTH,
        .height = PREVIEW_HEIGHT,
    };

    int failures = 0;
    const int seed_count = (int)(sizeof(kSeeds) / sizeof(kSeeds[0]));
    const int moment_count = (int)(sizeof(kMoments) / sizeof(kMoments[0]));

    for (int s = 0; s < seed_count; ++s) {
        tobytank_environment_init(environment, kSeeds[s], PREVIEW_WIDTH, PREVIEW_HEIGHT);
        float simulated = 0.0f;
        for (int m = 0; m < moment_count; ++m) {
            while (simulated + TOBYTANK_ENV_TIMESTEP_SECONDS <= kMoments[m]) {
                tobytank_environment_update(environment, TOBYTANK_ENV_TIMESTEP_SECONDS);
                simulated += TOBYTANK_ENV_TIMESTEP_SECONDS;
            }
            tobytank_environment_snapshot(environment, snapshot);

            memset(pixels, 0, pixel_count * sizeof(uint16_t));
            tobytank_background_draw(&canvas, snapshot);
            tobytank_effects_draw(&canvas, snapshot);
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

    if (failures != 0) {
        fprintf(stderr, "%d preview frame(s) failed\n", failures);
        return 1;
    }
    printf("preview complete: %d frames\n", seed_count * moment_count);
    return 0;
}
