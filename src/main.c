#include <string.h>

#include "esp_twai.h"
#include "esp_twai_onchip.h"
#include "freertos/FreeRTOS.h"

#include "driver/spi_common.h"
#include "esp_lcd_gc9a01.h"
#include "esp_lcd_io_spi.h"
#include "esp_lcd_panel_dev.h"
#include "esp_lcd_panel_interface.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_types.h"
#include "esp_timer.h"
#include "lvgl.h"

#define TEST_LCD_HOST SPI2_HOST
#define TEST_LCD_H_RES (240)
#define TEST_LCD_V_RES (240)
#define TEST_LCD_BIT_PER_PIXEL (16)

#define TEST_PIN_NUM_LCD_CS (GPIO_NUM_33)
#define TEST_PIN_NUM_LCD_DIN (GPIO_NUM_35)
#define TEST_PIN_NUM_LCD_PCLK (GPIO_NUM_34)
#define TEST_PIN_NUM_LCD_RST (GPIO_NUM_36)
#define TEST_PIN_NUM_LCD_DC (GPIO_NUM_21)

uint8_t recv_buff[8];
twai_frame_t rx_frame;
bool frameAccepted = false;
static bool twai_rx_cb(twai_node_handle_t handle, const twai_rx_done_event_data_t *edata, void *user_ctx)
{
    rx_frame.buffer = recv_buff;
    rx_frame.buffer_len = sizeof(recv_buff);

    if (ESP_OK == twai_node_receive_from_isr(handle, &rx_frame)) {
        frameAccepted = true;

        return true;
    }
    return false;
}

esp_lcd_panel_handle_t panel_handle = NULL;
void flush(lv_display_t *display, const lv_area_t *area, uint8_t *px_map) {
    lv_draw_sw_rgb565_swap(px_map, (area->x2 + 1 - area->x1) * (area->y2 + 1 - area->y1));
    // +1 needed, otherwise the image is distorted
    esp_lcd_panel_draw_bitmap(panel_handle, area->x1, area->y1, area->x2 + 1, area->y2 + 1, px_map);
    lv_display_flush_ready(display);
}

static void lv_tick_task(void *arg) {
    (void) arg;

    lv_tick_inc(1);
}

SemaphoreHandle_t xGuiSemaphore;
void app_main(void)
{
    xGuiSemaphore = xSemaphoreCreateMutex();

    spi_bus_config_t buscfg = {
        .sclk_io_num = TEST_PIN_NUM_LCD_PCLK,
        .mosi_io_num = TEST_PIN_NUM_LCD_DIN,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = SOC_SPI_MAXIMUM_BUFFER_SIZE //TEST_LCD_H_RES * 80 * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(TEST_LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = TEST_PIN_NUM_LCD_DC,
        .cs_gpio_num = TEST_PIN_NUM_LCD_CS,
        .pclk_hz = 60000000,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    // Attach the LCD to the SPI bus
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t) TEST_LCD_HOST, &io_config, &io_handle));

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = TEST_PIN_NUM_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = 16,
    };

    ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(io_handle, &panel_config, &panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, true, false));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, false));

    /* INIT LVGL */

    lv_init();
    lv_display_t *display = lv_display_create(TEST_LCD_H_RES, TEST_LCD_V_RES);
    size_t draw_buffer_sz = (TEST_LCD_H_RES * TEST_LCD_V_RES * 2);

    uint16_t *buf1 = heap_caps_malloc(draw_buffer_sz, MALLOC_CAP_SPIRAM);
    memset(buf1, 0, draw_buffer_sz);
    uint16_t *buf2 = heap_caps_malloc(draw_buffer_sz, MALLOC_CAP_SPIRAM);
    memset(buf2, 0, draw_buffer_sz);

    // This prevents that the display shows random colored pixels for a split of a second, during startup
    esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, 240, 240, buf1);

    // initialize LVGL draw buffers
    lv_display_set_buffers(display, buf1, buf2, draw_buffer_sz, LV_DISPLAY_RENDER_MODE_DIRECT);
    // set color depth
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    // set the callback which can copy the rendered image to an area of the display
    lv_display_set_flush_cb(display, flush);

    /* Create and start a periodic timer interrupt to call lv_tick_inc */
    const esp_timer_create_args_t periodic_timer_args = {
        .callback = &lv_tick_task,
        .name = "periodic_gui"
    };
    esp_timer_handle_t periodic_timer;
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, 1 * 1000));

    /*Create a white label, set its text and align it to the center*/
    lv_obj_t *label = lv_label_create(lv_screen_active());
    lv_label_set_text(label, "Hello world");
    lv_obj_set_style_text_color(lv_screen_active(), lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
    lv_display_flush_ready(display);

    // Clear the screen
    //lv_obj_clean(lv_screen_active());
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x000000), LV_PART_MAIN);
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    lv_style_t style;
    lv_style_init(&style);
    lv_style_set_line_color(&style, lv_color_make(255, 0, 0)); // RGB
    lv_style_set_line_width(&style, 2);
    lv_style_set_line_rounded(&style, false);

    lv_obj_t *vline = lv_line_create(lv_screen_active());
    static lv_point_precise_t line_points[] = {{120, 0}, {120, 240}};
    lv_line_set_points(vline, line_points, 2);
    lv_obj_add_style(vline, &style, 0);

    lv_style_t style2;
    lv_style_init(&style2);
    lv_style_set_line_color(&style2, lv_color_make(0, 255, 0)); // RBG
    lv_style_set_line_width(&style2, 2);
    lv_style_set_line_rounded(&style2, false);

    lv_obj_t *hline = lv_line_create(lv_screen_active());
    static lv_point_precise_t line_points2[] = {{0, 120}, {240, 120}};
    lv_line_set_points(hline, line_points2, 2);
    lv_obj_add_style(hline, &style2, 0);

    lv_obj_t *btn = lv_button_create(lv_screen_active()); /*Add a button the current screen*/
    lv_obj_set_pos(btn, 10, 10); /*Set its position*/
    lv_obj_set_size(btn, 120, 50); /*Set its size*/
    lv_obj_add_event_cb(btn, NULL, LV_EVENT_ALL, NULL); /*Assign a callback to the button*/
    lv_obj_center(btn);

    // lv_obj_t *label = lv_label_create(btn); /*Add a label to the button*/
    // lv_label_set_text(label, "Button"); /*Set the labels text*/
    // lv_obj_center(label);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10));

        /* Try to take the semaphore, call lvgl related function on success */
        if (pdTRUE == xSemaphoreTake(xGuiSemaphore, portMAX_DELAY)) {
            lv_task_handler();
            xSemaphoreGive(xGuiSemaphore);
        }
    }

    twai_node_handle_t node_hdl = NULL;
    twai_onchip_node_config_t node_config = {
        .io_cfg.tx = GPIO_NUM_9,             // TWAI TX GPIO pin
        .io_cfg.rx = GPIO_NUM_6,             // TWAI RX GPIO pin
        .bit_timing.bitrate = 200000,  // 200 kbps bitrate
        .tx_queue_depth = 5,        // Transmit queue depth set to 5
    };

    twai_event_callbacks_t user_cbs = {
        .on_rx_done = twai_rx_cb,
    };
    // Create a new TWAI controller driver instance
    ESP_ERROR_CHECK(twai_new_node_onchip(&node_config, &node_hdl));

    ESP_ERROR_CHECK(twai_node_register_event_callbacks(node_hdl, &user_cbs, NULL));

    // Start the TWAI controller
    ESP_ERROR_CHECK(twai_node_enable(node_hdl));

    while (true)
    {
        if (frameAccepted) {
            printf("Received a message with ID: %li and data: ", rx_frame.header.id);
            for (int i = 0; i < rx_frame.buffer_len; i++) {
                printf("%02X ", rx_frame.buffer[i]);
            }
            printf("\n");
            frameAccepted = false;
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
