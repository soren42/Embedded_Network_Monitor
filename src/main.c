/**
 * @file main.c
 * @brief Embedded Network Monitor - Entry point
 *
 * Initializes LVGL, display, touchscreen, network collectors,
 * and UI, then runs the main event loop.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>

#include "lvgl.h"
#include "app_conf.h"
#include "hal_display.h"
#include "hal_touch.h"
#include "log.h"
#include "config.h"
#include "ui_manager.h"
#include "ui_theme.h"
#include "ui_layout.h"
#include "net_collector.h"
#include "alerts.h"
#include "data_store.h"

static volatile int g_running = 1;
static config_t g_config;

static void signal_handler(int sig)
{
    (void)sig;
    g_running = 0;
}

static uint64_t get_time_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return ((uint64_t)tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}

static void setup_signals(void)
{
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

int main(int argc, char *argv[])
{
    const char *config_path = APP_CONFIG_PATH;
    if (argc > 1) {
        config_path = argv[1];
    }

    /* Setup signal handlers for clean shutdown */
    setup_signals();

    /* Initialize logging */
    log_init(LOG_LEVEL_INFO);
    LOG_INFO("Embedded Network Monitor v3.0.0 starting...");

    /* Load configuration */
    if (config_load(&g_config, config_path) < 0) {
        LOG_WARN("Config file not found at %s, using defaults", config_path);
    }

    /* Initialize LVGL */
    lv_init();

    /* Initialize display */
    if (hal_display_init() < 0) {
        LOG_ERROR("Failed to initialize display");
        return 1;
    }
    LOG_INFO("Display initialized: %dx%d", APP_DISP_HOR_RES, APP_DISP_VER_RES);

    /* Initialize touchscreen */
    if (hal_touch_init() < 0) {
        LOG_ERROR("Failed to initialize touchscreen");
        return 1;
    }
    LOG_INFO("Touchscreen initialized");

    /* Initialize layout geometry and UI theme */
    ui_layout_init();
    ui_theme_init();

    /* Initialize network data collector */
    net_collector_init(&g_config);

    /* Initialize alert system */
    alerts_init(&g_config);

    /* Load historical data */
    data_store_load();

    /* Create UI screens and show dashboard */
    ui_manager_init();
    ui_manager_show(SCREEN_STATUS);

    LOG_INFO("Initialization complete, entering main loop");

    /* Main event loop */
    uint64_t prev_ms = get_time_ms();
    while (g_running) {
        uint64_t now_ms = get_time_ms();
        uint32_t elapsed = (uint32_t)(now_ms - prev_ms);
        prev_ms = now_ms;
        lv_tick_inc(elapsed);
        lv_timer_handler();
        usleep(APP_TICK_PERIOD_MS * 1000);
    }

    /* Clean shutdown */
    LOG_INFO("Shutting down...");
    data_store_save();
    net_collector_shutdown();
    LOG_INFO("Goodbye");

    return 0;
}
