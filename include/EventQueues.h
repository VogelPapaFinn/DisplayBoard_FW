#pragma once

// espidf include
#include "esp_twai.h"

// FreeRTOS include
#include "freertos/FreeRTOS.h"

//! \brief The Queue used to send events to the GUI
extern QueueHandle_t g_guiEventQueue;

//! \brief The Queue used to send events to the main application (main.c)
extern QueueHandle_t g_mainEventQueue;

//! \brief A typedef enum that contains commands for all Queues
typedef enum
{
	/* CAN */
	CAN_DRIVER_CRASHED,
	RECEIVED_NEW_CAN_MESSAGE,

	/* GUI */
	NEW_SENSOR_DATA,
    DISPLAY_TEMPERATURE_SCREEN,
    DISPLAY_SPEED_SCREEN,
    DISPLAY_RPM_SCREEN
} QueueCommand_t;

//! \brief A typedef struct which is used in the event queues
typedef struct
{
	//! \brief The command of the event
	QueueCommand_t command;

  //! \brief An optional CAN frame
	twai_frame_t canFrame;
} QueueEvent_t;

//! \brief Creates the event queues
//! \retval Boolean indicating if the creation was successful
bool createEventQueues();
