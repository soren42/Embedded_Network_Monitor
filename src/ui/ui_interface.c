/**
 * @file ui_interface.c
 * @brief Interface detail screen - detailed stats, traffic chart, WiFi info
 */

#include "ui_interface.h"
#include "ui_theme.h"
#include "ui_widgets.h"
#include "ui_manager.h"
#include "net_collector.h"
#include "utils.h"
#include "app_conf.h"

#include <stdio.h>
#include <string.h>

/* ── Static widget references ──────────────────────────────────────── */

static int          g_iface_idx;
static lv_obj_t    *g_parent;
static lv_timer_t  *g_update_timer;

/* Header */
static lv_obj_t *g_lbl_iface_name;
static lv_obj_t *g_status_led;
static lv_obj_t *g_lbl_status;

/* Info stats */
static lv_obj_t *g_stat_ip;
static lv_obj_t *g_stat_mac;
static lv_obj_t *g_stat_netmask;
static lv_obj_t *g_stat_link_speed;

/* RX/TX live rates */
static lv_obj_t *g_stat_rx_rate;
static lv_obj_t *g_stat_tx_rate;
static lv_obj_t *g_stat_rx_packets;
static lv_obj_t *g_stat_tx_packets;
static lv_obj_t *g_stat_rx_errors;
static lv_obj_t *g_stat_tx_errors;
static lv_obj_t *g_stat_rx_drops;
static lv_obj_t *g_stat_tx_drops;

/* Peaks and daily totals */
static lv_obj_t *g_stat_peak_rx;
static lv_obj_t *g_stat_peak_tx;
static lv_obj_t *g_stat_daily_rx;
static lv_obj_t *g_stat_daily_tx;

/* WiFi section */
static lv_obj_t *g_wifi_section;
static lv_obj_t *g_stat_ssid;
static lv_obj_t *g_stat_signal;
static lv_obj_t *g_stat_quality;
static lv_obj_t *g_stat_bitrate;

/* Traffic chart (60s short history) */
static lv_obj_t           *g_chart;
static lv_chart_series_t  *g_ser_rx;
static lv_chart_series_t  *g_ser_tx;

/* ── Helper: create a row of stat labels ───────────────────────────── */

static lv_obj_t *create_stat_row(lv_obj_t *parent, int pad_col)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_column(row, pad_col, 0);
    lv_obj_set_style_pad_row(row, 6, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    return row;
}

/* ── Timer callback ────────────────────────────────────────────────── */

static void update_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if (ui_manager_current() != SCREEN_IFACE_DETAIL) return;
    ui_interface_update();
}

/* ── Public API ────────────────────────────────────────────────────── */

void ui_interface_create(lv_obj_t *parent)
{
    g_parent    = parent;
    g_iface_idx = 0;

    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(parent, 6, 0);
    lv_obj_add_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    /* ── Back button + header row ──────────────────────────────────── */
    lv_obj_t *hdr_row = lv_obj_create(parent);
    lv_obj_remove_style_all(hdr_row);
    lv_obj_set_size(hdr_row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(hdr_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(hdr_row, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(hdr_row, 8, 0);
    lv_obj_clear_flag(hdr_row, LV_OBJ_FLAG_SCROLLABLE);

    ui_create_back_btn(hdr_row);

    g_status_led = ui_create_status_led(hdr_row, 16);

    g_lbl_iface_name = lv_label_create(hdr_row);
    lv_label_set_text(g_lbl_iface_name, "---");
    lv_obj_set_style_text_font(g_lbl_iface_name, ui_font_title(), 0);
    lv_obj_set_style_text_color(g_lbl_iface_name, COLOR_TEXT_PRIMARY, 0);

    g_lbl_status = lv_label_create(hdr_row);
    lv_label_set_text(g_lbl_status, "");
    lv_obj_set_style_text_font(g_lbl_status, ui_font_small(), 0);
    lv_obj_set_style_text_color(g_lbl_status, COLOR_TEXT_SECONDARY, 0);

    /* ── Info card ─────────────────────────────────────────────────── */
    lv_obj_t *info_card = ui_create_card(parent, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(info_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(info_card, 10, 0);
    lv_obj_set_style_pad_row(info_card, 4, 0);

    ui_create_section_header(info_card, "Interface Info");

    lv_obj_t *info_row = create_stat_row(info_card, 24);
    g_stat_ip         = ui_create_stat_label(info_row, "IP Address");
    g_stat_mac        = ui_create_stat_label(info_row, "MAC");
    g_stat_netmask    = ui_create_stat_label(info_row, "Netmask");
    g_stat_link_speed = ui_create_stat_label(info_row, "Link Speed");

    /* ── Live rates card ───────────────────────────────────────────── */
    lv_obj_t *rate_card = ui_create_card(parent, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(rate_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(rate_card, 10, 0);
    lv_obj_set_style_pad_row(rate_card, 4, 0);

    ui_create_section_header(rate_card, "Live Traffic");

    lv_obj_t *rate_row1 = create_stat_row(rate_card, 24);
    g_stat_rx_rate    = ui_create_stat_label(rate_row1, LV_SYMBOL_DOWN " RX Rate");
    g_stat_tx_rate    = ui_create_stat_label(rate_row1, LV_SYMBOL_UP " TX Rate");
    g_stat_rx_packets = ui_create_stat_label(rate_row1, "RX Packets");
    g_stat_tx_packets = ui_create_stat_label(rate_row1, "TX Packets");

    lv_obj_t *rate_row2 = create_stat_row(rate_card, 24);
    g_stat_rx_errors = ui_create_stat_label(rate_row2, "RX Errors");
    g_stat_tx_errors = ui_create_stat_label(rate_row2, "TX Errors");
    g_stat_rx_drops  = ui_create_stat_label(rate_row2, "RX Drops");
    g_stat_tx_drops  = ui_create_stat_label(rate_row2, "TX Drops");

    /* ── Peaks and daily totals card ───────────────────────────────── */
    lv_obj_t *totals_card = ui_create_card(parent, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(totals_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(totals_card, 10, 0);
    lv_obj_set_style_pad_row(totals_card, 4, 0);

    ui_create_section_header(totals_card, "Peaks & Daily Totals");

    lv_obj_t *totals_row = create_stat_row(totals_card, 24);
    g_stat_peak_rx  = ui_create_stat_label(totals_row, "Peak RX");
    g_stat_peak_tx  = ui_create_stat_label(totals_row, "Peak TX");
    g_stat_daily_rx = ui_create_stat_label(totals_row, "Today RX");
    g_stat_daily_tx = ui_create_stat_label(totals_row, "Today TX");

    /* ── WiFi card (hidden for wired interfaces) ───────────────────── */
    g_wifi_section = ui_create_card(parent, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(g_wifi_section, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(g_wifi_section, 10, 0);
    lv_obj_set_style_pad_row(g_wifi_section, 4, 0);
    lv_obj_add_flag(g_wifi_section, LV_OBJ_FLAG_HIDDEN);

    ui_create_section_header(g_wifi_section, "WiFi");

    lv_obj_t *wifi_row = create_stat_row(g_wifi_section, 24);
    g_stat_ssid    = ui_create_stat_label(wifi_row, "SSID");
    g_stat_signal  = ui_create_stat_label(wifi_row, "Signal");
    g_stat_quality = ui_create_stat_label(wifi_row, "Quality");
    g_stat_bitrate = ui_create_stat_label(wifi_row, "Bitrate");

    /* ── Traffic chart (60s short history) ─────────────────────────── */
    lv_obj_t *chart_card = ui_create_card(parent, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(chart_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(chart_card, 10, 0);
    lv_obj_set_style_pad_row(chart_card, 4, 0);

    ui_create_section_header(chart_card, "Traffic (60s)");

    /* RX/TX legend */
    lv_obj_t *legend_row = lv_obj_create(chart_card);
    lv_obj_remove_style_all(legend_row);
    lv_obj_set_size(legend_row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(legend_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(legend_row, 16, 0);
    lv_obj_clear_flag(legend_row, LV_OBJ_FLAG_SCROLLABLE);

    /* RX legend indicator */
    lv_obj_t *rx_dot = lv_obj_create(legend_row);
    lv_obj_remove_style_all(rx_dot);
    lv_obj_set_size(rx_dot, 10, 10);
    lv_obj_set_style_bg_color(rx_dot, COLOR_CHART_RX, 0);
    lv_obj_set_style_bg_opa(rx_dot, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(rx_dot, LV_RADIUS_CIRCLE, 0);

    lv_obj_t *rx_lbl = lv_label_create(legend_row);
    lv_label_set_text(rx_lbl, "RX");
    lv_obj_set_style_text_font(rx_lbl, ui_font_small(), 0);
    lv_obj_set_style_text_color(rx_lbl, COLOR_CHART_RX, 0);

    /* TX legend indicator */
    lv_obj_t *tx_dot = lv_obj_create(legend_row);
    lv_obj_remove_style_all(tx_dot);
    lv_obj_set_size(tx_dot, 10, 10);
    lv_obj_set_style_bg_color(tx_dot, COLOR_CHART_TX, 0);
    lv_obj_set_style_bg_opa(tx_dot, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(tx_dot, LV_RADIUS_CIRCLE, 0);

    lv_obj_t *tx_lbl = lv_label_create(legend_row);
    lv_label_set_text(tx_lbl, "TX");
    lv_obj_set_style_text_font(tx_lbl, ui_font_small(), 0);
    lv_obj_set_style_text_color(tx_lbl, COLOR_CHART_TX, 0);

    /* Chart widget */
    g_chart = lv_chart_create(chart_card);
    lv_obj_set_size(g_chart, LV_PCT(100), 200);
    lv_chart_set_type(g_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(g_chart, APP_HISTORY_SHORT_LEN);
    lv_chart_set_div_line_count(g_chart, 4, 0);
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

    /* ── Periodic update timer ─────────────────────────────────────── */
    g_update_timer = lv_timer_create(update_timer_cb, APP_UI_REFRESH_MS, NULL);

    /* Initial update */
    ui_interface_update();
}

void ui_interface_set_iface(int iface_idx)
{
    if (iface_idx >= 0 && iface_idx < APP_MAX_INTERFACES) {
        g_iface_idx = iface_idx;
    }
}

void ui_interface_update(void)
{
    const net_state_t *state = net_get_state();
    char buf[64];

    if (g_iface_idx >= state->num_ifaces) return;

    const net_iface_state_t *iface = &state->ifaces[g_iface_idx];

    /* ── Header ────────────────────────────────────────────────────── */
    lv_label_set_text(g_lbl_iface_name, iface->info.name);

    if (iface->info.is_up && iface->info.is_running) {
        ui_set_status_led(g_status_led, 1);
        lv_label_set_text(g_lbl_status, "UP / RUNNING");
    } else if (iface->info.is_up) {
        ui_set_status_led(g_status_led, 2);
        lv_label_set_text(g_lbl_status, "UP / NO CARRIER");
    } else {
        ui_set_status_led(g_status_led, 0);
        lv_label_set_text(g_lbl_status, "DOWN");
    }

    /* ── Info stats ────────────────────────────────────────────────── */
    if (iface->info.ip_addr != 0) {
        format_ip(iface->info.ip_addr, buf, sizeof(buf));
    } else {
        snprintf(buf, sizeof(buf), "N/A");
    }
    ui_update_stat_label(g_stat_ip, buf);

    format_mac(iface->info.mac, buf, sizeof(buf));
    ui_update_stat_label(g_stat_mac, buf);

    if (iface->info.netmask != 0) {
        format_ip(iface->info.netmask, buf, sizeof(buf));
    } else {
        snprintf(buf, sizeof(buf), "N/A");
    }
    ui_update_stat_label(g_stat_netmask, buf);

    if (iface->info.speed_mbps > 0) {
        snprintf(buf, sizeof(buf), "%u Mbps", iface->info.speed_mbps);
    } else {
        snprintf(buf, sizeof(buf), "N/A");
    }
    ui_update_stat_label(g_stat_link_speed, buf);

    /* ── Live rates ────────────────────────────────────────────────── */
    format_rate(iface->rates.rx_bytes_per_sec, buf, sizeof(buf));
    ui_update_stat_label(g_stat_rx_rate, buf);

    format_rate(iface->rates.tx_bytes_per_sec, buf, sizeof(buf));
    ui_update_stat_label(g_stat_tx_rate, buf);

    snprintf(buf, sizeof(buf), "%llu",
             (unsigned long long)iface->stats.rx_packets);
    ui_update_stat_label(g_stat_rx_packets, buf);

    snprintf(buf, sizeof(buf), "%llu",
             (unsigned long long)iface->stats.tx_packets);
    ui_update_stat_label(g_stat_tx_packets, buf);

    snprintf(buf, sizeof(buf), "%llu",
             (unsigned long long)iface->stats.rx_errors);
    ui_update_stat_label(g_stat_rx_errors, buf);

    snprintf(buf, sizeof(buf), "%llu",
             (unsigned long long)iface->stats.tx_errors);
    ui_update_stat_label(g_stat_tx_errors, buf);

    snprintf(buf, sizeof(buf), "%llu",
             (unsigned long long)iface->stats.rx_dropped);
    ui_update_stat_label(g_stat_rx_drops, buf);

    snprintf(buf, sizeof(buf), "%llu",
             (unsigned long long)iface->stats.tx_dropped);
    ui_update_stat_label(g_stat_tx_drops, buf);

    /* ── Peaks and daily totals ────────────────────────────────────── */
    format_rate(iface->peak_rx_bps, buf, sizeof(buf));
    ui_update_stat_label(g_stat_peak_rx, buf);

    format_rate(iface->peak_tx_bps, buf, sizeof(buf));
    ui_update_stat_label(g_stat_peak_tx, buf);

    format_bytes(iface->rx_bytes_today, buf, sizeof(buf));
    ui_update_stat_label(g_stat_daily_rx, buf);

    format_bytes(iface->tx_bytes_today, buf, sizeof(buf));
    ui_update_stat_label(g_stat_daily_tx, buf);

    /* ── WiFi section ──────────────────────────────────────────────── */
    if (iface->info.is_wireless) {
        lv_obj_clear_flag(g_wifi_section, LV_OBJ_FLAG_HIDDEN);

        if (iface->wifi.ssid[0] != '\0') {
            ui_update_stat_label(g_stat_ssid, iface->wifi.ssid);
        } else {
            ui_update_stat_label(g_stat_ssid, "N/A");
        }

        snprintf(buf, sizeof(buf), "%d dBm", iface->wifi.signal_dbm);
        ui_update_stat_label(g_stat_signal, buf);

        snprintf(buf, sizeof(buf), "%d%%", iface->wifi.link_quality);
        ui_update_stat_label(g_stat_quality, buf);

        snprintf(buf, sizeof(buf), "%.1f Mbps", iface->wifi.bitrate_mbps);
        ui_update_stat_label(g_stat_bitrate, buf);
    } else {
        lv_obj_add_flag(g_wifi_section, LV_OBJ_FLAG_HIDDEN);
    }

    /* ── Traffic chart from short history ──────────────────────────── */
    const net_history_sample_t *hist;
    int count, head, capacity;
    net_get_short_history(g_iface_idx, &hist, &count, &head, &capacity);

    if (hist && count > 0) {
        /* Find max for auto-scaling */
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
