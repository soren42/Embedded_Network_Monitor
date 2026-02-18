/**
 * @file ui_alerts.h
 * @brief Alerts screen - alert log with severity and timestamps
 */

#ifndef UI_ALERTS_H
#define UI_ALERTS_H

#include "lvgl.h"

/**
 * Create the alerts content inside the given parent container.
 * Shows a scrollable list of alert entries with severity indicators.
 */
void ui_alerts_create(lv_obj_t *parent);

/**
 * Update alert list data (called from a periodic timer).
 */
void ui_alerts_update(void);

#endif /* UI_ALERTS_H */
