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
	} TYPE;

	//! \brief Default constructor of the Event
	Event(const TYPE type = UNKNOWN, const int data = 0)
	{
		this->type = type;
		this->data = data;
	}

	//! \brief Public instance of the TYPE enum
	TYPE type = UNKNOWN;

	//! \brief Public Integer data
	int data = 0;
};