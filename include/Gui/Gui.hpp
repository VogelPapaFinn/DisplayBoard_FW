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
		UNKOWN = -1,
		TEMPERATURE,
		SPEED,
		RPM
	} SCREEN;

	/*
	 *	Public Functions
	 */
	Gui(ST77916* physicalDisplay);

	SemaphoreHandle_t* getGuiMutex();

	void setScreen(const uint8_t screen);

	uint8_t getScreen() const;

	void queueEventFromISR(const Event& event) const;

	QueueHandle_t getEventQueue() const;

	void setLeftIndicatorActive(const bool& active) const;

	void setRightIndicatorActive(const bool& active) const;

	void setSpeed(const uint8_t& speed) const;

	void setRpm(const uint16_t& rpm) const;

	void setOilPressure(const bool& active) const;

	void setFuelLevel(const uint8_t& fuelLevelPercent) const;

	void setWaterTemperature(const int16_t& temperature) const;

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

	uint8_t currentScreen_ = UNKOWN;

	/*
	 *	Thread Stuff
	 */
	TaskHandle_t eventQueueHandle_;

	QueueHandle_t eventQueue_;

	SemaphoreHandle_t guiMutex_;

	TaskHandle_t lvglUpdateTask_;
};
