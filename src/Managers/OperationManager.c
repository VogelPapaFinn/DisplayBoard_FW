#include "Managers/OperationManager.h"

// Project includes
#include "Managers/CanUpdateManager.h"
#include "Managers/RegistrationManager.h"
#include "Version.h"
#include "can.h"

// espidf includes
#include <esp_log.h>

// FreeRTOS include
#include "freertos/FreeRTOS.h"

/*
 *	Private variables
 */
//! \brief Task handle of the CAN task
static TaskHandle_t g_taskHandle;

/*
 *	Prototypes
 */

/*
 *	Tasks
 */
//! \brief Task used to receive and handle CAN frames
//! \param p_param Unused parameters
static void canTask(void* p_param)
{
	// Wait for new queue events
	twai_frame_t rxFrame;
	while (true) {
		// Wait until we get a new event in the queue
		if (xQueueReceive(g_operationManagerCanQueue, &rxFrame, portMAX_DELAY) != pdPASS) {
			continue;
		}

		// Skip if it's not from the master
		if ((rxFrame.header.id & 0x1FFFFF) != 0) {
			continue;
		}

		// Get the frame id
		const uint8_t frameId = rxFrame.header.id >> CAN_FRAME_ID_OFFSET;

		/*
		 *	Broadcasts
		 */
		// New sensor data
		if (frameId == CAN_MSG_SENSOR_DATA) {
			// Create the event
			QueueEvent_t event;
			event.command = NEW_SENSOR_DATA;
			event.canFrame = rxFrame;

			// Queue the event
			xQueueSend(g_guiEventQueue, &event, pdMS_TO_TICKS(100));
			continue;
		}

		/*
		 *	Com id specific frames
		 */
		// Skip if we were not meant
		if (rxFrame.header.dlc == 0 || (rxFrame.header.dlc > 0 && *rxFrame.buffer != g_ownCanComId)) {
			continue;
		}

		// Request of the firmware version
		if (frameId == CAN_MSG_REQUEST_FIRMWARE_VERSION) {
			// Create the CAN answer frame
			TwaiFrame_t frame;

			// Set the buffer content
			frame.buffer[0] = VERSION_BETA;
			frame.buffer[1] = (uint8_t)*VERSION_MAJOR;
			frame.buffer[2] = (uint8_t)*VERSION_MINOR;
			frame.buffer[3] = (uint8_t)*VERSION_PATCH;

			// Initiate the frame
			canInitiateFrame(&frame, CAN_MSG_REQUEST_FIRMWARE_VERSION, 4);

			// Send the frame
			canQueueFrame(&frame);

			ESP_LOGI("OperationManager", "Send firmware version to the Sensor Board!");
			continue;
		}

		// Was it a git commit hash request?
		if (frameId == CAN_MSG_REQUEST_COMMIT_INFORMATION) {
			// Create the CAN answer frame
			TwaiFrame_t frame;

			// Set the buffer content
			frame.buffer[0] = (uint8_t)GIT_HASH[0];
			frame.buffer[1] = (uint8_t)GIT_HASH[1];
			frame.buffer[2] = (uint8_t)GIT_HASH[2];
			frame.buffer[3] = (uint8_t)GIT_HASH[3];
			frame.buffer[4] = (uint8_t)GIT_HASH[4];
			frame.buffer[5] = (uint8_t)GIT_HASH[5];
			frame.buffer[6] = (uint8_t)GIT_HASH[6];
			frame.buffer[7] = sizeof(GIT_HASH) > 7 ? 1 : 0; // If its size is longer than 7 it's a dirty commit!

			// Initiate the frame
			canInitiateFrame(&frame, CAN_MSG_REQUEST_COMMIT_INFORMATION, 8);

			// Send the frame
			canQueueFrame(&frame);

			ESP_LOGI("OperationManager", "Send hash information to the Sensor Board!");
			continue;
		}
	}
}

/*
 *	Public function implementations
 */
bool operationManagerInit()
{
	// Register to the CAN rx cb
	if (!canRegisterRxCbQueue(&g_operationManagerCanQueue)) {
		ESP_LOGE("OperationManager", "Couldn't register rx cb queue");

		return false;
	}

	// Start the can task
	if (xTaskCreate(canTask, "OperationManagerCanTask", 2048 * 4, NULL, 0, &g_taskHandle) != pdPASS) {
		ESP_LOGE("OperationManager", "Couldn't create CAN task!");

		return false;
	}

	// Destroy the registration manager
	registrationManagerDestroy();

	// Initiate the CAN Update Manager
	canUpdateManagerInit();

	return true;
}

void operationManagerDestroy()
{
	// Unregister from the CAN rx cb
	canUnregisterRxCbQueue(&g_operationManagerCanQueue);

	// Destroy the CAN task
	vTaskDelete(g_taskHandle);
}
