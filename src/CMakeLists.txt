set(CMAKE_C_STANDARD 23)

set(FILES
        # Start of Application
        "main.c"

        # GUI
        "../include/GUI.h"
        "GUI.c"

        # Screens
        "../include/Screens/LvglTemperatureScreen.h"
        "Screens/LvglTemperatureScreen.c"
        "../include/Screens/LvglRpmScreen.h"
        "Screens/LvglRpmScreen.c"
        "../include/Screens/LvglSpeedScreen.h"
        "Screens/LvglSpeedScreen.c"

        # Update Handler
        "../include/UpdateHandler.h"
        "UpdateHandler.c"

        # Event Queue
        "../include/EventQueues.h"
        "EventQueues.c"


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
        PRIV_REQUIRES src driver spi_flash esp_psram lvgl esp_lcd esp_lcd_gc9a01 esp_wifi nvs_flash can app_update
        INCLUDE_DIRS "../include/")