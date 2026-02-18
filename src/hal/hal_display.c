/**
 * @file hal_display.c
 * @brief Framebuffer display initialization for Luckfox Pico 86 Panel
 */

#include "hal_display.h"
#include "app_conf.h"
#include "display/fbdev.h"

/* Double-buffer for partial rendering: 40 lines at a time */
#define DISP_BUF_LINES 40

static lv_disp_draw_buf_t disp_buf;
static lv_color_t buf1[APP_DISP_HOR_RES * DISP_BUF_LINES];
static lv_color_t buf2[APP_DISP_HOR_RES * DISP_BUF_LINES];
static lv_disp_drv_t disp_drv;

int hal_display_init(void)
{
    fbdev_init();

    lv_disp_draw_buf_init(&disp_buf, buf1, buf2,
                          APP_DISP_HOR_RES * DISP_BUF_LINES);

    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = APP_DISP_HOR_RES;
    disp_drv.ver_res = APP_DISP_VER_RES;
    disp_drv.flush_cb = fbdev_flush;
    disp_drv.draw_buf = &disp_buf;

    if (lv_disp_drv_register(&disp_drv) == NULL) {
        return -1;
    }

    return 0;
}
