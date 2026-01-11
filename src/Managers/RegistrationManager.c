#include "Managers/RegistrationManager.h"

// Project includes
#include "Managers/OperationManager.h"
#include "can.h"

// espidf includes
#include <esp_log.h>
#include <esp_mac.h>

// FreeRTOS include
#include "freertos/FreeRTOS.h"

/*
 *	Private defines
 */
#define MAC_ADDRESS_LENGTH 6

/*
 *	Prototypes
 */
static bool doesMacMatch(const uint8_t* p_buffer);

static void broadcastUuid();

static void pullMacAddress();

/*
 *	External Variables
 */
uint8_t g_ownCanComId = 0;

/*
 *	Private variables
 */
//! \brief Task handle of the CAN task
static TaskHandle_t g_taskHandle;

// The MAC address
static uint8_t g_macAddress[MAC_ADDRESS_LENGTH];

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
		if (xQueueReceive(g_registrationManagerQueue, &rxFrame, portMAX_DELAY) != pdPASS) {
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
		// Was it an instruction to register ourselves?
		if (frameId == CAN_MSG_REGISTRATION) {
			// Send the HW UUID
			broadcastUuid();

			continue;
		}

		// Was it a new ID?
		if (frameId == CAN_MSG_COMID_ASSIGNATION) {
			// Check if the HW UUID matches
			if (rxFrame.header.dlc < 7 || !doesMacMatch(rxFrame.buffer)) {
				continue;
			}

			// Save the new ID
			g_ownCanComId = rxFrame.buffer[6];

			// Get the screen type
			const uint8_t screen = rxFrame.buffer[7];

			// Display the screen
			QueueEvent_t event;
			if (screen == SCREEN_TEMPERATURE) {
				event.command = DISPLAY_TEMPERATURE_SCREEN;
			}
			// Speed screen
			else if (screen == SCREEN_SPEED) {
				event.command = DISPLAY_SPEED_SCREEN;
			}
			// RPM screen
			else {
				event.command = DISPLAY_RPM_SCREEN;
			}
			xQueueSend(g_guiEventQueue, &event, portMAX_DELAY);

			// Logging
			ESP_LOGI("RegistrationManager", "Finished registration. Entering operation mode");

			// Then enter the operation mode
			operationManagerInit();

			// Delete the task
			vTaskDelete(NULL);

			continue;
		}
	}
}

/*
 *	Private functions
 */
static bool doesMacMatch(const uint8_t* p_buffer)
{
	return *(p_buffer) == g_macAddress[0] && *(p_buffer + 1) == g_macAddress[1] && *(p_buffer + 2) == g_macAddress[2] &&
		*(p_buffer + 3) == g_macAddress[3] && *(p_buffer + 4) == g_macAddress[4] && *(p_buffer + 5) == g_macAddress[5];
}

static void broadcastUuid()
{
	// Create the CAN answer frame
	TwaiFrame_t frame;

	// Set the buffer content
	frame.buffer[0] = g_macAddress[0];
	frame.buffer[1] = g_macAddress[1];
	frame.buffer[2] = g_macAddress[2];
	frame.buffer[3] = g_macAddress[3];
	frame.buffer[4] = g_macAddress[4];
	frame.buffer[5] = g_macAddress[5];

	// Initiate the frame
	canInitiateFrame(&frame, CAN_MSG_REGISTRATION, 6);

	// Send the frame
	canQueueFrame(&frame);

	// Logging
	ESP_LOGI("main", "Sent HW UUID '%d-%d-%d-%d-%d-%d' to Sensor Board!", g_macAddress[0], g_macAddress[1], g_macAddress[2],
			   g_macAddress[3], g_macAddress[4], g_macAddress[5]);
}

static void pullMacAddress()
{
	// Get the WiFi MAC address
	esp_read_mac(g_macAddress, ESP_MAC_WIFI_STA);

	// Generate the initial com id
	g_ownCanComId = 0x00;
	g_ownCanComId += g_macAddress[3] << 16;
	g_ownCanComId += g_macAddress[4] << 8;
	g_ownCanComId += g_macAddress[5];

	// ID 0x00 is reserved for the master
	if (g_ownCanComId == 0x00) {
		g_ownCanComId++;
	}
}

/*
 *	Public function implementations
 */
bool registrationManagerInit()
{
	// Get the MAC address
	pullMacAddress();

	// Register to the CAN rx cb
	if (!canRegisterRxCbQueue(&g_registrationManagerQueue)) {
		ESP_LOGE("RegistrationManager", "Couldn't register rx cb queue");

		return false;
	}

	// Start the can task
	if (xTaskCreate(canTask, "RegistrationManagerCanTask", 2048 * 4, NULL, 0, &g_taskHandle) != pdPASS) {
		ESP_LOGE("RegistrationManager", "Couldn't create CAN task!");

		return false;
	}

	// Broadcast our UUID once
	broadcastUuid();

	return true;
}

void registrationManagerDestroy()
{
	// Unregister from the CAN rx cb
	canUnregisterRxCbQueue(&g_registrationManagerQueue);
}
