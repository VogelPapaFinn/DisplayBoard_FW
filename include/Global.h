#pragma once

// Project includes
#include "Logger.h"

// FreeRTOS include
#include "freertos/FreeRTOS.h"

// espidf include
#include "esp_twai.h"

//! \brief The Queue used to send events to the GUI
extern QueueHandle_t guiEventQueue;

//! \brief The Queue used to send events to the main application (main.c)
extern QueueHandle_t mainEventQueue;

//! \brief The HW UUID
extern uint8_t HW_UUID;

//! \brief A typedef enum that contains commands for all Queues
typedef enum
{
	QUEUE_CMD_MAIN_REGISTER_UUID,
	QUEUE_CMD_MAIN_RESET,
	QUEUE_CMD_GUI_NEW_SENSOR_DATA,
	QUEUE_CMD_MAIN_ENTER_UPDATE_MODE,
	QUEUE_CMD_MAIN_RETURN_FW_VERSION
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
