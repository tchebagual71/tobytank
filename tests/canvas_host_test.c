#include <stdint.h>
#include <stdio.h>

#include "../main/render/canvas.h"

static int expect_u16(const char *name, uint16_t actual, uint16_t expected)
{
    if (actual != expected) {
        fprintf(stderr, "%s: got 0x%04x expected 0x%04x\n", name, actual, expected);
        return 1;
    }
    return 0;
}

static int expect_int(const char *name, int actual, int expected)
{
    if (actual != expected) {
        fprintf(stderr, "%s: got %d expected %d\n", name, actual, expected);
        return 1;
    }
    return 0;
}

int main(void)
{
    int failures = 0;

    failures += expect_u16("red byte order", tobytank_rgb565(255, 0, 0), 0x00f8);
    failures += expect_u16("green byte order", tobytank_rgb565(0, 255, 0), 0xe007);
    failures += expect_u16("blue byte order", tobytank_rgb565(0, 0, 255), 0x1f00);
    failures += expect_u16("white byte order", tobytank_rgb565(255, 255, 255), 0xffff);

    uint16_t pixels[16] = {0};
    const tobytank_canvas_t canvas = {
        .pixels = pixels,
        .width = 4,
        .height = 4,
    };

    tobytank_canvas_clear(&canvas, 0x1234);
    for (int i = 0; i < 16; ++i) {
        failures += expect_u16("clear", pixels[i], 0x1234);
    }

    tobytank_canvas_pixel(&canvas, -1, 0, 0xffff);
    tobytank_canvas_pixel(&canvas, 4, 0, 0xffff);
    tobytank_canvas_pixel(&canvas, 1, 1, 0xabcd);
    failures += expect_u16("clipped pixel keeps first cell", pixels[0], 0x1234);
    failures += expect_u16("valid pixel", pixels[5], 0xabcd);

    tobytank_canvas_fill_rect(&canvas, -1, -1, 3, 3, 0x5555);
    failures += expect_u16("rect clipped origin", pixels[0], 0x5555);
    failures += expect_u16("rect clipped interior", pixels[5], 0x5555);
    failures += expect_u16("rect outside untouched", pixels[10], 0x1234);

    tobytank_canvas_clear(&canvas, 0x1234);
    tobytank_canvas_frame_rect(&canvas, 0, 0, 4, 4, 1, 0x2222);
    for (int i = 0; i < 16; ++i) {
        const int x = i % 4;
        const int y = i / 4;
        const int is_edge = (x == 0 || x == 3 || y == 0 || y == 3);
        failures += expect_u16(is_edge ? "frame edge" : "frame interior",
                               pixels[i], is_edge ? 0x2222 : 0x1234);
    }

    tobytank_canvas_clear(&canvas, 0x1234);
    tobytank_canvas_frame_rect(&canvas, 0, 0, 4, 4, 2, 0x3333);
    for (int i = 0; i < 16; ++i) {
        failures += expect_u16("frame thicker than half fills", pixels[i], 0x3333);
    }

    tobytank_canvas_clear(&canvas, 0x1234);
    tobytank_canvas_frame_rect(&canvas, -2, -2, 8, 8, 1, 0x4444);
    for (int i = 0; i < 16; ++i) {
        failures += expect_u16("offscreen frame draws nothing visible", pixels[i], 0x1234);
    }
    tobytank_canvas_frame_rect(&canvas, 0, 0, 4, 4, 0, 0x4444);
    failures += expect_u16("zero thickness frame is ignored", pixels[0], 0x1234);

    tobytank_canvas_clear(&canvas, 0);
    tobytank_canvas_line(&canvas, -2, 0, 5, 3, 0x7777);
    int touched = 0;
    for (int i = 0; i < 16; ++i) {
        if (pixels[i] == 0x7777) {
            ++touched;
        }
    }
    failures += expect_int("clipped line touched visible pixels", touched, 4);

    if (failures != 0) {
        fprintf(stderr, "canvas_host_test failed: %d failure(s)\n", failures);
        return 1;
    }

    puts("canvas_host_test passed");
    return 0;
}
