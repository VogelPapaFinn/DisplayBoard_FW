// Project includes
#include "Can.hpp"
#include "Core.hpp"
#include "Event.hpp"
#include "State/Registration.hpp"

// espidf includes
#include "freertos/FreeRTOS.h"
#include <esp_log.h>

/*
 *	constexpr
 */
constexpr auto TAG = "main";

/*
 *	Private Static Variables
 */
static Core* core = nullptr;

static QueueHandle_t canQueueHandle = xQueueCreate(10, sizeof(Can::Frame));

static QueueHandle_t mainEventQueueHandle = xQueueCreate(20, sizeof(Event));

static std::shared_ptr<State> currentState;

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

		currentState->handleCanFrame(rxFrame);
	}
}

static void eventQueueTask(void* param)
{
	Event event;
	while (true) {
		if (xQueueReceive(mainEventQueueHandle, &event, portMAX_DELAY) != pdPASS) {
			continue;
		}

		switch (event.type) {
			case Event::UNKNOWN:
				break;
			case Event::SET_SCREEN:
				{
					core->getGui()->setScreen(event.data);
				}
				break;
			case Event::WAKE_UP:
				{
					core->getDisplayDriver()->setBacklightLevel(60);
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
	// MUSS BESTEHEN BLEIBEN
	// Filesystem* fs = Filesystem::get(false, true);
	vTaskDelay(pdMS_TO_TICKS(500));

	core = Core::get();
	core->setMainEventQueue(mainEventQueueHandle);
	core->getCan()->registerRxCbQueue(&canQueueHandle);

	TaskHandle_t canRxTaskHandle;
	if (xTaskCreate(canRxTask, "MainCanRxTask", 2048 * 4, NULL, 2, &canRxTaskHandle) != pdPASS) {
		ESP_LOGE(TAG, "Failed to create CAN RX Task. Restarting...");
		esp_restart();
		vTaskDelay(pdMS_TO_TICKS(100000)); // Fallback
	}

	TaskHandle_t eventQueueHandle;
	if (xTaskCreate(eventQueueTask, "MainEventQueueTask", 8048 * 4, NULL, 2, &eventQueueHandle) != pdPASS) {
		ESP_LOGE(TAG, "Failed to create Event Queue Task. Restarting...");
		esp_restart();
		vTaskDelay(pdMS_TO_TICKS(100000)); // Fallback
	}

	currentState = std::make_shared<Registration>();
	currentState->enter();

	while (true) {
		vTaskDelay(pdMS_TO_TICKS(1000));
	}
}
