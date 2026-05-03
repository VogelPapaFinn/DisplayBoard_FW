// Project includes
#include "Driver/ST77916.hpp"
#include "Gui/Gui.hpp"

// espidf includes
#include "freertos/FreeRTOS.h"
#include <esp_log.h>

extern "C" void app_main(void)
{
	ST77916 lcd;
	lcd.setBacklightLevel(50);

	Gui gui(&lcd);

	while (true) {
		vTaskDelay(pdMS_TO_TICKS(10));
	}
}