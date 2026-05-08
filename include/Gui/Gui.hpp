#pragma once

// Project includes
#include "Driver/ST77916.hpp"
#include "Event.hpp"

// espidf includes
#include "lvgl.h"

class Gui
{
public:
	/*
	 *	Public Enumeration
	 */
	enum
	{
		TEMPERATURE,
		SPEED,
		RPM
	} SCREEN;

	/*
	 *	Public Functions
	 */
	Gui(ST77916* physicalDisplay);

	SemaphoreHandle_t* getGuiMutex();

	void setScreen(const uint8_t screen) const;

	void queueEventFromISR(const Event& event) const;

	QueueHandle_t getEventQueue() const;

	void setLeftIndicatorActive(const bool& active) const;

	void setRightIndicatorActive(const bool& active) const;

	void setSpeed(const uint8_t& speed) const;

	void setRpm(const uint16_t& rpm) const;

	/*
	 *	Private Callback functions
	 */
	IRAM_ATTR void flushToDisplay(lv_display_t* p_display, const lv_area_t* p_area, uint8_t* p_pxMap) const;

private:
	/*
	 *	Private Variables
	 */
	ST77916* physicalDisplay_ = nullptr;

	lv_display_t* display_ = nullptr;

	uint16_t* frameBuffer1_ = nullptr;
	uint16_t* frameBuffer2_ = nullptr;

	/*
	 *	Thread Stuff
	 */
	TaskHandle_t eventQueueHandle_;

	QueueHandle_t eventQueue_;

	SemaphoreHandle_t guiMutex_;

	TaskHandle_t lvglUpdateTask_;
};
