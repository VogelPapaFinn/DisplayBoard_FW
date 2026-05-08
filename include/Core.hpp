#pragma once

// Project includes
#include "Can.hpp"
#include "Gui/Gui.hpp"

/*
 *	Public constexpr
 */
constexpr uint8_t MASTER_CAN_ID = 1;

/*
 *	Class
 */
class Core
{
public:
	static Core* get();

	void setMainEventQueue(QueueHandle_t queue);

	QueueHandle_t getMainEventQueue() const;

	ST77916* getDisplayDriver() const;

	Gui* getGui() const;

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
	uint8_t canId_ = 0;

	QueueHandle_t mainEventQueue_ = nullptr;
};
