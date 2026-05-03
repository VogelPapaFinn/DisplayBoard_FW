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
 *	Private Static
 */
static IRAM_ATTR bool onFrameDrawn(esp_lcd_panel_io_handle_t panelIo, esp_lcd_panel_io_event_data_t* edata,
								   void* user_ctx)
{
	if (user_ctx == nullptr) {
		return false;
	}

	ST77916* instance = static_cast<ST77916*>(user_ctx);

	instance->frameDrawnIsr();

	return false;
}

/*
 *	Public Function Implementations
 */
ST77916::ST77916()
{
	/* Display initializations */
	busConfig_ = {.data0_io_num = GPIO_D0,
				  .data1_io_num = GPIO_D1,
				  .sclk_io_num = GPIO_CLK,
				  .data2_io_num = GPIO_D2,
				  .data3_io_num = GPIO_D3,
				  .max_transfer_sz = SOC_SPI_MAXIMUM_BUFFER_SIZE};

	// SPI Bus
	if (spi_bus_initialize(SPI_HOST, &busConfig_, SPI_DMA_CH_AUTO) != ESP_OK) {
		ESP_LOGE(TAG, "Failed to initialize the SPI bus");
		return;
	}

	// LCD Panel IO
	ioConfig_ = ST77916_PANEL_IO_QSPI_CONFIG(GPIO_CS, onFrameDrawn, this);
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

	/* BL Logic */
	ledcTimer_ = {.speed_mode = BL_PWM_MODE,
				  .duty_resolution = BL_PWM_RES,
				  .timer_num = BL_PWM_TIMER,
				  .freq_hz = BL_PWM_FQ_HZ,
				  .clk_cfg = LEDC_AUTO_CLK};
	if (ledc_timer_config(&ledcTimer_) != ESP_OK) {
		ESP_LOGE(TAG, "Failed to initialize BL timer");
		return;
	}

	// 2. Kanal konfigurieren und mit GPIO und Timer verknüpfen
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

	initialized_ = true;
}

void ST77916::setLvglDisplay(lv_display_t* lvDisplay) { lvDisplay_ = lvDisplay; }

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

void ST77916::drawBitmap(const lv_area_t* p_area, const uint8_t* p_pxMap) const
{
	esp_lcd_panel_draw_bitmap(panelHandle_, p_area->x1, p_area->y1, p_area->x2 + 1, p_area->y2 + 1, p_pxMap);
}

/*
 *	Public Callback functions
 */
void ST77916::frameDrawnIsr() const
{
	if (lvDisplay_ == nullptr) {
		return;
	}

	lv_display_flush_ready(lvDisplay_);
}
