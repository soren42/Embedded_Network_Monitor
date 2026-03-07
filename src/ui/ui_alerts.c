/**
 * @file ui_alerts.c
 * @brief Alerts screen - scrollable alert log with severity indicators
 */

#include "ui_alerts.h"
#include "ui_theme.h"
#include "ui_widgets.h"
#include "ui_layout.h"
#include "ui_manager.h"
#include "alerts.h"
#include "utils.h"
#include "app_conf.h"

#include <stdio.h>
#include <string.h>

/* ── Module state ───────────────────────────────────────────────────── */

static lv_obj_t *g_list;
static lv_obj_t *g_lbl_empty;
static lv_timer_t *g_update_timer;
static int g_last_count;

/* ── Severity helpers ───────────────────────────────────────────────── */

static lv_color_t severity_color(alert_severity_t sev)
{
    switch (sev) {
        case ALERT_SEV_CRITICAL: return COLOR_DANGER;
        case ALERT_SEV_WARNING:  return COLOR_WARNING;
        case ALERT_SEV_INFO:     return COLOR_PRIMARY;
        default:                 return COLOR_TEXT_SECONDARY;
    }
}

static const char *severity_icon(alert_severity_t sev)
{
    switch (sev) {
        case ALERT_SEV_CRITICAL: return LV_SYMBOL_WARNING;
        case ALERT_SEV_WARNING:  return LV_SYMBOL_BELL;
        case ALERT_SEV_INFO:     return LV_SYMBOL_OK;
        default:                 return LV_SYMBOL_DUMMY;
    }
}

/* ── Clear All button callback ──────────────────────────────────────── */

static void clear_all_cb(lv_event_t *e)
{
    (void)e;
    alerts_clear_all();
    ui_alerts_update();
}

/* ── Update timer callback ─────────────────────────────────────────── */

static void update_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if (ui_manager_current() != SCREEN_ALERTS) return;
    ui_alerts_update();
}

/* ── Create a single alert entry row ────────────────────────────────── */

static void create_alert_entry(lv_obj_t *parent, const alert_entry_t *entry)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 8, 0);
    lv_obj_set_style_pad_top(row, 6, 0);
    lv_obj_set_style_pad_bottom(row, 6, 0);
    lv_obj_set_style_border_color(row, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_color_t col = severity_color(entry->severity);

    /* Severity icon */
    lv_obj_t *lbl_icon = lv_label_create(row);
    lv_label_set_text(lbl_icon, severity_icon(entry->severity));
    lv_obj_set_style_text_color(lbl_icon, col, 0);
    lv_obj_set_style_text_font(lbl_icon, ui_font_normal(), 0);
    lv_obj_set_style_min_width(lbl_icon, 28, 0);

    /* Timestamp */
    char ts_buf[24];
    format_timestamp(entry->timestamp, ts_buf, sizeof(ts_buf));
    lv_obj_t *lbl_ts = lv_label_create(row);
    lv_label_set_text(lbl_ts, ts_buf);
    lv_obj_set_style_text_color(lbl_ts, COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(lbl_ts, ui_font_small(), 0);
    lv_obj_set_style_min_width(lbl_ts, 100, 0);

    /* Interface name */
    lv_obj_t *lbl_iface = lv_label_create(row);
    lv_label_set_text(lbl_iface, entry->iface);
    lv_obj_set_style_text_color(lbl_iface, COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(lbl_iface, ui_font_small(), 0);
    lv_obj_set_style_min_width(lbl_iface, 70, 0);

    /* Message */
    lv_obj_t *lbl_msg = lv_label_create(row);
    lv_label_set_text(lbl_msg, entry->message);
    lv_obj_set_style_text_color(lbl_msg, COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_text_font(lbl_msg, ui_font_small(), 0);
    lv_obj_set_flex_grow(lbl_msg, 1);
    lv_label_set_long_mode(lbl_msg, LV_LABEL_LONG_DOT);
}

/* ── Public API ────────────────────────────────────────────────────── */

void ui_alerts_create(lv_obj_t *parent)
{
    /* Section header */
    ui_create_section_header(parent, "Alert Log");

    /* Scrollable list container */
    g_list = lv_obj_create(parent);
    lv_obj_remove_style_all(g_list);
    lv_obj_set_size(g_list, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_max_height(g_list, ui_content_h() - ui_scale(120), 0);
    lv_obj_set_style_bg_color(g_list, COLOR_CARD, 0);
    lv_obj_set_style_bg_opa(g_list, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(g_list, 8, 0);
    lv_obj_set_style_border_color(g_list, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(g_list, 1, 0);
    lv_obj_set_style_pad_all(g_list, ui_pad_normal(), 0);
    lv_obj_set_flex_flow(g_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_flag(g_list, LV_OBJ_FLAG_SCROLLABLE);

    /* "No alerts" placeholder */
    g_lbl_empty = lv_label_create(g_list);
    lv_label_set_text(g_lbl_empty, "No alerts recorded.");
    lv_obj_set_style_text_color(g_lbl_empty, COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(g_lbl_empty, ui_font_normal(), 0);

    /* Clear All button */
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, ui_scale(140), ui_scale(44));
    lv_obj_set_style_bg_color(btn, COLOR_DANGER, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);

    lv_obj_t *btn_lbl = lv_label_create(btn);
    lv_label_set_text(btn_lbl, "Clear All");
    lv_obj_set_style_text_font(btn_lbl, ui_font_normal(), 0);
    lv_obj_set_style_text_color(btn_lbl, COLOR_TEXT_PRIMARY, 0);
    lv_obj_center(btn_lbl);

    lv_obj_add_event_cb(btn, clear_all_cb, LV_EVENT_CLICKED, NULL);

    /* Register periodic update timer */
    g_update_timer = lv_timer_create(update_timer_cb, 1000, NULL);
    g_last_count = -1; /* force first rebuild */

    /* Initial update */
    ui_alerts_update();
}

void ui_alerts_update(void)
{
    const alert_entry_t *entries;
    int count;
    alerts_get_log(&entries, &count);

    /* Only rebuild if count changed to avoid unnecessary redraws */
    if (count == g_last_count) return;
    g_last_count = count;

    /* Remove all children from the list and rebuild */
    lv_obj_clean(g_list);

    if (count == 0) {
        /* Re-create placeholder */
        g_lbl_empty = lv_label_create(g_list);
        lv_label_set_text(g_lbl_empty, "No alerts recorded.");
        lv_obj_set_style_text_color(g_lbl_empty, COLOR_TEXT_SECONDARY, 0);
        lv_obj_set_style_text_font(g_lbl_empty, ui_font_normal(), 0);
        return;
    }

    /* Show entries in reverse chronological order (newest first) */
    for (int i = count - 1; i >= 0; i--) {
        create_alert_entry(g_list, &entries[i]);
    }
}
