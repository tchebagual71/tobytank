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
            "TOBYTANK_ENV_TIMESTEP_SECONDS",
            "TOBYTANK_MAX_STEPS_PER_FRAME",
            "tobytank_background_draw",
            "tobytank_effects_draw",
            "tobytank_particles_draw",
            "tobytank_display_acquire_frame",
            "tobytank_display_submit_frame",
            "render FPS",
        ):
            self.assertIn(token, renderer)
        # Milestone 1 is the empty tank; no visitor logic yet.
        self.assertNotIn("fish", renderer.lower())
        for forbidden in ("malloc(", "calloc(", "heap_caps_malloc", "heap_caps_aligned_alloc"):
            self.assertNotIn(forbidden, renderer,
                             "the frame loop must not allocate")

    def test_simulation_and_rasterizers_are_host_portable(self) -> None:
        for relative_path in (
            "main/aquarium/environment.c",
            "main/aquarium/environment.h",
            "main/render/canvas.c",
            "main/render/background.c",
            "main/render/effects.c",
            "main/render/particles.c",
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
        self.assertIn("splitmix64", source)
        for forbidden in ("rand()", "srand(", "time(", "clock("):
            self.assertNotIn(forbidden, source,
                             "environment generation must be seeded, not ambient")
        self.assertNotIn("fish", source.lower())

    def test_milestone_one_sources_are_registered(self) -> None:
        cmake = self.read_required("main/CMakeLists.txt")
        for source in (
            "aquarium/environment.c",
            "render/background.c",
            "render/effects.c",
            "render/particles.c",
        ):
            self.assertIn(source, cmake)

    def test_host_preview_output_is_dependency_free(self) -> None:
        ppm = self.read_required("tools/preview/ppm.c")
        preview = self.read_required("tools/preview/preview_main.c")
        self.assertIn("P6", ppm)
        self.assertIn("tobytank_ppm_encode", preview)
        self.assertIn("tobytank_environment_update", preview)
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
