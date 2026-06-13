#include "Driver/ST77916.hpp"

// Project includes
#include "Driver/ST77916_InitSequence.h"

// espidf includes
#include "display/lv_display.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "soc/gpio_num.h"

/*
 *	constexpr
 */
constexpr auto TAG = "ST77916";

constexpr uint16_t PIXEL = 360;

constexpr spi_host_device_t SPI_HOST = SPI2_HOST;

constexpr gpio_num_t GPIO_CLK = GPIO_NUM_10;
constexpr gpio_num_t GPIO_D0 = GPIO_NUM_14;
constexpr gpio_num_t GPIO_D1 = GPIO_NUM_13;
constexpr gpio_num_t GPIO_D2 = GPIO_NUM_12;
constexpr gpio_num_t GPIO_D3 = GPIO_NUM_11;
constexpr gpio_num_t GPIO_CS = GPIO_NUM_8;
constexpr gpio_num_t GPIO_RST = GPIO_NUM_9;
constexpr gpio_num_t GPIO_TE = GPIO_NUM_18;

constexpr st77916_vendor_config_t vendorConfig = {
	.init_cmds = st77916InitSequence,
	.init_cmds_size = sizeof(st77916InitSequence) / sizeof(st77916_lcd_init_cmd_t),
	.flags =
		{
			.use_qspi_interface = 1,
		},
};

constexpr gpio_num_t GPIO_BL = GPIO_NUM_7;
constexpr uint16_t BL_PWM_FQ_HZ = 5000;
constexpr ledc_timer_t BL_PWM_TIMER = LEDC_TIMER_0;
constexpr ledc_mode_t BL_PWM_MODE = LEDC_LOW_SPEED_MODE;
constexpr ledc_channel_t BL_PWM_CHANNEL = LEDC_CHANNEL_0;
constexpr ledc_timer_bit_t BL_PWM_RES = LEDC_TIMER_8_BIT;

/*
 *	Private static ISR
 */
//! \brief This ISR is triggered when the physical display is ready to accept new data.
//!
//! It then notifies an internal task which moves the new data into the display buffer
static IRAM_ATTR void teGpioIsr(void* arg)
{
	if (arg == nullptr) {
		return;
	}
	const auto taskHandle = static_cast<TaskHandle_t>(arg);

	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	vTaskNotifyGiveFromISR(taskHandle, &xHigherPriorityTaskWoken);
	portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

static IRAM_ATTR void drawToDisplayTask(void* param)
{
	if (param == nullptr) {
		return;
	}
	const auto drawData = static_cast<DrawData*>(param);

	while (true) {
		// Wait until we get notified
		if (!ulTaskNotifyTake(pdTRUE, portMAX_DELAY)) {
			continue;
		}

		if (!drawData->valid) {
			continue;
		}

		xSemaphoreTake(drawData->spiMutex, portMAX_DELAY);
		*drawData->isDrawing = true;

		// Draw data into the physical display buffer
		esp_lcd_panel_draw_bitmap(drawData->panelHandle, drawData->area.x1, drawData->area.y1, drawData->area.x2 + 1,
								  drawData->area.y2 + 1, drawData->pixelData);

		xSemaphoreGive(drawData->spiMutex);

		drawData->valid = false;
	}
}

static IRAM_ATTR bool onFrameDrawn(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t* edata,
								   void* user_ctx)
{
	const auto st77916 = static_cast<ST77916*>(user_ctx);
	if (st77916 != nullptr) {
		*st77916->getDrawData()->isDrawing = false;
		lv_display_flush_ready(st77916->getLvglDisplay());
	}
	return false;
}

/*
 *	Public Function Implementations
 */
ST77916::ST77916()
{
	/*
	 * Display initializations
	 */
	busConfig_ = {.data0_io_num = GPIO_D0,
				  .data1_io_num = GPIO_D1,
				  .sclk_io_num = GPIO_CLK,
				  .data2_io_num = GPIO_D2,
				  .data3_io_num = GPIO_D3,
				  .max_transfer_sz = 4092};

	// SPI Bus
	if (spi_bus_initialize(SPI_HOST, &busConfig_, SPI_DMA_CH_AUTO) != ESP_OK) {
		ESP_LOGE(TAG, "Failed to initialize the SPI bus");
		return;
	}

	// LCD Panel IO
	ioConfig_ = ST77916_PANEL_IO_QSPI_CONFIG(GPIO_CS, nullptr, this);
	ioConfig_.on_color_trans_done = onFrameDrawn;
	if (esp_lcd_new_panel_io_spi(SPI_HOST, &ioConfig_, &ioHandle_) != ESP_OK) {
		ESP_LOGE(TAG, "Failed to create LCD panel IO handle");
		return;
	}

	// LCD Panel
	const esp_lcd_panel_dev_config_t panelConfig = {
		.reset_gpio_num = GPIO_RST,
		.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
		.bits_per_pixel = 16,
		.vendor_config = (void*)&vendorConfig,
	};
	if (esp_lcd_new_panel_st77916(ioHandle_, &panelConfig, &panelHandle_) != ESP_OK) {
		ESP_LOGE(TAG, "Failed to create LCD panel");
		return;
	}

	if (esp_lcd_panel_reset(panelHandle_) != ESP_OK) {
		ESP_LOGE(TAG, "Failed to reset LCD panel");
		return;
	}
	if (esp_lcd_panel_init(panelHandle_) != ESP_OK) {
		ESP_LOGE(TAG, "Failed to initialize LCD panel");
		return;
	}
	if (esp_lcd_panel_disp_on_off(panelHandle_, true) != ESP_OK) {
		ESP_LOGE(TAG, "Failed to turn on LCD panel");
		return;
	}

	spiMutex_ = xSemaphoreCreateMutex();
	drawData_.isDrawing = &isDrawing_;
	drawData_.spiMutex = spiMutex_;

	/*
	 * BL Logic
	 */
	ledcTimer_ = {.speed_mode = BL_PWM_MODE,
				  .duty_resolution = BL_PWM_RES,
				  .timer_num = BL_PWM_TIMER,
				  .freq_hz = BL_PWM_FQ_HZ,
				  .clk_cfg = LEDC_AUTO_CLK};

	if (ledc_timer_config(&ledcTimer_) != ESP_OK) {
		ESP_LOGE(TAG, "Failed to initialize BL timer");
		return;
	}

	ledcChannel_ = {.gpio_num = GPIO_BL,
					.speed_mode = BL_PWM_MODE,
					.channel = BL_PWM_CHANNEL,
					.intr_type = LEDC_INTR_DISABLE,
					.timer_sel = BL_PWM_TIMER,
					.duty = 0,
					.hpoint = 0};
	if (ledc_channel_config(&ledcChannel_) != ESP_OK) {
		ESP_LOGE(TAG, "Failed to initialize the BL timer channel");
		return;
	}

	/*
	 *	TE GPIO Logic
	 */
	if (xTaskCreate(drawToDisplayTask, "DrawToDisplatTask", 4096, &this->drawData_, 4, &drawToDisplayTaskHandle_) !=
		pdPASS) {
		ESP_LOGE(TAG, "Failed to create draw to display task");
		esp_restart();
		vTaskDelay(pdMS_TO_TICKS(100000)); // Fallback
	}

	if (gpio_install_isr_service(ESP_INTR_FLAG_IRAM) != ESP_OK) {
		ESP_LOGE(TAG, "Failed to install the ISR service");
		return;
	}
	if (gpio_set_direction(GPIO_TE, GPIO_MODE_INPUT) != ESP_OK) {
		ESP_LOGE(TAG, "Failed to set the GPIO direction of the TE GPIO");
		return;
	}
	if (gpio_set_intr_type(GPIO_TE, GPIO_INTR_POSEDGE) != ESP_OK) {
		ESP_LOGE(TAG, "Failed to set the GPIO interruption type of the TE GPIO");
		return;
	}
	drawData_.panelHandle = panelHandle_;
	if (gpio_isr_handler_add(GPIO_TE, teGpioIsr, this->drawToDisplayTaskHandle_) != ESP_OK) {
		ESP_LOGE(TAG, "Failed to enable the ISR");
		return;
	}

	initialized_ = true;
}

void ST77916::setLvglDisplay(lv_display_t* lvDisplay)
{
	lvDisplay_ = lvDisplay;
	drawData_.lvDisplay = lvDisplay_;
}

lv_display_t* ST77916::getLvglDisplay() const { return lvDisplay_; }

void ST77916::setBacklightLevel(uint8_t percent)
{
	if (percent >= 100) {
		percent = 100;
	}

	constexpr uint32_t maxDuty = (1 << BL_PWM_RES) - 1;
	const uint32_t duty = (percent * maxDuty) / 100;

	// Neuen PWM-Wert setzen und anwenden
	ledc_set_duty(BL_PWM_MODE, BL_PWM_CHANNEL, duty);
	ledc_update_duty(BL_PWM_MODE, BL_PWM_CHANNEL);
}

DrawData* ST77916::getDrawData()
{
	return &drawData_;
}

void ST77916::drawBitmap(const lv_area_t* p_area, uint8_t* p_pxMap)
{
	drawData_.area = *p_area;
	drawData_.pixelData = p_pxMap;
	drawData_.valid = true;
}

void ST77916::setRotated(const bool& rotated) const
{
	if (spiMutex_ != nullptr) {
		xSemaphoreTake(spiMutex_, portMAX_DELAY);

		// Wait until frame was drawn
		while (isDrawing_) {
			vTaskDelay(pdMS_TO_TICKS(1));
		}
	}

	// Rotate the display
	if (esp_lcd_panel_mirror(panelHandle_, rotated, rotated) != ESP_OK) {
		ESP_LOGE(TAG, "Failed to rotate LCD panel");
	}

	// Return the mutex
	if (spiMutex_ != nullptr) {
		xSemaphoreGive(spiMutex_);
	}
}
