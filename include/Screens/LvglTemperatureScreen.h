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

void guiSetWaterTemp(const uint8_t temp, const SemaphoreHandle_t* p_guiSemaphore);

void guiSetFuelLevel(const uint8_t levelInPercent, const SemaphoreHandle_t* p_guiSemaphore);