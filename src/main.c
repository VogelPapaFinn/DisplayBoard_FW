// Project includes
#include "can.h"

void rxTask(void* arg)
{
	uint32_t notifValue;

	while (true) {
		// Blockiert, bis ISR eine Notification sendet
		xTaskNotifyWait(0, UINT32_MAX, &notifValue, -1);

		printf("RX Task: Nachricht empfangen, Wert = %lu\n", notifValue);
	}
}

void app_main(void)
{
	TaskHandle_t rxTaskHandle = NULL;
	xTaskCreate(rxTask, "RX_TASK", 4096, NULL, 10, &rxTaskHandle);

	ESP_ERROR_CHECK(initializeCanNode(GPIO_NUM_9, GPIO_NUM_6) == NULL);

	ESP_ERROR_CHECK(registerMessageReceivedCb(&rxTaskHandle) == false);

	ESP_ERROR_CHECK(enableCanNode() == false);

	while (1) {
		vTaskDelay(pdMS_TO_TICKS(100));
	}
}
