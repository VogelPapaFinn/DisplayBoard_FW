#pragma once

// C includes
#include <stdbool.h>

// FreeRTOS include
#include "freertos/FreeRTOS.h"

/*
 *  Public functions
 */
bool guiCreateAndShowSpeedScreen(const SemaphoreHandle_t* p_guiSemaphore);

void guiDestroySpeedScreen();

void guiSetSpeed(uint8_t speedKmh, const SemaphoreHandle_t* p_guiSemaphore);

void guiSetRightIndicatorActive(bool active, const SemaphoreHandle_t* p_guiSemaphore);