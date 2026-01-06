// Project includes
#include "GUI.h"
#include "EventQueues.h"
#include "can.h"
#include "statemachine.h"
#include "Version.h"

// FreeRTOS includes
#include "freertos/FreeRTOS.h"

// espidf includes
#include <esp_log.h>
#include <esp_mac.h>

/*
 *	Defines
 */
#define MAC_ADDRESS_LENGTH 6

/*
 *	Private typedefs
 */
typedef enum
{
	STATE_INIT,
	STATE_REGISTRATION,
	STATE_OPERATION,
} State_t;

/*
 *	Prototypes
 */

/*
 *	External Variables
 */
uint8_t g_ownCanComId = 0;

/*
 *	Private variables
 */
// The MAC address
uint8_t g_macAddress[MAC_ADDRESS_LENGTH];

/*
 *	Helper functions
*/
void getMacAddress()
{
	// Get the WiFi MAC address
	esp_read_mac(g_macAddress, ESP_MAC_WIFI_STA);
}

void buildCanComId()
{
	// Generate the initial COM_ID
	g_ownCanComId = 0x00;
	g_ownCanComId += g_macAddress[3] << 16;
	g_ownCanComId += g_macAddress[4] << 8;
	g_ownCanComId += g_macAddress[5];
	// ID 0x00 is reserved for the master
	if (g_ownCanComId == 0x00) {
		g_ownCanComId++;
	}
}

void broadcastUuid()
{
	// Create the CAN frame buffer
	uint8_t* buffer = (uint8_t*)malloc(sizeof(uint8_t) * 6);
	buffer[0] = g_macAddress[0];
	buffer[1] = g_macAddress[1];
	buffer[2] = g_macAddress[2];
	buffer[3] = g_macAddress[3];
	buffer[4] = g_macAddress[4];
	buffer[5] = g_macAddress[5];

	// Create the CAN frame
	twai_frame_t* frame = generateCanFrame(CAN_MSG_REGISTRATION, g_ownCanComId, &buffer, sizeof(uint8_t) * 6);

	// Send the frame
	queueCanBusMessage(frame, true, false);

	// Logging
	ESP_LOGI("main", "Sent HW UUID '%d-%d-%d-%d-%d-%d' to Sensor Board!", g_macAddress[0], g_macAddress[1], g_macAddress[2],
			   g_macAddress[3], g_macAddress[4], g_macAddress[5]);
}

/*
 *	Main function
 */
void app_main(void) // NOLINT
{
	ESP_LOGI("main", "Firmware Version: %s", VERSION_FULL);

	// Get the MAC address
	getMacAddress();

	// Get the initial CAN com id
	buildCanComId();

	// Create the event queues
	createEventQueues();

	// Initialize and enable the can node
	initializeCanNode(GPIO_NUM_9, GPIO_NUM_6);
	enableCanNode();

	// Register the queue to the CAN bus
	registerCanRxCbQueue(&g_mainEventQueue);

	// Initialize the GUI
	guiInit();

	// Broadcast the HW UUID once on startup
	broadcastUuid();

	// Wait for new queue events
	QueueEvent_t queueEvent;
	while (true) {
		// Wait until we get a new event in the queue
		if (xQueueReceive(g_mainEventQueue, &queueEvent, portMAX_DELAY)) {
			// Get current state
			const State_t currState = getCurrentState();

			// Act depending on the event
			switch (queueEvent.command) {
				/*
				 *	We received a CAN message
				 */
				case RECEIVED_NEW_CAN_MESSAGE:
				{
					// Get the frame
					const twai_frame_t recFrame = queueEvent.canFrame;

					// Get the message id
					const uint8_t messageId = recFrame.header.id >> CAN_FRAME_HEADER_OFFSET;

					// Did the message come from the master?
					if (((recFrame.header.id << (32 - CAN_FRAME_HEADER_OFFSET)) >> CAN_FRAME_HEADER_OFFSET) != 0) {
						continue;
					}

					// Should we restart?
					if (messageId == CAN_MSG_DISPLAY_RESTART) {
						// Were we meant?
						if (recFrame.buffer[0] != g_ownCanComId) {
							continue;
						}

						esp_rom_printf("Restarting\n");
						esp_restart();
					}

					// Are we in the INIT or REGISTRATION state?
					if (currState == STATE_INIT || currState == STATE_REGISTRATION) {
						// Was it an instruction to register ourselves?
						if (messageId == CAN_MSG_REGISTRATION) {
							// Change the mode
							setCurrentState(STATE_REGISTRATION);

							// Send the HW UUID
							broadcastUuid();

							continue;
						}

						// Was it a new ID?
						if (messageId == CAN_MSG_COMID_ASSIGNATION) {
							// Check if the HW UUID matches
							if (recFrame.buffer_len >= 7 && recFrame.buffer[0] == g_macAddress[0] &&
								recFrame.buffer[1] == g_macAddress[1] && recFrame.buffer[2] == g_macAddress[2] &&
								recFrame.buffer[3] == g_macAddress[3] && recFrame.buffer[4] == g_macAddress[4] &&
								recFrame.buffer[5] == g_macAddress[5]) {
								// Save the new ID
								g_ownCanComId = recFrame.buffer[6];

								// Get the screen type
								const uint8_t screen = recFrame.buffer[7];

								// Display the screen
								ESP_LOGI("main", "Received Screen: %d", screen);
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

								// Then enter the operation mode
								setCurrentState(STATE_OPERATION);

								// Logging
								ESP_LOGI("main", "Received ID: %d", g_ownCanComId);

								continue;
							}
						}
					}

					// Are we in the OPERATION state?
					if (currState == STATE_OPERATION) {
						// Was it new sensor data?
						if (messageId == CAN_MSG_SENSOR_DATA) {
							// Create the event
							QueueEvent_t event;
							event.command = NEW_SENSOR_DATA;
							event.canFrame = recFrame;

							// Queue the event
							xQueueSend(g_guiEventQueue, &event, pdMS_TO_TICKS(100));

							continue;
						}

						// Was it a firmware version request?
						if (messageId == CAN_MSG_REQUEST_FIRMWARE_VERSION) {
							/*
							 *	Send the firmware
							 */
							// Create the CAN frame buffer
							uint8_t* buffer = (uint8_t*)malloc(sizeof(uint8_t) * 4);
							buffer[0] = VERSION_BETA;
							buffer[1] = (uint8_t)*VERSION_MAJOR;
							buffer[2] = (uint8_t)*VERSION_MINOR;
							buffer[3] = (uint8_t)*VERSION_PATCH;

							// Create the CAN frame
							twai_frame_t* frame = generateCanFrame(CAN_MSG_REQUEST_FIRMWARE_VERSION, g_ownCanComId, &buffer, sizeof(uint8_t) * 6);

							// Send the frame
							queueCanBusMessage(frame, true, false);

							ESP_LOGI("main", "Send firmware version to the Sensor Board!");

							continue;
						}

						// Was it a git commit hash request?
						if (messageId == CAN_MSG_REQUEST_COMMIT_INFORMATION) {
							/*
							 *	Send the commit hash information
							 */
							// Create the CAN frame buffer
							uint8_t* buffer = (uint8_t*)malloc(sizeof(uint8_t) * 8);
							buffer[0] = (uint8_t)GIT_HASH[0];
							buffer[1] = (uint8_t)GIT_HASH[1];
							buffer[2] = (uint8_t)GIT_HASH[2];
							buffer[3] = (uint8_t)GIT_HASH[3];
							buffer[4] = (uint8_t)GIT_HASH[4];
							buffer[5] = (uint8_t)GIT_HASH[5];
							buffer[6] = (uint8_t)GIT_HASH[6];
							buffer[7] = sizeof(GIT_HASH) > 7 ? 1 : 0; // If its size is longer than 7 it's a dirty commit!

							// Create the CAN frame
							twai_frame_t* frame = generateCanFrame(CAN_MSG_REQUEST_COMMIT_INFORMATION, g_ownCanComId, &buffer, sizeof(uint8_t) * 8);

							// Send the frame
							queueCanBusMessage(frame, true, false);

							ESP_LOGI("main", "Send hash information to the Sensor Board!");

							continue;
						}
					}

					break;
				}

				/*
				 *	Fallback
				 */
				default:
					break;
			}
		}
	}
}