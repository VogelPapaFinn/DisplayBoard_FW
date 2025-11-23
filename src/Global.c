// Project includes
#include "Global.h"

QueueHandle_t guiEventQueue = NULL;

QueueHandle_t mainEventQueue = NULL;

bool createEventQueues()
{
	// Create the event Queue for the GUI
	guiEventQueue = xQueueCreate(20, sizeof(QUEUE_EVENT_T));
	if (guiEventQueue == 0) {
		loggerCritical("Couldn't create guiEventQueue");

		return false;
	}

	// Create the main Queue for the GUI
	mainEventQueue = xQueueCreate(5, sizeof(QUEUE_EVENT_T));
	if (mainEventQueue == 0) {
		loggerCritical("Couldn't create mainEventQueue");

		return false;
	}

	// Logging
	loggerInfo("Created event queues");

	return true;
}
