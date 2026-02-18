/**
 * @file ui_settings.c
 * @brief Settings / More screen - configuration display and navigation
 */

#include "ui_settings.h"
#include "ui_theme.h"
#include "ui_widgets.h"
#include "ui_manager.h"
#include "app_conf.h"

#include <stdio.h>
#include <string.h>

/* ── Connections button callback ────────────────────────────────────── */

static void connections_btn_cb(lv_event_t *e)
{
    (void)e;
    ui_manager_show(SCREEN_CONNECTIONS);
}

/* ── Helper: create a display-only setting row ──────────────────────── */

static lv_obj_t *create_setting_row(lv_obj_t *parent,
                                     const char *label_text,
                                     const char *value_text)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_top(row, 8, 0);
    lv_obj_set_style_pad_bottom(row, 8, 0);
    lv_obj_set_style_border_color(row, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl_key = lv_label_create(row);
    lv_label_set_text(lbl_key, label_text);
    lv_obj_set_style_text_color(lbl_key, COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(lbl_key, ui_font_normal(), 0);

    lv_obj_t *lbl_val = lv_label_create(row);
    lv_label_set_text(lbl_val, value_text);
    lv_obj_set_style_text_color(lbl_val, COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_text_font(lbl_val, ui_font_normal(), 0);

    return row;
}

/* ── Public API ────────────────────────────────────────────────────── */

void ui_settings_create(lv_obj_t *parent)
{
    char buf[64];

    /* Back button */
    ui_create_back_btn(parent);

    /* Section header */
    ui_create_section_header(parent, "Settings");

    /* ── Network settings card ─────────────────────────────────────── */
    lv_obj_t *net_card = ui_create_card(parent, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(net_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(net_card, 12, 0);
    lv_obj_set_style_pad_row(net_card, 0, 0);

    lv_obj_t *net_title = lv_label_create(net_card);
    lv_label_set_text(net_title, "Network");
    lv_obj_set_style_text_font(net_title, ui_font_normal(), 0);
    lv_obj_set_style_text_color(net_title, COLOR_PRIMARY, 0);
    lv_obj_set_style_pad_bottom(net_title, 8, 0);

    create_setting_row(net_card, "Ping Target", APP_DEFAULT_PING_TARGET);
    create_setting_row(net_card, "DNS Server",  APP_DEFAULT_DNS_TARGET);

    /* ── Alert thresholds card ─────────────────────────────────────── */
    lv_obj_t *alert_card = ui_create_card(parent, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(alert_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(alert_card, 12, 0);
    lv_obj_set_style_pad_row(alert_card, 0, 0);

    lv_obj_t *alert_title = lv_label_create(alert_card);
    lv_label_set_text(alert_title, "Alert Thresholds");
    lv_obj_set_style_text_font(alert_title, ui_font_normal(), 0);
    lv_obj_set_style_text_color(alert_title, COLOR_PRIMARY, 0);
    lv_obj_set_style_pad_bottom(alert_title, 8, 0);

    snprintf(buf, sizeof(buf), "%d%%", APP_ALERT_BW_WARN_PCT);
    create_setting_row(alert_card, "BW Warning", buf);

    snprintf(buf, sizeof(buf), "%d ms", APP_ALERT_PING_WARN_MS);
    create_setting_row(alert_card, "Ping Warning", buf);

    snprintf(buf, sizeof(buf), "%d ms", APP_ALERT_PING_CRIT_MS);
    create_setting_row(alert_card, "Ping Critical", buf);

    /* ── Navigation card ───────────────────────────────────────────── */
    lv_obj_t *nav_card = ui_create_card(parent, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(nav_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(nav_card, 12, 0);
    lv_obj_set_style_pad_row(nav_card, 8, 0);

    lv_obj_t *nav_title = lv_label_create(nav_card);
    lv_label_set_text(nav_title, "Views");
    lv_obj_set_style_text_font(nav_title, ui_font_normal(), 0);
    lv_obj_set_style_text_color(nav_title, COLOR_PRIMARY, 0);

    lv_obj_t *conn_btn = lv_btn_create(nav_card);
    lv_obj_set_size(conn_btn, LV_PCT(100), 44);
    lv_obj_set_style_bg_color(conn_btn, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_bg_opa(conn_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(conn_btn, 8, 0);
    lv_obj_set_style_shadow_width(conn_btn, 0, 0);

    lv_obj_t *conn_lbl = lv_label_create(conn_btn);
    lv_label_set_text(conn_lbl, "Connections");
    lv_obj_set_style_text_font(conn_lbl, ui_font_normal(), 0);
    lv_obj_set_style_text_color(conn_lbl, COLOR_TEXT_PRIMARY, 0);
    lv_obj_center(conn_lbl);

    lv_obj_add_event_cb(conn_btn, connections_btn_cb, LV_EVENT_CLICKED, NULL);

    /* ── About card ────────────────────────────────────────────────── */
    lv_obj_t *about_card = ui_create_card(parent, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(about_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(about_card, 12, 0);
    lv_obj_set_style_pad_row(about_card, 4, 0);

    lv_obj_t *about_title = lv_label_create(about_card);
    lv_label_set_text(about_title, "About");
    lv_obj_set_style_text_font(about_title, ui_font_normal(), 0);
    lv_obj_set_style_text_color(about_title, COLOR_PRIMARY, 0);

    lv_obj_t *lbl_app = lv_label_create(about_card);
    lv_label_set_text(lbl_app, "LP Network Monitor");
    lv_obj_set_style_text_font(lbl_app, ui_font_normal(), 0);
    lv_obj_set_style_text_color(lbl_app, COLOR_TEXT_PRIMARY, 0);

    lv_obj_t *lbl_ver = lv_label_create(about_card);
    lv_label_set_text(lbl_ver, "Version 1.0.0");
    lv_obj_set_style_text_font(lbl_ver, ui_font_small(), 0);
    lv_obj_set_style_text_color(lbl_ver, COLOR_TEXT_SECONDARY, 0);

    lv_obj_t *lbl_target = lv_label_create(about_card);
    lv_label_set_text(lbl_target, "720x720 Touch Display  |  Raspberry Pi");
    lv_obj_set_style_text_font(lbl_target, ui_font_small(), 0);
    lv_obj_set_style_text_color(lbl_target, COLOR_TEXT_SECONDARY, 0);
}
