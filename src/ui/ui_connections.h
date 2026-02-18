/**
 * @file ui_connections.h
 * @brief Connections screen - active TCP/UDP connection table
 */

#ifndef UI_CONNECTIONS_H
#define UI_CONNECTIONS_H

#include "lvgl.h"

/**
 * Create the connections content inside the given parent container.
 * Shows a table of active TCP/UDP connections from /proc/net.
 */
void ui_connections_create(lv_obj_t *parent);

/**
 * Update connections table data (called from a periodic timer).
 */
void ui_connections_update(void);

#endif /* UI_CONNECTIONS_H */
