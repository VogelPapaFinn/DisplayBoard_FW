#include "Gui/Gui.hpp"

//  Project includes
#include "Core.hpp"

// espidf includes
#include <string>
#include "display/lv_display_private.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

// ui includes
extern "C" {
#include "ui/screens.h"
}

/*
 *	constexpr
 */
constexpr auto TAG = "Gui";

constexpr uint16_t RESOLUTION = 360;
constexpr uint8_t BIT_DEPTH_B = 2;

constexpr uint32_t FRAME_BUFFER_SIZE_B = (RESOLUTION * RESOLUTION * BIT_DEPTH_B) / 4;

/*
 *	Private Mutex Class
 */
class GuiLock
{
public:
	GuiLock(SemaphoreHandle_t mutex) : mutex_(mutex) { xSemaphoreTakeRecursive(mutex_, portMAX_DELAY); }

	~GuiLock() { xSemaphoreGiveRecursive(mutex_); }

private:
	SemaphoreHandle_t mutex_;
};

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

	const Gui* instance = static_cast<Gui*>(p_display->user_data);
	instance->flushToDisplay(p_display, const_cast<lv_area_t*>(p_area), p_pxMap);
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
		{
			GuiLock lock(*mutex);

			lv_timer_handler();
		}

		vTaskDelay(pdMS_TO_TICKS(10));
	}
}

static void eventQueueTask(void* param)
{
	if (param == nullptr) {
		esp_rom_printf("Null\n");
		vTaskDelete(nullptr);
	}

	Gui* gui = static_cast<Gui*>(param);

	Event event;
	while (true) {
		if (xQueueReceive(gui->getEventQueue(), &event, portMAX_DELAY) != pdPASS) {
			continue;
		}

		switch (event.type) {
			case Event::UNKNOWN:
				break;
			case Event::SET_SCREEN:
				{
					gui->setScreen(event.intData);
				}
				break;
			case Event::WAKE_UP:
				{
					Core::get()->getDisplayDriver()->setBacklightLevel(60);
				}
				break;
			case Event::NEW_SENSOR_DATA:
				{
					// Fuel Level, Oil Pressure, WaterTemperature
					// Rpm, Speed, LIndicator, RIndicator
					switch (gui->getScreen()) {
						case Gui::TEMPERATURE:
							{
								gui->setFuelLevel(event.canData[0]);
								gui->setOilPressure(event.canData[1]);
								gui->setWaterTemperature(event.canData[2]);
							}
							break;
						case Gui::SPEED:
							{
								gui->setSpeed(event.canData[5]);
								gui->setRightIndicatorActive(event.canData[7]);
							}
							break;
						case Gui::RPM:
							{
								gui->setRpm((event.canData[3] << 8) + event.canData[4]);
								gui->setLeftIndicatorActive(event.canData[6]);
							}
							break;
						default:;
					}
				}
				break;
			default:
				esp_rom_printf("default\n");
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

	// Init display
	display_ = lv_display_create(RESOLUTION, RESOLUTION);
	lv_display_set_user_data(display_, this);
	physicalDisplay_->setLvglDisplay(display_);

	// Create frame buffers
	frameBuffer1_ = static_cast<uint16_t*>(malloc(FRAME_BUFFER_SIZE_B));
	frameBuffer2_ = static_cast<uint16_t*>(malloc(FRAME_BUFFER_SIZE_B));
	if (frameBuffer1_ == nullptr || frameBuffer2_ == nullptr) {
		ESP_LOGE(TAG, "Failed to allocate memory for the frame buffers");
		return;
	}
	memset(frameBuffer1_, 0, FRAME_BUFFER_SIZE_B);
	memset(frameBuffer2_, 0, FRAME_BUFFER_SIZE_B);

	// Set frame buffers
	lv_display_set_buffers(display_, frameBuffer1_, frameBuffer2_, FRAME_BUFFER_SIZE_B, LV_DISPLAY_RENDER_MODE_PARTIAL);
	lv_display_set_color_format(display_, LV_COLOR_FORMAT_RGB565);
	lv_obj_set_style_bg_color(lv_display_get_screen_active(display_), lv_color_hex(0x000000),
							  LV_PART_MAIN); // Black color

	guiMutex_ = xSemaphoreCreateRecursiveMutex();

	// Register callbacks
	lv_display_set_flush_cb(display_, staticFlushToDisplay);
	lv_tick_set_cb(xTaskGetTickCount);

	// Pass the lvgl display to the ST77916 driver
	physicalDisplay_->setLvglDisplay(display_);

	// Create tasks
	if (xTaskCreate(lvglUpdateTaskFunc, "lvglUpdateTaskFunc", 10000, this, 3, &lvglUpdateTask_) != pdPASS) {
		ESP_LOGE(TAG, "Failed to create LVGL update task");
		return;
	}
	eventQueue_ = xQueueCreate(20, sizeof(Event));
	if (xTaskCreate(eventQueueTask, "GuiEventQueueTask", 8192, this, 4, &eventQueueHandle_) != pdPASS) {
		ESP_LOGE(TAG, "Failed to create Event Queue Task. Restarting...");
		esp_restart();
		vTaskDelay(pdMS_TO_TICKS(100000)); // Fallback
	}
}

SemaphoreHandle_t* Gui::getGuiMutex() { return &guiMutex_; }

void Gui::setScreen(const uint8_t screen)
{
	currentScreen_ = screen;

	GuiLock lock(guiMutex_);

	switch (screen) {
		case TEMPERATURE:
			create_screen_temperature();
			lv_scr_load(objects.temperature);
			break;
		case SPEED:
			create_screen_speed();
			lv_scr_load(objects.speed);
			break;
		case RPM:
			create_screen_rpm();
			lv_scr_load(objects.rpm);
			break;
		default:
			break;
	}
}

uint8_t Gui::getScreen() const { return currentScreen_; }

void Gui::queueEventFromISR(const Event& event) const
{
	BaseType_t woken = pdFALSE;
	if (xQueueSendFromISR(eventQueue_, &event, &woken) == pdFALSE) {
		esp_rom_printf("FALSE\n");
	}

	// Execute Context Switch if needed (woken == pdTRUE)
	portYIELD_FROM_ISR(woken);
}

QueueHandle_t Gui::getEventQueue() const { return eventQueue_; }

void Gui::setLeftIndicatorActive(const bool& active) const
{
	if (objects.left_indicator == nullptr) {
		return;
	}

	GuiLock lock(guiMutex_);

	if (active) {
		lv_obj_set_style_image_opa(objects.left_indicator, LV_OPA_100, LV_PART_MAIN);
	}
	else {
		lv_obj_set_style_image_opa(objects.left_indicator, LV_OPA_20, LV_PART_MAIN);
	}
}

void Gui::setRightIndicatorActive(const bool& active) const
{
	if (objects.right_indicator == nullptr) {
		return;
	}

	GuiLock lock(guiMutex_);

	if (active) {
		lv_obj_set_style_image_opa(objects.right_indicator, LV_OPA_100, LV_PART_MAIN);
	}
	else {
		lv_obj_set_style_image_opa(objects.right_indicator, LV_OPA_20, LV_PART_MAIN);
	}
}

void Gui::setSpeed(const uint8_t& speed) const
{
	if (objects.speed_label == nullptr) {
		return;
	}

	GuiLock lock(guiMutex_);

	lv_label_set_text(objects.speed_label, std::to_string(speed).c_str());
}

void Gui::setRpm(const uint16_t& rpm) const
{
	if (objects.rpm_label == nullptr) {
		return;
	}

	GuiLock lock(guiMutex_);

	lv_label_set_text(objects.rpm_label, std::to_string(rpm).c_str());
}

void Gui::setOilPressure(const bool& active) const
{
	if (objects.oil_can == nullptr) {
		return;
	}

	GuiLock lock(guiMutex_);

	if (active) {
		lv_obj_set_style_image_opa(objects.oil_can, LV_OPA_0, LV_PART_MAIN);
	}
	else {
		lv_obj_set_style_image_opa(objects.oil_can, LV_OPA_100, LV_PART_MAIN);
	}
}

void Gui::setFuelLevel(const uint8_t& fuelLevelPercent) const
{
	if (objects.percent_label == nullptr) {
		return;
	}

	GuiLock lock(guiMutex_);

	lv_label_set_text(objects.percent_label, (std::to_string(fuelLevelPercent) + "%").c_str());

	const std::vector<lv_obj_t*> arcs = {objects.arc0, objects.arc1, objects.arc2, objects.arc3, objects.arc4,
										 objects.arc5, objects.arc6, objects.arc7, objects.arc8, objects.arc9};
	for (uint8_t i = 0; i < fuelLevelPercent / 10; i++) {
		lv_obj_set_style_arc_opa(arcs[i], LV_OPA_100, LV_PART_MAIN);
	}
	for (uint8_t i = fuelLevelPercent / 10; i < 10; i++) {
		lv_obj_set_style_arc_opa(arcs[i], LV_OPA_20, LV_PART_MAIN);
	}
}

void Gui::setWaterTemperature(const int16_t& temperature) const
{
	if (objects.temperature_label == nullptr) {
		return;
	}

	GuiLock lock(guiMutex_);

	lv_label_set_text(objects.temperature_label, std::to_string(temperature).c_str());
}

/*
 *	Private Callback functions
 */
void Gui::flushToDisplay(lv_display_t* p_display, lv_area_t* p_area, uint8_t* p_pxMap) const
{
	if (physicalDisplay_ == nullptr) {
		return;
	}

	GuiLock lock(guiMutex_);

	// Swap each lower and upper byte to display the correct colors
	const uint32_t amountOfPixels = (p_area->x2 - p_area->x1 + 1) * (p_area->y2 - p_area->y1 + 1);
	lv_draw_sw_rgb565_swap(p_pxMap, amountOfPixels);

	physicalDisplay_->drawBitmap(p_area, p_pxMap);

	// lv_display_flush_ready is called by the ST77916 driver as the physical display
	// notifies when the buffer can be reused!
}
