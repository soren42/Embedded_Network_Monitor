/**
 * @file hal_touch.c
 * @brief Evdev touchscreen initialization for Luckfox Pico 86 Panel
 */

#include "hal_touch.h"
#include "app_conf.h"
#include "indev/evdev.h"

static lv_indev_drv_t indev_drv;

int hal_touch_init(void)
{
    evdev_init();

    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = evdev_read;

    if (lv_indev_drv_register(&indev_drv) == NULL) {
        return -1;
    }

    return 0;
}
