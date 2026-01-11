#include "GUI.h"

// Project includes
#include "EventQueues.h"
#include "Screens/LvglRpmScreen.h"
#include "Screens/LvglSpeedScreen.h"
#include "Screens/LvglTemperatureScreen.h"

// espidf includes
#include <driver/gpio.h>
#include <driver/spi_common.h>
#include <esp_lcd_gc9a01.h>
#include <esp_lcd_io_spi.h>
#include <esp_lcd_panel_dev.h>
#include <esp_lcd_panel_interface.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_types.h>
#include <esp_log.h>

// LVGL includes
#include "lvgl.h"

/*
 *	Private defines
 */
#define LCD_SPI_HOST SPI2_HOST
#define SPI_SPEED 10000000

#define LCD_RESOLUTION 240
#define LCD_BIT_DEPTH 16
#define LCD_BYTE_DEPTH (LCD_BIT_DEPTH / 8)
#define FRAME_BUFFER_SIZE_B (LCD_RESOLUTION * LCD_RESOLUTION * LCD_BYTE_DEPTH)
#define DELAY_BETWEEN_DRAWING_MS 1

#define GPIO_LCD_CS GPIO_NUM_33
#define GPIO_LCD_CLK GPIO_NUM_34
#define GPIO_LCD_DIN GPIO_NUM_35
#define GPIO_LCD_RST GPIO_NUM_36
#define GPIO_LCD_DC GPIO_NUM_21

/*
 *	Private typedefs
 */
typedef void (*EventHandlerFunction_t)(const QueueEvent_t* p_queueEvent);

/*
 *	Prototypes
 */
static void flushPixelsToDisplay(lv_display_t* p_display, const lv_area_t* p_area, uint8_t* p_pxMap);

//! \brief Task which is needed for lvgl to work
//! \param p_params void* needed for FreeRTOS to accept this function as task!
static void IRAM_ATTR lvglUpdateTask(void* p_params);

static void handleNewSensorData(const QueueEvent_t* p_queueEvent);

/*
 *	Private variables
 */
static bool g_refresh = true;

static esp_lcd_panel_io_handle_t g_lcdPanelIoHandle = NULL;
static esp_lcd_panel_handle_t g_lcdPanelHandle = NULL;

static SemaphoreHandle_t g_lvglGuiSemaphore = NULL;
static SemaphoreHandle_t g_lvglDrawSemaphore = NULL;

static lv_display_t* g_lvglDisplay = NULL;
static uint16_t* g_lvglFrameBuffer1 = NULL;
static uint16_t* g_lvglFrameBuffer2 = NULL;

static bool g_lvglFirstFrameDrawn = false;
static Screen_t g_currentScreen = SCREEN_UNKNOWN;

/*
 *	ISRs and Tasks
 */
static void flushPixelsToDisplay(lv_display_t* p_display, const lv_area_t* p_area, uint8_t* p_pxMap)
{
	// Swap the color channels as needed
	lv_draw_sw_rgb565_swap(p_pxMap, (p_area->x2 + 1 - p_area->x1) * (p_area->y2 + 1 - p_area->y1)); // NOLINT

	// Then draw the bitmap to the physical display (+1 needed, otherwise the image is distorted)
	if (xSemaphoreTake(g_lvglDrawSemaphore, portMAX_DELAY) == pdTRUE) {
		esp_lcd_panel_draw_bitmap(g_lcdPanelHandle, p_area->x1, p_area->y1, p_area->x2 + 1, p_area->y2 + 1, // NOLINT
								  p_pxMap); // NOLINT
		// vTaskDelay(pdMS_TO_TICKS(DELAY_BETWEEN_DRAWING_MS));
		xSemaphoreGive(g_lvglDrawSemaphore);
	}

	// Turn the lcd panel on if it's the first image drawn
	if (!g_lvglFirstFrameDrawn) {
		g_lvglFirstFrameDrawn = true;
		// Turn the display on
		if (esp_lcd_panel_disp_on_off(g_lcdPanelHandle, true) != ESP_OK) {
			esp_rom_printf("GUI", "Couldn't turn on LCD panel");
		}
	}

	lv_display_flush_ready(g_lvglDisplay);
}

static void lvglUpdateTask(void* p_params)
{
	while (true) {
		// Try to get the semaphore
		if (xSemaphoreTake(g_lvglGuiSemaphore, portMAX_DELAY) == pdTRUE) {
			// Run the lvgl task handler
			lv_timer_handler();

			// Give the semaphore free
			xSemaphoreGive(g_lvglGuiSemaphore);

			// Wait 10ms
			vTaskDelay(pdMS_TO_TICKS(10));
		}
	}
}

static void guiEventQueueTask()
{
	QueueEvent_t queueEvent;
	while (true) {
		// Wait until we get a new event
		if (xQueueReceive(g_guiEventQueue, &queueEvent, portMAX_DELAY)) {
			if (!g_refresh) {
				continue;
			}

			switch (queueEvent.command) {
				case DISPLAY_TEMPERATURE_SCREEN:
					{
						guiDisplayScreen(SCREEN_TEMPERATURE);
						break;
					}
				case DISPLAY_SPEED_SCREEN:
					{
						guiDisplayScreen(SCREEN_SPEED);
						break;
					}
				case DISPLAY_RPM_SCREEN:
					{
						guiDisplayScreen(SCREEN_RPM);
						break;
					}
				case NEW_SENSOR_DATA:
					handleNewSensorData(&queueEvent);
					break;
				default:
					break;
			}
		}
	}
}

/*
 *	Private functions
 */
static bool initDisplay()
{
	// Create the SPI config for the LCD
	const esp_lcd_panel_io_spi_config_t lcdPanelIoConfig = {
		.dc_gpio_num = GPIO_LCD_DC,
		.cs_gpio_num = GPIO_LCD_CS,
		.pclk_hz = SPI_SPEED,
		.lcd_cmd_bits = 8,
		.lcd_param_bits = 8,
		.spi_mode = 0,
		.trans_queue_depth = 10,
	};

	// Initialize the LCD
	if (esp_lcd_new_panel_io_spi(LCD_SPI_HOST, &lcdPanelIoConfig, &g_lcdPanelIoHandle) != ESP_OK) {
		ESP_LOGE("GUI", "Failed to initialize LCD panel");

		return false;
	}

	// Then create the panel config
	const esp_lcd_panel_dev_config_t lcdPanelDevConfig = {
		.reset_gpio_num = GPIO_LCD_RST,
		.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
		.bits_per_pixel = 16,
	};

	// Create a new GC9A01 panel
	if (esp_lcd_new_panel_gc9a01(g_lcdPanelIoHandle, &lcdPanelDevConfig, &g_lcdPanelHandle) != ESP_OK) {
		ESP_LOGE("GUI", "Couldn't create GC9A01 panel");
		return false;
	}

	// Reset the panel
	if (esp_lcd_panel_reset(g_lcdPanelHandle) != ESP_OK) {
		ESP_LOGE("GUI", "Couldn't reset panel");
		return false;
	}

	// Initialize the panel
	if (esp_lcd_panel_init(g_lcdPanelHandle)) {
		ESP_LOGE("GUI", "Couldn't initialize LCD panel");
		return false;
	}

	// Invert the color
	if (esp_lcd_panel_invert_color(g_lcdPanelHandle, true) != ESP_OK) {
		ESP_LOGE("GUI", "Couldn't invert color of LCD panel");
		return false;
	}

	// Mirror the screen on the x-axis
	if (esp_lcd_panel_mirror(g_lcdPanelHandle, true, false) != ESP_OK) {
		ESP_LOGE("GUI", "Couldn't mirror LCD panel");
		return false;
	}

	// Turn the display off
	if (esp_lcd_panel_disp_on_off(g_lcdPanelHandle, false) != ESP_OK) {
		ESP_LOGE("GUI", "Couldn't turn off LCD panel");
		return false;
	}

	// Everything was successful
	return true;
}

static bool initLvgl()
{
	// Create the needed semaphores
	g_lvglGuiSemaphore = xSemaphoreCreateMutex();
	g_lvglDrawSemaphore = xSemaphoreCreateMutex();

	// Call the lvgl init function
	lv_init();

	// Create the display
	g_lvglDisplay = lv_display_create(LCD_RESOLUTION, LCD_RESOLUTION);

	// Create the two frame buffers
	g_lvglFrameBuffer1 = (uint16_t*)malloc(FRAME_BUFFER_SIZE_B);
	g_lvglFrameBuffer2 = (uint16_t*)malloc(FRAME_BUFFER_SIZE_B);

	// Check if they were allocated
	if (g_lvglFrameBuffer1 == NULL || g_lvglFrameBuffer2 == NULL) {
		ESP_LOGE("GUI", "Couldn't allocate memory for the two frame buffers");
		return false;
	}

	// Clear the buffers
	memset(g_lvglFrameBuffer1, 0, FRAME_BUFFER_SIZE_B);
	memset(g_lvglFrameBuffer2, 0, FRAME_BUFFER_SIZE_B);

	// Pass LVGL the draw buffers
	lv_display_set_buffers(g_lvglDisplay, g_lvglFrameBuffer1, g_lvglFrameBuffer2, FRAME_BUFFER_SIZE_B,
						   LV_DISPLAY_RENDER_MODE_PARTIAL);

	// Set the color formats
	lv_display_set_color_format(g_lvglDisplay, LV_COLOR_FORMAT_RGB565);

	// Set the background for the active screen
	lv_obj_set_style_bg_color(lv_display_get_screen_active(g_lvglDisplay), lv_color_hex(0x000000), LV_PART_MAIN);

	// Set the callback function, to draw to the physical displays
	lv_display_set_flush_cb(g_lvglDisplay, flushPixelsToDisplay);

	// Set tick interface for animations etc.
	lv_tick_set_cb(xTaskGetTickCount);

	if (xTaskCreate(lvglUpdateTask, "lvglUpdateTask", 10000, NULL, 0, NULL) != pdPASS) {
		// Logging
		ESP_LOGE("GUI", "Failed to create task: \"lvglUpdateTask\"!");

		return false;
	}

	// Everything worked
	return true;
}

static void handleNewSensorData(const QueueEvent_t* p_queueEvent)
{
	if (p_queueEvent == NULL || p_queueEvent->canFrame.buffer == NULL || g_currentScreen == SCREEN_UNKNOWN) {
		return;
	}

	// Get the message
	const uint8_t* frameBuffer = p_queueEvent->canFrame.buffer;

	// Get the oil pressure
	const bool oilPressure = *(frameBuffer + 5);

	// Act depending on the current screen
	switch (g_currentScreen) {
		case SCREEN_TEMPERATURE:
			{
				// Set the temperature
				const uint8_t waterTemp = *(frameBuffer + 4);
				guiSetWaterTemp(waterTemp, &g_lvglGuiSemaphore);

				// Set the fuel level in %
				const uint8_t fuelLevel = *(frameBuffer + 3);
				guiSetFuelLevel(fuelLevel, &g_lvglGuiSemaphore);

				break;
			}
		case SCREEN_SPEED:
			{
				// Set the speed
				const uint8_t speedKmh = *(frameBuffer + 0);
				guiSetSpeed(speedKmh, &g_lvglGuiSemaphore);

				// Get the status of the right indicator
				const bool indicatorActive = *(frameBuffer + 7);
				guiSetRightIndicatorActive((bool)indicatorActive, &g_lvglGuiSemaphore);

				break;
			}
		case SCREEN_RPM:
			{
				// Set the rpm
				const uint16_t lowerRpmByte = *(frameBuffer + 2);
				const uint16_t upperRpmByte = *(frameBuffer + 1) << 8;
				const uint16_t rpm = lowerRpmByte + upperRpmByte;
				guiSetRpm(rpm, &g_lvglGuiSemaphore);

				// Get the status of the left indicator
				const bool indicatorActive = *(frameBuffer + 6);
				guiSetLeftIndicatorActive((bool)indicatorActive, &g_lvglGuiSemaphore);

				break;
			}
		default:
			ESP_LOGE("GUI", "Currently displaying an invalid screen. Couldn't update data");
			break;
	}
}

/*
 *	Public function implementations
 */
bool guiInit()
{
	// Create SPI bus config
	const spi_bus_config_t spiBusConfig = {.sclk_io_num = GPIO_LCD_CLK,
										   .mosi_io_num = GPIO_LCD_DIN,
										   .miso_io_num = -1,
										   .quadwp_io_num = -1,
										   .quadhd_io_num = -1,
										   .max_transfer_sz = SOC_SPI_MAXIMUM_BUFFER_SIZE};

	// Initialize SPI bus
	if (spi_bus_initialize(LCD_SPI_HOST, &spiBusConfig, SPI_DMA_CH_AUTO) != ESP_OK) {
		// Logging
		ESP_LOGE("GUI", "Failed to initialize SPI bus");

		return false;
	}

	// Initialize the display
	if (!initDisplay()) {
		ESP_LOGE("GUI", "Failed to initialize display");
		return false;
	}

	// Initialize lvgl if not yet happened
	if (!initLvgl()) {
		ESP_LOGE("GUI", "Failed to initialize LVGL");

		return false;
	}

	// Start the task which will handle all the queue events
	if (xTaskCreate(guiEventQueueTask, "handleGuiEventQueueTask", 16384 / 4, NULL, 2, NULL) != pdPASS) {
		// Logging
		ESP_LOGE("GUI", "Failed to create task: \"handleGuiEventQueueTask\"!");

		return false;
	}

	return true;
}

void guiActivateRefreshing()
{
	g_refresh = true;
}

void guiDeactivateRefreshing()
{
	g_refresh = false;
}

bool guiDisplayScreen(const Screen_t screen)
{
	ESP_LOGI("GUI", "Displaying screen: %d", screen);

	// Destroy all screens if necessary
	if (g_currentScreen != SCREEN_UNKNOWN) {
		guiDestroyTemperatureScreen();
		guiDestroySpeedScreen();
		guiDestroyRpmScreen();
	}

	// Create the screen and show it
	g_currentScreen = screen;
	switch (screen) {
		case SCREEN_TEMPERATURE:
			return guiCreateAndShowTemperatureScreen(&g_lvglGuiSemaphore);
		case SCREEN_SPEED:
			return guiCreateAndShowSpeedScreen(&g_lvglGuiSemaphore);
		case SCREEN_RPM:
			return guiCreateAndShowRpmScreen(&g_lvglGuiSemaphore);
		default:
			ESP_LOGW("GUI", "Unknown screen: %d", screen);
			return false;
	}
}
