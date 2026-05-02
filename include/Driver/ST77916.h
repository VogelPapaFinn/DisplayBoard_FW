#pragma once

// C++ includes
#include <vector>

// espidf includes
#include "driver/ledc.h"
#include "driver/spi_common.h"
#include "esp_lcd_io_spi.h"
#include "esp_lcd_types.h"
#include "lvgl.h"

class ST77916
{
public:
	ST77916();

	void setLvglDisplay(lv_display_t* lvDisplay);

	static void setBacklightLevel(uint8_t percent);

	/*
	 *	Public Callback functions
	 */
	void frameDrawnIsr() const;

private:
	bool initialized_ = false;

	/*
	 *	Display LCD Stuff
	 */
	spi_bus_config_t busConfig_ = {};
	esp_lcd_panel_io_handle_t ioHandle_ = nullptr;
	esp_lcd_panel_io_spi_config_t ioConfig_ = {};
	esp_lcd_panel_handle_t panelHandle_ = nullptr;

	lv_display_t* lvDisplay_ = nullptr;

	/*
	 *	Backlight stuff
	 */
	ledc_timer_config_t ledcTimer_ = {};
	ledc_channel_config_t ledcChannel_ = {};
};
