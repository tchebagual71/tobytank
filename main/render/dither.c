#include "render/dither.h"

static const int kBayer4[4][4] = {
    {0, 8, 2, 10},
    {12, 4, 14, 6},
    {3, 11, 1, 9},
    {15, 7, 13, 5},
};

static int bias(int x, int y, int step)
{
    const int threshold = kBayer4[y & 3][x & 3];
    return ((threshold - 7) * step) / 16;
}

int tobytank_dither_bias_5bit(int x, int y)
{
    return bias(x, y, 8);
}

int tobytank_dither_bias_6bit(int x, int y)
{
    return bias(x, y, 4);
}

