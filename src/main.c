// Project includes
#include "ComCenter.h"
#include "GUI.h"
#include "Global.h"
#include "can.h"

// FreeRTOS includes
#include "freertos/FreeRTOS.h"

// espidf includes
#include <esp_mac.h>


// The HW UUID
uint8_t HW_UUID;

void app_main(void)
{
	// Generate the HW UUID
	uint8_t mac[8];
	esp_read_mac(mac, ESP_MAC_WIFI_STA);
	HW_UUID = mac[0] ^ mac[1] ^ mac[2] ^ mac[3] ^ mac[4] ^ mac[5] ^ mac[6] ^ mac[7];

	// Create the event queues
	createEventQueues();

	// Start the Communication Center
	startCommunicationCenter();

	// Init the GUI
	guiInit();

	// Wait for new queue events
	QUEUE_EVENT_T queueEvent;
	while (1) {
		// Wait until we get a new event in the queue
		if (xQueueReceive(mainEventQueue, &queueEvent, portMAX_DELAY)) {
			switch (queueEvent.command) {
				// Send the HW UUID to the SensorBoard
				case QUEUE_CMD_MAIN_REGISTER_UUID:
					// Create the can frame
					twai_frame_t* frame = malloc(sizeof(twai_frame_t));

					// Init the header
					frame->header.id = CAN_MSG_REGISTER_HW_UUID;
					frame->header.ide = false;
					frame->header.fdf = false;
					frame->header.dlc = sizeof(HW_UUID);

					// Init the frame
					HW_UUID += 1;
					frame->buffer = &HW_UUID;
					frame->buffer_len = sizeof(HW_UUID);

					// Send the frame
					queueCanBusMessage(frame, true, false);

					// Logging
					loggerInfo("Sent HW UUID '%d' to Sensor Board!", HW_UUID);

					break;
				// Reset the MCU
				case QUEUE_CMD_MAIN_RESET:
					esp_restart();
					break;
				default:
					break;
			}
		}

		// Wait 100ms
		vTaskDelay(pdMS_TO_TICKS(100));
	}
}
