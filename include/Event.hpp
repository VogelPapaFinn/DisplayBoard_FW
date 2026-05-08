#pragma once

struct Event
{
	typedef enum
	{
		UNKNOWN,
		SET_SCREEN,
		WAKE_UP,
	} TYPE;

	Event(const TYPE type = UNKNOWN, const int data = 0)
	{
		this->type = type;
		this->data = data;
	}

	TYPE type = UNKNOWN;

	int data = 0;
};