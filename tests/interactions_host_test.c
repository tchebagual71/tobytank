#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "aquarium/environment.h"
#include "aquarium/interactions.h"
#include "aquarium/lifecycle.h"
#include "input/motion_filter.h"
#include "render/background.h"
#include "render/canvas.h"
#include "render/effects.h"

#define TEST_WIDTH 160
#define TEST_HEIGHT 192
#define STEPS_PER_SECOND 60
#define TEST_TIMESTEP_SECONDS (1.0f / 60.0f)
#define GUARD 64
#define GUARD_VALUE 0xCAFEu

typedef struct {
    tobytank_identity_t next;
    int calls;
} identity_source_t;

static int failures;
static uint16_t guarded[GUARD + TEST_WIDTH * TEST_HEIGHT + GUARD];
static uint16_t before_pixels[TEST_WIDTH * TEST_HEIGHT];

static void fail(const char *message)
{
    fprintf(stderr, "interactions_host_test: %s\n", message);
    ++failures;
}

static void expect(int condition, const char *message)
{
    if (!condition) {
        fail(message);
    }
}

static int identity_source(void *context, tobytank_identity_t *out_identity)
{
    identity_source_t *source = (identity_source_t *)context;
    if (source == NULL || out_identity == NULL) {
        return 0;
    }
    *out_identity = source->next++;
    ++source->calls;
    return 1;
}

static void test_touch_gestures_are_deterministic(void)
{
    const tobytank_touch_sample_t samples[] = {
        {0, 0, 0, 0},
        {1, 20, 30, 60},
        {1, 24, 31, 60},
        {1, 42, 36, 60},
        {0, 42, 36, 0},
        {1, 300, -12, 60},
    };

    tobytank_touch_filter_t left;
    tobytank_touch_filter_t right;
    tobytank_touch_filter_init(&left, TEST_WIDTH, TEST_HEIGHT);
    tobytank_touch_filter_init(&right, TEST_WIDTH, TEST_HEIGHT);
    for (unsigned i = 0; i < sizeof(samples) / sizeof(samples[0]); ++i) {
        tobytank_touch_filter_update(&left, &samples[i], TEST_TIMESTEP_SECONDS);
        tobytank_touch_filter_update(&right, &samples[i], TEST_TIMESTEP_SECONDS);
        if (memcmp(&left, &right, sizeof(left)) != 0) {
            fail("same touch stream diverged");
            return;
        }
    }
    expect(left.x == (float)(TEST_WIDTH - 1), "touch x was not clamped");
    expect(left.y == 0.0f, "touch y was not clamped");
}

static void test_imu_filter_is_bounded_and_falls_back(void)
{
    tobytank_imu_filter_t filter;
    tobytank_imu_filter_init(&filter);
    const tobytank_imu_sample_t high = {1, 4.0f, -3.0f, 0.1f};
    for (int i = 0; i < 120; ++i) {
        tobytank_imu_filter_update(&filter, &high, TEST_TIMESTEP_SECONDS);
    }
    expect(filter.available == 1, "available IMU sample was ignored");
    expect(filter.tilt_x <= 1.0f && filter.tilt_x >= -1.0f, "tilt x escaped bounds");
    expect(filter.tilt_y <= 1.0f && filter.tilt_y >= -1.0f, "tilt y escaped bounds");

    const tobytank_imu_sample_t missing = {0, 0.0f, 0.0f, 0.0f};
    tobytank_imu_filter_update(&filter, &missing, TEST_TIMESTEP_SECONDS);
    expect(filter.available == 0, "missing IMU did not clear availability");
}

static void test_interactions_are_bounded_and_repeatable(void)
{
    tobytank_touch_filter_t touch_a;
    tobytank_touch_filter_t touch_b;
    tobytank_imu_filter_t imu_a;
    tobytank_imu_filter_t imu_b;
    tobytank_interactions_t a;
    tobytank_interactions_t b;
    tobytank_touch_filter_init(&touch_a, TEST_WIDTH, TEST_HEIGHT);
    tobytank_touch_filter_init(&touch_b, TEST_WIDTH, TEST_HEIGHT);
    tobytank_imu_filter_init(&imu_a);
    tobytank_imu_filter_init(&imu_b);
    tobytank_interactions_init(&a, TEST_WIDTH, TEST_HEIGHT);
    tobytank_interactions_init(&b, TEST_WIDTH, TEST_HEIGHT);

    for (int step = 0; step < 240; ++step) {
        const tobytank_touch_sample_t touch = {
            .pressed = step > 8 && step < 95,
            .x = 20 + step,
            .y = 70 + (step % 11),
            .strength = 80,
        };
        const tobytank_imu_sample_t imu = {
            .available = 1,
            .accel_x = 0.35f,
            .accel_y = -0.24f,
            .accel_z = 0.9f,
        };
        tobytank_touch_filter_update(&touch_a, &touch, TEST_TIMESTEP_SECONDS);
        tobytank_touch_filter_update(&touch_b, &touch, TEST_TIMESTEP_SECONDS);
        tobytank_imu_filter_update(&imu_a, &imu, TEST_TIMESTEP_SECONDS);
        tobytank_imu_filter_update(&imu_b, &imu, TEST_TIMESTEP_SECONDS);
        tobytank_interactions_update(&a, &touch_a, &imu_a, TEST_TIMESTEP_SECONDS);
        tobytank_interactions_update(&b, &touch_b, &imu_b, TEST_TIMESTEP_SECONDS);
        if (memcmp(&a, &b, sizeof(a)) != 0) {
            fail("same interaction stream diverged");
            return;
        }
        if (a.current_x < -28.1f || a.current_x > 28.1f ||
            a.current_y < -18.1f || a.current_y > 18.1f ||
            a.parallax_x < -9.1f || a.parallax_x > 9.1f ||
            a.parallax_y < -7.1f || a.parallax_y > 7.1f ||
            a.attention_strength < 0.0f || a.attention_strength > 1.01f) {
            fail("interaction output escaped bounds");
            return;
        }
    }
}

static void test_environment_current_and_lifecycle_invariants(void)
{
    identity_source_t source = {10, 0};
    tobytank_environment_t environment;
    tobytank_env_snapshot_t snapshot;
    tobytank_lifecycle_t lifecycle;
    tobytank_interactions_t interactions;
    tobytank_touch_filter_t touch;
    tobytank_imu_filter_t imu;
    tobytank_environment_init(&environment, 99, TEST_WIDTH, TEST_HEIGHT);
    expect(tobytank_lifecycle_init(&lifecycle, 101, TEST_WIDTH, TEST_HEIGHT,
                                   identity_source, &source),
           "lifecycle init failed");
    tobytank_interactions_init(&interactions, TEST_WIDTH, TEST_HEIGHT);
    tobytank_touch_filter_init(&touch, TEST_WIDTH, TEST_HEIGHT);
    tobytank_imu_filter_init(&imu);

    tobytank_identity_t active = TOBYTANK_IDENTITY_INVALID;
    for (int step = 0; step < 180 * STEPS_PER_SECOND; ++step) {
        const tobytank_touch_sample_t touch_sample = {
            .pressed = (step % 180) < 42,
            .x = 40 + (step % 80),
            .y = 52 + (step % 35),
            .strength = 90,
        };
        tobytank_touch_filter_update(&touch, &touch_sample, TEST_TIMESTEP_SECONDS);
        tobytank_interactions_update(&interactions, &touch, &imu,
                                     TEST_TIMESTEP_SECONDS);
        tobytank_environment_update_with_interactions(&environment,
                                                      TEST_TIMESTEP_SECONDS,
                                                      &interactions);
        tobytank_lifecycle_update_with_interactions(&lifecycle,
                                                    TEST_TIMESTEP_SECONDS,
                                                    &interactions);
        tobytank_fish_snapshot_t fish;
        tobytank_lifecycle_snapshot(&lifecycle, &fish);
        if (fish.has_fish) {
            if (active == TOBYTANK_IDENTITY_INVALID) {
                active = fish.identity;
            } else if (active != fish.identity) {
                fail("touch interaction admitted a second fish");
                return;
            }
        } else {
            active = TOBYTANK_IDENTITY_INVALID;
        }
    }

    tobytank_environment_snapshot(&environment, &snapshot);
    for (int i = 0; i < snapshot.bubble_count; ++i) {
        if (snapshot.bubbles[i].x < -TOBYTANK_ENV_MARGIN ||
            snapshot.bubbles[i].x > (float)TEST_WIDTH + TOBYTANK_ENV_MARGIN) {
            fail("interaction current pushed a bubble out of bounds");
            return;
        }
    }
    expect(source.calls >= 1, "interactions prevented visitor admission");
}

static void test_interaction_ripple_render_is_clipped(void)
{
    for (int i = 0; i < GUARD + TEST_WIDTH * TEST_HEIGHT + GUARD; ++i) {
        guarded[i] = GUARD_VALUE;
    }
    const tobytank_canvas_t canvas = {
        .pixels = guarded + GUARD,
        .width = TEST_WIDTH,
        .height = TEST_HEIGHT,
    };
    tobytank_environment_t environment;
    tobytank_env_snapshot_t snapshot;
    tobytank_interactions_t interactions;
    tobytank_environment_init(&environment, 55, TEST_WIDTH, TEST_HEIGHT);
    tobytank_environment_snapshot(&environment, &snapshot);
    tobytank_interactions_init(&interactions, TEST_WIDTH, TEST_HEIGHT);
    interactions.ripple_active = 1;
    interactions.ripple_x = 2.0f;
    interactions.ripple_y = 2.0f;
    interactions.ripple_radius = 38.0f;
    interactions.ripple_strength = 1.0f;

    tobytank_background_draw(&canvas, &snapshot);
    tobytank_effects_draw_interactions(&canvas, &snapshot, &interactions);
    for (int i = 0; i < GUARD; ++i) {
        if (guarded[i] != GUARD_VALUE ||
            guarded[GUARD + TEST_WIDTH * TEST_HEIGHT + i] != GUARD_VALUE) {
            fail("interaction ripple wrote outside the canvas");
            return;
        }
    }
}

static void test_interaction_ripple_is_visible(void)
{
    for (int i = 0; i < GUARD + TEST_WIDTH * TEST_HEIGHT + GUARD; ++i) {
        guarded[i] = GUARD_VALUE;
    }
    const tobytank_canvas_t canvas = {
        .pixels = guarded + GUARD,
        .width = TEST_WIDTH,
        .height = TEST_HEIGHT,
    };
    tobytank_environment_t environment;
    tobytank_env_snapshot_t snapshot;
    tobytank_interactions_t interactions;
    tobytank_environment_init(&environment, 77, TEST_WIDTH, TEST_HEIGHT);
    tobytank_environment_snapshot(&environment, &snapshot);
    tobytank_interactions_init(&interactions, TEST_WIDTH, TEST_HEIGHT);
    interactions.ripple_active = 1;
    interactions.ripple_x = (float)TEST_WIDTH * 0.5f;
    interactions.ripple_y = (float)TEST_HEIGHT * 0.45f;
    interactions.ripple_radius = 22.0f;
    interactions.ripple_strength = 0.92f;

    tobytank_background_draw(&canvas, &snapshot);
    uint32_t before_sum = 0;
    for (int i = 0; i < TEST_WIDTH * TEST_HEIGHT; ++i) {
        before_pixels[i] = canvas.pixels[i];
        before_sum += canvas.pixels[i];
    }
    tobytank_effects_draw_interactions(&canvas, &snapshot, &interactions);
    uint32_t changed = 0;
    uint32_t after_sum = 0;
    for (int i = 0; i < TEST_WIDTH * TEST_HEIGHT; ++i) {
        after_sum += canvas.pixels[i];
        if (canvas.pixels[i] != before_pixels[i]) {
            ++changed;
        }
    }
    expect(changed > 80, "interaction ripple changed too few pixels");
    expect(after_sum != before_sum, "interaction ripple did not alter frame values");
}

int main(void)
{
    test_touch_gestures_are_deterministic();
    test_imu_filter_is_bounded_and_falls_back();
    test_interactions_are_bounded_and_repeatable();
    test_environment_current_and_lifecycle_invariants();
    test_interaction_ripple_render_is_clipped();
    test_interaction_ripple_is_visible();

    if (failures != 0) {
        fprintf(stderr, "interactions_host_test failed: %d failure(s)\n", failures);
        return 1;
    }
    puts("interactions_host_test passed");
    return 0;
}
