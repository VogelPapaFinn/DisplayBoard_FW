#pragma once

// espidf includes
#include "esp_event.h"

ESP_EVENT_DECLARE_BASE(SYSTEM_EVENT_BASE);

/*
 *	Events
 */
enum SYSTEM_EVENT_ID
{
	/*
	 *	System
	 */
	REGISTRATION_COMPLETED,

	/*
	 *	Display
	 */
	LOAD_SCREEN,
	TURN_ON,
	NEW_SENSOR_DATA,

	/*
	 *	Update
	 */
	EXECUTE_UPDATE,

	/*
	 *	CAN
	 */
	CAN_FRAME_RECEIVED,
};
