/**
 * @file ui_status.h
 * @brief Network Status Dashboard - at-a-glance network health overview
 */

#ifndef UI_STATUS_H
#define UI_STATUS_H

#include "lvgl.h"

/**
 * Create the status dashboard inside the given parent container.
 * Shows overall network health: router, DNS, WAN link quality,
 * WiFi status, and infrastructure summary.
 */
void ui_status_create(lv_obj_t *parent);

/**
 * Update status dashboard data (called from a periodic timer).
 */
void ui_status_update(void);

#endif /* UI_STATUS_H */
