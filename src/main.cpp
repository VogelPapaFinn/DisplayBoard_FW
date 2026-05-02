// Project includes
#include "Driver/ST77916.h"

// espidf includes
#include <esp_log.h>
#include "freertos/FreeRTOS.h"

extern "C" void app_main(void)
{
	ST77916 lcd;
	lcd.setBacklightLevel(50);

	while (true) {
		vTaskDelay(pdMS_TO_TICKS(500));
	}
}