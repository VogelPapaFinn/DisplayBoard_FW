set(CMAKE_C_STANDARD 23)

set(FILES
        # Start of Application
        "main.c"

        # GUI
        "../include/GUI.h"
        "GUI.c"

        # Global stuff
        "../include/Global.h"
        "Global.c"


        # *** RESOURCES *** #
        # Fonts
        "../res/fonts/c/E1234_80.c"
        "../res/fonts/c/E1234_70.c"
        "../res/fonts/c/VCR_OSD_MONO_24.c"
)

idf_component_register(SRCS ${FILES}
        PRIV_REQUIRES src driver spi_flash esp_psram lvgl esp_lcd esp_lcd_gc9a01 esp_wifi nvs_flash can logger statemachine
        INCLUDE_DIRS "../include/")