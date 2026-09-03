#include "render/renderer.h"

#include <stdbool.h>
#include <stdint.h>

#include "aquarium/environment.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "hardware/display.h"
#include "render/background.h"
#include "render/canvas.h"
#include "render/effects.h"
#include "render/particles.h"

static const char *TAG = "tobytank_renderer";

/*
 * Fixed scene seed for now. Milestone 2 introduces persistent identity and can
 * derive a per-device seed; until then a constant keeps device output identical
 * to the host preview.
 */
#define TOBYTANK_ENVIRONMENT_SEED 0x546F627954616E6BULL

/* Caps catch-up work if the render loop ever falls far behind the simulation. */
#define TOBYTANK_MAX_STEPS_PER_FRAME 6

/* Large enough to live in static storage rather than on the task stack. */
static tobytank_environment_t s_environment;
static tobytank_env_snapshot_t s_snapshot;

static float s_accumulator;
static int64_t s_last_update_us;
static uint32_t s_frames;
static uint32_t s_window_frames;
static uint32_t s_sim_steps;
static uint32_t s_draw_us;
static uint32_t s_water_us;
static uint32_t s_terrain_us;
static uint32_t s_plants_us;
static uint32_t s_effects_us;
static uint32_t s_particles_us;
static uint32_t s_submit_us;
static uint32_t s_wait_us;
static int64_t s_window_start_us;
static bool s_initialized;

esp_err_t tobytank_renderer_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    tobytank_environment_init(&s_environment, TOBYTANK_ENVIRONMENT_SEED,
                              TOBYTANK_DISPLAY_WIDTH, TOBYTANK_DISPLAY_HEIGHT);
    if (s_environment.scene.width != TOBYTANK_DISPLAY_WIDTH ||
        s_environment.scene.height != TOBYTANK_DISPLAY_HEIGHT) {
        ESP_LOGE(TAG, "Environment generation rejected the display size");
        return ESP_ERR_INVALID_STATE;
    }
    tobytank_environment_snapshot(&s_environment, &s_snapshot);

    s_accumulator = 0.0f;
    s_last_update_us = esp_timer_get_time();
    s_window_start_us = s_last_update_us;
    s_initialized = true;

    ESP_LOGI(TAG, "Aquarium environment ready: seed=0x%016llx, %d plants, %d stones, %d shafts",
             (unsigned long long)s_environment.seed,
             s_snapshot.plant_count, s_snapshot.stone_count, s_snapshot.shaft_count);
    ESP_LOGI(TAG, "Simulation fixed timestep %.4f s, snapshot %u bytes",
             (double)TOBYTANK_ENV_TIMESTEP_SECONDS, (unsigned)sizeof(s_snapshot));
    return ESP_OK;
}

esp_err_t tobytank_renderer_frame(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    const int64_t now_us = esp_timer_get_time();
    float elapsed = (float)(now_us - s_last_update_us) / 1000000.0f;
    s_last_update_us = now_us;
    if (elapsed < 0.0f) {
        elapsed = 0.0f;
    } else if (elapsed > 0.25f) {
        elapsed = 0.25f;
    }
    s_accumulator += elapsed;

    int steps = 0;
    while (s_accumulator >= TOBYTANK_ENV_TIMESTEP_SECONDS &&
           steps < TOBYTANK_MAX_STEPS_PER_FRAME) {
        tobytank_environment_update(&s_environment, TOBYTANK_ENV_TIMESTEP_SECONDS);
        s_accumulator -= TOBYTANK_ENV_TIMESTEP_SECONDS;
        ++steps;
        ++s_sim_steps;
    }
    if (steps == TOBYTANK_MAX_STEPS_PER_FRAME) {
        /* Drop the backlog instead of spiralling; rendering paces simulation. */
        s_accumulator = 0.0f;
    }

    tobytank_environment_snapshot(&s_environment, &s_snapshot);

    const int64_t wait_start_us = esp_timer_get_time();
    uint16_t *frame = tobytank_display_acquire_frame();
    if (frame == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    s_wait_us += (uint32_t)(esp_timer_get_time() - wait_start_us);

    const tobytank_canvas_t canvas = {
        .pixels = frame,
        .width = TOBYTANK_DISPLAY_WIDTH,
        .height = TOBYTANK_DISPLAY_HEIGHT,
    };
    const int64_t draw_start_us = esp_timer_get_time();
    tobytank_background_draw_water(&canvas, &s_snapshot);
    const int64_t after_water_us = esp_timer_get_time();
    tobytank_background_draw_terrain(&canvas, &s_snapshot);
    const int64_t after_terrain_us = esp_timer_get_time();
    tobytank_background_draw_plants(&canvas, &s_snapshot);
    const int64_t after_plants_us = esp_timer_get_time();
    tobytank_effects_draw(&canvas, &s_snapshot);
    const int64_t after_effects_us = esp_timer_get_time();
    tobytank_particles_draw(&canvas, &s_snapshot);
    const int64_t submit_start_us = esp_timer_get_time();
    s_water_us += (uint32_t)(after_water_us - draw_start_us);
    s_terrain_us += (uint32_t)(after_terrain_us - after_water_us);
    s_plants_us += (uint32_t)(after_plants_us - after_terrain_us);
    s_effects_us += (uint32_t)(after_effects_us - after_plants_us);
    s_particles_us += (uint32_t)(submit_start_us - after_effects_us);
    s_draw_us += (uint32_t)(submit_start_us - draw_start_us);

    const esp_err_t ret = tobytank_display_submit_frame(frame);
    s_submit_us += (uint32_t)(esp_timer_get_time() - submit_start_us);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Frame submission failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ++s_frames;
    ++s_window_frames;
    const int64_t after_us = esp_timer_get_time();
    const int64_t window_us = after_us - s_window_start_us;
    if (window_us >= 1000000) {
        const unsigned window_frames = s_window_frames > 0 ? s_window_frames : 1;
        ESP_LOGI(TAG,
                 "render FPS: %.1f, frames=%u, sim_steps=%u, draw=%ums "
                 "(water=%ums terrain=%ums plants=%ums fx=%ums part=%ums), wait=%ums, "
                 "free_internal=%u, free_psram=%u",
                 (double)s_window_frames * 1000000.0 / (double)window_us,
                 (unsigned)s_frames,
                 (unsigned)s_sim_steps,
                 (unsigned)(s_draw_us / window_frames / 1000u),
                 (unsigned)(s_water_us / window_frames / 1000u),
                 (unsigned)(s_terrain_us / window_frames / 1000u),
                 (unsigned)(s_plants_us / window_frames / 1000u),
                 (unsigned)(s_effects_us / window_frames / 1000u),
                 (unsigned)(s_particles_us / window_frames / 1000u),
                 (unsigned)(s_wait_us / window_frames / 1000u),
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
        s_window_start_us = after_us;
        s_window_frames = 0;
        s_draw_us = 0;
        s_water_us = 0;
        s_terrain_us = 0;
        s_plants_us = 0;
        s_effects_us = 0;
        s_particles_us = 0;
        s_submit_us = 0;
        s_wait_us = 0;
    }

    return ESP_OK;
}
