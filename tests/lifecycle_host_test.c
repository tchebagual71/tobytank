#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "aquarium/lifecycle.h"
#include "sim/snapshot.h"

#define TEST_WIDTH 368
#define TEST_HEIGHT 448
#define STEPS_PER_SECOND 60
#define TEST_TIMESTEP_SECONDS (1.0f / 60.0f)

typedef struct {
    tobytank_identity_t next;
    int calls;
    int fail_after;
} identity_source_t;

static int failures;

static void fail(const char *message)
{
    fprintf(stderr, "lifecycle_host_test: %s\n", message);
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
    if (source->fail_after >= 0 && source->calls >= source->fail_after) {
        return 0;
    }
    *out_identity = source->next++;
    ++source->calls;
    return 1;
}

static void step(tobytank_lifecycle_t *lifecycle, int steps)
{
    for (int i = 0; i < steps; ++i) {
        tobytank_lifecycle_update(lifecycle, TEST_TIMESTEP_SECONDS);
    }
}

static void test_initial_empty_and_identity_delay(void)
{
    identity_source_t source = {1, 0, -1};
    tobytank_lifecycle_t lifecycle;
    expect(tobytank_lifecycle_init(&lifecycle, 0xAABBCCDDULL, TEST_WIDTH, TEST_HEIGHT,
                                   identity_source, &source),
           "lifecycle init failed");
    expect(lifecycle.state == TOBYTANK_VISITOR_EMPTY, "lifecycle did not start empty");
    expect(source.calls == 0, "identity was consumed during init");

    tobytank_fish_snapshot_t snapshot;
    tobytank_lifecycle_snapshot(&lifecycle, &snapshot);
    expect(snapshot.has_fish == 0, "empty snapshot reports a fish");
    expect(snapshot.identity == TOBYTANK_IDENTITY_INVALID, "empty snapshot has an identity");

    step(&lifecycle, 12 * STEPS_PER_SECOND);
    expect(source.calls == 1, "first admission did not consume exactly one identity");
    tobytank_lifecycle_snapshot(&lifecycle, &snapshot);
    expect(snapshot.has_fish == 1, "admitted visitor is missing from snapshot");
    expect(snapshot.identity == 1, "first visitor identity is wrong");
    expect(snapshot.state == TOBYTANK_VISITOR_ENTERING ||
               snapshot.state == TOBYTANK_VISITOR_EXPLORING,
           "visitor did not enter or explore after admission");
}

static void test_replay_is_deterministic(void)
{
    identity_source_t left_source = {1000, 0, -1};
    identity_source_t right_source = {1000, 0, -1};
    tobytank_lifecycle_t left;
    tobytank_lifecycle_t right;
    expect(tobytank_lifecycle_init(&left, 55, TEST_WIDTH, TEST_HEIGHT,
                                   identity_source, &left_source), "left init failed");
    expect(tobytank_lifecycle_init(&right, 55, TEST_WIDTH, TEST_HEIGHT,
                                   identity_source, &right_source), "right init failed");

    for (int i = 0; i < 140 * STEPS_PER_SECOND; ++i) {
        tobytank_lifecycle_update(&left, TEST_TIMESTEP_SECONDS);
        tobytank_lifecycle_update(&right, TEST_TIMESTEP_SECONDS);
        tobytank_fish_snapshot_t left_snapshot;
        tobytank_fish_snapshot_t right_snapshot;
        tobytank_lifecycle_snapshot(&left, &left_snapshot);
        tobytank_lifecycle_snapshot(&right, &right_snapshot);
        if (memcmp(&left_snapshot, &right_snapshot, sizeof(left_snapshot)) != 0 ||
            left_source.calls != right_source.calls) {
            fail("two lifecycles with the same seed and identities diverged");
            return;
        }
    }
}

static void test_one_fish_invariant_and_bounded_exit(void)
{
    identity_source_t source = {20, 0, -1};
    tobytank_lifecycle_t lifecycle;
    expect(tobytank_lifecycle_init(&lifecycle, 77, TEST_WIDTH, TEST_HEIGHT,
                                   identity_source, &source), "init failed");
    tobytank_identity_t active = TOBYTANK_IDENTITY_INVALID;
    int visitors_seen = 0;
    int empty_seen_after_visitor = 0;
    int max_exit_steps = 0;
    int exit_steps = 0;

    for (int i = 0; i < 260 * STEPS_PER_SECOND; ++i) {
        tobytank_lifecycle_update(&lifecycle, TEST_TIMESTEP_SECONDS);
        tobytank_fish_snapshot_t snapshot;
        tobytank_lifecycle_snapshot(&lifecycle, &snapshot);
        if (snapshot.has_fish) {
            if (active == TOBYTANK_IDENTITY_INVALID) {
                active = snapshot.identity;
                ++visitors_seen;
            } else if (snapshot.identity != active) {
                fail("a second visitor appeared before the first cleared");
                return;
            }
            if (snapshot.state == TOBYTANK_VISITOR_EXITING) {
                ++exit_steps;
            }
        } else {
            if (active != TOBYTANK_IDENTITY_INVALID) {
                active = TOBYTANK_IDENTITY_INVALID;
                empty_seen_after_visitor = 1;
                if (exit_steps > max_exit_steps) {
                    max_exit_steps = exit_steps;
                }
                exit_steps = 0;
            }
        }
    }

    expect(visitors_seen >= 2, "not enough visitors cycled through");
    expect(empty_seen_after_visitor, "no empty interval occurred after a visitor");
    expect(max_exit_steps < 22 * STEPS_PER_SECOND, "exit exceeded the bounded timeout");
}

static void test_entry_starts_fully_offscreen_and_motion_is_continuous(void)
{
    identity_source_t source = {500, 0, -1};
    tobytank_lifecycle_t lifecycle;
    expect(tobytank_lifecycle_init(&lifecycle, 99, TEST_WIDTH, TEST_HEIGHT,
                                   identity_source, &source), "init failed");

    tobytank_fish_snapshot_t previous = {0};
    int saw_entering = 0;
    int saw_exploring = 0;
    for (int i = 0; i < 90 * STEPS_PER_SECOND; ++i) {
        tobytank_lifecycle_update(&lifecycle, TEST_TIMESTEP_SECONDS);
        tobytank_fish_snapshot_t snapshot;
        tobytank_lifecycle_snapshot(&lifecycle, &snapshot);
        if (snapshot.has_fish && snapshot.state == TOBYTANK_VISITOR_ENTERING && !saw_entering) {
            saw_entering = 1;
            expect(snapshot.x < -80.0f, "entry did not start fully offscreen");
        }
        if (snapshot.has_fish && previous.has_fish && snapshot.identity == previous.identity) {
            const float dx = snapshot.x - previous.x;
            const float dy = snapshot.y - previous.y;
            expect(dx * dx + dy * dy < 8.0f * 8.0f,
                   "motion has a frame-to-frame discontinuity");
        }
        if (snapshot.state == TOBYTANK_VISITOR_EXPLORING) {
            saw_exploring = 1;
        }
        previous = snapshot;
    }
    expect(saw_entering, "entry state was never observed");
    expect(saw_exploring, "exploration state was never observed");
}

static void test_identity_source_failure_keeps_tank_empty(void)
{
    identity_source_t source = {1, 0, 0};
    tobytank_lifecycle_t lifecycle;
    expect(tobytank_lifecycle_init(&lifecycle, 123, TEST_WIDTH, TEST_HEIGHT,
                                   identity_source, &source), "init failed");
    step(&lifecycle, 60 * STEPS_PER_SECOND);
    tobytank_fish_snapshot_t snapshot;
    tobytank_lifecycle_snapshot(&lifecycle, &snapshot);
    expect(snapshot.has_fish == 0, "failed identity source still admitted a fish");
    expect(source.calls == 0, "failing identity source consumed an identity");
}

static void test_invalid_inputs_are_safe(void)
{
    expect(tobytank_lifecycle_init(NULL, 1, TEST_WIDTH, TEST_HEIGHT,
                                   identity_source, NULL) == 0,
           "null lifecycle initialized");
    tobytank_lifecycle_t lifecycle;
    expect(tobytank_lifecycle_init(&lifecycle, 1, 12, 12,
                                   identity_source, NULL) == 0,
           "undersized lifecycle initialized");
    tobytank_lifecycle_update(NULL, TEST_TIMESTEP_SECONDS);
    tobytank_lifecycle_snapshot(NULL, NULL);
    expect(strcmp(tobytank_visitor_state_name(TOBYTANK_VISITOR_ENTERING),
                  "entering") == 0,
           "state name is wrong");
}

int main(void)
{
    test_initial_empty_and_identity_delay();
    test_replay_is_deterministic();
    test_one_fish_invariant_and_bounded_exit();
    test_entry_starts_fully_offscreen_and_motion_is_continuous();
    test_identity_source_failure_keeps_tank_empty();
    test_invalid_inputs_are_safe();

    if (failures != 0) {
        fprintf(stderr, "lifecycle_host_test failed: %d failure(s)\n", failures);
        return 1;
    }
    puts("lifecycle_host_test passed");
    return 0;
}
