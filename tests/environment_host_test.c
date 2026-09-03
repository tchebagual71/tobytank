#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "aquarium/environment.h"
#include "ppm.h"
#include "render/background.h"
#include "render/canvas.h"
#include "render/effects.h"
#include "render/particles.h"

#define TEST_WIDTH 96
#define TEST_HEIGHT 128
#define GUARD 64
#define GUARD_VALUE 0xA5A5u

static int failures;

static void fail(const char *message)
{
    fprintf(stderr, "environment_host_test: %s\n", message);
    ++failures;
}

static void expect(int condition, const char *message)
{
    if (!condition) {
        fail(message);
    }
}

static tobytank_environment_t environment_a;
static tobytank_environment_t environment_b;
static tobytank_env_snapshot_t snapshot_a;
static tobytank_env_snapshot_t snapshot_b;
static uint16_t guarded[GUARD + TEST_WIDTH * TEST_HEIGHT + GUARD];

static void advance(tobytank_environment_t *environment, int steps)
{
    for (int i = 0; i < steps; ++i) {
        tobytank_environment_update(environment, TOBYTANK_ENV_TIMESTEP_SECONDS);
    }
}

static void test_generation_is_deterministic(void)
{
    tobytank_environment_init(&environment_a, 0x1234ABCDULL, TEST_WIDTH, TEST_HEIGHT);
    tobytank_environment_init(&environment_b, 0x1234ABCDULL, TEST_WIDTH, TEST_HEIGHT);
    tobytank_environment_snapshot(&environment_a, &snapshot_a);
    tobytank_environment_snapshot(&environment_b, &snapshot_b);
    expect(memcmp(&snapshot_a, &snapshot_b, sizeof(snapshot_a)) == 0,
           "same seed produced different scenes");

    advance(&environment_a, 900);
    advance(&environment_b, 900);
    tobytank_environment_snapshot(&environment_a, &snapshot_a);
    tobytank_environment_snapshot(&environment_b, &snapshot_b);
    expect(memcmp(&snapshot_a, &snapshot_b, sizeof(snapshot_a)) == 0,
           "same seed diverged after 900 steps");

    tobytank_environment_init(&environment_b, 0x1234ABCEULL, TEST_WIDTH, TEST_HEIGHT);
    tobytank_environment_snapshot(&environment_b, &snapshot_b);
    expect(memcmp(&snapshot_a, &snapshot_b, sizeof(snapshot_a)) != 0,
           "different seeds produced the same scene");
}

static void test_generated_parameters_stay_in_range(void)
{
    for (uint64_t seed = 0; seed < 40; ++seed) {
        tobytank_environment_init(&environment_a, seed * 0x9E3779B9ULL, TEST_WIDTH, TEST_HEIGHT);
        tobytank_environment_snapshot(&environment_a, &snapshot_a);

        expect(snapshot_a.width == TEST_WIDTH && snapshot_a.height == TEST_HEIGHT,
               "snapshot size does not match the requested size");
        expect(snapshot_a.plant_count >= 4 && snapshot_a.plant_count <= TOBYTANK_ENV_MAX_PLANTS,
               "plant count out of range");
        expect(snapshot_a.stone_count >= 3 && snapshot_a.stone_count <= TOBYTANK_ENV_MAX_STONES,
               "stone count out of range");
        expect(snapshot_a.shaft_count >= 2 && snapshot_a.shaft_count <= TOBYTANK_ENV_MAX_SHAFTS,
               "shaft count out of range");
        expect(snapshot_a.bubble_count == TOBYTANK_ENV_MAX_BUBBLES,
               "bubble count is not the full pool");
        expect(snapshot_a.mote_count == TOBYTANK_ENV_MAX_MOTES,
               "mote count is not the full pool");

        for (int i = 0; i < TOBYTANK_ENV_SUBSTRATE_POINTS; ++i) {
            const float height = snapshot_a.substrate_height[i];
            expect(height >= (float)TEST_HEIGHT * 0.78f - 0.01f &&
                       height <= (float)TEST_HEIGHT * 0.95f + 0.01f,
                   "substrate control point outside the documented band");
        }

        for (int i = 0; i < snapshot_a.plant_count; ++i) {
            const tobytank_plant_t *plant = &snapshot_a.plants[i];
            expect(plant->blades >= 3 && plant->blades <= 7, "plant blade count out of range");
            expect(plant->height > 0.0f && plant->height <= (float)TEST_HEIGHT * 0.42f + 0.01f,
                   "plant height out of range");
            expect(plant->base_x >= 0.0f && plant->base_x <= (float)TEST_WIDTH,
                   "plant base outside the tank");
        }

        for (int i = 0; i < snapshot_a.bubble_count; ++i) {
            const tobytank_bubble_t *bubble = &snapshot_a.bubbles[i];
            expect(bubble->radius >= TOBYTANK_ENV_BUBBLE_MIN_RADIUS &&
                       bubble->radius <= TOBYTANK_ENV_BUBBLE_MAX_RADIUS,
                   "bubble radius out of range");
            expect(bubble->rise_speed > 0.0f, "bubble does not rise");
        }
    }
}

static void test_particles_stay_bounded(void)
{
    tobytank_environment_init(&environment_a, 0xFEEDFACEULL, TEST_WIDTH, TEST_HEIGHT);
    const float limit_x = (float)TEST_WIDTH + TOBYTANK_ENV_MARGIN;
    const float limit_y = (float)TEST_HEIGHT + TOBYTANK_ENV_MARGIN;
    /* A bubble is retired once its whole disc has cleared the top margin, so
       its centre may sit one radius above that margin before it respawns. */
    const float bubble_top = -TOBYTANK_ENV_MARGIN - TOBYTANK_ENV_BUBBLE_MAX_RADIUS;

    for (int step = 0; step < 4000; ++step) {
        tobytank_environment_update(&environment_a, TOBYTANK_ENV_TIMESTEP_SECONDS);
        tobytank_environment_snapshot(&environment_a, &snapshot_a);

        expect(snapshot_a.bubble_count == TOBYTANK_ENV_MAX_BUBBLES,
               "bubble pool size changed while running");
        expect(snapshot_a.mote_count == TOBYTANK_ENV_MAX_MOTES,
               "mote pool size changed while running");

        for (int i = 0; i < snapshot_a.bubble_count; ++i) {
            const tobytank_bubble_t *bubble = &snapshot_a.bubbles[i];
            if (bubble->x < -TOBYTANK_ENV_MARGIN || bubble->x > limit_x ||
                bubble->y < bubble_top || bubble->y > limit_y) {
                fail("bubble escaped the tank bounds");
                return;
            }
            if (bubble->radius < TOBYTANK_ENV_BUBBLE_MIN_RADIUS ||
                bubble->radius > TOBYTANK_ENV_BUBBLE_MAX_RADIUS) {
                fail("bubble radius left its range while running");
                return;
            }
        }
        for (int i = 0; i < snapshot_a.mote_count; ++i) {
            const tobytank_mote_t *mote = &snapshot_a.motes[i];
            if (mote->x < -TOBYTANK_ENV_MARGIN - 1.0f || mote->x > limit_x + 1.0f ||
                mote->y < -TOBYTANK_ENV_MARGIN || mote->y > limit_y) {
                fail("mote escaped the tank bounds");
                return;
            }
        }
    }
}

static void test_snapshot_is_a_copy(void)
{
    tobytank_environment_init(&environment_a, 0x2468ULL, TEST_WIDTH, TEST_HEIGHT);
    tobytank_environment_snapshot(&environment_a, &snapshot_a);
    snapshot_a.plant_count = 0;
    snapshot_a.bubbles[0].x = -9999.0f;
    tobytank_environment_snapshot(&environment_a, &snapshot_b);
    expect(snapshot_b.plant_count > 0, "mutating a snapshot changed the environment");
    expect(snapshot_b.bubbles[0].x > -9999.0f,
           "mutating a snapshot changed environment particles");
}

static void test_substrate_lookup_is_clamped(void)
{
    tobytank_environment_init(&environment_a, 0x777ULL, TEST_WIDTH, TEST_HEIGHT);
    tobytank_environment_snapshot(&environment_a, &snapshot_a);

    const float left = tobytank_environment_substrate_at(&snapshot_a, -500.0f);
    const float right = tobytank_environment_substrate_at(&snapshot_a, 5000.0f);
    expect(left >= (float)TEST_HEIGHT * 0.78f - 0.01f && left <= (float)TEST_HEIGHT * 0.95f + 0.01f,
           "substrate lookup left of the tank is not clamped");
    expect(right >= (float)TEST_HEIGHT * 0.78f - 0.01f &&
               right <= (float)TEST_HEIGHT * 0.95f + 0.01f,
           "substrate lookup right of the tank is not clamped");

    for (int x = 0; x < TEST_WIDTH; ++x) {
        const float height = tobytank_environment_substrate_at(&snapshot_a, (float)x);
        if (height < 0.0f || height > (float)TEST_HEIGHT) {
            fail("substrate lookup left the canvas");
            return;
        }
    }
}

static void test_full_scene_render_stays_inside_the_canvas(void)
{
    for (int i = 0; i < GUARD + TEST_WIDTH * TEST_HEIGHT + GUARD; ++i) {
        guarded[i] = (uint16_t)GUARD_VALUE;
    }

    const tobytank_canvas_t canvas = {
        .pixels = guarded + GUARD,
        .width = TEST_WIDTH,
        .height = TEST_HEIGHT,
    };

    tobytank_environment_init(&environment_a, 0xC0FFEEULL, TEST_WIDTH, TEST_HEIGHT);
    for (int frame = 0; frame < 240; ++frame) {
        tobytank_environment_update(&environment_a, TOBYTANK_ENV_TIMESTEP_SECONDS);
        tobytank_environment_snapshot(&environment_a, &snapshot_a);
        tobytank_background_draw(&canvas, &snapshot_a);
        tobytank_effects_draw(&canvas, &snapshot_a);
        tobytank_particles_draw(&canvas, &snapshot_a);
    }

    for (int i = 0; i < GUARD; ++i) {
        if (guarded[i] != (uint16_t)GUARD_VALUE) {
            fail("rendering wrote before the canvas");
            return;
        }
        if (guarded[GUARD + TEST_WIDTH * TEST_HEIGHT + i] != (uint16_t)GUARD_VALUE) {
            fail("rendering wrote past the canvas");
            return;
        }
    }

    int painted = 0;
    for (int i = 0; i < TEST_WIDTH * TEST_HEIGHT; ++i) {
        if (guarded[GUARD + i] != (uint16_t)GUARD_VALUE) {
            ++painted;
        }
    }
    expect(painted == TEST_WIDTH * TEST_HEIGHT, "the scene did not cover every pixel");
}

static void test_null_and_degenerate_inputs_are_safe(void)
{
    tobytank_environment_init(NULL, 1, TEST_WIDTH, TEST_HEIGHT);
    tobytank_environment_update(NULL, TOBYTANK_ENV_TIMESTEP_SECONDS);
    tobytank_environment_snapshot(NULL, &snapshot_a);
    tobytank_environment_snapshot(&environment_a, NULL);

    memset(&environment_b, 0, sizeof(environment_b));
    tobytank_environment_init(&environment_b, 5, 32, 32);
    expect(environment_b.scene.width == 0, "an undersized tank was accepted");

    tobytank_background_draw(NULL, &snapshot_a);
    tobytank_effects_draw(NULL, &snapshot_a);
    tobytank_particles_draw(NULL, &snapshot_a);
}

static void test_ppm_encoding(void)
{
    uint16_t pixels[8];
    const tobytank_canvas_t canvas = {
        .pixels = pixels,
        .width = 4,
        .height = 2,
    };
    tobytank_canvas_clear(&canvas, tobytank_rgb565(0, 0, 0));
    tobytank_canvas_pixel(&canvas, 0, 0, tobytank_rgb565(255, 0, 0));
    tobytank_canvas_pixel(&canvas, 1, 0, tobytank_rgb565(0, 255, 0));
    tobytank_canvas_pixel(&canvas, 2, 0, tobytank_rgb565(0, 0, 255));
    tobytank_canvas_pixel(&canvas, 3, 0, tobytank_rgb565(255, 255, 255));

    const size_t expected_size = tobytank_ppm_size(4, 2);
    expect(expected_size == strlen("P6\n4 2\n255\n") + 4u * 2u * 3u,
           "ppm size does not match header plus payload");

    unsigned char buffer[128];
    size_t written = 0;
    expect(tobytank_ppm_encode(&canvas, buffer, sizeof(buffer), &written) == 1,
           "ppm encoding failed");
    expect(written == expected_size, "ppm encoding wrote an unexpected length");
    expect(memcmp(buffer, "P6\n4 2\n255\n", strlen("P6\n4 2\n255\n")) == 0,
           "ppm header is wrong");

    const unsigned char *body = buffer + strlen("P6\n4 2\n255\n");
    expect(body[0] == 255 && body[1] == 0 && body[2] == 0, "red pixel did not round-trip");
    expect(body[3] == 0 && body[4] == 255 && body[5] == 0, "green pixel did not round-trip");
    expect(body[6] == 0 && body[7] == 0 && body[8] == 255, "blue pixel did not round-trip");
    expect(body[9] == 255 && body[10] == 255 && body[11] == 255,
           "white pixel did not round-trip");

    expect(tobytank_ppm_encode(&canvas, buffer, 4, &written) == 0,
           "ppm encoding ignored a short buffer");
    expect(tobytank_ppm_encode(NULL, buffer, sizeof(buffer), &written) == 0,
           "ppm encoding accepted a null canvas");
}

int main(void)
{
    test_generation_is_deterministic();
    test_generated_parameters_stay_in_range();
    test_particles_stay_bounded();
    test_snapshot_is_a_copy();
    test_substrate_lookup_is_clamped();
    test_full_scene_render_stays_inside_the_canvas();
    test_null_and_degenerate_inputs_are_safe();
    test_ppm_encoding();

    if (failures != 0) {
        fprintf(stderr, "environment_host_test failed: %d failure(s)\n", failures);
        return 1;
    }
    puts("environment_host_test passed");
    return 0;
}
