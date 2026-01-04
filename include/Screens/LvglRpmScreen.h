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

void guiSetRpm(uint16_t rpm, const SemaphoreHandle_t* p_guiSemaphore);

void guiSetLeftIndicatorActive(bool active, const SemaphoreHandle_t* p_guiSemaphore);