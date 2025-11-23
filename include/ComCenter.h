#pragma once

// espidf includes

// FreeRTOS includes
#include "freertos/FreeRTOS.h"

// C includes

// Defines

/*
 *	Static Variables
 */


/*
 *  Functions
 */

//! \brief Starts and registers the CAN bus message distributor
//! \retval Boolean indicating if the start was successfull
bool startCommunicationCenter();
