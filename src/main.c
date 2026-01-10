// Project includes
#include "GUI.h"
#include "EventQueues.h"
#include "Version.h"
#include "can.h"
#include "UpdateHandlers/CanUpdater.h"

// FreeRTOS includes
#include "freertos/FreeRTOS.h"

// espidf includes
#include <esp_log.h>
#include <esp_mac.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>

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

State_t g_currentState = STATE_INIT;

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

/*
 *	Main function
 */
void app_main(void) // NOLINT
{
	ESP_LOGI("main", "--- --- --- --- --- --- ---");
	ESP_LOGI("main", "Firmware Version: %s", VERSION_FULL);

	// Check on which OTA partition we are
	const esp_partition_t* partition = esp_ota_get_running_partition();
	if (partition != NULL) {
		ESP_LOGI("main", "Current running partition: %s", partition->label);
		ESP_LOGI("main", "Type: %d, Subtype: %d", partition->type, partition->subtype);
		ESP_LOGI("main", "Address: 0x%08"PRIx32, partition->address);
		ESP_LOGI("main", "Size: 0x%08"PRIx32" bytes", partition->size);
	} else {
		ESP_LOGE("main", "Couldn't load OTA partition!");
	}

	// Get the MAC address
	getMacAddress();

	// Get the initial CAN com id
	buildCanComId();

	// Create the event queues
	createEventQueues();

	// Initialize and enable the can node
	canInitializeNode(GPIO_NUM_9, GPIO_NUM_6);
	canEnableNode();

	// Register the queue to the CAN bus
	canRegisterRxCbQueue(&g_mainEventQueue);

	// Initialize the GUI
	guiInit();

	// Initialize the CAN Updater
	canUpdaterInit();

	// Broadcast the HW UUID once on startup
	broadcastUuid();

	// Wait for new queue events
	QueueEvent_t queueEvent;
	while (true) {
		// Wait until we get a new event in the queue
		if (xQueueReceive(g_mainEventQueue, &queueEvent, portMAX_DELAY)) {
			// Act depending on the event
			switch (queueEvent.command) {
				/*
				 *	We received a CAN message
				 */
				case RECEIVED_NEW_CAN_MESSAGE:
				{
					// Get the frame
					const twai_frame_t rxFrame = queueEvent.canFrame;

					// Get the message id
					const uint8_t messageId = rxFrame.header.id >> CAN_MESSAGE_ID_OFFSET;

					// Did the message come from the master?
					if ((rxFrame.header.id & 0x1FFFFF) != 0) {
						continue;
					}

					// Should we restart?
					if (messageId == CAN_MSG_DISPLAY_RESTART) {
						// Were we meant?
						if (rxFrame.buffer[0] != g_ownCanComId) {
							continue;
						}

						esp_rom_printf("Restarting\n");
						esp_restart();
					}

					// Are we in the INIT or REGISTRATION state?
					if (g_currentState == STATE_INIT) {
						// Was it an instruction to register ourselves?
						if (messageId == CAN_MSG_REGISTRATION) {
							// Send the HW UUID
							broadcastUuid();
							continue;
						}

						// Was it a new ID?
						if (messageId == CAN_MSG_COMID_ASSIGNATION) {
							// Check if the HW UUID matches
							if (rxFrame.buffer_len >= 7 && rxFrame.buffer[0] == g_macAddress[0] &&
								rxFrame.buffer[1] == g_macAddress[1] && rxFrame.buffer[2] == g_macAddress[2] &&
								rxFrame.buffer[3] == g_macAddress[3] && rxFrame.buffer[4] == g_macAddress[4] &&
								rxFrame.buffer[5] == g_macAddress[5]) {
								// Save the new ID
								g_ownCanComId = rxFrame.buffer[6];

								// Get the screen type
								const uint8_t screen = rxFrame.buffer[7];

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
								g_currentState = STATE_OPERATION;

								// Logging
								ESP_LOGI("main", "Received ID: %d", g_ownCanComId);

								continue;
							}
						}
					}

					// Are we in the OPERATION state?
					if (g_currentState == STATE_OPERATION) {
						// Was it new sensor data?
						if (messageId == CAN_MSG_SENSOR_DATA) {
							// Create the event
							QueueEvent_t event;
							event.command = NEW_SENSOR_DATA;
							event.canFrame = rxFrame;

							// Queue the event
							xQueueSend(g_guiEventQueue, &event, pdMS_TO_TICKS(100));
							continue;
						}

						// Were we meant?
						if (messageId == CAN_MSG_REQUEST_FIRMWARE_VERSION) {
							ESP_LOGI("main", "CAN_MSG_REQUEST_FIRMWARE_VERSION, %d, %d", rxFrame.buffer[0], g_ownCanComId);
						}
						if (*rxFrame.buffer != g_ownCanComId) {
							continue;
						}

						// Was it a firmware version request?
						if (messageId == CAN_MSG_REQUEST_FIRMWARE_VERSION) {
							ESP_LOGI("main", "messageId == CAN_MSG_REQUEST_FIRMWARE_VERSION");

							/*
							 *	Send the firmware
							 */
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

							ESP_LOGI("main", "Send firmware version to the Sensor Board!");
							continue;
						}

						// Was it a git commit hash request?
						if (messageId == CAN_MSG_REQUEST_COMMIT_INFORMATION) {
							/*
							 *	Send the commit hash information
							 */
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