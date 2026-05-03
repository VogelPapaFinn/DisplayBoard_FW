#include "Gui/Gui.hpp"

// espidf includes
#include "display/lv_display_private.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

/*
 *	constexpr
 */
constexpr auto TAG = "Gui";

constexpr uint16_t RESOLUTION = 360;
constexpr uint8_t BIT_DEPTH_B = 2;

constexpr uint64_t FRAME_BUFFER_SIZE_B = RESOLUTION * RESOLUTION * BIT_DEPTH_B;

/*
 *	Private ISR's
 */
static void staticFlushToDisplay(lv_display_t* p_display, const lv_area_t* p_area, uint8_t* p_pxMap)
{
	if (p_display == nullptr || p_area == nullptr || p_pxMap == nullptr) {
		return;
	}

	if (p_display->user_data == nullptr) {
		return;
	}

	Gui* instance_ = static_cast<Gui*>(p_display->user_data);

	instance_->flushToDisplay(p_display, p_area, p_pxMap);
}

static void lvglUpdateTaskFunc(void* p_params)
{
	if (p_params == nullptr) {
		vTaskDelete(nullptr);
		return;
	}

	Gui* instance_ = static_cast<Gui*>(p_params);
	const auto mutex = instance_->getGuiMutex();

	while (true) {
		// Try to get the semaphore
		if (xSemaphoreTake(*mutex, portMAX_DELAY) == pdTRUE) {
			lv_timer_handler();

			xSemaphoreGive(*mutex);

			vTaskDelay(pdMS_TO_TICKS(10));
		}
	}
}

/*
 *	Public Function Implementations
 */
Gui::Gui(ST77916* physicalDisplay)
{
	physicalDisplay_ = physicalDisplay;

	lv_init();

	display_ = lv_display_create(RESOLUTION, RESOLUTION);
	lv_display_set_user_data(display_, this);
	physicalDisplay_->setLvglDisplay(display_);

	frameBuffer1_ = static_cast<uint16_t*>(malloc(FRAME_BUFFER_SIZE_B));
	frameBuffer2_ = static_cast<uint16_t*>(malloc(FRAME_BUFFER_SIZE_B));

	if (frameBuffer1_ == nullptr || frameBuffer2_ == nullptr) {
		ESP_LOGE(TAG, "Failed to allocate memory for the frame buffers");
		return;
	}

	memset(frameBuffer1_, 0, FRAME_BUFFER_SIZE_B);
	memset(frameBuffer2_, 0, FRAME_BUFFER_SIZE_B);

	lv_display_set_buffers(display_, frameBuffer1_, frameBuffer2_, FRAME_BUFFER_SIZE_B, LV_DISPLAY_RENDER_MODE_FULL);

	lv_display_set_color_format(display_, LV_COLOR_FORMAT_RGB565);

	// Make the display bg black for now
	lv_obj_set_style_bg_color(lv_display_get_screen_active(display_), lv_color_hex(0x000000), LV_PART_MAIN);

	guiMutex_ = xSemaphoreCreateMutex();

	lv_display_set_flush_cb(display_, staticFlushToDisplay);

	lv_tick_set_cb(xTaskGetTickCount);

	if (xTaskCreate(lvglUpdateTaskFunc, "lvglUpdateTaskFunc", 10000, nullptr, 0, &lvglUpdateTask_) != pdPASS) {
		ESP_LOGE(TAG, "Failed to create LVGL update task");

		return;
	}
}

SemaphoreHandle_t* Gui::getGuiMutex()
{
	return &guiMutex_;
}

void Gui::setScreen(const Screen& screen)
{

}

/*
 *	Private Callback functions
 */
void Gui::flushToDisplay(lv_display_t* p_display, const lv_area_t* p_area, const uint8_t* p_pxMap) const
{
	if (physicalDisplay_ == nullptr) {
		return;
	}

	if (xSemaphoreTake(guiMutex_, portMAX_DELAY) == pdTRUE) {
		physicalDisplay_->drawBitmap(p_area, p_pxMap);

		xSemaphoreGive(guiMutex_);
	}

	// lv_display_flush_ready is called by the ST77916 driver as the physical display
	// notifies when the buffer can be reused!
}

