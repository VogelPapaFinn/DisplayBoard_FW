// Project includes
#include "ComCenter.h"
#include "Global.h"

// espidf includes
#include <can.h>

#include "esp_twai.h"
#include "esp_twai_onchip.h"

/*
 *	Private variables
 */
//! \brief The task handle of the CAN rx cb
TaskHandle_t canRxTaskHandle_;

//! \brief Pointer to the CAN node
twai_node_handle_t* canNodeHandle_ = NULL;

/*
 *	Private functions
 */
void IRAM_ATTR canMessageDistributorTask(void* arg)
{
	while (true) {
		// Blocks until we are notified
		xTaskNotifyWait(0, UINT32_MAX, NULL, portMAX_DELAY);

		// Get the new message
		const twai_frame_t message = getLastReceivedMessage();

		// Decide on its type
		QUEUE_EVENT_T event;
		switch (message.header.id) {
			// Send the HW UUID
			case CAN_MSG_REQUEST_HW_UUID:
				// Add the register hw uuid event to the queue
				QUEUE_EVENT_T registerHwUUID;
				registerHwUUID.command = QUEUE_CMD_MAIN_REGISTER_UUID;
				xQueueSend(mainEventQueue, &registerHwUUID, 0);

				break;

			// Reset the MCU
			case CAN_MSG_REQUEST_HW_RESET:
				// Create the event
				event.command = QUEUE_CMD_MAIN_RESET;

				// Send it to the Queue
				xQueueSend(mainEventQueue, &event, 0);
				break;

			// Got new sensor data
			case CAN_MSG_NEW_SENSOR_DATA:
				// Create the event
				event.command = QUEUE_CMD_GUI_NEW_SENSOR_DATA;

				// Send it to the Queue
				xQueueSend(guiEventQueue, &event, 0);
				break;

			default:
				break;
		}
	}
}

bool startCommunicationCenter()
{
	bool success = true;

	// Create the CAN RX Task
	const BaseType_t taskSuccess = xTaskCreate(canMessageDistributorTask, "CAN_RX_Distributor", 4096 / 4, NULL,
											   tskIDLE_PRIORITY, &canRxTaskHandle_);
	success &= taskSuccess;

	// Initialize the can node
	canNodeHandle_ = initializeCanNode(GPIO_NUM_9, GPIO_NUM_6);
	success &= canNodeHandle_ != NULL;

	// Register the rx callback
	success &= registerMessageReceivedCb(&canRxTaskHandle_);

	// Enable the node
	success &= enableCanNode();

	return success == pdPASS;
}
