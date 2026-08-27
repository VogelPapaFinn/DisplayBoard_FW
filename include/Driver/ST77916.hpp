#pragma once

// C++ includes
#include <atomic>
#include <bits/shared_ptr_atomic.h>

// espidf includes
#include "driver/ledc.h"
#include "driver/spi_common.h"
#include "esp_lcd_io_spi.h"
#include "esp_lcd_types.h"
#include "lvgl.h"

/*
 *	Struct
 */
typedef struct
{
	lv_display_t* lvDisplay = nullptr;
	esp_lcd_panel_handle_t panelHandle = nullptr;
	lv_area_t area;
	uint8_t* pixelData = nullptr;
	bool valid = false;

	std::atomic<bool>* isDrawing;
	SemaphoreHandle_t spiMutex;
} DrawData;

/*
 *	class
 */
class ST77916
{
public:
	ST77916();

	void setLvglDisplay(lv_display_t* lvDisplay);

	lv_display_t* getLvglDisplay() const;

	static void setBacklightLevel(uint8_t percent);

	DrawData* getDrawData();

	void drawBitmap(const lv_area_t* p_area, uint8_t* p_pxMap);

	void setRotated(const bool& rotated) const;

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

	std::atomic<bool> isDrawing_{false};
	SemaphoreHandle_t spiMutex_ = nullptr;

	/*
	 *	TE GPIO Stuff
	 */
	DrawData drawData_;

	TaskHandle_t drawToDisplayTaskHandle_ = nullptr;

	/*
	 *	Backlight stuff
	 */
	ledc_timer_config_t ledcTimer_ = {};
	ledc_channel_config_t ledcChannel_ = {};
};
