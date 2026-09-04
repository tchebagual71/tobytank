#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fish/genome.h"
#include "fish/genome_validate.h"
#include "fish/identity.h"
#include "fish/prng.h"

#define SAMPLE_COUNT 4000

static int failures;

static void fail(const char *message)
{
    fprintf(stderr, "genome_host_test: %s\n", message);
    ++failures;
}

static void expect(int condition, const char *message)
{
    if (!condition) {
        fail(message);
    }
}

static tobytank_genome_t genome_a;
static tobytank_genome_t genome_b;
static uint64_t fingerprints[SAMPLE_COUNT];

static void test_generation_is_a_pure_function_of_identity(void)
{
    for (tobytank_identity_t identity = 1; identity <= 200; ++identity) {
        expect(tobytank_genome_generate(&genome_a, identity) == 1,
               "generation failed for a low identity");
        expect(tobytank_genome_generate(&genome_b, identity) == 1,
               "regeneration failed for a low identity");
        if (memcmp(&genome_a, &genome_b, sizeof(genome_a)) != 0) {
            fail("the same identity produced two different genomes");
            return;
        }
        if (tobytank_genome_fingerprint(&genome_a) != tobytank_genome_fingerprint(&genome_b)) {
            fail("the same genome produced two different fingerprints");
            return;
        }
        if (genome_a.identity != identity) {
            fail("the genome does not carry its identity");
            return;
        }
    }
}

static int compare_u64(const void *left, const void *right)
{
    const uint64_t a = *(const uint64_t *)left;
    const uint64_t b = *(const uint64_t *)right;
    if (a < b) {
        return -1;
    }
    return a > b ? 1 : 0;
}

static void test_consecutive_identities_differ_completely(void)
{
    /* Neighbouring identities are the case that matters: a visitor and the one
       before it must not look like the same fish in a different colour. */
    for (tobytank_identity_t identity = 1; identity <= 500; ++identity) {
        expect(tobytank_genome_generate(&genome_a, identity) == 1, "generation failed");
        expect(tobytank_genome_generate(&genome_b, identity + 1) == 1, "generation failed");
        if (tobytank_genome_fingerprint(&genome_a) == tobytank_genome_fingerprint(&genome_b)) {
            fail("consecutive identities share a fingerprint");
            return;
        }
        int differing_traits = 0;
        differing_traits += genome_a.body_length != genome_b.body_length;
        differing_traits += genome_a.depth_ratio != genome_b.depth_ratio;
        differing_traits += genome_a.base_hue != genome_b.base_hue;
        differing_traits += genome_a.pattern_type != genome_b.pattern_type ||
                            genome_a.pattern_density != genome_b.pattern_density;
        differing_traits += genome_a.preferred_speed != genome_b.preferred_speed;
        if (differing_traits < 4) {
            fail("consecutive identities differ in too few trait groups");
            return;
        }
    }

    for (int i = 0; i < SAMPLE_COUNT; ++i) {
        const tobytank_identity_t identity = (tobytank_identity_t)(i + 1);
        expect(tobytank_genome_generate(&genome_a, identity) == 1, "generation failed");
        fingerprints[i] = tobytank_genome_fingerprint(&genome_a);
    }
    qsort(fingerprints, SAMPLE_COUNT, sizeof(fingerprints[0]), compare_u64);
    for (int i = 1; i < SAMPLE_COUNT; ++i) {
        if (fingerprints[i] == fingerprints[i - 1]) {
            fail("two identities collided on one fingerprint");
            return;
        }
    }
}

static void test_thousands_of_genomes_validate(void)
{
    int accepted = 0;
    int first_attempt_rejected = 0;
    for (int i = 0; i < SAMPLE_COUNT; ++i) {
        const tobytank_identity_t identity = (tobytank_identity_t)(i + 1);
        if (tobytank_genome_generate(&genome_a, identity)) {
            ++accepted;
            if (tobytank_genome_validate(&genome_a) != TOBYTANK_GENOME_OK) {
                fail("generate returned a genome that does not validate");
                return;
            }
            if (genome_a.variant > 0) {
                ++first_attempt_rejected;
            }
        }
    }
    expect(accepted == SAMPLE_COUNT, "some identities produced no usable genome");
    printf("  %d/%d accepted, %d needed regeneration\n", accepted, SAMPLE_COUNT,
           first_attempt_rejected);
}

static void test_every_accepted_genome_fits_the_sprite_box(void)
{
    for (int i = 0; i < SAMPLE_COUNT; ++i) {
        const tobytank_identity_t identity = (tobytank_identity_t)(i + 1);
        if (!tobytank_genome_generate(&genome_a, identity)) {
            continue;
        }
        float width = 0.0f;
        float height = 0.0f;
        tobytank_genome_extent(&genome_a, &width, &height);
        if (width <= 0.0f || height <= 0.0f ||
            width > (float)TOBYTANK_FISH_MAX_WIDTH ||
            height > (float)TOBYTANK_FISH_MAX_HEIGHT) {
            fail("an accepted genome does not fit the sprite box");
            return;
        }
    }
}

static void test_rejection_and_regeneration_are_deterministic(void)
{
    /* Find identities whose first attempt is rejected, then prove the retry
       lands on the same variant every time. */
    tobytank_identity_t rejected_identity = 0;
    int rejected_count = 0;
    int reasons[TOBYTANK_GENOME_REJECT_MOTION + 1] = {0};
    for (int i = 0; i < SAMPLE_COUNT; ++i) {
        const tobytank_identity_t identity = (tobytank_identity_t)(i + 1);
        tobytank_genome_generate_variant(&genome_a, identity, 0);
        const tobytank_genome_verdict_t verdict = tobytank_genome_validate(&genome_a);
        if (verdict != TOBYTANK_GENOME_OK) {
            ++rejected_count;
            ++reasons[verdict];
            if (rejected_identity == 0) {
                rejected_identity = identity;
            }
        }
    }
    printf("  %d/%d identities rejected on the first attempt\n", rejected_count, SAMPLE_COUNT);
    for (int reason = 1; reason <= TOBYTANK_GENOME_REJECT_MOTION; ++reason) {
        if (reasons[reason] > 0) {
            printf("    %s: %d\n",
                   tobytank_genome_verdict_name((tobytank_genome_verdict_t)reason),
                   reasons[reason]);
        }
    }
    /* Validation is a safety net, not half the generator. If most first
       attempts fail, the generation ranges have drifted from the limits. */
    expect(rejected_count * 10 < SAMPLE_COUNT,
           "more than a tenth of first attempts are rejected");

    if (rejected_identity == 0) {
        /* Not a failure, but the retry path would then be untested here. */
        printf("  no first-attempt rejection in this sample\n");
    } else {
        expect(tobytank_genome_generate(&genome_a, rejected_identity) == 1,
               "a rejected identity produced no genome at all");
        expect(genome_a.variant > 0, "a rejected identity was accepted at variant 0");
        for (int repeat = 0; repeat < 5; ++repeat) {
            expect(tobytank_genome_generate(&genome_b, rejected_identity) == 1,
                   "regeneration failed on repeat");
            if (memcmp(&genome_a, &genome_b, sizeof(genome_a)) != 0) {
                fail("regeneration after rejection is not deterministic");
                return;
            }
        }
    }

    /* Each variant must actually be a different draw, or retrying is pointless. */
    tobytank_genome_generate_variant(&genome_a, 12345u, 0);
    tobytank_genome_generate_variant(&genome_b, 12345u, 1);
    expect(tobytank_genome_fingerprint(&genome_a) != tobytank_genome_fingerprint(&genome_b),
           "two variants of one identity are identical");
}

static void test_invalid_input_is_reported(void)
{
    expect(tobytank_genome_generate(&genome_a, TOBYTANK_IDENTITY_INVALID) == 0,
           "identity 0 was accepted");
    expect(genome_a.identity == 0 && genome_a.body_length == 0.0f,
           "a rejected generation left a partly filled genome");
    expect(tobytank_genome_generate(NULL, 1) == 0, "a null genome pointer was accepted");
    expect(tobytank_genome_validate(NULL) == TOBYTANK_GENOME_REJECT_NULL,
           "validating null did not report null");
    expect(tobytank_genome_fingerprint(NULL) == 0, "fingerprinting null did not return 0");

    tobytank_genome_generate(&genome_a, 7);
    genome_a.body_length = 5.0f;
    expect(tobytank_genome_validate(&genome_a) == TOBYTANK_GENOME_REJECT_BODY_SIZE,
           "an impossible body size was accepted");
    expect(strcmp(tobytank_genome_verdict_name(TOBYTANK_GENOME_REJECT_BODY_SIZE),
                  "body_size") == 0,
           "verdict names do not match their codes");
}

static void test_prng_streams_are_independent_and_repeatable(void)
{
    tobytank_prng_t first;
    tobytank_prng_t second;

    tobytank_prng_seed(&first, 99, TOBYTANK_PRNG_STREAM_ANATOMY, 0);
    tobytank_prng_seed(&second, 99, TOBYTANK_PRNG_STREAM_ANATOMY, 0);
    for (int i = 0; i < 64; ++i) {
        if (tobytank_prng_next_u64(&first) != tobytank_prng_next_u64(&second)) {
            fail("one stream is not repeatable");
            return;
        }
    }

    tobytank_prng_seed(&first, 99, TOBYTANK_PRNG_STREAM_ANATOMY, 0);
    tobytank_prng_seed(&second, 99, TOBYTANK_PRNG_STREAM_COLOR, 0);
    expect(tobytank_prng_next_u64(&first) != tobytank_prng_next_u64(&second),
           "two streams of one identity agree");

    tobytank_prng_seed(&first, 99, TOBYTANK_PRNG_STREAM_ANATOMY, 0);
    tobytank_prng_seed(&second, 99, TOBYTANK_PRNG_STREAM_ANATOMY, 1);
    expect(tobytank_prng_next_u64(&first) != tobytank_prng_next_u64(&second),
           "two variants of one stream agree");

    tobytank_prng_seed(&first, 0, TOBYTANK_PRNG_STREAM_ANATOMY, 0);
    expect(first.state != 0, "a zero seed left the generator stuck at zero");

    tobytank_prng_seed(&first, 5, TOBYTANK_PRNG_STREAM_MOTION, 0);
    for (int i = 0; i < 4096; ++i) {
        const float unit = tobytank_prng_unit(&first);
        if (unit < 0.0f || unit >= 1.0f) {
            fail("unit values left [0,1)");
            return;
        }
        const int value = tobytank_prng_int(&first, -3, 7);
        if (value < -3 || value > 7) {
            fail("integer values left their range");
            return;
        }
        const float ranged = tobytank_prng_range(&first, 2.5f, 9.5f);
        if (ranged < 2.5f || ranged >= 9.5f) {
            fail("ranged values left their range");
            return;
        }
    }
    expect(tobytank_prng_range(&first, 4.0f, 4.0f) == 4.0f, "an empty range moved");
    expect(tobytank_prng_int(&first, 6, 6) == 6, "an empty integer range moved");
    expect(tobytank_prng_chance(&first, 0.0f) == 0 && tobytank_prng_chance(&first, 1.0f) == 1,
           "certain chances are not certain");
}

static void test_reboot_skips_the_unused_remainder_of_a_block(void)
{
    tobytank_identity_allocator_t allocator;
    tobytank_identity_t identity = 0;

    /* First boot on a fresh device. */
    tobytank_identity_reserve_block(&allocator, 0);
    expect(allocator.next_identity == 1, "the first identity is not 1");
    expect(allocator.reserved_counter == TOBYTANK_IDENTITY_BLOCK_SIZE,
           "the first block reserved the wrong count");
    expect(tobytank_identity_block_remaining(&allocator) == TOBYTANK_IDENTITY_BLOCK_SIZE,
           "the first block reports the wrong remaining count");

    for (int i = 0; i < 3; ++i) {
        expect(tobytank_identity_next(&allocator, &identity) == 1, "allocation failed");
        expect(identity == (tobytank_identity_t)(i + 1), "identities are not sequential");
    }
    const uint64_t persisted = allocator.reserved_counter;

    /* Power loss, then a reboot that reads the persisted counter. */
    tobytank_identity_reserve_block(&allocator, persisted);
    expect(allocator.next_identity == TOBYTANK_IDENTITY_BLOCK_SIZE + 1,
           "a reboot did not skip the unused remainder of the block");
    expect(tobytank_identity_next(&allocator, &identity) == 1, "allocation failed after reboot");
    expect(identity == TOBYTANK_IDENTITY_BLOCK_SIZE + 1,
           "the first identity after a reboot was reused from the old block");

    /* Exhausting a block must refuse rather than wrap. */
    tobytank_identity_reserve_block(&allocator, 1000);
    for (uint64_t i = 0; i < TOBYTANK_IDENTITY_BLOCK_SIZE; ++i) {
        expect(tobytank_identity_next(&allocator, &identity) == 1,
               "allocation failed inside a fresh block");
    }
    expect(tobytank_identity_block_exhausted(&allocator) == 1,
           "an exhausted block does not report itself exhausted");
    expect(tobytank_identity_block_remaining(&allocator) == 0,
           "an exhausted block reports identities remaining");
    identity = 0xDEADBEEF;
    expect(tobytank_identity_next(&allocator, &identity) == 0,
           "an exhausted block still handed out an identity");
    expect(identity == 0xDEADBEEF, "a failed allocation still wrote an identity");

    tobytank_identity_reserve_block(&allocator,
                                    UINT64_MAX - TOBYTANK_IDENTITY_BLOCK_SIZE + 1u);
    expect(tobytank_identity_block_exhausted(&allocator) == 1,
           "a near-overflow counter did not fail closed");
    expect(tobytank_identity_next(&allocator, &identity) == 0,
           "a near-overflow block handed out an identity");

    /* Across many simulated boots, no identity may ever repeat. */
    uint64_t counter = 0;
    tobytank_identity_t highest = 0;
    for (int boot = 0; boot < 200; ++boot) {
        tobytank_identity_reserve_block(&allocator, counter);
        if (allocator.next_identity <= highest) {
            fail("a reboot handed out an identity at or below a previous one");
            return;
        }
        const int take = (boot % 7) + 1;
        for (int i = 0; i < take; ++i) {
            expect(tobytank_identity_next(&allocator, &identity) == 1, "allocation failed");
            if (identity <= highest) {
                fail("an identity repeated across boots");
                return;
            }
            highest = identity;
        }
        counter = allocator.reserved_counter;
    }

    expect(tobytank_identity_is_valid(TOBYTANK_IDENTITY_INVALID) == 0,
           "identity 0 is treated as valid");
    expect(tobytank_identity_is_valid(1) == 1, "identity 1 is treated as invalid");
    tobytank_identity_reserve_block(NULL, 0);
    expect(tobytank_identity_next(NULL, &identity) == 0, "a null allocator was accepted");
    expect(tobytank_identity_block_exhausted(NULL) == 1,
           "a null allocator is not reported exhausted");
}

int main(void)
{
    test_generation_is_a_pure_function_of_identity();
    test_consecutive_identities_differ_completely();
    test_thousands_of_genomes_validate();
    test_every_accepted_genome_fits_the_sprite_box();
    test_rejection_and_regeneration_are_deterministic();
    test_invalid_input_is_reported();
    test_prng_streams_are_independent_and_repeatable();
    test_reboot_skips_the_unused_remainder_of_a_block();

    if (failures != 0) {
        fprintf(stderr, "genome_host_test failed: %d failure(s)\n", failures);
        return 1;
    }
    puts("genome_host_test passed");
    return 0;
}
