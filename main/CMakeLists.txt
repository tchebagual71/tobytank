idf_component_register(
    SRCS
        "app_main.c"
        "aquarium/environment.c"
        "diagnostics/boot_diagnostics.c"
        "hardware/board.c"
        "hardware/display.c"
        "render/background.c"
        "render/canvas.c"
        "render/effects.c"
        "render/particles.c"
        "render/renderer.c"
    INCLUDE_DIRS
        "."
    REQUIRES
        esp32_s3_touch_amoled_1_8
        esp_app_format
        esp_psram
        esp_system
        spi_flash
        esp_timer
        esp_lcd
        esp_lcd_touch
)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
