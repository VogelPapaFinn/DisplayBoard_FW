// Project includes
#include "GUI.h"
#include "Global.h"
#include "can.h"
#include "statemachine.h"

// FreeRTOS includes
#include "freertos/FreeRTOS.h"

// espidf includes
#include <esp_mac.h>

// The HW UUID
uint8_t HW_UUID;

// The COM ID
uint8_t COM_ID;

void app_main(void)
{
	// Generate the HW UUID
	uint8_t mac[8];
	esp_read_mac(mac, ESP_MAC_WIFI_STA);
	HW_UUID = mac[0] ^ mac[1] ^ mac[2] ^ mac[3] ^ mac[4] ^ mac[5] ^ mac[6] ^ mac[7];

	// Create the event queues
	createEventQueues();

	// Initialize the can node
	twai_node_handle_t* canNodeHandle = initializeCanNode(GPIO_NUM_9, GPIO_NUM_6);
	enableCanNode();

	// Init the GUI
	guiInit();

	// Register the queue to the CAN bus
	registerMessageReceivedCbQueue(&mainEventQueue);

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

					// Reset the MCU
					if (recFrame.header.id == CAN_MSG_RESET) {
						if (recFrame.buffer_len >= 1 && recFrame.buffer[0] == COM_ID) {
							esp_restart();
						}
					}

					// Are we in the INIT or REGISTRATION state?
					if (currState == STATE_INIT || currState == STATE_REGISTRATION) {
						// Was it an instruction to register ourselves?
						if (recFrame.header.id == CAN_MSG_REQUEST_HW_UUID) {
							// Change the mode
							setCurrentState(STATE_REGISTRATION);

							// Create the CAN frame
							twai_frame_t* frame = malloc(sizeof(twai_frame_t));

							// Init the header
							frame->header.id = HW_UUID;
							frame->header.ide = false;
							frame->header.fdf = false;
							frame->header.dlc = sizeof(HW_UUID);

							// Init the frame
							frame->buffer = &HW_UUID;
							frame->buffer_len = sizeof(HW_UUID);

							// Send the frame
							queueCanBusMessage(frame, true, false);

							// Logging
							loggerInfo("Sent HW UUID '%d' to Sensor Board!", HW_UUID);
						}

						// Was it a new ID?
						else if (recFrame.header.id == CAN_MSG_SET_ID) {
							// Check if the HW UUID matches
							if (recFrame.buffer_len >= 2 && recFrame.buffer[0] == HW_UUID) {
								// Save the new ID
								COM_ID = recFrame.buffer[1];

								// Then enter the operation mode
								setCurrentState(STATE_OPERATION);

								// Logging
								loggerInfo("Received ID: %d", COM_ID);
							}
						}
					}

					// Are we in the OPERATION state?
					if (currState == STATE_OPERATION) {
						// Was it new sensor data?
						if (recFrame.header.id == CAN_MSG_NEW_SENSOR_DATA) {
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
