#include "ppm.h"

#include <stdio.h>
#include <stdint.h>

static size_t header_length(int width, int height, char *buffer, size_t capacity)
{
    const int written = snprintf(buffer, capacity, "P6\n%d %d\n255\n", width, height);
    if (written < 0) {
        return 0;
    }
    return (size_t)written;
}

size_t tobytank_ppm_size(int width, int height)
{
    if (width <= 0 || height <= 0) {
        return 0;
    }
    char header[32];
    const size_t length = header_length(width, height, header, sizeof(header));
    if (length == 0) {
        return 0;
    }
    return length + (size_t)width * (size_t)height * 3u;
}

int tobytank_ppm_encode(const tobytank_canvas_t *canvas,
                        unsigned char *out, size_t capacity, size_t *written_bytes)
{
    if (canvas == NULL || canvas->pixels == NULL || out == NULL ||
        canvas->width <= 0 || canvas->height <= 0) {
        return 0;
    }

    char header[32];
    const size_t length = header_length(canvas->width, canvas->height, header, sizeof(header));
    const size_t total = tobytank_ppm_size(canvas->width, canvas->height);
    if (length == 0 || total == 0 || capacity < total) {
        return 0;
    }

    for (size_t i = 0; i < length; ++i) {
        out[i] = (unsigned char)header[i];
    }

    size_t offset = length;
    for (int y = 0; y < canvas->height; ++y) {
        for (int x = 0; x < canvas->width; ++x) {
            /* Canvas pixels are stored byte-swapped for the panel. */
            const uint16_t stored = canvas->pixels[y * canvas->width + x];
            const uint16_t native = (uint16_t)((stored >> 8) | (stored << 8));
            const uint8_t five_red = (uint8_t)((native >> 11) & 0x1Fu);
            const uint8_t six_green = (uint8_t)((native >> 5) & 0x3Fu);
            const uint8_t five_blue = (uint8_t)(native & 0x1Fu);
            out[offset++] = (unsigned char)((five_red << 3) | (five_red >> 2));
            out[offset++] = (unsigned char)((six_green << 2) | (six_green >> 4));
            out[offset++] = (unsigned char)((five_blue << 3) | (five_blue >> 2));
        }
    }

    if (written_bytes != NULL) {
        *written_bytes = offset;
    }
    return 1;
}
