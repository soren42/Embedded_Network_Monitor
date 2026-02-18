/**
 * @file hal_display.h
 * @brief Framebuffer display initialization and management
 */

#ifndef HAL_DISPLAY_H
#define HAL_DISPLAY_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize the framebuffer display driver and register with LVGL.
 * @return 0 on success, -1 on failure
 */
int hal_display_init(void);

#ifdef __cplusplus
}
#endif

#endif /* HAL_DISPLAY_H */
