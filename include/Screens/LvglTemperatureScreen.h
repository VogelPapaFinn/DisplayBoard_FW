#pragma once

// C includes
#include <stdbool.h>

// FreeRTOS include
#include "freertos/FreeRTOS.h"

/*
 *  Public functions
 */
bool guiCreateAndShowTemperatureScreen(const SemaphoreHandle_t* p_guiSemaphore);

void guiDestroyTemperatureScreen();