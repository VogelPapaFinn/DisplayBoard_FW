#pragma once

struct Event
{
	typedef enum
	{
		UNKNOWN,
		SET_SCREEN,
		WAKE_UP,
		REGISTRATION_FINISHED,
		NEW_SENSOR_DATA
	} TYPE;

	Event(const TYPE type = UNKNOWN)
	{
		this->type = type;
	}

	TYPE type = UNKNOWN;

	union
	{
		uint8_t canData[8] = {0x00};
		int intData;
	};
};