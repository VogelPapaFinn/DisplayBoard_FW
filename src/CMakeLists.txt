set(CMAKE_C_STANDARD 23)

set(FILES
        # Start of Application
        "main.c"
)

idf_component_register(SRCS ${FILES}
        PRIV_REQUIRES src driver spi_flash esp_psram lvgl esp_lcd esp_lcd_gc9a01
        INCLUDE_DIRS "../include")