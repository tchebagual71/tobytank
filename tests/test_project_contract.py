from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[1]


class ProjectContractTest(unittest.TestCase):
    def read_required(self, relative_path: str) -> str:
        path = ROOT / relative_path
        self.assertTrue(path.is_file(), f"required file is missing: {relative_path}")
        return path.read_text(encoding="utf-8")

    def test_project_identity_and_bsp_dependency(self) -> None:
        project = self.read_required("CMakeLists.txt")
        manifest = self.read_required("main/idf_component.yml")

        self.assertIn("project(tobytank)", project)
        self.assertIn('idf: ">=5.5,<5.6"', manifest)
        self.assertIn("waveshare/esp32_s3_touch_amoled_1_8:", manifest)
        self.assertIn('version: "^2.0.3"', manifest)
        self.assertIn("public: true", manifest)

    def test_board_memory_and_flash_defaults(self) -> None:
        defaults = self.read_required("sdkconfig.defaults")
        for setting in (
            'CONFIG_IDF_TARGET="esp32s3"',
            "CONFIG_ESPTOOLPY_FLASHMODE_QIO=y",
            "CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y",
            "CONFIG_PARTITION_TABLE_CUSTOM=y",
            'CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"',
            "CONFIG_COMPILER_OPTIMIZATION_PERF=y",
            "CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_240=y",
            "CONFIG_SPIRAM=y",
            "CONFIG_SPIRAM_MODE_OCT=y",
            "CONFIG_SPIRAM_SPEED_80M=y",
        ):
            self.assertIn(setting, defaults)

    def test_partition_layout_fits_16mb_flash(self) -> None:
        partitions = self.read_required("partitions.csv")
        self.assertIn("nvs,      data, nvs,     0x9000,  0x6000", partitions)
        self.assertIn("phy_init, data, phy,     0xf000,  0x1000", partitions)
        self.assertIn("factory,  app,  factory, 0x10000, 8M", partitions)
        self.assertIn("storage,  data, spiffs,          , 4M", partitions)
        self.assertLessEqual(0x10000 + 8 * 1024 * 1024 + 4 * 1024 * 1024,
                             16 * 1024 * 1024)

    def test_display_contract_and_buffer_sizes(self) -> None:
        display_header = self.read_required("main/hardware/display.h")
        display_source = self.read_required("main/hardware/display.c")
        board_source = self.read_required("main/hardware/board.c")

        self.assertIn("#define TOBYTANK_DISPLAY_WIDTH 368", display_header)
        self.assertIn("#define TOBYTANK_DISPLAY_HEIGHT 448", display_header)
        self.assertIn("#define TOBYTANK_DISPLAY_BITS_PER_PIXEL 16", display_header)
        self.assertIn("#define TOBYTANK_DISPLAY_BAND_ROWS 16", display_header)
        self.assertIn("#define TOBYTANK_DISPLAY_BUFFER_COUNT 2", display_header)

        width = int(re.search(r"TOBYTANK_DISPLAY_WIDTH (\d+)", display_header).group(1))
        height = int(re.search(r"TOBYTANK_DISPLAY_HEIGHT (\d+)", display_header).group(1))
        band_rows = int(re.search(r"TOBYTANK_DISPLAY_BAND_ROWS (\d+)", display_header).group(1))
        self.assertEqual(width * height * 2, 329728)
        self.assertEqual(width * band_rows * 2, 11776)

        for api in (
            "bsp_touch_new",
            "bsp_display_new",
            "bsp_display_brightness_set",
            "esp_lcd_panel_io_register_event_callbacks",
            "esp_lcd_panel_draw_bitmap",
            "MALLOC_CAP_SPIRAM",
            "MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL",
            "heap_caps_aligned_alloc",
        ):
            self.assertIn(api, display_source)
        self.assertLess(display_source.find("bsp_touch_new"),
                        display_source.find("bsp_display_new"))
        for forbidden in ("GPIO_NUM_", "CO5300_PANEL_", "esp_lcd_panel_io_tx_param",
                          "esp_lcd_new_panel_co5300", "esp_lcd_panel_set_gap",
                          "0x2A", "0x2B"):
            self.assertNotIn(forbidden, display_source)
            self.assertNotIn(forbidden, board_source)

    def test_diagnostics_report_required_runtime_facts(self) -> None:
        diagnostics = self.read_required("main/diagnostics/boot_diagnostics.c")
        for token in (
            "esp_app_get_description",
            "esp_chip_info",
            "esp_flash_get_size",
            "esp_psram_is_initialized",
            "esp_psram_get_size",
            "BSP_LCD_H_RES",
            "BSP_LCD_V_RES",
            "BSP_LCD_BITS_PER_PIXEL",
            "heap_caps_get_free_size(MALLOC_CAP_INTERNAL)",
            "heap_caps_get_free_size(MALLOC_CAP_SPIRAM)",
        ):
            self.assertIn(token, diagnostics)

    def test_renderer_drives_a_fixed_timestep_simulation(self) -> None:
        renderer = self.read_required("main/render/renderer.c")
        for token in (
            "tobytank_environment_init",
            "tobytank_environment_update",
            "tobytank_environment_snapshot",
            "tobytank_lifecycle_init",
            "tobytank_lifecycle_update",
            "tobytank_lifecycle_snapshot",
            "tobytank_interactions_update",
            "TOBYTANK_ENV_TIMESTEP_SECONDS",
            "TOBYTANK_MAX_STEPS_PER_FRAME",
            "tobytank_fish_cache_prepare",
            "tobytank_composite_sprite",
            "tobytank_background_draw",
            "tobytank_effects_draw",
            "tobytank_particles_draw",
            "tobytank_display_acquire_frame",
            "tobytank_display_submit_frame",
            "render FPS",
        ):
            self.assertIn(token, renderer)
        frame_loop = renderer[renderer.index("esp_err_t tobytank_renderer_frame"):]
        for forbidden in ("malloc(", "calloc(", "heap_caps_malloc", "heap_caps_aligned_alloc"):
            self.assertNotIn(forbidden, frame_loop,
                             "the frame loop must not allocate")

    def test_simulation_and_rasterizers_are_host_portable(self) -> None:
        for relative_path in (
            "main/aquarium/environment.c",
            "main/aquarium/environment.h",
            "main/aquarium/interactions.c",
            "main/aquarium/interactions.h",
            "main/aquarium/lifecycle.c",
            "main/aquarium/lifecycle.h",
            "main/input/motion_filter.c",
            "main/input/motion_filter.h",
            "main/render/canvas.c",
            "main/render/background.c",
            "main/render/effects.c",
            "main/render/particles.c",
            "main/render/composite.c",
            "main/render/composite.h",
            "main/render/dither.c",
            "main/render/dither.h",
            "main/render/fish_cache.c",
            "main/render/fish_cache.h",
            "main/render/fish_rasterizer.c",
            "main/render/fish_rasterizer.h",
            "main/render/fish_sprite.h",
            "main/fish/prng.c",
            "main/fish/prng.h",
            "main/fish/identity.c",
            "main/fish/identity.h",
            "main/fish/behavior.c",
            "main/fish/behavior.h",
            "main/fish/genome.c",
            "main/fish/genome.h",
            "main/fish/genome_validate.c",
            "main/fish/genome_validate.h",
            "main/fish/motion.c",
            "main/fish/motion.h",
            "main/fish/portrait.c",
            "main/fish/portrait.h",
            "main/sim/snapshot.c",
            "main/sim/snapshot.h",
        ):
            source = self.read_required(relative_path)
            for forbidden in ('#include "esp_', '#include "freertos', '#include "bsp/',
                              "ESP_LOG", "esp_err_t"):
                self.assertNotIn(forbidden, source,
                                 f"{relative_path} must stay host portable")

    def test_environment_generation_is_deterministic_and_bounded(self) -> None:
        header = self.read_required("main/aquarium/environment.h")
        source = self.read_required("main/aquarium/environment.c")
        for token in (
            "TOBYTANK_ENV_TIMESTEP_SECONDS",
            "TOBYTANK_ENV_MAX_BUBBLES",
            "TOBYTANK_ENV_MAX_MOTES",
            "TOBYTANK_ENV_MAX_PLANTS",
            "TOBYTANK_ENV_MAX_STONES",
            "TOBYTANK_ENV_MAX_SHAFTS",
            "tobytank_env_snapshot_t",
            "tobytank_environment_snapshot",
        ):
            self.assertIn(token, header)
        self.assertIn("tobytank_prng_seed_raw", source)
        for forbidden in ("rand()", "srand(", "time(", "clock("):
            self.assertNotIn(forbidden, source,
                             "environment generation must be seeded, not ambient")

    def test_milestone_sources_are_registered(self) -> None:
        cmake = self.read_required("main/CMakeLists.txt")
        for source in (
            "aquarium/environment.c",
            "aquarium/interactions.c",
            "aquarium/lifecycle.c",
            "fish/behavior.c",
            "render/background.c",
            "render/effects.c",
            "render/particles.c",
            "fish/prng.c",
            "fish/identity.c",
            "fish/motion.c",
            "fish/genome.c",
            "fish/genome_validate.c",
            "fish/portrait.c",
            "input/imu.c",
            "input/motion_filter.c",
            "input/touch.c",
            "memory/identity_store.c",
            "render/composite.c",
            "render/dither.c",
            "render/fish_cache.c",
            "render/fish_rasterizer.c",
            "sim/snapshot.c",
        ):
            self.assertIn(source, cmake)
        self.assertIn("nvs_flash", cmake)

    def test_one_shared_deterministic_generator(self) -> None:
        # Two PRNG implementations would let the aquarium and the fish drift
        # apart, so the environment must use the shared streams module.
        environment = self.read_required("main/aquarium/environment.c")
        self.assertIn("tobytank_prng_", environment)
        self.assertNotIn("static uint64_t splitmix64", environment)
        self.assertNotIn("static uint64_t next_u64", environment)

        prng = self.read_required("main/fish/prng.c")
        self.assertIn("splitmix64", prng)
        for forbidden in ("rand()", "srand(", "time(", "clock(", "esp_random"):
            self.assertNotIn(forbidden, prng)

    def test_genome_covers_the_specified_trait_groups(self) -> None:
        header = self.read_required("main/fish/genome.h")
        for trait in (
            "body_length", "body_depth", "front_taper", "rear_taper",
            "back_curve", "belly_curve", "peduncle_depth",
            "eye_size", "mouth_size", "gill_offset",
            "caudal_type", "dorsal_height", "anal_height",
            "pelvic_size", "pectoral_size", "fin_rays", "membrane_softness",
            "base_hue", "iridescence", "scale_contrast",
            "pattern_type", "accent_marks",
            "swim_cadence", "preferred_speed", "turn_response",
            "curiosity", "depth_preference", "hover_tendency",
        ):
            self.assertIn(trait, header, f"genome is missing {trait}")
        self.assertIn("TOBYTANK_GENOME_MAX_ATTEMPTS", header)
        self.assertIn("tobytank_genome_fingerprint", header)

    def test_identity_uniqueness_is_block_reserved(self) -> None:
        header = self.read_required("main/fish/identity.h")
        store = self.read_required("main/memory/identity_store.c")
        self.assertIn("TOBYTANK_IDENTITY_BLOCK_SIZE", header)
        self.assertIn("TOBYTANK_IDENTITY_INVALID", header)
        # The counter must be persisted before any identity from the block is
        # used, otherwise power loss could hand the same identity out twice.
        reserve = store[store.index("static esp_err_t reserve_block"):]
        self.assertLess(reserve.index("write_counter"), reserve.index("s_allocator = candidate"))
        self.assertIn("nvs_commit", store)
        self.assertIn("history_lost", store)
        # NVS recovery erases only the NVS partition, never the whole flash.
        self.assertIn("nvs_flash_erase", store)
        self.assertNotIn("esp_flash_erase_chip", store)

    def test_fish_rasterizer_is_sprite_cached_and_clipped(self) -> None:
        rasterizer = self.read_required("main/render/fish_rasterizer.c")
        cache = self.read_required("main/render/fish_cache.c")
        composite = self.read_required("main/render/composite.c")
        test = self.read_required("tests/fish_rasterizer_host_test.c")
        for token in (
            "tobytank_fish_rasterize",
            "tobytank_genome_validate",
            "TOBYTANK_FISH_MAX_WIDTH",
            "TOBYTANK_FISH_MAX_HEIGHT",
            "sprite->alpha",
        ):
            self.assertIn(token, rasterizer)
        self.assertIn("tobytank_fish_cache_prepare", cache)
        self.assertIn("tobytank_composite_sprite", composite)
        self.assertIn("tobytank_composite_sprite_facing", composite)
        self.assertIn("composite wrote before the canvas", test)
        self.assertIn("right-facing composite did not mirror the nose pixel", test)
        self.assertIn("markings painted outside the fish alpha mask", test)
        self.assertIn("extreme genome", test)
        self.assertIn("same genome rasterized differently", test)

    def test_visitor_lifecycle_is_single_fish_and_deterministic(self) -> None:
        lifecycle = self.read_required("main/aquarium/lifecycle.c")
        motion = self.read_required("main/fish/motion.c")
        snapshot = self.read_required("main/sim/snapshot.c")
        test = self.read_required("tests/lifecycle_host_test.c")
        for token in (
            "TOBYTANK_VISITOR_EMPTY",
            "TOBYTANK_VISITOR_ENTERING",
            "TOBYTANK_VISITOR_EXPLORING",
            "TOBYTANK_VISITOR_EXITING",
            "tobytank_identity_source_fn",
            "tobytank_genome_generate",
            "tobytank_behavior_plan",
            "tobytank_motion_update",
            "tobytank_lifecycle_snapshot",
        ):
            self.assertIn(token, lifecycle)
        self.assertIn("OFFSCREEN_MARGIN", motion)
        self.assertIn("tobytank_visitor_state_name", snapshot)
        self.assertIn("two lifecycles with the same seed and identities diverged", test)
        self.assertIn("a second visitor appeared before the first cleared", test)
        self.assertIn("exploration state was never observed", test)

    def test_touch_imu_and_interactions_are_bounded(self) -> None:
        display = self.read_required("main/hardware/display.c")
        touch = self.read_required("main/input/touch.c")
        imu = self.read_required("main/input/imu.c")
        filter_source = self.read_required("main/input/motion_filter.c")
        interactions = self.read_required("main/aquarium/interactions.c")
        test = self.read_required("tests/interactions_host_test.c")

        self.assertIn("tobytank_display_touch_handle", display)
        self.assertIn("esp_lcd_touch_read_data", touch)
        self.assertIn("esp_lcd_touch_get_data", touch)
        self.assertNotIn("esp_lcd_touch_get_coordinates", touch)
        self.assertIn("BSP_CAPS_IMU", imu)
        self.assertIn("IMU unavailable", imu)
        for forbidden in ("QMI8658", "GPIO_NUM_", "0x6A", "0x6B"):
            self.assertNotIn(forbidden, imu)
        for token in (
            "TOBYTANK_TOUCH_EVENT_DOWN",
            "TOBYTANK_TOUCH_EVENT_DRAG",
            "TOBYTANK_TOUCH_EVENT_TAP",
            "tobytank_imu_filter_update",
        ):
            self.assertIn(token, filter_source)
        for token in (
            "ripple_active",
            "current_x",
            "parallax_x",
            "attention_strength",
        ):
            self.assertIn(token, interactions)
        self.assertIn("same touch stream diverged", test)
        self.assertIn("interaction output escaped bounds", test)
        self.assertIn("touch interaction admitted a second fish", test)
        self.assertIn("interaction ripple wrote outside the canvas", test)

    def test_host_preview_output_is_dependency_free(self) -> None:
        ppm = self.read_required("tools/preview/ppm.c")
        preview = self.read_required("tools/preview/preview_main.c")
        contact_sheet = self.read_required("tools/preview/contact_sheet_main.c")
        self.assertIn("P6", ppm)
        self.assertIn("tobytank_ppm_encode", preview)
        self.assertIn("tobytank_environment_update", preview)
        self.assertIn("tobytank_lifecycle_update", preview)
        self.assertIn("tobytank_lifecycle_snapshot", preview)
        self.assertIn("tobytank_interactions_update", preview)
        self.assertIn("tobytank_effects_draw_interactions", preview)
        self.assertIn("tobytank_fish_cache_prepare", contact_sheet)
        gitignore = self.read_required(".gitignore")
        self.assertIn("tools/preview/out/", gitignore)

    def test_generated_idf_files_are_ignored(self) -> None:
        gitignore = self.read_required(".gitignore")
        for generated_path in (
            "build/",
            "managed_components/",
            "sdkconfig",
            "sdkconfig.old",
            "dependencies.lock",
            "*.bin",
            "*.elf",
            "*.map",
        ):
            self.assertIn(generated_path, gitignore)

    def test_documentation_exists_and_tracks_milestone_zero(self) -> None:
        for relative_path in ("AGENTS.md", "PRODUCT_SPEC.md", "ROADMAP.md", "DEVELOPMENT.md", "README.md"):
            text = self.read_required(relative_path)
            self.assertGreater(len(text.strip()), 200, f"{relative_path} is unexpectedly small")


if __name__ == "__main__":
    unittest.main()
