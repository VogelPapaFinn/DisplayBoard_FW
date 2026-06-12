// Project includes
#include "Can.hpp"
#include "Core.hpp"
#include "Event.hpp"
#include "State/Operation.hpp"
#include "State/Registration.hpp"
#include "wifi/WifiJoin.hpp"

// espidf includes
#include <esp_log.h>
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"

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

		if (rxFrame.group == CanFrame::CONFIGURATION && rxFrame.function == CanFrame::CONFIGURATION::RESTART) {
			esp_restart();
		}

		currentState->handleCanFrame(rxFrame);
	}
}

static void mainEventTask(void* param)
{
	Event event;
	while (true) {
		if (xQueueReceive(mainEventQueueHandle, &event, portMAX_DELAY) != pdPASS) {
			continue;
		}

		// Act depending on the event
		switch (event.type) {
			case Event::REGISTRATION_FINISHED:
				{
					currentState = std::make_shared<Operation>();
					currentState->enter();
				}
				break;

			case Event::JOIN_WIFI:
				{
					auto wifi = core->getWifi();

					// Connect to AP
					wifi->callOnSuccess(
						[]
						{
							Can::Frame txFrame;
							txFrame.sender = core->getCanId();
							;
							txFrame.target = CAN_MASTER_ID;
							txFrame.group = CanFrame::GROUP::WIFI;
							txFrame.function = CanFrame::WIFI::JOIN_WIFI;
							txFrame.answer = true;

							Core::get()->getCan()->queueFrame(txFrame);
						});

					wifi->start();
				}
				break;

			case Event::EXECUTE_UPDATE:
				{
					ESP_LOGI(TAG, "Starting OTA update");

					// Get the master ip address
					const auto& masterIp = core->getWifi()->getMasterIp();
					std::string downloadPath = std::format("http://{}.{}.{}.{}/display-update.bin", masterIp[0], masterIp[1], masterIp[2], masterIp[3]);

					esp_http_client_config_t config = {
						.url = downloadPath.c_str(),
						.timeout_ms = 5000,
						.keep_alive_enable = true,
					};

					esp_https_ota_config_t otaConfig = {
						.http_config = &config,
					};

					esp_err_t ret = esp_https_ota(&otaConfig);
					if (ret == ESP_OK) {
						ESP_LOGI(TAG, "OTA Update sucessful!");

						Can::Frame txFrame;
						txFrame.sender = core->getCanId();
						txFrame.target = CAN_MASTER_ID;
						txFrame.group = CanFrame::GROUP::WIFI;
						txFrame.function = CanFrame::WIFI::EXECUTE_UPDATE;
						txFrame.answer = 1;

						Core::get()->getCan()->queueFrame(txFrame);
					}
					else {
						ESP_LOGE(TAG, "OTA Update error: %s", esp_err_to_name(ret));
					}
				}
				break;

			default:;
		}
	}
}

/*
 *	main function
 */
extern "C" void app_main(void)
{
	// MUSS STEHEN BLEIBEN FUERS DEBUGGING
	vTaskDelay(pdMS_TO_TICKS(500));

	core = Core::get();
	core->setMainEventQueue(mainEventQueueHandle);
	core->getCan()->registerRxCbQueue(&canQueueHandle);

	TaskHandle_t canRxTaskHandle;
	if (xTaskCreate(canRxTask, "MainCanRxTask", 2048 * 4, NULL, 5, &canRxTaskHandle) != pdPASS) {
		ESP_LOGE(TAG, "Failed to create CAN RX Task. Restarting...");
		esp_restart();
		vTaskDelay(pdMS_TO_TICKS(100000)); // Fallback
	}

	if (xTaskCreate(mainEventTask, "MainEventTask", 4096, NULL, 2, NULL) != pdPASS) {
		ESP_LOGE(TAG, "Failed to create main event task");
		esp_restart();
		vTaskDelay(pdMS_TO_TICKS(100000)); // Fallback
	}

	currentState = std::make_shared<Registration>();
	currentState->enter();

	while (true) {
		vTaskDelay(pdMS_TO_TICKS(1000));
	}
}
