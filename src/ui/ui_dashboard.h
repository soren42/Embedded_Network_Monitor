/**
 * @file ui_dashboard.h
 * @brief Dashboard screen - main overview
 */

#ifndef UI_DASHBOARD_H
#define UI_DASHBOARD_H

#include "lvgl.h"

/**
 * Create the dashboard content inside the given parent container.
 * Shows interface status cards with sparkline charts and
 * connectivity indicators.
 */
void ui_dashboard_create(lv_obj_t *parent);

/**
 * Update dashboard data (called from a periodic timer).
 */
void ui_dashboard_update(void);

#endif /* UI_DASHBOARD_H */
