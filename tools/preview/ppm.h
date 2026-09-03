#pragma once

#include <stddef.h>

#include "render/canvas.h"

/*
 * Encodes a canvas as a binary PPM (P6). PPM is chosen because it needs no
 * image library on the host: the encoder is a header plus raw RGB triples, so
 * preview output stays dependency-free and diffable.
 *
 * Returns 1 on success and writes the encoded length to written_bytes. Returns
 * 0 if the arguments are invalid or the buffer is too small.
 */
int tobytank_ppm_encode(const tobytank_canvas_t *canvas,
                        unsigned char *out, size_t capacity, size_t *written_bytes);

/* Exact buffer size tobytank_ppm_encode needs for a canvas of this size. */
size_t tobytank_ppm_size(int width, int height);
