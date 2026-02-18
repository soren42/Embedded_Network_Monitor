#ifndef UI_WIDGETS_H
#define UI_WIDGETS_H

#include "lvgl.h"
#include "ui_theme.h"
#include "app_conf.h"

/* ── Card panel ─────────────────────────────────────────────────────── */

/**
 * Create a styled card (rounded, bordered panel).
 * @param parent  Parent LVGL object.
 * @param w       Width in pixels.
 * @param h       Height in pixels.
 * @return        The card object.
 */
lv_obj_t *ui_create_card(lv_obj_t *parent, lv_coord_t w, lv_coord_t h);

/* ── Stat label (title + value) ─────────────────────────────────────── */

/**
 * Create a two-line label: small title on top, larger value below.
 * The returned object is the outer container.
 * Use ui_update_stat_label() to change the value text.
 *
 * @param parent  Parent LVGL object.
 * @param title   Short descriptive title string (e.g. "Ping").
 * @return        The container holding both labels.
 */
lv_obj_t *ui_create_stat_label(lv_obj_t *parent, const char *title);

/**
 * Update the value portion of a stat-label widget.
 * @param stat_obj  Object previously returned by ui_create_stat_label().
 * @param value     New value string (e.g. "12 ms").
 */
void ui_update_stat_label(lv_obj_t *stat_obj, const char *value);

/* ── Status LED indicator ───────────────────────────────────────────── */

/**
 * Create a circular LED indicator.
 * @param parent  Parent LVGL object.
 * @param size    Diameter in pixels.
 * @return        The LED object.
 */
lv_obj_t *ui_create_status_led(lv_obj_t *parent, lv_coord_t size);

/**
 * Set LED colour by status code.
 * @param led     LED previously returned by ui_create_status_led().
 * @param status  0 = red (down/error), 1 = green (up/ok), 2 = yellow (warn).
 */
void ui_set_status_led(lv_obj_t *led, int status);

/* ── Section header ─────────────────────────────────────────────────── */

/**
 * Create a bold section header label.
 * @param parent  Parent LVGL object.
 * @param text    Header text.
 * @return        The label object.
 */
lv_obj_t *ui_create_section_header(lv_obj_t *parent, const char *text);

/* ── Back button ────────────────────────────────────────────────────── */

/**
 * Create a back-arrow button in the top-left of @p parent.
 * Pressing it calls ui_manager_back().
 * @param parent  Parent LVGL object.
 * @return        The button object.
 */
lv_obj_t *ui_create_back_btn(lv_obj_t *parent);

#endif /* UI_WIDGETS_H */
