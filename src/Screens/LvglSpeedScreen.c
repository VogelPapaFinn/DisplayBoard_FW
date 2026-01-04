#include "Screens/LvglSpeedScreen.h"

// C includes
#include <string.h>

// LVGL include
#include "lvgl.h"

/*
 *	Private typedefs
 */
typedef struct
{
	lv_obj_t* screen;
	lv_obj_t* speedLabel;
	lv_style_t speedLabelStyle;
	lv_obj_t* kmhLabel;
	lv_style_t kmhLabelStyle;
	lv_obj_t* rightIndicator;
	char speed[4];
} SpeedScreen_t;

/*
 *	Private variables
 */
static SpeedScreen_t* g_instance = NULL;

/*
 *	Public function implementations
 */
bool guiCreateAndShowSpeedScreen(const SemaphoreHandle_t* p_guiSemaphore)
{
	if (g_instance != NULL) {
		return false;
	}

	// Create the struct
	g_instance = malloc(sizeof(SpeedScreen_t));

	// Create the GUI
	if (xSemaphoreTake(*p_guiSemaphore, portMAX_DELAY) == pdTRUE) {
		// Create a new g_instance->screen
		g_instance->screen = lv_obj_create(NULL);

		// Set the background for the display
		lv_obj_set_style_bg_color(g_instance->screen, lv_color_hex(0x000000), LV_PART_MAIN);

		// Include fonts
		LV_FONT_DECLARE(E1234_80_FONT);
		LV_FONT_DECLARE(VCR_OSD_MONO_24_FONT);
		LV_IMAGE_DECLARE(rightIndicator);

		/*
		 *	Speedometer label
		 */

		// Create the speedometer label
		g_instance->speedLabel = lv_label_create(g_instance->screen);
		lv_style_init(&g_instance->speedLabelStyle);
		lv_style_set_text_color(&g_instance->speedLabelStyle, lv_color_hex(0x008F3C));
		lv_style_set_text_font(&g_instance->speedLabelStyle, &E1234_80_FONT);
		lv_obj_add_style(g_instance->speedLabel, &g_instance->speedLabelStyle, LV_PART_MAIN);

		// Style the speedometer label
		lv_obj_center(g_instance->speedLabel);
		lv_label_set_text(g_instance->speedLabel, "200");

		/*
		 *	Kmh label
		 */

		// Create the kmh label
		g_instance->kmhLabel = lv_label_create(g_instance->screen);
		lv_style_init(&g_instance->kmhLabelStyle);
		lv_style_set_text_color(&g_instance->kmhLabelStyle, lv_color_hex(0x008F3C));
		lv_style_set_text_font(&g_instance->kmhLabelStyle, &VCR_OSD_MONO_24_FONT);
		lv_obj_add_style(g_instance->kmhLabel, &g_instance->kmhLabelStyle, LV_PART_MAIN);

		// Style the kmh label
		lv_obj_align(g_instance->kmhLabel, LV_ALIGN_CENTER, 0, -80);
		lv_label_set_text(g_instance->kmhLabel, "kmh");

		/*
		 *	Right indicator
		 */

		// Create the indicator right arrow
		g_instance->rightIndicator = lv_image_create(g_instance->screen);
		lv_image_set_src(g_instance->rightIndicator, &rightIndicator);

		// Position it centered at the bottom
		lv_obj_align(g_instance->rightIndicator, LV_ALIGN_CENTER, 0, 90);

		// Disable the indicator visually
		lv_obj_set_style_opa(g_instance->rightIndicator, LV_OPA_20, LV_PART_MAIN);

		/*
		 *	Load the screen
		 */
		lv_screen_load(g_instance->screen);

		// Free the semaphore
		xSemaphoreGive(*p_guiSemaphore);
		return true;
	}

	return false;
}

void guiDestroySpeedScreen()
{
	if (g_instance != NULL) {
		free(g_instance);
	}
}
void guiSetSpeed(const uint8_t speedKmh, const SemaphoreHandle_t* p_guiSemaphore)
{
	if (g_instance == NULL) {
		return;
	}

	if (xSemaphoreTake(*p_guiSemaphore, portMAX_DELAY) == pdTRUE) {
		// Clear the old text
		memset(&g_instance->speed, ' ', sizeof(g_instance->speed));

		// Set the text
		snprintf(g_instance->speed, sizeof(g_instance->speed), "%d", speedKmh);

		// Apply it to the label
		lv_label_set_text(g_instance->speedLabel, g_instance->speed);

		xSemaphoreGive(*p_guiSemaphore);
	}
}

void guiSetRightIndicatorActive(const bool active, const SemaphoreHandle_t* p_guiSemaphore)
{
	if (g_instance == NULL) {
		return;
	}

	if (xSemaphoreTake(*p_guiSemaphore, portMAX_DELAY) == pdTRUE) {
		if (active) {
			// Activate the indicator visually
			lv_obj_set_style_opa(g_instance->rightIndicator, LV_OPA_100, LV_PART_MAIN);
		} else {
			// Deactivate the indicator visually
			lv_obj_set_style_opa(g_instance->rightIndicator, LV_OPA_20, LV_PART_MAIN);
		}

		xSemaphoreGive(*p_guiSemaphore);
	}
}