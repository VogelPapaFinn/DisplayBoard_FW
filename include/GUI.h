#pragma once

// Project includes
#include "can.h"

// C includes
#include <stdbool.h>

/*
 *  Public functions
 */
bool guiInit();

void guiActivateRefreshing();

void guiDeactivateRefreshing();

bool guiDisplayScreen(Screen_t screen);