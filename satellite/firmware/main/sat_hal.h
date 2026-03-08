/*
 * Hardware Abstraction Layer for satellite display boards
 *
 * Each board implementation provides these functions.
 * Select the active board by changing the SRCS in main/CMakeLists.txt.
 */

#ifndef SAT_HAL_H
#define SAT_HAL_H

#include "lvgl.h"

/* Display resolution - set by board implementation */
extern const int SAT_DISP_HOR_RES;
extern const int SAT_DISP_VER_RES;

/* Initialize display hardware and register LVGL display driver.
 * Returns the LVGL display pointer. */
lv_disp_t *hal_display_init(void);

/* Initialize touch hardware and register LVGL input driver.
 * Returns the LVGL input device pointer, or NULL if no touch. */
lv_indev_t *hal_touch_init(void);

/* Play an alert tone. severity: 0=none, 1=info, 2=warn, 3=critical */
void hal_play_alert(int severity);

#endif /* SAT_HAL_H */
