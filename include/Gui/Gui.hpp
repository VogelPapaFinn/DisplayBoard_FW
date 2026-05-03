#pragma once

// Project includes
#include "Driver/ST77916.hpp"

// espidf includes
#include "lvgl.h"

class Gui
{
public:
	/*
	 *	Public Enumeration
	 */
	typedef enum
	{
		TEMPERATURE,
		SPEED,
		RPM
	} Screen;

	/*
	 *	Public Functions
	 */
	Gui(ST77916* physicalDisplay);

	SemaphoreHandle_t* getGuiMutex();

	void setScreen(const Screen& screen) const;

	/*
	 *	Private Callback functions
	 */
	IRAM_ATTR void flushToDisplay(lv_display_t* p_display, const lv_area_t* p_area, uint8_t* p_pxMap) const;

private:
	ST77916* physicalDisplay_ = nullptr;

	lv_display_t* display_ = nullptr;

	uint16_t* frameBuffer1_ = nullptr;
	uint16_t* frameBuffer2_ = nullptr;

	/*
	 *	Thread Stuff
	 */
	SemaphoreHandle_t guiMutex_;

	TaskHandle_t lvglUpdateTask_;
};
