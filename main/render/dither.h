#pragma once

#include <stdint.h>

/*
 * Tiny ordered-dither helpers shared by procedural rasterizers. The returned
 * value is a signed channel bias in roughly one RGB565 quantization step.
 */
int tobytank_dither_bias_5bit(int x, int y);
int tobytank_dither_bias_6bit(int x, int y);

