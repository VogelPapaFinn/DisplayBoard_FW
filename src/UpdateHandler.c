#include "UpdateHandler.h"

// C includes
#include <stddef.h>
#include <stdlib.h>

// espidf includes
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>

/*
 *	Private variables
 */
static bool g_inUpdateProcedure = false;
static uint32_t g_updateSizeB = 0;
static uint8_t* g_updateFileBuffer = NULL;
static uint32_t g_updateFilePart = 0;

/*
 *	Public function implementations
 */
bool updateHandlerInitialize(const uint32_t updateSizeB)
{
	// Save the size
	g_updateSizeB = updateSizeB;

	// Reset the update file part
	g_updateFilePart = 0;

	// Allocate the memory
	g_updateFileBuffer = (uint8_t*)malloc(g_updateSizeB);
	if (g_updateFileBuffer == NULL) {
		ESP_LOGE("UpdateHandler", "Couldn't allocate %d bytes for the update file", g_updateSizeB);
		return false;
	}

	// Clear the memory
	memset(g_updateFileBuffer, 0, g_updateSizeB);

	// We are now in an update procedure
	g_inUpdateProcedure = true;

	return true;
}

void updateWriteToBuffer(const uint8_t* p_bytes, const uint8_t amount)
{
	static uint32_t s_updateByteIndex = 0;

	if (!g_inUpdateProcedure) {
		ESP_LOGE("UpdateHandler", "Update Handler not initialized!");
		return;
	}
	if (p_bytes == NULL) {
		ESP_LOGE("UpdateHandler", "Received NULL pointer to write to the update buffer");
		return;
	}

	// Overflow check
	if (s_updateByteIndex >= g_updateSizeB || s_updateByteIndex + amount > g_updateSizeB) {
		ESP_LOGE("UpdateHandler", "Update Buffer Overflow in part %d, byte index: %d, amount: %d, update size: %ld", g_updateFilePart, s_updateByteIndex, amount, g_updateSizeB);
		return;
	}

	// Then copy the memory to that location
	memcpy(g_updateFileBuffer + s_updateByteIndex, p_bytes, amount);

	// Calculate where to write next time
	s_updateByteIndex += amount;
	g_updateFilePart++;

	// Logging
	if (g_updateFilePart % 1000 == 0 || amount != UPDATE_PART_SIZE_B) {
		ESP_LOGI("UpdateHandler", "Part %d: Wrote %d bytes to the update file buffer", g_updateFilePart,
				 amount);
	}
}

bool updateExecute()
{
	if (!g_inUpdateProcedure) {
		ESP_LOGE("UpdateHandler", "Update Handler not initialized!");
		return false;
	}

	if (g_updateFileBuffer == NULL) {
		ESP_LOGE("UpdateHandler", "Couldn't update, update file Buffer is empty!");
		return false;
	}

	// Get the other OTA partition
	const esp_partition_t* updatePartition = esp_ota_get_next_update_partition(NULL);
	if (updatePartition == NULL) {
		ESP_LOGE("UpdateHandler", "Couldn't find update partition!");
		return false;
	}

	ESP_LOGI("UpdateHandler", "Writing update file to partition %s", updatePartition->label);

	// Initiate partition
	esp_ota_handle_t updateHandle;
	if (esp_ota_begin(updatePartition, g_updateSizeB, &updateHandle) != ESP_OK) {
		ESP_LOGE("UpdateHandler", "Couldn't initiate update partition");
		return false;
	}

	// Write the update file
	for (uint32_t i = 0; i < g_updateSizeB; i++) {
		// Write to the partition
		if (esp_ota_write(updateHandle, g_updateFileBuffer + i, 1) != ESP_OK) {
			ESP_LOGE("UpdateHandler", "Couldn't write update block %d. Aborting", i);
			esp_ota_abort(updateHandle);
			return false;
		}
	}

	// Finish the update
	if (esp_ota_end(updateHandle) != ESP_OK) {
		ESP_LOGE("UpdateHandler", "Couldn't end update partition");
		esp_ota_abort(updateHandle);
		return false;
	}

	// Switch data partition
	if (esp_ota_set_boot_partition(updatePartition) != ESP_OK) {
		ESP_LOGE("UpdateHandler", "Couldn't switch to update partition");
		esp_ota_abort(updateHandle);
		return false;
	}

	ESP_LOGI("UpdateHandler", "Wrote %d bytes to partition %s", g_updateSizeB, updatePartition->label);
	free(g_updateFileBuffer);

	return true;
}
