/**
 * @file hal_touch.h
 * @brief Touchscreen input initialization and management
 */

#ifndef HAL_TOUCH_H
#define HAL_TOUCH_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize the evdev touchscreen driver and register with LVGL.
 * @return 0 on success, -1 on failure
 */
int hal_touch_init(void);

#ifdef __cplusplus
}
#endif

#endif /* HAL_TOUCH_H */
