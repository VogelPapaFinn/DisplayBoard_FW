#include "Screens/LvglTemperatureScreen.h"

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
	lv_obj_t* tempLabel;
	lv_style_t tempLabelStyle;
	lv_obj_t* celsiusLabel;
	lv_style_t celsiusStyle;
	lv_obj_t* fuelLevelArcs[10];
	lv_style_t fuelLevelArcStyle;
	lv_obj_t* fuelLevelInPercentLabel;
	lv_obj_t* fuelLevelInLitreLabel;
	lv_style_t fuelLevelLabelStyle;
	int lastFuelInPercent;
	char waterTemp[4];
	char fuelLevelP[5];
	char fuelLevelL[5];
} TempScreen_t;

/*
 *	Private variables
 */
static TempScreen_t* g_instance = NULL;

/*
 *	Public function implementations
 */
bool guiCreateAndShowTemperatureScreen(const SemaphoreHandle_t* p_guiSemaphore) // NOLINT
{
	if (g_instance != NULL) {
		return false;
	}

	// Create the struct
	g_instance = malloc(sizeof(TempScreen_t));

	// Create the GUI
	if (xSemaphoreTake(*p_guiSemaphore, portMAX_DELAY) == pdTRUE) {
		// Create a new screen
		g_instance->screen = lv_obj_create(NULL);

		// Set the background for the display
		lv_obj_set_style_bg_color(g_instance->screen, lv_color_hex(0x000000), LV_PART_MAIN);

		// Include fonts
		LV_FONT_DECLARE(E1234_80_FONT);
		LV_FONT_DECLARE(VCR_OSD_MONO_24_FONT);

		/*
		 *	Temp label
		 */

		// Create the temp label
		g_instance->tempLabel = lv_label_create(g_instance->screen);
		lv_style_init(&g_instance->tempLabelStyle);
		lv_style_set_text_color(&g_instance->tempLabelStyle, lv_color_hex(0x008F3C));
		lv_style_set_text_font(&g_instance->tempLabelStyle, &E1234_80_FONT);
		lv_obj_add_style(g_instance->tempLabel, &g_instance->tempLabelStyle, LV_PART_MAIN);

		// Style the temp label
		lv_obj_align(g_instance->tempLabel, LV_ALIGN_CENTER, 10, 0);
		lv_label_set_text(g_instance->tempLabel, "90");

		/*
		 *	Temp title label
		 */

		// Create the temp title label
		g_instance->celsiusLabel = lv_label_create(g_instance->screen);
		lv_style_init(&g_instance->celsiusStyle);
		lv_style_set_text_color(&g_instance->celsiusStyle, lv_color_hex(0x008F3C));
		lv_style_set_text_font(&g_instance->celsiusStyle, &VCR_OSD_MONO_24_FONT);
		lv_obj_add_style(g_instance->celsiusLabel, &g_instance->celsiusStyle, LV_PART_MAIN);

		// Style the temp title label
		lv_obj_align(g_instance->celsiusLabel, LV_ALIGN_RIGHT_MID, -10, 20);
		lv_label_set_text(g_instance->celsiusLabel, "°C");

		/*
		 *	Fuel level arc
		 */

		// Create the arc background style
		lv_style_init(&g_instance->fuelLevelArcStyle);
		lv_style_set_arc_color(&g_instance->fuelLevelArcStyle, lv_color_hex(0x008F3C));
		lv_style_set_arc_rounded(&g_instance->fuelLevelArcStyle, false);

		// Create the 10 arcs for the fuel level
		for (int i = 0; i < 10; i++) {
			// Create the arc
			g_instance->fuelLevelArcs[i] = lv_arc_create(g_instance->screen);
			lv_obj_set_width(g_instance->fuelLevelArcs[i], 220);
			lv_obj_set_height(g_instance->fuelLevelArcs[i], 220);
			lv_obj_set_style_arc_width(g_instance->fuelLevelArcs[i], 20, LV_PART_MAIN);

			// Position it
			lv_obj_center(g_instance->fuelLevelArcs[i]);
			lv_arc_set_bg_angles(g_instance->fuelLevelArcs[i], 0, 12);
			lv_arc_set_rotation(g_instance->fuelLevelArcs[i], 100 + i * 16); // Initial rotation + offset per arc
			lv_arc_set_value(g_instance->fuelLevelArcs[i], 100);

			// Remove the knob & indicator
			lv_obj_set_style_opa(g_instance->fuelLevelArcs[i], LV_OPA_0, LV_PART_KNOB);
			lv_obj_set_style_opa(g_instance->fuelLevelArcs[i], LV_OPA_0, LV_PART_INDICATOR);

			// Apply the background style
			lv_obj_add_style(g_instance->fuelLevelArcs[i], &g_instance->fuelLevelArcStyle, LV_PART_MAIN);

			// Color it accordingly
			if (i == 0) {
				lv_obj_set_style_arc_color(g_instance->fuelLevelArcs[i], lv_color_hex(0x992600), LV_PART_MAIN);
			}
			else if (i <= 2) {
				lv_obj_set_style_arc_color(g_instance->fuelLevelArcs[i], lv_color_hex(0xC69800), LV_PART_MAIN);
			}
		}

		/*
		 *	Fuel level in percent label
		 */

		// Create the fuel in percent label
		g_instance->fuelLevelInPercentLabel = lv_label_create(g_instance->screen);
		lv_style_init(&g_instance->fuelLevelLabelStyle);
		lv_style_set_text_color(&g_instance->fuelLevelLabelStyle, lv_color_hex(0x008F3C));
		lv_style_set_text_font(&g_instance->fuelLevelLabelStyle, &VCR_OSD_MONO_24_FONT);
		lv_obj_add_style(g_instance->fuelLevelInPercentLabel, &g_instance->fuelLevelLabelStyle, LV_PART_MAIN);

		// Style the fuel level in percent label
		lv_obj_align(g_instance->fuelLevelInPercentLabel, LV_ALIGN_TOP_MID, 15, 30);
		lv_label_set_text(g_instance->fuelLevelInPercentLabel, "100%");

		/*
		 *	Fuel level in litre label
		 */

		// Create the fuel in litre label
		g_instance->fuelLevelInLitreLabel = lv_label_create(g_instance->screen);
		lv_obj_add_style(g_instance->fuelLevelInLitreLabel, &g_instance->fuelLevelLabelStyle, LV_PART_MAIN);

		// Style the fuel level in litre label
		lv_obj_align(g_instance->fuelLevelInLitreLabel, LV_ALIGN_BOTTOM_MID, 15, -30);
		lv_label_set_text(g_instance->fuelLevelInLitreLabel, "50L");

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

void guiDestroyTemperatureScreen()
{
	if (g_instance != NULL) {
		free(g_instance);
	}
}
void guiSetWaterTemp(const uint8_t temp, const SemaphoreHandle_t* p_guiSemaphore)
{
	if (g_instance == NULL) {
		return;
	}

	if (xSemaphoreTake(*p_guiSemaphore, portMAX_DELAY) == pdTRUE) {
		// Clear the old text
		memset(&g_instance->waterTemp, ' ', sizeof(g_instance->waterTemp));

		// Set the text
		snprintf(g_instance->waterTemp, sizeof(g_instance->waterTemp), "%d", temp);

		// Apply it to the label
		lv_label_set_text(g_instance->tempLabel, g_instance->waterTemp);

		xSemaphoreGive(*p_guiSemaphore);
	}
}

void guiSetFuelLevel(const uint8_t levelInPercent, const SemaphoreHandle_t* p_guiSemaphore)
{
	if (g_instance == NULL) {
		return;
	}

	if (xSemaphoreTake(*p_guiSemaphore, portMAX_DELAY) == pdTRUE) {
		// Clear the old text
		memset(&g_instance->fuelLevelP, ' ', sizeof(g_instance->fuelLevelP));
		memset(&g_instance->fuelLevelL, ' ', sizeof(g_instance->fuelLevelL));

		// Set the text
		snprintf(g_instance->fuelLevelP, sizeof(g_instance->fuelLevelP), "%d%%", levelInPercent);

		// Convert the percent to a double
		const double percent = ((double)levelInPercent) / 100.0;

		// Special case if levelInPercent is 0
		if (percent == 0) {
			snprintf(g_instance->fuelLevelL, sizeof(g_instance->fuelLevelL), "%dL", 0);
		} else {
			snprintf(g_instance->fuelLevelL, sizeof(g_instance->fuelLevelL), "%dL",
					 (uint8_t)(50.0 / percent));
		}

		// Apply the text to the labels
		lv_label_set_text(g_instance->fuelLevelInLitreLabel, g_instance->fuelLevelL);
		lv_label_set_text(g_instance->fuelLevelInPercentLabel, g_instance->fuelLevelP);

		xSemaphoreGive(*p_guiSemaphore);
	}
}
