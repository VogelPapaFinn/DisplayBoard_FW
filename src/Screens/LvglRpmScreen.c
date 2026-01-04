#include "Screens/LvglRpmScreen.h"

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
	lv_obj_t* rpmLabel;
	lv_style_t rpmLabelStyle;
	lv_obj_t* rpmTitleLabel;
	lv_style_t rpmTitleStyle;
	lv_obj_t* leftIndicator;
	char rpm[7];
} RpmScreen_t;

/*
 *	Private variables
 */
static RpmScreen_t* g_instance = NULL;

/*
 *	Public function implementations
 */
bool guiCreateAndShowRpmScreen(const SemaphoreHandle_t* p_guiSemaphore)
{
	if (g_instance != NULL) {
		return false;
	}

	// Create the struct
	g_instance = malloc(sizeof(RpmScreen_t));

	// Create the GUI
	if (xSemaphoreTake(*p_guiSemaphore, portMAX_DELAY) == pdTRUE) {
		// Create a new g_instance->g_instance->screen
		g_instance->screen = lv_obj_create(NULL);

		// Set the background for the display
		lv_obj_set_style_bg_color(g_instance->screen, lv_color_hex(0x000000), LV_PART_MAIN);

		// Include fonts
		LV_FONT_DECLARE(E1234_70_FONT);
		LV_FONT_DECLARE(VCR_OSD_MONO_24_FONT);
		LV_IMAGE_DECLARE(leftIndicator);

		/*
		 *	Rpm label
		 */

		// Create the rpm label
		g_instance->rpmLabel = lv_label_create(g_instance->screen);
		lv_style_init(&g_instance->rpmLabelStyle);
		lv_style_set_text_color(&g_instance->rpmLabelStyle, lv_color_hex(0x008F3C));
		lv_style_set_text_font(&g_instance->rpmLabelStyle, &E1234_70_FONT);
		lv_obj_add_style(g_instance->rpmLabel, &g_instance->rpmLabelStyle, LV_PART_MAIN);

		// Style the rpm label
		lv_obj_center(g_instance->rpmLabel);
		lv_label_set_text(g_instance->rpmLabel, "7700");

		/*
		 *	Rpm title label
		 */

		// Create the rpm title label
		g_instance->rpmTitleLabel = lv_label_create(g_instance->screen);
		lv_style_init(&g_instance->rpmTitleStyle);
		lv_style_set_text_color(&g_instance->rpmTitleStyle, lv_color_hex(0x008F3C));
		lv_style_set_text_font(&g_instance->rpmTitleStyle, &VCR_OSD_MONO_24_FONT);
		lv_obj_add_style(g_instance->rpmTitleLabel, &g_instance->rpmTitleStyle, LV_PART_MAIN);

		// Style the rpm title label
		lv_obj_align(g_instance->rpmTitleLabel, LV_ALIGN_CENTER, 0, -80);
		lv_label_set_text(g_instance->rpmTitleLabel, "RPM");

		/*
		 *	Left indicator
		 */

		// Create the left indicator arrow
		g_instance->leftIndicator = lv_image_create(g_instance->screen);
		lv_image_set_src(g_instance->leftIndicator, &leftIndicator);

		// Style the left indicator
		lv_obj_align(g_instance->leftIndicator, LV_ALIGN_CENTER, 0, 90);

		// Disable the indicator visually
		lv_obj_set_style_opa(g_instance->leftIndicator, LV_OPA_20, LV_PART_MAIN);

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

void guiDestroyRpmScreen()
{
	if (g_instance != NULL) {
		free(g_instance);
	}
}

void guiSetRpm(const uint16_t rpm, const SemaphoreHandle_t* p_guiSemaphore)
{
	if (g_instance == NULL) {
		return;
	}

	if (xSemaphoreTake(*p_guiSemaphore, portMAX_DELAY) == pdTRUE) {
		// Clear the old text
		memset(&g_instance->rpm, ' ', sizeof(g_instance->rpm));

		// Set the text
		snprintf(g_instance->rpm, sizeof(g_instance->rpm), "%d", rpm);

		// Apply it to the label
		lv_label_set_text(g_instance->rpmLabel, g_instance->rpm);

		xSemaphoreGive(*p_guiSemaphore);
	}
}

void guiSetLeftIndicatorActive(const bool active, const SemaphoreHandle_t* p_guiSemaphore)
{
	if (g_instance == NULL) {
		return;
	}

	if (xSemaphoreTake(*p_guiSemaphore, portMAX_DELAY) == pdTRUE) {
		if (active) {
			// Activate the indicator visually
			lv_obj_set_style_opa(g_instance->leftIndicator, LV_OPA_100, LV_PART_MAIN);
		} else {
			// Deactivate the indicator visually
			lv_obj_set_style_opa(g_instance->leftIndicator, LV_OPA_20, LV_PART_MAIN);
		}

		xSemaphoreGive(*p_guiSemaphore);
	}
}
