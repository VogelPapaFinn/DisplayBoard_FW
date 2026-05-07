// Project includes
#include "Can.hpp"

// espidf includes
#include "freertos/FreeRTOS.h"
#include <esp_log.h>

/*
 *	constexpr
 */
constexpr auto TAG = "main";

constexpr gpio_num_t GPIO_CAN_RX = GPIO_NUM_39;
constexpr gpio_num_t GPIO_CAN_TX = GPIO_NUM_40;

/*
 *	Private Static Variables
 */
static Can can(GPIO_CAN_RX, GPIO_CAN_TX);
static uint8_t canId = 0;
static QueueHandle_t canQueueHandle = xQueueCreate(10, sizeof(Can::Frame));

bool waitingForWakeUp = false;

/*
 *	Private Functions
 */
void broadcastId()
{
	Can::Frame txFrame;
	txFrame.sender = canId;
	txFrame.target = 0;
	txFrame.group = CanFrame::GROUP::CONFIGURATION;
	txFrame.function = CanFrame::CONFIGURATION::BROADCAST_STARTUP;
	txFrame.answer = 0;

	can.queueFrame(txFrame);
}

void confirmNewId()
{
	Can::Frame txFrame;
	txFrame.sender = canId;
	txFrame.target = 1;
	txFrame.group = CanFrame::GROUP::CONFIGURATION;
	txFrame.function = CanFrame::CONFIGURATION::SET_ID;
	txFrame.answer = 1;

	can.queueFrame(txFrame);
}

/*
 *	Can rx callback function
 */
static void canRxTask(void* param)
{
	Can::Frame rxFrame;
	while (true) {
		if (xQueueReceive(canQueueHandle, &rxFrame, portMAX_DELAY) != pdPASS) {
			continue;
		}

		if (rxFrame.group != CanFrame::GROUP::CONFIGURATION) {
			continue;
		}

		if (rxFrame.sender != 1) {
			continue;
		}

		if (rxFrame.target != canId) {
			continue;
		}

		// Act depending on the function type
		switch (rxFrame.function) {
			case CanFrame::SET_ID:
				{
					esp_rom_printf("SET_ID\n");

					if (rxFrame.target != canId) {
						continue;
					}

					if (rxFrame.dataLengthCode <= 0) {
						continue;
					}

					canId = rxFrame.data[0];

					confirmNewId();
					waitingForWakeUp = true;
					continue;
				}
				break;

			case CanFrame::CONFIRM_ID:
				{
					esp_rom_printf("CONFIRM_ID\n");

					waitingForWakeUp = true;
				}
				break;

			default:
				{
					esp_rom_printf("DEFAULT\n");
				}
				break;
		}
	}
}

/*
 *	main function
 */
extern "C" void app_main(void)
{
	can.initialize();
	can.enable();
	can.registerRxCbQueue(&canQueueHandle);

	TaskHandle_t canRxTaskHandle;
	if (xTaskCreate(canRxTask, "MainCanRxTask", 2048 * 4, NULL, 2, &canRxTaskHandle) != pdPASS) {
		ESP_LOGE(TAG, "Failed to create CAN RX Task. Restarting...");
		esp_restart();
		vTaskDelay(pdMS_TO_TICKS(100000)); // Fallback
	}

	// Broadcast
	broadcastId();

	while (true) {
		vTaskDelay(pdMS_TO_TICKS(1000));
	}
}
