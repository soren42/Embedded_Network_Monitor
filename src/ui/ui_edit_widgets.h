/**
 * @file ui_edit_widgets.h
 * @brief Reusable editing controls (IP editor, stepper) for settings screen
 */

#ifndef UI_EDIT_WIDGETS_H
#define UI_EDIT_WIDGETS_H

#include "lvgl.h"
#include <stdint.h>

/**
 * Create an IP address editor using 4 rollers with dot separators.
 * @param parent     Parent LVGL object
 * @param initial_ip IP address as dotted-decimal string (e.g. "10.0.0.1")
 * @return           Container object holding the 4 rollers
 */
lv_obj_t *ui_create_ip_editor(lv_obj_t *parent, const char *initial_ip);

/**
 * Get the IP address string from an IP editor widget.
 * @param editor  Object returned by ui_create_ip_editor()
 * @param buf     Buffer to receive dotted-decimal string (at least 16 bytes)
 * @param buf_len Size of buffer
 */
void ui_ip_editor_get_value(lv_obj_t *editor, char *buf, int buf_len);

/**
 * Create a numeric stepper widget: [ - ] value [ + ]
 * @param parent   Parent LVGL object
 * @param min      Minimum value
 * @param max      Maximum value
 * @param step     Increment/decrement step
 * @param initial  Starting value
 * @param fmt      Printf format for display (e.g. "%d%%" or "%d ms")
 * @return         Container object
 */
lv_obj_t *ui_create_stepper(lv_obj_t *parent, int min, int max,
                             int step, int initial, const char *fmt);

/**
 * Get the current value of a stepper widget.
 * @param stepper  Object returned by ui_create_stepper()
 * @return         Current value
 */
int ui_stepper_get_value(lv_obj_t *stepper);

#endif /* UI_EDIT_WIDGETS_H */
