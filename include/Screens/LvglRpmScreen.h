#pragma once

// C includes
#include <stdbool.h>

// FreeRTOS include
#include "freertos/FreeRTOS.h"

/*
 *  Public functions
 */
bool guiCreateAndShowRpmScreen(const SemaphoreHandle_t* p_guiSemaphore);

void guiDestroyRpmScreen();