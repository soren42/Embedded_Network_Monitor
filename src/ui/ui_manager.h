#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include "lvgl.h"

/* ── Screen identifiers ─────────────────────────────────────────────── */
typedef enum {
    SCREEN_DASHBOARD = 0,
    SCREEN_TRAFFIC,
    SCREEN_ALERTS,
    SCREEN_MORE,
    SCREEN_IFACE_DETAIL,
    SCREEN_CONNECTIONS,
    SCREEN_SETTINGS,
    SCREEN_COUNT          /* sentinel -- keep last */
} screen_id_t;

/* ── Public API ─────────────────────────────────────────────────────── */

/**
 * Create the root screen layout: status bar (top 50 px), content area
 * (middle), and navigation bar (bottom 60 px).  Call once after
 * ui_theme_init().
 */
void ui_manager_init(void);

/**
 * Switch the visible content area to the given screen.
 * Screens are lazily created on first access.
 */
void ui_manager_show(screen_id_t screen);

/**
 * Navigate to the interface-detail screen for a specific interface.
 * @param iface_idx  Index into the interface array (0..APP_MAX_INTERFACES-1).
 */
void ui_manager_show_iface_detail(int iface_idx);

/**
 * Return to the previously shown screen (simple one-level back stack).
 */
void ui_manager_back(void);

/**
 * Return the screen currently being displayed.
 */
screen_id_t ui_manager_current(void);

/**
 * Update the alert badge count shown in the status bar.
 * Pass 0 to hide the badge.
 */
void ui_manager_set_alert_count(int count);

/**
 * Return the content container so that screen builders can parent their
 * objects to it.  The container is sized to APP_CONTENT_H x 720.
 */
lv_obj_t *ui_manager_get_content(void);

#endif /* UI_MANAGER_H */
