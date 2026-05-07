set(CMAKE_C_STANDARD 23)

set(FILES
        # Start of Application
        "main.cpp"

        # Drivers
        "Driver/ST77916.cpp"

        # Gui
        "Gui/Gui.cpp"
)

file(GLOB UI
        "../res/ui/*.h"
        "../res/ui/*.c"
)

idf_component_register(SRCS ${FILES} ${UI}
        PRIV_REQUIRES src driver spi_flash esp_psram lvgl esp_lcd esp_wifi can app_update esp_lcd_st77916 filesystem
        INCLUDE_DIRS "../include/" "../res/")