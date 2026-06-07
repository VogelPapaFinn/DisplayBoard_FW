//! \file Core.hpp
//! \brief Core class where important Singleton instances are concentrated

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
//! \brief Core class as Singleton
class Core
{
public:
	/*
	 *	Public Functions
	 */
	//! \brief Singleton get function
	//! \retval The instance of the Core
	static Core* get();

	//! \brief Returns a ptr to the instance of the Display Driver
	//! \retval Ptr to the \c ST77916 instance
	ST77916* getDisplayDriver() const;

	QueueHandle_t getMainEventQueue() const;

	void setMainEventQueue(QueueHandle_t queue);

	//! \brief Returns a ptr to the instance of the Gui
	//! \retval Ptr to the \c Gui instance
	Gui* getGui() const;

	//! \brief Returns a ptr to the instance of the config file on the spiffs partition
	//! \retval Ptr to the \c Config instance
	ArduinoJson::JsonDocument* getConfig() const;

	//! \brief Writes all changes of the Config file to the spiffs partition
	void saveConfig() const;

	/*
	 *	CAN related functions
	*/
	//! \brief Returns a ptr to the instance of the CAN node
	//! \retval Ptr to the \c CAN instance
	Can* getCan() const;

	//! \brief Changes the ID used for sending & receiving on the CAN bus
	//! \param canId The new CAN ID
	void setCanId(const uint8_t& canId);

	//! \brief Returns the current CAN ID
	//! \retval The current CAN ID
	uint8_t getCanId() const;

private:
	/*
	 *	Instances
	 */
	//! \brief The ptr to the self instance
	static Core* self_;

	//! \brief The ptr to the CAN instance
	Can* can_ = nullptr;

	//! \brief The ptr to the ST77916 instance
	ST77916* displayDriver_ = nullptr;

	//! \brief The ptr to the Gui instance
	Gui* gui_ = nullptr;

	/*
	 *	Private Functions
	 */
	//! \brief Private constructor. Instantiates & prepares everything
	Core();

	/*
	 *	Private Variables
	 */
	//! \brief The current CAN ID
	QueueHandle_t mainEventQueue_ = nullptr;

	//! \brief The current CAN ID
	uint8_t canId_ = 0;

	//! \brief Ptr to the Config instance
	Config* config_ = nullptr;

	//! \brief Ptr to the JSON representation of the Config instance
	ArduinoJson::JsonDocument* jsonConfig_ = nullptr;
};
