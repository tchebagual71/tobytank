#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fish/genome.h"
#include "fish/portrait.h"
#include "render/canvas.h"
#include "render/composite.h"
#include "render/fish_cache.h"
#include "render/fish_rasterizer.h"

#define CANVAS_WIDTH 220
#define CANVAS_HEIGHT 180
#define GUARD 128
#define GUARD_VALUE 0xBEEFu
#define SAMPLE_COUNT 600

static int failures;
static uint16_t sprite_pixels[TOBYTANK_FISH_MAX_WIDTH * TOBYTANK_FISH_MAX_HEIGHT];
static uint8_t sprite_alpha[TOBYTANK_FISH_MAX_WIDTH * TOBYTANK_FISH_MAX_HEIGHT];
static uint8_t base_alpha[TOBYTANK_FISH_MAX_WIDTH * TOBYTANK_FISH_MAX_HEIGHT];
static uint16_t guarded[GUARD + CANVAS_WIDTH * CANVAS_HEIGHT + GUARD];

static void fail(const char *message)
{
    fprintf(stderr, "fish_rasterizer_host_test: %s\n", message);
    ++failures;
}

static void expect(int condition, const char *message)
{
    if (!condition) {
        fail(message);
    }
}

static int count_opaque(const tobytank_fish_sprite_t *sprite)
{
    int count = 0;
    for (int i = 0; i < sprite->width * sprite->height; ++i) {
        if (sprite->alpha[i] != 0) {
            ++count;
        }
    }
    return count;
}

static uint64_t sprite_digest(const tobytank_fish_sprite_t *sprite)
{
    uint64_t hash = 1469598103934665603ULL;
    for (int i = 0; i < sprite->width * sprite->height; ++i) {
        hash ^= sprite->pixels[i];
        hash *= 1099511628211ULL;
        hash ^= sprite->alpha[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

static tobytank_fish_sprite_t make_sprite(void)
{
    const tobytank_fish_sprite_t sprite = {
        .pixels = sprite_pixels,
        .alpha = sprite_alpha,
        .width = TOBYTANK_FISH_MAX_WIDTH,
        .height = TOBYTANK_FISH_MAX_HEIGHT,
        .origin_x = 0,
        .origin_y = 0,
    };
    return sprite;
}

static void prepare_guarded_canvas(tobytank_canvas_t *canvas)
{
    for (int i = 0; i < GUARD + CANVAS_WIDTH * CANVAS_HEIGHT + GUARD; ++i) {
        guarded[i] = GUARD_VALUE;
    }
    canvas->pixels = guarded + GUARD;
    canvas->width = CANVAS_WIDTH;
    canvas->height = CANVAS_HEIGHT;
    tobytank_canvas_clear(canvas, tobytank_rgb565(2, 8, 14));
}

static void expect_guards_intact(void)
{
    for (int i = 0; i < GUARD; ++i) {
        if (guarded[i] != GUARD_VALUE) {
            fail("composite wrote before the canvas");
            return;
        }
        if (guarded[GUARD + CANVAS_WIDTH * CANVAS_HEIGHT + i] != GUARD_VALUE) {
            fail("composite wrote after the canvas");
            return;
        }
    }
}

static void test_invalid_inputs_are_safe(void)
{
    tobytank_genome_t genome;
    expect(tobytank_genome_generate(&genome, 1), "failed to generate a baseline genome");
    tobytank_fish_sprite_t sprite = make_sprite();

    expect(tobytank_fish_rasterize(NULL, &sprite) == 0, "null genome was accepted");
    expect(tobytank_fish_rasterize(&genome, NULL) == 0, "null sprite was accepted");

    tobytank_fish_sprite_t small = sprite;
    small.width = TOBYTANK_FISH_MAX_WIDTH - 1;
    expect(tobytank_fish_rasterize(&genome, &small) == 0, "undersized sprite was accepted");

    genome.identity = TOBYTANK_IDENTITY_INVALID;
    expect(tobytank_fish_rasterize(&genome, &sprite) == 0, "invalid genome was accepted");

    tobytank_composite_sprite(NULL, &sprite, 0, 0);
    tobytank_composite_sprite(&(tobytank_canvas_t){0}, &sprite, 0, 0);
    tobytank_composite_sprite(&(tobytank_canvas_t){guarded, 1, 1}, NULL, 0, 0);
}

static void test_many_genomes_rasterize_deterministically(void)
{
    tobytank_fish_sprite_t sprite = make_sprite();
    for (tobytank_identity_t identity = 1; identity <= SAMPLE_COUNT; ++identity) {
        tobytank_genome_t genome;
        expect(tobytank_genome_generate(&genome, identity), "genome generation failed");
        expect(tobytank_fish_rasterize(&genome, &sprite), "rasterization failed");
        const int opaque = count_opaque(&sprite);
        if (opaque < 450 || opaque > TOBYTANK_FISH_MAX_WIDTH * TOBYTANK_FISH_MAX_HEIGHT / 2) {
            fail("sprite opaque coverage is implausible");
            return;
        }
        const uint64_t first = sprite_digest(&sprite);
        expect(tobytank_fish_rasterize(&genome, &sprite), "repeat rasterization failed");
        if (sprite_digest(&sprite) != first) {
            fail("same genome rasterized differently");
            return;
        }
    }
}

static void test_composite_clips_at_every_edge(void)
{
    tobytank_genome_t genome;
    expect(tobytank_genome_generate(&genome, 42), "genome generation failed");
    tobytank_fish_sprite_t sprite = make_sprite();
    expect(tobytank_portrait_render(&genome, &sprite), "portrait render failed");

    const int positions[][2] = {
        {-40, -20},
        {CANVAS_WIDTH + 30, 20},
        {20, CANVAS_HEIGHT + 20},
        {CANVAS_WIDTH / 2, CANVAS_HEIGHT / 2},
        {0, CANVAS_HEIGHT / 2},
        {CANVAS_WIDTH - 1, CANVAS_HEIGHT / 2},
    };
    for (unsigned i = 0; i < sizeof(positions) / sizeof(positions[0]); ++i) {
        tobytank_canvas_t canvas;
        prepare_guarded_canvas(&canvas);
        tobytank_composite_sprite(&canvas, &sprite, positions[i][0], positions[i][1]);
        expect_guards_intact();
    }
}

static void test_composite_facing_mirrors_horizontally(void)
{
    uint16_t pixels[4] = {
        tobytank_rgb565_pack(255, 0, 0),
        tobytank_rgb565_pack(0, 255, 0),
        0,
        0,
    };
    uint8_t alpha[4] = {255, 255, 0, 0};
    const tobytank_fish_sprite_t sprite = {
        .pixels = pixels,
        .alpha = alpha,
        .width = 4,
        .height = 1,
        .origin_x = 0,
        .origin_y = 0,
    };

    uint16_t canvas_pixels[4];
    const tobytank_canvas_t canvas = {
        .pixels = canvas_pixels,
        .width = 4,
        .height = 1,
    };

    tobytank_canvas_clear(&canvas, tobytank_rgb565(0, 0, 0));
    tobytank_composite_sprite_facing(&canvas, &sprite, 0, 0, -1.0f);
    expect(canvas_pixels[0] == tobytank_rgb565(255, 0, 0),
           "left-facing composite moved the nose pixel");
    expect(canvas_pixels[1] == tobytank_rgb565(0, 255, 0),
           "left-facing composite moved the body pixel");

    tobytank_canvas_clear(&canvas, tobytank_rgb565(0, 0, 0));
    tobytank_composite_sprite_facing(&canvas, &sprite, 0, 0, 1.0f);
    expect(canvas_pixels[2] == tobytank_rgb565(0, 255, 0),
           "right-facing composite did not mirror the body pixel");
    expect(canvas_pixels[3] == tobytank_rgb565(255, 0, 0),
           "right-facing composite did not mirror the nose pixel");
}

static void test_markings_do_not_create_rectangular_alpha_boxes(void)
{
    tobytank_fish_sprite_t sprite = make_sprite();
    for (tobytank_identity_t identity = 1; identity <= 200; ++identity) {
        tobytank_genome_t genome;
        expect(tobytank_genome_generate(&genome, identity), "genome generation failed");

        genome.pattern_type = TOBYTANK_PATTERN_NONE;
        genome.accent_marks = 0;
        expect(tobytank_fish_rasterize(&genome, &sprite), "base rasterization failed");
        memcpy(base_alpha, sprite.alpha,
               sizeof(uint8_t) * TOBYTANK_FISH_MAX_WIDTH * TOBYTANK_FISH_MAX_HEIGHT);

        genome.pattern_type = TOBYTANK_PATTERN_VERTICAL_BARS;
        genome.pattern_contrast = 0.95f;
        genome.pattern_density = 0.9f;
        genome.accent_marks = 4;
        expect(tobytank_fish_rasterize(&genome, &sprite), "marked rasterization failed");
        for (int i = 0; i < sprite.width * sprite.height; ++i) {
            if (base_alpha[i] == 0 && sprite.alpha[i] != 0) {
                fail("markings painted outside the fish alpha mask");
                return;
            }
        }
    }
}

static void test_cache_reuses_and_invalidates_by_fingerprint(void)
{
    tobytank_fish_cache_t cache;
    tobytank_fish_cache_init(&cache, sprite_pixels, sprite_alpha);
    expect(cache.valid == 0, "new cache starts valid");
    expect(tobytank_fish_cache_pixel_bytes() ==
               sizeof(uint16_t) * TOBYTANK_FISH_MAX_WIDTH * TOBYTANK_FISH_MAX_HEIGHT,
           "pixel byte count is wrong");
    expect(tobytank_fish_cache_alpha_bytes() ==
               sizeof(uint8_t) * TOBYTANK_FISH_MAX_WIDTH * TOBYTANK_FISH_MAX_HEIGHT,
           "alpha byte count is wrong");

    tobytank_genome_t first;
    tobytank_genome_t second;
    expect(tobytank_genome_generate(&first, 77), "first genome generation failed");
    expect(tobytank_genome_generate(&second, 78), "second genome generation failed");
    expect(tobytank_fish_cache_prepare(&cache, &first), "cache prepare failed");
    const uint64_t first_digest = sprite_digest(&cache.sprite);
    expect(cache.valid == 1 && cache.identity == first.identity, "cache metadata is wrong");
    expect(tobytank_fish_cache_prepare(&cache, &first), "cache reuse failed");
    expect(sprite_digest(&cache.sprite) == first_digest, "cache reuse changed the sprite");
    expect(tobytank_fish_cache_prepare(&cache, &second), "cache invalidation failed");
    expect(sprite_digest(&cache.sprite) != first_digest, "different genome produced same sprite");
}

static void test_extreme_valid_variants_fit_the_sprite(void)
{
    tobytank_fish_sprite_t sprite = make_sprite();
    int tested = 0;
    for (tobytank_identity_t identity = 1; identity < 20000 && tested < 80; ++identity) {
        tobytank_genome_t genome;
        if (!tobytank_genome_generate(&genome, identity)) {
            continue;
        }
        float width = 0.0f;
        float height = 0.0f;
        tobytank_genome_extent(&genome, &width, &height);
        if (width < 125.0f && height < 80.0f && genome.body_depth < 46.0f) {
            continue;
        }
        expect(tobytank_fish_rasterize(&genome, &sprite), "extreme genome did not rasterize");
        expect(count_opaque(&sprite) > 500, "extreme genome produced an empty sprite");
        ++tested;
    }
    expect(tested >= 40, "not enough extreme genomes were tested");
}

int main(void)
{
    test_invalid_inputs_are_safe();
    test_many_genomes_rasterize_deterministically();
    test_composite_clips_at_every_edge();
    test_composite_facing_mirrors_horizontally();
    test_markings_do_not_create_rectangular_alpha_boxes();
    test_cache_reuses_and_invalidates_by_fingerprint();
    test_extreme_valid_variants_fit_the_sprite();

    if (failures != 0) {
        fprintf(stderr, "fish_rasterizer_host_test failed: %d failure(s)\n", failures);
        return 1;
    }
    puts("fish_rasterizer_host_test passed");
    return 0;
}
