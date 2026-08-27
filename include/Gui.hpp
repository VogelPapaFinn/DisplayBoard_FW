#pragma once

// Project includes
#include "Can.hpp"
#include "Driver/ST77916.hpp"
#include "Events.hpp"
#include "SystemContext.hpp"

// espidf includes
#include "esp_event_base.h"
#include "lvgl.h"

class Gui
{
public:
	/*
	 *	Public non-Functions
	 */
	typedef enum
	{
		UNKOWN = -1,
		TEMPERATURE,
		SPEED,
		RPM
	} SCREEN;

	struct SensorData
	{
		uint8_t fuelLevel = 0;
		bool oilPressure = false;
		uint8_t waterTemperature = 0;
		uint16_t rpm = 0;
		uint8_t speed = 0;
		bool leftIndicatorActive = false;
		bool rightIndicatorActive = false;
	};

	/*
	 *	Public Functions
	 */
	Gui(SystemContext* p_sysCon);

	SemaphoreHandle_t getGuiMutex() const;
private:
	/*
	 *	Private Functions
	 */
	void registerToEvents();

	void loadScreen(SCREEN screen);

	IRAM_ATTR void flushToDisplay(const lv_area_t* p_area, uint8_t* p_pxMap);

	/*
	 *	Private Setter-Functions
	 */
	void setLeftIndicatorActive(const bool& active) const;

	void setRightIndicatorActive(const bool& active) const;

	void setSpeed(const uint8_t& speed) const;

	void setRpm(const uint16_t& rpm) const;

	void setOilPressure(const bool& active) const;

	void setFuelLevel(const uint8_t& fuelLevelPercent) const;

	void setWaterTemperature(const int8_t& temperature) const;

	/*
	 *	Private Variables
	 */
	SystemContext* sysCon_ = nullptr;

	std::vector<std::tuple<esp_event_base_t, SYSTEM_EVENT_ID, esp_event_handler_instance_t>> eventHandlers_;

	ST77916 st77916_;

	lv_display_t* display_ = nullptr;

	uint16_t* frameBuffer1_ = nullptr;
	uint16_t* frameBuffer2_ = nullptr;

	SCREEN currentScreen_ = UNKOWN;

	/*
	 *	Thread Stuff
	 */
	SemaphoreHandle_t guiMutex_;

	TaskHandle_t lvglUpdateTask_;
};
