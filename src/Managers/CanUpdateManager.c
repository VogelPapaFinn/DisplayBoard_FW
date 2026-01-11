#include "../../include/Managers/CanUpdateManager.h"

// Project includes
#include "GUI.h"
#include "Managers/ManagerUtils.h"
#include "can.h"

// C includes
#include <stdlib.h>

// espidf includes
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>

// FreeRTOS include
#include "freertos/FreeRTOS.h"

/*
 *  Private defines
 */
#define UPDATE_PART_SIZE_B 7

/*
 *	Private variables
 */
//! \brief Task handle of the CAN task
static TaskHandle_t g_taskHandle;

//! \brief Bool indicating if an update is in progress
static uint32_t g_sizeB = 0;
static uint8_t* g_buffer = NULL;
static uint32_t g_block = 0;

/*
 *	Prototypes
 */
//! \brief Prepares everything for the update
//! \retval Bool indicating if the preparations were successful
static bool prepareUpdate();

//! \brief Writes a block of bytes to the update buffer
//! \param p_bytes The array of bytes
//! \param amount Amount of bytes to write
static void writeFileBlock(const uint8_t* p_bytes, uint8_t amount);

//! \brief Tries to execute the update
//! \retval Bool indicating if the update succeeded
static bool executeUpdate();

/*
 *	Tasks
 */
//! \brief Task used to receive Update related CAN frames
//! \param p_param Unused parameters
static void canTask(void* p_param)
{
	// Wait for new queue events
	QueueEvent_t queueEvent;
	while (true) {
		// Wait until we get a new event in the queue
		if (xQueueReceive(g_canUpdateManagerQueue, &queueEvent, portMAX_DELAY)) {
			// Skip if it's not a new can message, or it's not from the master
			if (queueEvent.command != RECEIVED_NEW_CAN_FRAME || (queueEvent.canFrame.header.id & 0x1FFFFF) != 0) {
				continue;
			}

			// Get the frame
			const twai_frame_t rxFrame = queueEvent.canFrame;

			// Get the message id
			const uint8_t messageId = rxFrame.header.id >> CAN_FRAME_ID_OFFSET;

			// Act depending on the CAN message
			if (messageId == CAN_MSG_PREPARE_UPDATE) {
				// Get the update file size
				g_sizeB = rxFrame.buffer[1] << 24;
				g_sizeB += rxFrame.buffer[2] << 16;
				g_sizeB += rxFrame.buffer[3] << 8;
				g_sizeB += rxFrame.buffer[4];

				// Logging
				ESP_LOGI("main", "Received Update File Size: %d", g_sizeB);

				// Init the update handler
				if (!prepareUpdate()) {
					ESP_LOGW("main", "Something failed, cant initialize update mode");

					continue;
				}

				// Create the CAN answer frame
				TwaiFrame_t frame;

				// Initiate the frame
				canInitiateFrame(&frame, CAN_MSG_PREPARE_UPDATE, 0);

				// Send the frame
				canQueueFrame(&frame);

				continue;
			}

			// Block of the update file
			if (messageId == CAN_MSG_TRANSMIT_UPDATE_FILE) {
				writeFileBlock(rxFrame.buffer + 1, rxFrame.header.dlc - 1);

				// Create the CAN answer frame
				TwaiFrame_t frame;

				// Initiate the frame
				canInitiateFrame(&frame, CAN_MSG_TRANSMIT_UPDATE_FILE, 0);

				// Send the frame
				canQueueFrame(&frame);

				continue;
			}

			// Execute the update
			if (messageId == CAN_MSG_EXECUTE_UPDATE) {
				const bool success = executeUpdate();

				// Create the CAN answer frame
				TwaiFrame_t frame;

				// Set the buffer content
				frame.buffer[0] = (uint8_t)success;

				// Initiate the frame
				canInitiateFrame(&frame, CAN_MSG_EXECUTE_UPDATE, 1);

				// Send the frame
				canQueueFrame(&frame);

				continue;
			}
		}
	}
}

/*
 *	Private functions
*/
static bool prepareUpdate()
{
	// Reset the file block
	g_block = 0;

	// Allocate the memory
	g_buffer = (uint8_t*)malloc(g_sizeB);
	if (g_buffer == NULL) {
		ESP_LOGE("UpdateHandler", "Couldn't allocate %d bytes for the update file", g_sizeB);
		return false;
	}

	// Clear the buffer
	memset(g_buffer, 0, g_sizeB);

	// We are now in an update procedure
	g_canUpdateActive = true;

	// Stop the GUI from refreshing to boost performance
	guiDeactivateRefreshing();

	return true;
}

static void writeFileBlock(const uint8_t* p_bytes, const uint8_t amount)
{
	if (p_bytes == NULL) {
		ESP_LOGE("UpdateHandler", "Received NULL pointer to write to the update buffer");
		return;
	}
	if (g_buffer == NULL) {
		ESP_LOGE("CanUpdater", "No update file buffer initialized");

		return;
	}

	static uint32_t s_byteIndex = 0;

	// Overflow check
	if (s_byteIndex >= g_sizeB || s_byteIndex + amount > g_sizeB) {
		ESP_LOGE("UpdateHandler", "Update Buffer Overflow in block %d, byte index: %d, amount: %d, update size: %ld", g_block, s_byteIndex, amount, g_sizeB);
		return;
	}

	// Then copy the memory into the buffer
	memcpy(g_buffer + s_byteIndex, p_bytes, amount);

	// Calculate the next index
	s_byteIndex += amount;
	g_block++;

	// Logging
	if (g_block % 1000 == 0 || amount != UPDATE_PART_SIZE_B) {
		ESP_LOGI("UpdateHandler", "Block %d: Wrote %d bytes to the update file buffer", g_block,
				 amount);
	}
}

static bool executeUpdate()
{
	if (g_buffer == NULL) {
		ESP_LOGE("UpdateHandler", "Couldn't update, update file Buffer is empty!");
		return false;
	}

	// Get the update OTA partition
	const esp_partition_t* updatePartition = esp_ota_get_next_update_partition(NULL);
	if (updatePartition == NULL) {
		ESP_LOGE("UpdateHandler", "Couldn't find update partition!");
		return false;
	}

	// Logging
	ESP_LOGI("UpdateHandler", "Writing update file to partition %s", updatePartition->label);

	// Initiate partition
	esp_ota_handle_t updateHandle;
	if (esp_ota_begin(updatePartition, g_sizeB, &updateHandle) != ESP_OK) {
		ESP_LOGE("UpdateHandler", "Couldn't initiate update partition");

		// Update failed
		g_canUpdateActive = false;
		return false;
	}

	// Write the update file
	for (uint32_t i = 0; i < g_sizeB; i++) {
		// Write to the partition
		if (esp_ota_write(updateHandle, g_buffer + i, 1) != ESP_OK) {
			ESP_LOGE("UpdateHandler", "Couldn't write update block %d. Aborting", i);

			esp_ota_abort(updateHandle);
			return false;
		}
	}

	// Finish the update
	if (esp_ota_end(updateHandle) != ESP_OK) {
		ESP_LOGE("UpdateHandler", "Couldn't end update partition");
		esp_ota_abort(updateHandle);

		// Update failed
		g_canUpdateActive = false;
		return false;
	}

	// Switch data partition
	if (esp_ota_set_boot_partition(updatePartition) != ESP_OK) {
		ESP_LOGE("UpdateHandler", "Couldn't switch to update partition");
		esp_ota_abort(updateHandle);

		// Update failed
		g_canUpdateActive = false;
		return false;
	}

	ESP_LOGI("UpdateHandler", "Wrote %d bytes to partition %s", g_sizeB, updatePartition->label);
	free(g_buffer);

	// Reactivate the refreshing of the GUI
	guiActivateRefreshing();

	// Update finished
	g_canUpdateActive = false;

	return true;
}

/*
 *	Public function implementations
 */
bool canUpdateManagerInit()
{
	// Register to the CAN rx cb
	if (!canRegisterRxCbQueue(&g_canUpdateManagerQueue)) {
		ESP_LOGE("DisplayUpdate", "Couldn't register rx cb queue");

		return false;
	}

	// Start the can task
	if (xTaskCreate(canTask, "CanUpdaterTask", 2048 * 4, NULL, 2, &g_taskHandle) != pdPASS) {
		ESP_LOGE("DisplayUpdate", "Couldn't create can updater task!");

		return false;
	}

	return true;
}