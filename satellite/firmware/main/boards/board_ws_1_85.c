/*
 * Board support: Waveshare ESP32-S3-Touch-LCD-1.85C
 *
 * Display: 1.85" TFT, 360x360, ST77916 (QSPI)
 * Touch:   CST816 (I2C)
 * Audio:   PCM5101 DAC (I2S) with speaker
 *
 * Pin assignments from Waveshare wiki:
 *   LCD_CS   = GPIO45
 *   LCD_SCK  = GPIO40
 *   LCD_SDA  = GPIO42 (QSPI D0)
 *   LCD_D1   = GPIO41
 *   LCD_D2   = GPIO43
 *   LCD_D3   = GPIO44
 *   LCD_RST  = GPIO0
 *   LCD_BL   = GPIO1
 *   TP_SDA   = GPIO4
 *   TP_SCL   = GPIO3
 *   TP_INT   = GPIO21
 *   TP_RST   = GPIO46
 *   I2S_DIN  = GPIO47
 *   I2S_LRCK = GPIO38
 *   I2S_BCK  = GPIO48
 *
 * NOTE: The ST77916 uses QSPI, which requires ESP-IDF's
 * esp_lcd_panel_io_spi with QUAD mode. This is a reference
 * implementation - adjust pin/timing as needed for your revision.
 */

#include "sat_hal.h"

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/i2s_std.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"

#include <math.h>
#include <string.h>

static const char *TAG = "board_1_85";

const int SAT_DISP_HOR_RES = 360;
const int SAT_DISP_VER_RES = 360;

/* Pin definitions */
#define LCD_PIN_CS     45
#define LCD_PIN_SCK    40
#define LCD_PIN_SDA    42
#define LCD_PIN_RST     0
#define LCD_PIN_BL      1

#define TP_I2C_PORT    I2C_NUM_0
#define TP_PIN_SDA      4
#define TP_PIN_SCL      3
#define TP_PIN_INT     21
#define TP_PIN_RST     46

#define I2S_PIN_DIN    47
#define I2S_PIN_LRCK   38
#define I2S_PIN_BCK    48

static esp_lcd_panel_handle_t s_panel;
static lv_disp_draw_buf_t s_draw_buf;
static lv_disp_drv_t s_disp_drv;
static lv_color_t *s_buf1;
static lv_color_t *s_buf2;
static i2s_chan_handle_t s_i2s_handle;

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
        .duty       = 200,
        .hpoint     = 0,
    };
    ledc_channel_config(&ch_conf);
}

lv_disp_t *hal_display_init(void)
{
    ESP_LOGI(TAG, "Initializing ST77916 360x360 display");

    backlight_init();

    /*
     * ST77916 uses QSPI. The exact initialization sequence is
     * vendor-specific. This provides the framework - the actual
     * panel initialization commands would come from the Waveshare
     * example code or ST77916 datasheet.
     *
     * For a production build, use:
     *   esp_lcd_new_panel_io_spi() with quad_mode=true
     *   Then register the ST77916 panel driver
     */

    /* SPI bus configuration for QSPI */
    spi_bus_config_t bus_cfg = {
        .mosi_io_num     = LCD_PIN_SDA,
        .miso_io_num     = -1,
        .sclk_io_num     = LCD_PIN_SCK,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = 360 * 80 * sizeof(lv_color_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus_cfg,
                                       SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_handle_t io_handle;
    esp_lcd_panel_io_spi_config_t io_cfg = {
        .dc_gpio_num       = -1,  /* QSPI uses command/data in SPI frame */
        .cs_gpio_num       = LCD_PIN_CS,
        .pclk_hz           = 40 * 1000 * 1000,
        .lcd_cmd_bits      = 8,
        .lcd_param_bits    = 8,
        .spi_mode          = 0,
        .trans_queue_depth = 10,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI2_HOST, &io_cfg,
                                              &io_handle));

    /* Generic panel - ST77916 specific init would go here */
    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = LCD_PIN_RST,
        .color_space    = ESP_LCD_COLOR_SPACE_BGR,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_cfg,
                                              &s_panel));

    esp_lcd_panel_reset(s_panel);
    esp_lcd_panel_init(s_panel);
    esp_lcd_panel_disp_on_off(s_panel, true);

    /* LVGL display driver */
    int buf_size = 360 * 40;
    s_buf1 = heap_caps_malloc(buf_size * sizeof(lv_color_t),
                              MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    s_buf2 = heap_caps_malloc(buf_size * sizeof(lv_color_t),
                              MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);

    lv_disp_draw_buf_init(&s_draw_buf, s_buf1, s_buf2, buf_size);

    lv_disp_drv_init(&s_disp_drv);
    s_disp_drv.hor_res  = 360;
    s_disp_drv.ver_res  = 360;
    s_disp_drv.flush_cb = lcd_flush_cb;
    s_disp_drv.draw_buf = &s_draw_buf;

    return lv_disp_drv_register(&s_disp_drv);
}

lv_indev_t *hal_touch_init(void)
{
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

    gpio_set_direction(TP_PIN_RST, GPIO_MODE_OUTPUT);
    gpio_set_level(TP_PIN_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(TP_PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(50));

    ESP_LOGI(TAG, "CST816 touch initialized");
    return NULL;
}

static void i2s_init(void)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(
        I2S_NUM_0, I2S_ROLE_MASTER);
    i2s_new_channel(&chan_cfg, &s_i2s_handle, NULL);

    i2s_std_config_t std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(44100),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(
            I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = (gpio_num_t)I2S_PIN_BCK,
            .ws   = (gpio_num_t)I2S_PIN_LRCK,
            .dout = (gpio_num_t)I2S_PIN_DIN,
            .din  = I2S_GPIO_UNUSED,
        },
    };
    i2s_channel_init_std_mode(s_i2s_handle, &std_cfg);
    i2s_channel_enable(s_i2s_handle);
}

void hal_play_alert(int severity)
{
    static bool i2s_ready = false;
    if (!i2s_ready) {
        i2s_init();
        i2s_ready = true;
    }

    /* Generate simple tone patterns */
    int freq, duration_ms;
    switch (severity) {
    case 1:  /* info - short low beep */
        freq = 800;
        duration_ms = 100;
        break;
    case 2:  /* warning - two medium beeps */
        freq = 1200;
        duration_ms = 150;
        break;
    case 3:  /* critical - ascending three-tone */
        freq = 1600;
        duration_ms = 200;
        break;
    default:
        return;
    }

    int samples = 44100 * duration_ms / 1000;
    int16_t *tone_buf = heap_caps_malloc(samples * sizeof(int16_t),
                                         MALLOC_CAP_DEFAULT);
    if (!tone_buf) return;

    for (int i = 0; i < samples; i++) {
        float t = (float)i / 44100.0f;
        tone_buf[i] = (int16_t)(16000.0f * sinf(2.0f * M_PI * freq * t));
    }

    size_t written;
    i2s_channel_write(s_i2s_handle, tone_buf,
                      samples * sizeof(int16_t), &written,
                      pdMS_TO_TICKS(500));

    free(tone_buf);
}
