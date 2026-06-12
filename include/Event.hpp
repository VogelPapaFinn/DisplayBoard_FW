//! \file Event.hpp
//! \brief Declaration of an Event used in internal FreeRTOS Queues

#pragma once

//! \brief Represents an Event
struct Event
{
	//! \brief The type of the event
	//!
	//! Contains all types of events: For the GUI, for the StateMachine etc.
	typedef enum
	{
		UNKNOWN,		//!< Unknown Event, used for error detection
		SET_SCREEN,		//!< Loads the specified screen in the GUI
		WAKE_UP,		//!< Turns on the BL of the ST77916 and displays the GUI
		REGISTRATION_FINISHED,
		NEW_SENSOR_DATA,
		JOIN_WIFI,
		EXECUTE_UPDATE
	} TYPE;

	//! \brief Default constructor of the Event
	Event(const TYPE type = UNKNOWN)
	{
		this->type = type;
	}

	//! \brief Public instance of the TYPE enum
	TYPE type = UNKNOWN;

	union
	{
		uint8_t canData[8] = {0x00};
		int intData;
	};
};