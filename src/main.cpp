// Project includes
#include "Config.hpp"
#include "Events.hpp"
#include "Filesystem.hpp"
#include "Gui.hpp"
#include "Handler/WifiHandler.hpp"
#include "State/Operation.hpp"
#include "State/Registration.hpp"
#include "WifiJoin.hpp"

// espidf includes
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"

/*
 *	constexpr
 */
constexpr auto TAG = "main";

constexpr gpio_num_t GPIO_CAN_RX = GPIO_NUM_39;
constexpr gpio_num_t GPIO_CAN_TX = GPIO_NUM_40;

constexpr auto CONFIG_NAME = "config.json";
constexpr auto DEFAULT_CONFIG_NAME = "default/config.json";

/*
 *	Private Static Variables
 */
static std::shared_ptr<State> g_currentState;

/*
 *	Helper functions
 */
static void registerToEvents(SystemContext* p_sysCon)
{
	/*
	 *	Registration Completed
	 */
	esp_event_handler_instance_register(
		SYSTEM_EVENT_BASE, REGISTRATION_COMPLETED,
		[](void* p_systemContext, esp_event_base_t, int32_t, void*)
		{
			/*
			 *	Get the system context
			 */
			if (p_systemContext == nullptr) {
				return;
			}

			auto sysCon = static_cast<SystemContext*>(p_systemContext);

			/*
			 *	Enter the operation state
			 */
			g_currentState = std::make_shared<Operation>(sysCon);
			g_currentState->enter();
		},
		p_sysCon, nullptr);
}

static void createAndOpenConfigFile(SystemContext& sysCon)
{
	/*
	 *	Create default config, if config doesnt exist
	 */
	if (!sysCon.filesystem->doesFileExist(CONFIG_NAME, Filesystem::CONFIG_PARTITION)) {
		sysCon.filesystem->createFile(CONFIG_NAME, Filesystem::CONFIG_PARTITION);

		if (!sysCon.filesystem->doesFileExist(DEFAULT_CONFIG_NAME, Filesystem::CONFIG_PARTITION)) {
			Config defaultConfig(&sysCon);
			defaultConfig.open(DEFAULT_CONFIG_NAME);

			Config newConfig(&sysCon);
			newConfig.open(CONFIG_NAME);
			*newConfig.getJson() = *defaultConfig.getJson();

			newConfig.save();
		}
	}

	/*
	 *	Load the config file
	 */
	sysCon.config->open(CONFIG_NAME);
	const auto& jsonConfig = sysCon.config->getJson();
	if (jsonConfig != nullptr) {
		std::string str;
		serializeJsonPretty(*jsonConfig, str);
		ESP_LOGI(TAG, "%s", str.c_str());
	}
}

/*
 *	main function
 */
extern "C" void app_main(void)
{
	// NEEDED FOR DEBUGGING
	vTaskDelay(pdMS_TO_TICKS(250));

	/*
	 *	Print startup logging header
	 */
	ESP_LOGI(TAG, "--- --- --- --- --- --- ---");
	ESP_LOGI(TAG, "Startup");

	/*
	 *	Start the event loop
	 */
	if (esp_event_loop_create_default() != ESP_OK) {
		ESP_LOGE(TAG, "Error creating default event loop. Rebooting...");
		esp_restart();
	}

	/*
	 *	Create all necessary instances
	 */
	// System Context
	SystemContext sysCon;

	// Can
	Can can(GPIO_CAN_RX, GPIO_CAN_TX);
	can.initialize();
	can.enable();

	// Filesystem
	Filesystem fs(false, true, false);

	// Config
	Config config(&sysCon);

	// Wifi
	WifiJoin wifi(&sysCon);

	// Display driver
	Gui gui(&sysCon);

	// Wifi Handler
	WifiHandler wifiHandler(&sysCon);

	/*
	 *	Register the necessary events
	 */
	registerToEvents(&sysCon);

	/*
	 *	Build the SystemContext
	 */
	sysCon.can = &can;
	sysCon.filesystem = &fs;
	sysCon.config = &config;
	sysCon.wifi = &wifi;

	/*
	 *	Ensure the config file exists and is loaded
	 */
	createAndOpenConfigFile(sysCon);

	/*
	 *	Create and enter the registration state
	 */
	g_currentState = std::make_shared<Registration>(&sysCon);
	g_currentState->enter();

	while (true) {
		vTaskDelay(pdMS_TO_TICKS(1000));
	}
}
