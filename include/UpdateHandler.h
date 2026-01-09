#pragma once

// C includes
#include <stdbool.h>
#include <stdint.h>

/*
 *  Public defines
 */
#define UPDATE_PART_SIZE_B 7

/*
 *	Public functions
 */
bool updateHandlerInitialize(uint32_t updateSizeB);

void updateWriteToBuffer(const uint8_t* p_bytes, const uint8_t amount);

bool updateExecute();