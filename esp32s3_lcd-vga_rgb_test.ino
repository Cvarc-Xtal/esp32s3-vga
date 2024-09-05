#include <Arduino.h>
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "lvgl.h" //version 8.3.11
#include "global.h"
#include "lv_screen.h"

static bool on_vsync_event(esp_lcd_panel_handle_t panel, const esp_lcd_rgb_panel_event_data_t *event_data, void *user_data)
{
    BaseType_t high_task_awoken = pdFALSE;
    return high_task_awoken == pdTRUE;
}

void setup()
{
    static lv_disp_draw_buf_t disp_buf; // contains internal graphic buffer(s) called draw buffer(s)
    static lv_disp_drv_t disp_drv;      // contains callback functions

    ESP_LOGI(TAG, "Install RGB LCD panel driver");
    esp_lcd_panel_handle_t panel_handle = NULL;

    esp_lcd_rgb_panel_config_t panel_config{};
    panel_config.data_width = 16;
    panel_config.num_fbs = 0;
    panel_config.bounce_buffer_size_px = bounceBufferLines * lcd_h_res;
    //panel_config.dma_burst_size = 64;                  //this arduino-esp32 version 3.1 and above only
    panel_config.flags.fb_in_psram = true;               // allocate frame buffer in PSRAM
    panel_config.psram_trans_align = 64,
    panel_config.clk_src = LCD_CLK_SRC_PLL240M;
    panel_config.pclk_gpio_num = PIN_NUM_PCLK;
    panel_config.pclk_gpio_num = PIN_NUM_PCLK;
    panel_config.vsync_gpio_num = PIN_NUM_VSYNC;
    panel_config.hsync_gpio_num = PIN_NUM_HSYNC;
    panel_config.de_gpio_num = PIN_NUM_DE;
    panel_config.data_gpio_nums[0] = PIN_NUM_DATA0;
    panel_config.data_gpio_nums[1] = PIN_NUM_DATA1;
    panel_config.data_gpio_nums[2] = PIN_NUM_DATA2;
    panel_config.data_gpio_nums[3] = PIN_NUM_DATA3;
    panel_config.data_gpio_nums[4] = PIN_NUM_DATA4;
    panel_config.data_gpio_nums[5] = PIN_NUM_DATA5;
    panel_config.data_gpio_nums[6] = PIN_NUM_DATA6;
    panel_config.data_gpio_nums[7] = PIN_NUM_DATA7;
    panel_config.data_gpio_nums[8] = PIN_NUM_DATA8;
    panel_config.data_gpio_nums[9] = PIN_NUM_DATA9;
    panel_config.data_gpio_nums[10] = PIN_NUM_DATA10;
    panel_config.data_gpio_nums[11] = PIN_NUM_DATA11;
    panel_config.data_gpio_nums[12] = PIN_NUM_DATA12;
    panel_config.data_gpio_nums[13] = PIN_NUM_DATA13;
    panel_config.data_gpio_nums[14] = PIN_NUM_DATA14;
    panel_config.data_gpio_nums[15] = PIN_NUM_DATA15;
    panel_config.timings.pclk_hz = LCD_PIXEL_CLOCK_HZ;
    panel_config.timings.h_res = lcd_h_res;
    panel_config.timings.v_res = lcd_v_res;
    panel_config.timings.hsync_front_porch = hsync_front_porch;
    panel_config.timings.hsync_pulse_width = hsync_pulse_width;
    panel_config.timings.hsync_back_porch = hsync_back_porch;
    panel_config.timings.vsync_front_porch = vsync_front_porch;
    panel_config.timings.vsync_pulse_width = vsync_pulse_width;
    panel_config.timings.vsync_back_porch = vsync_back_porch;

    ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(&panel_config, &panel_handle));

    ESP_LOGI(TAG, "Register event callbacks");
    esp_lcd_rgb_panel_event_callbacks_t cbs = {
        .on_vsync = on_vsync_event,
        //.on_bounce_empty =        //function for alternate draw metode
    };
    ESP_ERROR_CHECK(esp_lcd_rgb_panel_register_event_callbacks(panel_handle, &cbs, &disp_drv));

    ESP_LOGI(TAG, "Initialize RGB LCD panel");
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));

    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();
    void *buf1 = NULL;
    void *buf2 = NULL;
    ESP_LOGI(TAG, "Allocate separate LVGL draw buffers from PSRAM");
    buf1 = heap_caps_malloc(lcd_h_res * 100 * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    assert(buf1);
    // initialize LVGL draw buffers
    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, lcd_h_res * 100);


    ESP_LOGI(TAG, "Register display driver to LVGL");
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = lcd_h_res;
    disp_drv.ver_res = lcd_v_res;
    disp_drv.flush_cb = lvgl_flush_cb;
    disp_drv.draw_buf = &disp_buf;
    disp_drv.user_data = panel_handle;

    lv_disp_t *disp = lv_disp_drv_register(&disp_drv);

    ESP_LOGI(TAG, "Install LVGL tick timer");
    // Tick interface for LVGL (using esp_timer to generate 2ms periodic event)
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &increase_lvgl_tick,
        .name = "lvgl_tick"
    };
    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, LVGL_TICK_PERIOD_MS * 10));

    lvgl_mux = xSemaphoreCreateRecursiveMutex();
    assert(lvgl_mux);
    ESP_LOGI(TAG, "Create LVGL task");
    xTaskCreatePinnedToCore(lvgl_port_task, "LVGL", LVGL_TASK_STACK_SIZE, NULL, LVGL_TASK_PRIORITY, NULL,1);
    ESP_LOGI(TAG, "Display LVGL Scatter Chart");
    // Lock the mutex due to the LVGL APIs are not thread-safe
    if (lvgl_lock(-1)) {
        lvgl_demo_ui(disp);
        // Release the mutex
        lvgl_unlock();
    }
}

void loop() {
  // put your main code here, to run repeatedly:
}
