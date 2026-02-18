/**
 * @file ui_traffic.h
 * @brief Traffic chart screen - 24-hour history with interface selector
 */

#ifndef UI_TRAFFIC_H
#define UI_TRAFFIC_H

#include "lvgl.h"

/**
 * Create the traffic chart content inside the given parent container.
 * Shows a full-width 24h chart with interface selector and daily totals.
 */
void ui_traffic_create(lv_obj_t *parent);

/**
 * Update traffic chart data (called from a periodic timer).
 */
void ui_traffic_update(void);

#endif /* UI_TRAFFIC_H */
