// Project includes
#include "EventQueues.h"

// espidf includes
#include <esp_log.h>

/*
 *	Public variables
 */
QueueHandle_t g_guiEventQueue = NULL;

QueueHandle_t g_mainEventQueue = NULL;

/*
 *	Public function implementations
 */
bool createEventQueues()
{
	// Create the event Queue for the GUI
	g_guiEventQueue = xQueueCreate(50, sizeof(QueueEvent_t));
	if (g_guiEventQueue == 0) {
		ESP_LOGE("EventQueues", "Couldn't create guiEventQueue");

		return false;
	}

	// Create the event Queue for the CanUpdater
	g_canUpdaterEventQueue = xQueueCreate(10, sizeof(QueueEvent_t));
	if (g_canUpdaterEventQueue == 0) {
		ESP_LOGE("EventQueues", "Couldn't create canUpdaterEventQueue");

		return false;
	}

	// Create the main Queue for the GUI
	g_mainEventQueue = xQueueCreate(5, sizeof(QueueEvent_t));
	if (g_mainEventQueue == 0) {
		ESP_LOGE("EventQueues", "Couldn't create mainEventQueue");

		return false;
	}

	// Logging
	ESP_LOGI("EventQueues", "Created event queues");

	return true;
}
