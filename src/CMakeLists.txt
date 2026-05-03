set(CMAKE_C_STANDARD 23)

set(FILES
        # Start of Application
        "main.cpp"

        # Drivers
        "Driver/ST77916.cpp"

        # Gui
        "Gui/Gui.cpp"

        # *** RESOURCES *** #
        # Fonts
        "../res/fonts/c/E1234_80.c"
        "../res/fonts/c/E1234_70.c"
        "../res/fonts/c/VCR_OSD_MONO_24.c"

        # Images
        "../res/img/c/leftIndicator.c"
        "../res/img/c/rightIndicator.c"
)

idf_component_register(SRCS ${FILES}
        PRIV_REQUIRES src driver spi_flash esp_psram lvgl esp_lcd esp_wifi can app_update esp_lcd_st77916 filesystem
        INCLUDE_DIRS "../include/")