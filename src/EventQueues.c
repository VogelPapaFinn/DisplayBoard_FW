// Project includes
#include "EventQueues.h"
#include "can.h"

// espidf includes
#include <esp_log.h>

/*
 *	Public variables
 */
QueueHandle_t g_registrationManagerQueue = NULL;

QueueHandle_t g_operationManagerCanQueue = NULL;

QueueHandle_t g_canUpdateManagerQueue = NULL;

QueueHandle_t g_guiEventQueue = NULL;

QueueHandle_t g_mainQueue = NULL;

/*
 *	Public function implementations
 */
bool createEventQueues()
{
	// Create the event Queue for the registration manager
	g_registrationManagerQueue = xQueueCreate(10, sizeof(TwaiFrame_t));
	if (g_registrationManagerQueue == 0) {
		ESP_LOGE("EventQueues", "Couldn't create can queue for the registration manager");

		return false;
	}

	// Create the event Queue for the operation manager
	g_operationManagerCanQueue = xQueueCreate(10, sizeof(TwaiFrame_t));
	if (g_operationManagerCanQueue == 0) {
		ESP_LOGE("EventQueues", "Couldn't create can queue for the operation manager");

		return false;
	}

	// Create the can Queue for the CanUpdater
	g_canUpdateManagerQueue = xQueueCreate(10, sizeof(TwaiFrame_t));
	if (g_canUpdateManagerQueue == 0) {
		ESP_LOGE("EventQueues", "Couldn't create can queue for the can update manager");

		return false;
	}



	// Create the event Queue for the GUI
	g_guiEventQueue = xQueueCreate(50, sizeof(QueueEvent_t));
	if (g_guiEventQueue == 0) {
		ESP_LOGE("EventQueues", "Couldn't create guiEventQueue");

		return false;
	}

	// Create the main Queue for the GUI
	g_mainQueue = xQueueCreate(5, sizeof(QueueEvent_t));
	if (g_mainQueue == 0) {
		ESP_LOGE("EventQueues", "Couldn't create mainEventQueue");

		return false;
	}

	// Logging
	ESP_LOGI("EventQueues", "Created event queues");

	return true;
}
