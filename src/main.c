// Project includes
#include "GUI.h"
#include "Global.h"
#include "can.h"
#include "statemachine.h"

// FreeRTOS includes
#include "freertos/FreeRTOS.h"

// espidf includes
#include <esp_mac.h>

/*
 *	Prototypes
 */
void sendHwUUID();

// The COM ID
uint8_t COM_ID;

// The MAC address
uint8_t MAC_ADDRESS[6];

void app_main(void)
{
	// Get the WiFi MAC address
	esp_read_mac(MAC_ADDRESS, ESP_MAC_WIFI_STA);

	// Generate the initial COM_ID
	COM_ID = 0x00;
	COM_ID += MAC_ADDRESS[3] << 16;
	COM_ID += MAC_ADDRESS[4] << 8;
	COM_ID += MAC_ADDRESS[5];
	// ID 0x00 is reserved for the master
	if (COM_ID == 0x00) {
		COM_ID = 0x01;
	}

	// Create the event queues
	createEventQueues();

	// Initialize the can node
	twai_node_handle_t* canNodeHandle = initializeCanNode(GPIO_NUM_9, GPIO_NUM_6);
	enableCanNode();

	// Init the GUI
	guiInit();

	// Register the queue to the CAN bus
	registerCanRxCbQueue(&mainEventQueue);

	// Broadcast the HW UUID once on startup
	sendHwUUID();

	// Wait for new queue events
	QUEUE_EVENT_T queueEvent;
	while (1) {
		// Wait until we get a new event in the queue
		if (xQueueReceive(mainEventQueue, &queueEvent, portMAX_DELAY)) {
			// Get current state
			const State_t currState = getCurrentState();

			// Act depending on the event
			switch (queueEvent.command) {
				/*
				 *	We received a CAN message
				 */
				case QUEUE_RECEIVED_NEW_CAN_MESSAGE:
					// Get the frame
					const twai_frame_t recFrame = queueEvent.canFrame;

					// Should we restart?
					if ((recFrame.header.id >> 21) == CAN_MSG_DISPLAY_RESTART) {
						esp_rom_printf("Restarting\n");
						esp_restart();
					}

					// Are we in the INIT or REGISTRATION state?
					if (currState == STATE_INIT || currState == STATE_REGISTRATION) {
						// Was it an instruction to register ourselves?
						if ((recFrame.header.id >> 21) == CAN_MSG_REGISTRATION) {
							// Change the mode
							setCurrentState(STATE_REGISTRATION);

							// Send the HW UUID
							sendHwUUID();
						}

						// Was it a new ID?
						else if ((recFrame.header.id >> 21) == CAN_MSG_COMID_ASSIGNATION) {
							// Check if the HW UUID matches
							if (recFrame.buffer_len >= 7 && recFrame.buffer[0] == MAC_ADDRESS[5] &&
								recFrame.buffer[1] == MAC_ADDRESS[4] && recFrame.buffer[2] == MAC_ADDRESS[3] &&
								recFrame.buffer[3] == MAC_ADDRESS[2] && recFrame.buffer[4] == MAC_ADDRESS[1] &&
								recFrame.buffer[5] == MAC_ADDRESS[0]) {
								// Save the new ID
								COM_ID = recFrame.buffer[6];

								// Then enter the operation mode
								setCurrentState(STATE_OPERATION);

								// Logging
								loggerInfo("Received ID: %d", COM_ID);
							}
						}
					}

					// Are we in the OPERATION state?
					else if (currState == STATE_OPERATION) {
						// Was it new sensor data?
						if ((recFrame.header.id >> 21) == CAN_MSG_SENSOR_DATA) {
							break; // TODO: implement
							// Create the event
							QUEUE_EVENT_T event;
							event.command = QUEUE_CMD_GUI_NEW_SENSOR_DATA;
							event.canFrame = recFrame;

							// Queue the event
							xQueueSend(guiEventQueue, &event, pdMS_TO_TICKS(100));

							// Logging
							loggerDebug("Received new Sensor Data");
						}
					}
					break;


				/*
				 *	Fallback
				 */
				default:
					break;
			}
		}
	}
}

void sendHwUUID()
{
	// Create the CAN frame buffer
	uint8_t* buffer = (uint8_t*)malloc(sizeof(uint8_t) * 6);
	buffer[0] = MAC_ADDRESS[5];
	buffer[1] = MAC_ADDRESS[4];
	buffer[2] = MAC_ADDRESS[3];
	buffer[3] = MAC_ADDRESS[2];
	buffer[4] = MAC_ADDRESS[1];
	buffer[5] = MAC_ADDRESS[0];

	// Create the CAN frame
	twai_frame_t* frame = generateCanFrame(CAN_MSG_REGISTRATION, COM_ID, buffer, sizeof(uint8_t) * 6);

	// Send the frame
	queueCanBusMessage(frame, true, false);

	// Logging
	loggerInfo("Sent HW UUID '%d-%d-%d-%d-%d-%d' to Sensor Board!", MAC_ADDRESS[0], MAC_ADDRESS[1], MAC_ADDRESS[2],
			   MAC_ADDRESS[3], MAC_ADDRESS[4], MAC_ADDRESS[5]);
}
