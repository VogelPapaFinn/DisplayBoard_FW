#pragma once

// C includes
#include "stdbool.h"

/*
 *	Public functions
 */
//! \brief Initializes the operation manager
//! \retval Bool indicating if the initialization was successful
bool operationManagerInit();

//! \brief Destroys the operation manager
void operationManagerDestroy();