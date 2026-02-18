/**
 * @file ui_interface.h
 * @brief Interface detail screen - per-interface stats, chart, and WiFi info
 */

#ifndef UI_INTERFACE_H
#define UI_INTERFACE_H

#include "lvgl.h"

/**
 * Create the interface detail content inside the given parent container.
 * Builds all widgets; actual data is populated by ui_interface_update().
 */
void ui_interface_create(lv_obj_t *parent);

/**
 * Set which interface the detail screen should display.
 * @param iface_idx  Index into the interface array (0..APP_MAX_INTERFACES-1).
 */
void ui_interface_set_iface(int iface_idx);

/**
 * Update interface detail data (called from a periodic timer).
 */
void ui_interface_update(void);

#endif /* UI_INTERFACE_H */
