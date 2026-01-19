// Project includes
#include "EventQueues.h"
#include "GUI.h"
#include "Version.h"
#include "can.h"
#include "Managers/RegistrationManager.h"

// FreeRTOS includes
#include "freertos/FreeRTOS.h"

// espidf includes
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>

/*
 *	Main function
 */
void app_main(void) // NOLINT
{
	/*
	 *	Initial Logging
	 */
	ESP_LOGI("main", "--- --- --- --- --- --- ---");
	ESP_LOGI("main", "Firmware Version: %s", VERSION_FULL);

	// Check on which OTA partition we are
	const esp_partition_t* partition = esp_ota_get_running_partition();
	if (partition != NULL) {
		ESP_LOGI("main", "Current running partition: %s", partition->label);
		ESP_LOGI("main", "Type: %d, Subtype: %d", partition->type, partition->subtype);
		ESP_LOGI("main", "Address: 0x%08" PRIx32, partition->address);
		ESP_LOGI("main", "Size: 0x%08" PRIx32 " bytes", partition->size);
	}
	else {
		ESP_LOGE("main", "Couldn't load OTA partition!");
	}

	/*
	 *	Initialization of Drivers etc.
	 */
	// Event Queues
	createEventQueues();

	// CAN
	canInitializeNode(GPIO_NUM_9, GPIO_NUM_6);
	canEnableNode();

	// GUI
	guiInit();

	/*
	 *	Other preparations
	 */
	// Register the queue to the CAN bus
	canRegisterRxCbQueue(&g_mainQueue);

	/*
	 *	Initialization of the registration manager
	 */
	registrationManagerInit();

	// Wait for new queue events
	QueueEvent_t queueEvent;
	while (true) {
		// Wait until we get a new event in the queue
		if (xQueueReceive(g_mainQueue, &queueEvent, portMAX_DELAY) != pdPASS) {
			continue;
		}

		// Skip if it's not a new can frame, or it's not from the master
		if (queueEvent.command != RECEIVED_NEW_CAN_FRAME || (queueEvent.frameId & 0x1FFFFF) != 0) {
			continue;
		}

		// Get the frame id
		const uint8_t frameId = queueEvent.frameId >> CAN_FRAME_ID_OFFSET;

		// The CAN driver crashed
		if (queueEvent.command == CAN_DRIVER_CRASHED) {
			if (canRecoverDriver() == ESP_OK) {
				ESP_LOGI("main", "Recovered CAN driver");

				continue;
			}

			ESP_LOGE("main", "Couldn't recover CAN driver");

			continue;
		}

		// Should we restart?
		if (frameId == CAN_MSG_DISPLAY_RESTART) {
			// Were we meant?
			if (queueEvent.frameBuffer[0] != g_ownCanComId) {
				continue;
			}

			esp_rom_printf("Restarting\n");
			esp_restart();
		}

		break;
	}
}
