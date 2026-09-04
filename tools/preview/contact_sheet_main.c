/*
 * Host contact sheet for generated fish. Writes a PPM image using the same
 * genome and rasterizer code as firmware.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fish/genome.h"
#include "ppm.h"
#include "render/canvas.h"
#include "render/composite.h"
#include "render/fish_cache.h"

#define CELL_WIDTH 184
#define CELL_HEIGHT 136
#define COLUMNS 4
#define ROWS 4

static uint16_t sprite_pixels[TOBYTANK_FISH_MAX_WIDTH * TOBYTANK_FISH_MAX_HEIGHT];
static uint8_t sprite_alpha[TOBYTANK_FISH_MAX_WIDTH * TOBYTANK_FISH_MAX_HEIGHT];

int main(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : "out/fish_contact_sheet.ppm";
    const int width = CELL_WIDTH * COLUMNS;
    const int height = CELL_HEIGHT * ROWS;
    const size_t pixel_count = (size_t)width * (size_t)height;
    uint16_t *pixels = (uint16_t *)malloc(pixel_count * sizeof(uint16_t));
    const size_t ppm_capacity = tobytank_ppm_size(width, height);
    unsigned char *ppm = (unsigned char *)malloc(ppm_capacity);
    if (pixels == NULL || ppm == NULL) {
        fprintf(stderr, "out of memory\n");
        free(pixels);
        free(ppm);
        return 1;
    }

    const tobytank_canvas_t canvas = {
        .pixels = pixels,
        .width = width,
        .height = height,
    };
    tobytank_canvas_clear(&canvas, tobytank_rgb565(3, 12, 18));

    tobytank_fish_cache_t cache;
    tobytank_fish_cache_init(&cache, sprite_pixels, sprite_alpha);

    for (int row = 0; row < ROWS; ++row) {
        for (int col = 0; col < COLUMNS; ++col) {
            const tobytank_identity_t identity = (tobytank_identity_t)(1 + row * COLUMNS + col);
            tobytank_genome_t genome;
            if (!tobytank_genome_generate(&genome, identity) ||
                !tobytank_fish_cache_prepare(&cache, &genome)) {
                fprintf(stderr, "failed to render identity %llu\n",
                        (unsigned long long)identity);
                free(pixels);
                free(ppm);
                return 1;
            }
            const int center_x = col * CELL_WIDTH + CELL_WIDTH / 2;
            const int center_y = row * CELL_HEIGHT + CELL_HEIGHT / 2;
            tobytank_composite_sprite(&canvas, &cache.sprite, center_x, center_y);
        }
    }

    size_t written = 0;
    if (!tobytank_ppm_encode(&canvas, ppm, ppm_capacity, &written)) {
        fprintf(stderr, "failed to encode contact sheet\n");
        free(pixels);
        free(ppm);
        return 1;
    }

    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        fprintf(stderr, "failed to open %s\n", path);
        free(pixels);
        free(ppm);
        return 1;
    }
    const size_t stored = fwrite(ppm, 1, written, file);
    fclose(file);
    free(pixels);
    free(ppm);
    if (stored != written) {
        fprintf(stderr, "short write for %s\n", path);
        return 1;
    }
    printf("wrote %s (%zu bytes)\n", path, written);
    return 0;
}

