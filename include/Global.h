#pragma once

// FreeRTOS include
#include "freertos/FreeRTOS.h"

// espidf include
#include "esp_twai.h"

//! \brief The Queue used to send events to the GUI
extern QueueHandle_t guiEventQueue;

//! \brief The Queue used to send events to the main application (main.c)
extern QueueHandle_t mainEventQueue;

//! \brief The ID assigned by the SensorBoard
extern uint8_t COM_ID;

//! \brief The HW MAC address of the chip. [0] contains the part that changes rarely
//! [5] contains the part that changes often.
extern uint8_t MAC_ADDRESS[6];

typedef enum
{
	STATE_INIT,
	STATE_REGISTRATION,
	STATE_OPERATION,
} State_t;

//! \brief A typedef enum that contains commands for all Queues
typedef enum
{
	/* CAN */
	QUEUE_RECEIVED_NEW_CAN_MESSAGE,

	/* GUI */
	QUEUE_CMD_GUI_NEW_SENSOR_DATA,
} QUEUE_COMMAND_T;

//! \brief A typedef struct which is used in the event Queues
typedef struct
{
	QUEUE_COMMAND_T command;
	twai_frame_t canFrame;
} QUEUE_EVENT_T;

//! \brief Creates the event queues
//! \retval Boolean indicating if the creation was successfull
bool createEventQueues();
