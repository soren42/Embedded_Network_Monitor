/*
 * Board support: Waveshare ESP32-S3-Touch-LCD-1.28
 *
 * Display: 1.28" IPS, 240x240, GC9A01A (SPI)
 * Touch:   CST816S (I2C)
 * Audio:   None on this board
 *
 * Pin assignments from Waveshare wiki:
 *   LCD_SDA  = GPIO11 (MOSI)
 *   LCD_SCL  = GPIO12 (SCLK)
 *   LCD_CS   = GPIO10
 *   LCD_DC   = GPIO8
 *   LCD_RST  = GPIO14
 *   LCD_BL   = GPIO2
 *   TP_SDA   = GPIO6
 *   TP_SCL   = GPIO7
 *   TP_INT   = GPIO5
 *   TP_RST   = GPIO13
 */

#include "sat_hal.h"

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_lcd_gc9a01.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"

static const char *TAG = "board_1_28";

const int SAT_DISP_HOR_RES = 240;
const int SAT_DISP_VER_RES = 240;

/* Pin definitions */
#define LCD_SPI_HOST   SPI2_HOST
#define LCD_PIN_MOSI   11
#define LCD_PIN_SCLK   12
#define LCD_PIN_CS     10
#define LCD_PIN_DC      8
#define LCD_PIN_RST    14
#define LCD_PIN_BL      2

#define TP_I2C_PORT    I2C_NUM_0
#define TP_PIN_SDA      6
#define TP_PIN_SCL      7
#define TP_PIN_INT      5
#define TP_PIN_RST     13

#define LCD_SPI_FREQ_HZ  (40 * 1000 * 1000)  /* 40 MHz */

static esp_lcd_panel_handle_t s_panel;
static lv_disp_draw_buf_t s_draw_buf;
static lv_disp_drv_t s_disp_drv;
static lv_color_t *s_buf1;
static lv_color_t *s_buf2;

static void lcd_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area,
                         lv_color_t *color_map)
{
    int x1 = area->x1, y1 = area->y1;
    int x2 = area->x2, y2 = area->y2;

    esp_lcd_panel_draw_bitmap(s_panel, x1, y1, x2 + 1, y2 + 1, color_map);
    lv_disp_flush_ready(drv);
}

static void backlight_init(void)
{
    ledc_timer_config_t timer_conf = {
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num       = LEDC_TIMER_0,
        .freq_hz         = 5000,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer_conf);

    ledc_channel_config_t ch_conf = {
        .gpio_num   = LCD_PIN_BL,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = LEDC_CHANNEL_0,
        .timer_sel  = LEDC_TIMER_0,
        .duty       = 200,  /* ~78% brightness */
        .hpoint     = 0,
    };
    ledc_channel_config(&ch_conf);
}

lv_disp_t *hal_display_init(void)
{
    ESP_LOGI(TAG, "Initializing GC9A01A 240x240 display");

    backlight_init();

    /* SPI bus */
    spi_bus_config_t bus_cfg = {
        .mosi_io_num     = LCD_PIN_MOSI,
        .miso_io_num     = -1,
        .sclk_io_num     = LCD_PIN_SCLK,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = 240 * 240 * sizeof(lv_color_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_SPI_HOST, &bus_cfg,
                                       SPI_DMA_CH_AUTO));

    /* Panel IO */
    esp_lcd_panel_io_handle_t io_handle;
    esp_lcd_panel_io_spi_config_t io_cfg = {
        .dc_gpio_num       = LCD_PIN_DC,
        .cs_gpio_num       = LCD_PIN_CS,
        .pclk_hz           = LCD_SPI_FREQ_HZ,
        .lcd_cmd_bits      = 8,
        .lcd_param_bits    = 8,
        .spi_mode          = 0,
        .trans_queue_depth = 10,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(LCD_SPI_HOST, &io_cfg,
                                              &io_handle));

    /* GC9A01 panel */
    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = LCD_PIN_RST,
        .color_space    = ESP_LCD_COLOR_SPACE_BGR,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(io_handle, &panel_cfg,
                                              &s_panel));

    esp_lcd_panel_reset(s_panel);
    esp_lcd_panel_init(s_panel);
    esp_lcd_panel_invert_color(s_panel, true);
    esp_lcd_panel_disp_on_off(s_panel, true);

    /* LVGL display driver */
    int buf_size = 240 * 40;  /* 40-line buffer */
    s_buf1 = heap_caps_malloc(buf_size * sizeof(lv_color_t),
                              MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    s_buf2 = heap_caps_malloc(buf_size * sizeof(lv_color_t),
                              MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);

    lv_disp_draw_buf_init(&s_draw_buf, s_buf1, s_buf2, buf_size);

    lv_disp_drv_init(&s_disp_drv);
    s_disp_drv.hor_res  = 240;
    s_disp_drv.ver_res  = 240;
    s_disp_drv.flush_cb = lcd_flush_cb;
    s_disp_drv.draw_buf = &s_draw_buf;

    return lv_disp_drv_register(&s_disp_drv);
}

lv_indev_t *hal_touch_init(void)
{
    /* CST816S I2C touch - basic init */
    i2c_config_t i2c_cfg = {
        .mode             = I2C_MODE_MASTER,
        .sda_io_num       = TP_PIN_SDA,
        .scl_io_num       = TP_PIN_SCL,
        .sda_pullup_en    = GPIO_PULLUP_ENABLE,
        .scl_pullup_en    = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,
    };
    i2c_param_config(TP_I2C_PORT, &i2c_cfg);
    i2c_driver_install(TP_I2C_PORT, I2C_MODE_MASTER, 0, 0, 0);

    /* Reset touch controller */
    gpio_set_direction(TP_PIN_RST, GPIO_MODE_OUTPUT);
    gpio_set_level(TP_PIN_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(TP_PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(50));

    ESP_LOGI(TAG, "CST816S touch initialized");

    /* Touch input device registration is board-specific;
     * for this read-only display, touch is optional.
     * A full implementation would register an lv_indev_drv_t
     * with a read callback that polls CST816S over I2C. */
    return NULL;
}

void hal_play_alert(int severity)
{
    /* No audio hardware on the 1.28" board */
    (void)severity;
}
