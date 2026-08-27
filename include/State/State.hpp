#pragma once

// Project includes
#include "Events.hpp"
#include "SystemContext.hpp"

// C++ includes
#include "vector"

class State
{
public:
	/*
	 *	Public enum
	 */
	typedef enum
	{
		UNKNOWN,
		REGISTRATION,
		OPERATING
	} TYPE;

	/*
	 *	Public Functions
	 */
	State(TYPE type = UNKNOWN);

	virtual void enter() = 0;

	TYPE getType() const;

protected:
	/*
	 *	Private Variables
	 */
	TYPE type_ = UNKNOWN;

	SystemContext* sysCon_ = nullptr;

	std::vector<std::tuple<esp_event_base_t, SYSTEM_EVENT_ID, esp_event_handler_instance_t>> eventHandlers_;
};
