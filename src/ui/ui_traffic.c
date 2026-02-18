/**
 * @file ui_traffic.c
 * @brief Traffic chart screen - 24-hour history with interface selector
 */

#include "ui_traffic.h"
#include "ui_theme.h"
#include "ui_widgets.h"
#include "ui_manager.h"
#include "net_collector.h"
#include "utils.h"
#include "app_conf.h"

#include <stdio.h>
#include <string.h>

/* ── Static widget references ──────────────────────────────────────── */

static lv_obj_t    *g_parent;
static lv_timer_t  *g_update_timer;

/* Interface selector */
static lv_obj_t *g_dropdown;
static int       g_selected_iface;

/* Chart */
static lv_obj_t          *g_chart;
static lv_chart_series_t *g_ser_rx;
static lv_chart_series_t *g_ser_tx;

/* Daily totals */
static lv_obj_t *g_stat_daily_rx;
static lv_obj_t *g_stat_daily_tx;
static lv_obj_t *g_stat_daily_total;

/* ── Dropdown callback ─────────────────────────────────────────────── */

static void dropdown_cb(lv_event_t *e)
{
    lv_obj_t *dd = lv_event_get_target(e);
    g_selected_iface = lv_dropdown_get_selected(dd);
    ui_traffic_update();
}

/* ── Rebuild dropdown options from current interface list ──────────── */

static void rebuild_dropdown(void)
{
    const net_state_t *state = net_get_state();
    char options[APP_MAX_INTERFACES * APP_IFACE_NAME_MAX];
    int  pos = 0;

    options[0] = '\0';

    for (int i = 0; i < state->num_ifaces && i < APP_MAX_INTERFACES; i++) {
        if (i > 0 && pos < (int)sizeof(options) - 1) {
            options[pos++] = '\n';
            options[pos]   = '\0';
        }
        int remaining = (int)sizeof(options) - pos;
        int written   = snprintf(options + pos, remaining, "%s",
                                 state->ifaces[i].info.name);
        if (written > 0 && written < remaining) {
            pos += written;
        }
    }

    lv_dropdown_set_options(g_dropdown, options);

    if (g_selected_iface >= state->num_ifaces) {
        g_selected_iface = 0;
    }
    lv_dropdown_set_selected(g_dropdown, g_selected_iface);
}

/* ── Timer callback ────────────────────────────────────────────────── */

static void update_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if (ui_manager_current() != SCREEN_TRAFFIC) return;
    ui_traffic_update();
}

/* ── Public API ────────────────────────────────────────────────────── */

void ui_traffic_create(lv_obj_t *parent)
{
    g_parent         = parent;
    g_selected_iface = 0;

    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(parent, 8, 0);
    lv_obj_add_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    /* ── Title row with interface selector ─────────────────────────── */
    lv_obj_t *hdr_row = lv_obj_create(parent);
    lv_obj_remove_style_all(hdr_row);
    lv_obj_set_size(hdr_row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(hdr_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(hdr_row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(hdr_row, 12, 0);
    lv_obj_clear_flag(hdr_row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(hdr_row);
    lv_label_set_text(title, "Traffic (24h)");
    lv_obj_set_style_text_font(title, ui_font_title(), 0);
    lv_obj_set_style_text_color(title, COLOR_TEXT_PRIMARY, 0);

    /* Dropdown for interface selection */
    g_dropdown = lv_dropdown_create(hdr_row);
    lv_obj_set_width(g_dropdown, 160);
    lv_obj_set_style_bg_color(g_dropdown, COLOR_CARD, 0);
    lv_obj_set_style_bg_opa(g_dropdown, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(g_dropdown, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(g_dropdown, 1, 0);
    lv_obj_set_style_text_color(g_dropdown, COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_text_font(g_dropdown, ui_font_small(), 0);
    lv_obj_set_style_pad_all(g_dropdown, 6, 0);

    /* Dropdown list styling */
    lv_obj_t *list = lv_dropdown_get_list(g_dropdown);
    if (list) {
        lv_obj_set_style_bg_color(list, COLOR_CARD, 0);
        lv_obj_set_style_bg_opa(list, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(list, COLOR_CARD_BORDER, 0);
        lv_obj_set_style_text_color(list, COLOR_TEXT_PRIMARY, 0);
        lv_obj_set_style_text_font(list, ui_font_small(), 0);
        lv_obj_set_style_bg_color(list, COLOR_PRIMARY,
                                  LV_PART_SELECTED | LV_STATE_CHECKED);
    }

    lv_obj_add_event_cb(g_dropdown, dropdown_cb, LV_EVENT_VALUE_CHANGED, NULL);

    rebuild_dropdown();

    /* ── Chart card ────────────────────────────────────────────────── */
    lv_obj_t *chart_card = ui_create_card(parent, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(chart_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(chart_card, 10, 0);
    lv_obj_set_style_pad_row(chart_card, 6, 0);

    /* Legend row */
    lv_obj_t *legend_row = lv_obj_create(chart_card);
    lv_obj_remove_style_all(legend_row);
    lv_obj_set_size(legend_row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(legend_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(legend_row, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(legend_row, 16, 0);
    lv_obj_clear_flag(legend_row, LV_OBJ_FLAG_SCROLLABLE);

    /* RX legend */
    lv_obj_t *rx_dot = lv_obj_create(legend_row);
    lv_obj_remove_style_all(rx_dot);
    lv_obj_set_size(rx_dot, 12, 12);
    lv_obj_set_style_bg_color(rx_dot, COLOR_CHART_RX, 0);
    lv_obj_set_style_bg_opa(rx_dot, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(rx_dot, LV_RADIUS_CIRCLE, 0);

    lv_obj_t *rx_lbl = lv_label_create(legend_row);
    lv_label_set_text(rx_lbl, "RX (Download)");
    lv_obj_set_style_text_font(rx_lbl, ui_font_small(), 0);
    lv_obj_set_style_text_color(rx_lbl, COLOR_CHART_RX, 0);

    /* TX legend */
    lv_obj_t *tx_dot = lv_obj_create(legend_row);
    lv_obj_remove_style_all(tx_dot);
    lv_obj_set_size(tx_dot, 12, 12);
    lv_obj_set_style_bg_color(tx_dot, COLOR_CHART_TX, 0);
    lv_obj_set_style_bg_opa(tx_dot, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(tx_dot, LV_RADIUS_CIRCLE, 0);

    lv_obj_t *tx_lbl = lv_label_create(legend_row);
    lv_label_set_text(tx_lbl, "TX (Upload)");
    lv_obj_set_style_text_font(tx_lbl, ui_font_small(), 0);
    lv_obj_set_style_text_color(tx_lbl, COLOR_CHART_TX, 0);

    /* The chart itself */
    g_chart = lv_chart_create(chart_card);
    lv_obj_set_size(g_chart, LV_PCT(100), 300);
    lv_chart_set_type(g_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(g_chart, APP_HISTORY_LONG_LEN);
    lv_chart_set_div_line_count(g_chart, 5, 6);
    lv_chart_set_update_mode(g_chart, LV_CHART_UPDATE_MODE_SHIFT);

    lv_obj_set_style_bg_color(g_chart, COLOR_CARD, 0);
    lv_obj_set_style_bg_opa(g_chart, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(g_chart, 0, 0);
    lv_obj_set_style_pad_all(g_chart, 4, 0);
    lv_obj_set_style_line_color(g_chart, lv_color_hex(0x2A2A4A), LV_PART_MAIN);
    lv_obj_set_style_size(g_chart, 0, LV_PART_INDICATOR);
    lv_obj_set_style_line_width(g_chart, 2, LV_PART_ITEMS);

    g_ser_rx = lv_chart_add_series(g_chart, COLOR_CHART_RX,
                                    LV_CHART_AXIS_PRIMARY_Y);
    g_ser_tx = lv_chart_add_series(g_chart, COLOR_CHART_TX,
                                    LV_CHART_AXIS_PRIMARY_Y);

    /* ── Daily totals card ─────────────────────────────────────────── */
    lv_obj_t *totals_card = ui_create_card(parent, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(totals_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(totals_card, 10, 0);
    lv_obj_set_style_pad_row(totals_card, 4, 0);

    ui_create_section_header(totals_card, "Daily Totals");

    lv_obj_t *totals_row = lv_obj_create(totals_card);
    lv_obj_remove_style_all(totals_row);
    lv_obj_set_size(totals_row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(totals_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(totals_row, LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_column(totals_row, 16, 0);
    lv_obj_clear_flag(totals_row, LV_OBJ_FLAG_SCROLLABLE);

    g_stat_daily_rx    = ui_create_stat_label(totals_row, LV_SYMBOL_DOWN " RX Today");
    g_stat_daily_tx    = ui_create_stat_label(totals_row, LV_SYMBOL_UP " TX Today");
    g_stat_daily_total = ui_create_stat_label(totals_row, "Total Today");

    /* ── Periodic update timer ─────────────────────────────────────── */
    g_update_timer = lv_timer_create(update_timer_cb, 2000, NULL);

    /* Initial update */
    ui_traffic_update();
}

void ui_traffic_update(void)
{
    const net_state_t *state = net_get_state();
    char buf[64];

    /* Refresh dropdown if interface count changed */
    {
        char current_opts[APP_MAX_INTERFACES * APP_IFACE_NAME_MAX];
        lv_dropdown_get_selected_str(g_dropdown, current_opts,
                                     sizeof(current_opts));
        /* Simple check: rebuild if selected interface name doesn't match */
        if (g_selected_iface < state->num_ifaces &&
            strcmp(current_opts,
                   state->ifaces[g_selected_iface].info.name) != 0) {
            rebuild_dropdown();
        }
    }

    if (g_selected_iface >= state->num_ifaces) {
        g_selected_iface = 0;
        if (state->num_ifaces == 0) return;
    }

    const net_iface_state_t *iface = &state->ifaces[g_selected_iface];

    /* ── Daily totals ──────────────────────────────────────────────── */
    format_bytes(iface->rx_bytes_today, buf, sizeof(buf));
    ui_update_stat_label(g_stat_daily_rx, buf);

    format_bytes(iface->tx_bytes_today, buf, sizeof(buf));
    ui_update_stat_label(g_stat_daily_tx, buf);

    format_bytes(iface->rx_bytes_today + iface->tx_bytes_today,
                 buf, sizeof(buf));
    ui_update_stat_label(g_stat_daily_total, buf);

    /* ── Chart from long history (24h) ─────────────────────────────── */
    const net_history_sample_t *hist;
    int count, head, capacity;
    net_get_long_history(g_selected_iface, &hist, &count, &head, &capacity);

    if (hist && count > 0) {
        /* Auto-scale: find max value in KiB/s */
        lv_coord_t max_val = 1;
        for (int j = 0; j < count; j++) {
            int idx = (head - count + j + capacity) % capacity;
            lv_coord_t rx = (lv_coord_t)(hist[idx].rx_bps / 1024.0f);
            lv_coord_t tx = (lv_coord_t)(hist[idx].tx_bps / 1024.0f);
            if (rx > max_val) max_val = rx;
            if (tx > max_val) max_val = tx;
        }
        lv_chart_set_range(g_chart, LV_CHART_AXIS_PRIMARY_Y,
                           0, max_val + max_val / 10 + 1);

        /* Clear and refill series */
        lv_chart_set_all_value(g_chart, g_ser_rx, 0);
        lv_chart_set_all_value(g_chart, g_ser_tx, 0);

        for (int j = 0; j < count; j++) {
            int idx = (head - count + j + capacity) % capacity;
            lv_coord_t rx = (lv_coord_t)(hist[idx].rx_bps / 1024.0f);
            lv_coord_t tx = (lv_coord_t)(hist[idx].tx_bps / 1024.0f);
            lv_chart_set_next_value(g_chart, g_ser_rx, rx);
            lv_chart_set_next_value(g_chart, g_ser_tx, tx);
        }
    }
}
