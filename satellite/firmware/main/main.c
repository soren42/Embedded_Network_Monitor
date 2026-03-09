/*
 * Satellite Display Firmware - Main Entry Point
 * Embedded Network Monitor
 *
 * ESP32-S3 based round display satellite that receives network
 * monitoring data over USB serial and displays it on a cockpit-style
 * circular gauge interface.
 */

#include "sat_hal.h"
#include "sat_receiver.h"
#include "sat_ui.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"

static const char *TAG = "sat_main";

#define LVGL_TICK_PERIOD_MS 5

static void lvgl_tick_cb(void *arg)
{
    (void)arg;
    lv_tick_inc(LVGL_TICK_PERIOD_MS);
}

void app_main(void)
{
    ESP_LOGI(TAG, "Satellite Display v1.0 starting");

    /* Initialize LVGL */
    lv_init();

    /* Initialize board-specific display and touch */
    hal_display_init();
    hal_touch_init();

    /* Set up LVGL tick timer */
    const esp_timer_create_args_t tick_args = {
        .callback = lvgl_tick_cb,
        .name     = "lvgl_tick",
    };
    esp_timer_handle_t tick_timer;
    ESP_ERROR_CHECK(esp_timer_create(&tick_args, &tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(tick_timer,
                                              LVGL_TICK_PERIOD_MS * 1000));

    /* Initialize the cockpit UI */
    int disp_size = SAT_DISP_HOR_RES;
    sat_ui_init(disp_size);

    ESP_LOGI(TAG, "Display: %dx%d round", SAT_DISP_HOR_RES, SAT_DISP_VER_RES);

    /* Start USB serial receiver */
    sat_receiver_init();

    ESP_LOGI(TAG, "Ready - waiting for data on USB CDC");

    /* Main LVGL loop */
    while (1) {
        uint32_t delay = lv_timer_handler();
        if (delay < 1) delay = 1;
        if (delay > 50) delay = 50;
        vTaskDelay(pdMS_TO_TICKS(delay));
    }
}
