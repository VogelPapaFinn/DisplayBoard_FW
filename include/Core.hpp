#pragma once

// Project includes
#include "Can.hpp"
#include "Gui/Gui.hpp"
#include "Config.hpp"

// espidf includes
#include "ArduinoJson.hpp"

/*
 *	Class
 */
class Core
{
public:
	/*
	 *	Public Functions
	 */
	static Core* get();

	void setMainEventQueue(QueueHandle_t queue);

	QueueHandle_t getMainEventQueue() const;

	ST77916* getDisplayDriver() const;

	Gui* getGui() const;

	ArduinoJson::JsonDocument* getConfig() const;

	void saveConfig() const;

	/*
	 *	CAN related functions
	 */
	Can* getCan() const;

	void setCanId(const uint8_t& canId);

	uint8_t getCanId() const;

private:
	/*
	 *	Instances
	 */
	static Core* self_;

	Can* can_ = nullptr;

	ST77916* displayDriver_ = nullptr;

	Gui* gui_ = nullptr;

	/*
	 *	Private Functions
	 */
	Core();

	/*
	 *	Private Variables
	 */
	QueueHandle_t mainEventQueue_ = nullptr;

	uint8_t canId_ = 0;

	Config* config_ = nullptr;

	ArduinoJson::JsonDocument* jsonConfig_ = nullptr;
};
