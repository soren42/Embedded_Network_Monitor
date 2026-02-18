/**
 * @file ui_settings.h
 * @brief Settings / More screen - configuration display and navigation
 */

#ifndef UI_SETTINGS_H
#define UI_SETTINGS_H

#include "lvgl.h"

/**
 * Create the settings content inside the given parent container.
 * Shows current configuration values and navigation to sub-screens.
 */
void ui_settings_create(lv_obj_t *parent);

#endif /* UI_SETTINGS_H */
