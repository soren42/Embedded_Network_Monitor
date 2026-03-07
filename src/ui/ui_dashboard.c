/**
 * @file ui_dashboard.c
 * @brief Dashboard screen - main overview with interface cards and charts
 */

#include "ui_dashboard.h"
#include "ui_theme.h"
#include "ui_widgets.h"
#include "ui_layout.h"
#include "ui_manager.h"
#include "net_collector.h"
#include "alerts.h"
#include "utils.h"
#include "app_conf.h"

#include <stdio.h>
#include <string.h>

/* ── Per-interface card widgets ─────────────────────────────────────── */

typedef struct {
    lv_obj_t *card;
    lv_obj_t *led;
    lv_obj_t *lbl_name;
    lv_obj_t *lbl_ip;
    lv_obj_t *lbl_rx_rate;
    lv_obj_t *lbl_tx_rate;
    lv_obj_t *chart;
    lv_chart_series_t *ser_rx;
    lv_chart_series_t *ser_tx;
    int       iface_idx;
} iface_card_t;

static iface_card_t g_cards[APP_MAX_INTERFACES];
static int g_num_cards;

/* Connectivity panel */
static lv_obj_t *g_conn_panel;
static lv_obj_t *g_gw_led;
static lv_obj_t *g_dns_led;
static lv_obj_t *g_lbl_gw_latency;
static lv_obj_t *g_lbl_uptime;
static lv_obj_t *g_lbl_mem;

/* Infrastructure panel */
static lv_obj_t *g_infra_panel;
static lv_obj_t *g_infra_leds[APP_MAX_INFRA_DEVICES];
static lv_obj_t *g_infra_names[APP_MAX_INFRA_DEVICES];
static lv_obj_t *g_infra_latencies[APP_MAX_INFRA_DEVICES];
static int g_infra_count;

/* Dashboard parent */
static lv_obj_t *g_parent;
static lv_timer_t *g_update_timer;

/* ── Card tap callback ─────────────────────────────────────────────── */

static void card_click_cb(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    ui_manager_show_iface_detail(idx);
}

/* ── Create a single interface card ────────────────────────────────── */

static void create_iface_card(lv_obj_t *parent, iface_card_t *ic, int idx)
{
    ic->iface_idx = idx;

    /* Card container */
    ic->card = ui_create_card(parent, ui_card_w_half(), ui_scale(180));
    lv_obj_set_flex_flow(ic->card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(ic->card, ui_scale(4), 0);
    lv_obj_add_flag(ic->card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ic->card, card_click_cb, LV_EVENT_CLICKED,
                         (void *)(intptr_t)idx);

    /* Top row: LED + name + IP */
    lv_obj_t *top_row = lv_obj_create(ic->card);
    lv_obj_remove_style_all(top_row);
    lv_obj_set_size(top_row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(top_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(top_row, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(top_row, ui_scale(8), 0);
    lv_obj_clear_flag(top_row, LV_OBJ_FLAG_SCROLLABLE);

    ic->led = ui_create_status_led(top_row, ui_scale(14));

    ic->lbl_name = lv_label_create(top_row);
    lv_label_set_text(ic->lbl_name, "---");
    lv_obj_set_style_text_font(ic->lbl_name, ui_font_normal(), 0);
    lv_obj_set_style_text_color(ic->lbl_name, COLOR_TEXT_PRIMARY, 0);

    ic->lbl_ip = lv_label_create(top_row);
    lv_label_set_text(ic->lbl_ip, "");
    lv_obj_set_style_text_font(ic->lbl_ip, ui_font_small(), 0);
    lv_obj_set_style_text_color(ic->lbl_ip, COLOR_TEXT_SECONDARY, 0);

    /* Rate row */
    lv_obj_t *rate_row = lv_obj_create(ic->card);
    lv_obj_remove_style_all(rate_row);
    lv_obj_set_size(rate_row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(rate_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(rate_row, ui_scale(16), 0);
    lv_obj_clear_flag(rate_row, LV_OBJ_FLAG_SCROLLABLE);

    /* RX label */
    lv_obj_t *rx_cont = lv_obj_create(rate_row);
    lv_obj_remove_style_all(rx_cont);
    lv_obj_set_size(rx_cont, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(rx_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(rx_cont, ui_scale(4), 0);
    lv_obj_clear_flag(rx_cont, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *rx_arrow = lv_label_create(rx_cont);
    lv_label_set_text(rx_arrow, LV_SYMBOL_DOWN);
    lv_obj_set_style_text_color(rx_arrow, COLOR_CHART_RX, 0);
    lv_obj_set_style_text_font(rx_arrow, ui_font_small(), 0);

    ic->lbl_rx_rate = lv_label_create(rx_cont);
    lv_label_set_text(ic->lbl_rx_rate, "0 B/s");
    lv_obj_set_style_text_font(ic->lbl_rx_rate, ui_font_normal(), 0);
    lv_obj_set_style_text_color(ic->lbl_rx_rate, COLOR_CHART_RX, 0);

    /* TX label */
    lv_obj_t *tx_cont = lv_obj_create(rate_row);
    lv_obj_remove_style_all(tx_cont);
    lv_obj_set_size(tx_cont, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(tx_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(tx_cont, ui_scale(4), 0);
    lv_obj_clear_flag(tx_cont, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *tx_arrow = lv_label_create(tx_cont);
    lv_label_set_text(tx_arrow, LV_SYMBOL_UP);
    lv_obj_set_style_text_color(tx_arrow, COLOR_CHART_TX, 0);
    lv_obj_set_style_text_font(tx_arrow, ui_font_small(), 0);

    ic->lbl_tx_rate = lv_label_create(tx_cont);
    lv_label_set_text(ic->lbl_tx_rate, "0 B/s");
    lv_obj_set_style_text_font(ic->lbl_tx_rate, ui_font_normal(), 0);
    lv_obj_set_style_text_color(ic->lbl_tx_rate, COLOR_CHART_TX, 0);

    /* Sparkline chart */
    ic->chart = lv_chart_create(ic->card);
    lv_obj_set_size(ic->chart, LV_PCT(100), ui_scale(60));
    lv_chart_set_type(ic->chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(ic->chart, APP_HISTORY_SHORT_LEN);
    lv_chart_set_div_line_count(ic->chart, 0, 0);
    lv_chart_set_update_mode(ic->chart, LV_CHART_UPDATE_MODE_SHIFT);

    lv_obj_set_style_bg_opa(ic->chart, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ic->chart, 0, 0);
    lv_obj_set_style_pad_all(ic->chart, 0, 0);
    lv_obj_set_style_size(ic->chart, 0, LV_PART_INDICATOR); /* no dots */
    lv_obj_set_style_line_width(ic->chart, 2, LV_PART_ITEMS);

    ic->ser_rx = lv_chart_add_series(ic->chart, COLOR_CHART_RX,
                                      LV_CHART_AXIS_PRIMARY_Y);
    ic->ser_tx = lv_chart_add_series(ic->chart, COLOR_CHART_TX,
                                      LV_CHART_AXIS_PRIMARY_Y);
}

/* ── Create connectivity panel ─────────────────────────────────────── */

static void create_connectivity_panel(lv_obj_t *parent)
{
    g_conn_panel = ui_create_card(parent, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(g_conn_panel, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(g_conn_panel, LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(g_conn_panel, ui_scale(8), 0);
    lv_obj_set_style_pad_column(g_conn_panel, ui_scale(8), 0);

    /* Gateway status */
    lv_obj_t *gw_cont = lv_obj_create(g_conn_panel);
    lv_obj_remove_style_all(gw_cont);
    lv_obj_set_size(gw_cont, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(gw_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(gw_cont, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(gw_cont, 2, 0);
    lv_obj_clear_flag(gw_cont, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *gw_lbl = lv_label_create(gw_cont);
    lv_label_set_text(gw_lbl, "Gateway");
    lv_obj_set_style_text_font(gw_lbl, ui_font_small(), 0);
    lv_obj_set_style_text_color(gw_lbl, COLOR_TEXT_SECONDARY, 0);

    g_gw_led = ui_create_status_led(gw_cont, ui_scale(16));

    g_lbl_gw_latency = lv_label_create(gw_cont);
    lv_label_set_text(g_lbl_gw_latency, "-- ms");
    lv_obj_set_style_text_font(g_lbl_gw_latency, ui_font_small(), 0);
    lv_obj_set_style_text_color(g_lbl_gw_latency, COLOR_TEXT_PRIMARY, 0);

    /* DNS status */
    lv_obj_t *dns_cont = lv_obj_create(g_conn_panel);
    lv_obj_remove_style_all(dns_cont);
    lv_obj_set_size(dns_cont, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(dns_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(dns_cont, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(dns_cont, 2, 0);
    lv_obj_clear_flag(dns_cont, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *dns_lbl = lv_label_create(dns_cont);
    lv_label_set_text(dns_lbl, "DNS");
    lv_obj_set_style_text_font(dns_lbl, ui_font_small(), 0);
    lv_obj_set_style_text_color(dns_lbl, COLOR_TEXT_SECONDARY, 0);

    g_dns_led = ui_create_status_led(dns_cont, ui_scale(16));

    /* Uptime */
    lv_obj_t *up_cont = lv_obj_create(g_conn_panel);
    lv_obj_remove_style_all(up_cont);
    lv_obj_set_size(up_cont, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(up_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(up_cont, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(up_cont, 2, 0);
    lv_obj_clear_flag(up_cont, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *up_lbl = lv_label_create(up_cont);
    lv_label_set_text(up_lbl, "Uptime");
    lv_obj_set_style_text_font(up_lbl, ui_font_small(), 0);
    lv_obj_set_style_text_color(up_lbl, COLOR_TEXT_SECONDARY, 0);

    g_lbl_uptime = lv_label_create(up_cont);
    lv_label_set_text(g_lbl_uptime, "--");
    lv_obj_set_style_text_font(g_lbl_uptime, ui_font_small(), 0);
    lv_obj_set_style_text_color(g_lbl_uptime, COLOR_TEXT_PRIMARY, 0);

    /* Memory */
    lv_obj_t *mem_cont = lv_obj_create(g_conn_panel);
    lv_obj_remove_style_all(mem_cont);
    lv_obj_set_size(mem_cont, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(mem_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(mem_cont, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(mem_cont, 2, 0);
    lv_obj_clear_flag(mem_cont, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *mem_lbl = lv_label_create(mem_cont);
    lv_label_set_text(mem_lbl, "Memory");
    lv_obj_set_style_text_font(mem_lbl, ui_font_small(), 0);
    lv_obj_set_style_text_color(mem_lbl, COLOR_TEXT_SECONDARY, 0);

    g_lbl_mem = lv_label_create(mem_cont);
    lv_label_set_text(g_lbl_mem, "--");
    lv_obj_set_style_text_font(g_lbl_mem, ui_font_small(), 0);
    lv_obj_set_style_text_color(g_lbl_mem, COLOR_TEXT_PRIMARY, 0);
}

/* ── Create infrastructure panel ───────────────────────────────────── */

static void create_infra_panel(lv_obj_t *parent)
{
    const net_state_t *state = net_get_state();
    if (state->num_infra <= 0) return;

    g_infra_panel = ui_create_card(parent, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(g_infra_panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(g_infra_panel, ui_scale(6), 0);

    lv_obj_t *title = lv_label_create(g_infra_panel);
    lv_label_set_text(title, "Infrastructure");
    lv_obj_set_style_text_font(title, ui_font_small(), 0);
    lv_obj_set_style_text_color(title, COLOR_PRIMARY, 0);

    /* Grid of devices */
    lv_obj_t *grid = lv_obj_create(g_infra_panel);
    lv_obj_remove_style_all(grid);
    lv_obj_set_size(grid, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(grid, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_column(grid, ui_scale(16), 0);
    lv_obj_set_style_pad_row(grid, ui_scale(8), 0);
    lv_obj_clear_flag(grid, LV_OBJ_FLAG_SCROLLABLE);

    g_infra_count = state->num_infra;
    if (g_infra_count > APP_MAX_INFRA_DEVICES) g_infra_count = APP_MAX_INFRA_DEVICES;

    for (int i = 0; i < g_infra_count; i++) {
        lv_obj_t *dev_cont = lv_obj_create(grid);
        lv_obj_remove_style_all(dev_cont);
        lv_obj_set_size(dev_cont, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(dev_cont, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(dev_cont, LV_FLEX_ALIGN_START,
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(dev_cont, ui_scale(6), 0);
        lv_obj_clear_flag(dev_cont, LV_OBJ_FLAG_SCROLLABLE);

        g_infra_leds[i] = ui_create_status_led(dev_cont, ui_scale(12));

        lv_obj_t *text_cont = lv_obj_create(dev_cont);
        lv_obj_remove_style_all(text_cont);
        lv_obj_set_size(text_cont, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(text_cont, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_row(text_cont, 0, 0);
        lv_obj_clear_flag(text_cont, LV_OBJ_FLAG_SCROLLABLE);

        g_infra_names[i] = lv_label_create(text_cont);
        lv_label_set_text(g_infra_names[i], state->infra[i].name);
        lv_obj_set_style_text_font(g_infra_names[i], ui_font_small(), 0);
        lv_obj_set_style_text_color(g_infra_names[i], COLOR_TEXT_PRIMARY, 0);

        g_infra_latencies[i] = lv_label_create(text_cont);
        lv_label_set_text(g_infra_latencies[i], "-- ms");
        lv_obj_set_style_text_font(g_infra_latencies[i], ui_font_small(), 0);
        lv_obj_set_style_text_color(g_infra_latencies[i], COLOR_TEXT_SECONDARY, 0);
    }
}

/* ── Update timer callback ─────────────────────────────────────────── */

static void update_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if (ui_manager_current() != SCREEN_DASHBOARD) return;
    ui_dashboard_update();
}

/* ── Public API ────────────────────────────────────────────────────── */

void ui_dashboard_create(lv_obj_t *parent)
{
    g_parent = parent;
    g_num_cards = 0;

    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(parent, ui_pad_small(), 0);
    lv_obj_set_style_pad_column(parent, ui_pad_small(), 0);
    lv_obj_add_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    const net_state_t *state = net_get_state();

    /* Create interface cards (skip USB interfaces) */
    for (int i = 0; i < state->num_ifaces && i < APP_MAX_INTERFACES; i++) {
        if (net_iface_is_hidden(state->ifaces[i].info.name)) continue;
        create_iface_card(parent, &g_cards[g_num_cards], i);
        g_num_cards++;
    }

    /* Connectivity panel (full width below cards) */
    create_connectivity_panel(parent);

    /* Infrastructure panel (if any infra devices configured) */
    create_infra_panel(parent);

    /* Register periodic update timer */
    g_update_timer = lv_timer_create(update_timer_cb, APP_UI_REFRESH_MS, NULL);

    /* Initial update */
    ui_dashboard_update();
}

void ui_dashboard_update(void)
{
    const net_state_t *state = net_get_state();
    char buf[64];

    /* Add new cards if interfaces were added since creation (skip USB) */
    int visible_count = 0;
    for (int i = 0; i < state->num_ifaces && i < APP_MAX_INTERFACES; i++) {
        if (!net_iface_is_hidden(state->ifaces[i].info.name))
            visible_count++;
    }
    while (g_num_cards < visible_count && g_num_cards < APP_MAX_INTERFACES) {
        /* Find the next non-hidden interface to add */
        int iface_idx = 0, seen = 0;
        for (int i = 0; i < state->num_ifaces; i++) {
            if (net_iface_is_hidden(state->ifaces[i].info.name)) continue;
            if (seen == g_num_cards) { iface_idx = i; break; }
            seen++;
        }
        create_iface_card(g_parent, &g_cards[g_num_cards], iface_idx);
        g_num_cards++;
    }

    /* Update each interface card */
    for (int i = 0; i < g_num_cards; i++) {
        iface_card_t *ic = &g_cards[i];
        const net_iface_state_t *iface = &state->ifaces[i];

        /* Name */
        lv_label_set_text(ic->lbl_name, iface->info.name);

        /* IP address */
        if (iface->info.ip_addr != 0) {
            format_ip(iface->info.ip_addr, buf, sizeof(buf));
            lv_label_set_text(ic->lbl_ip, buf);
        } else {
            lv_label_set_text(ic->lbl_ip, "No IP");
        }

        /* Status LED */
        if (iface->info.is_up && iface->info.is_running) {
            ui_set_status_led(ic->led, 1);  /* green */
        } else if (iface->info.is_up) {
            ui_set_status_led(ic->led, 2);  /* yellow */
        } else {
            ui_set_status_led(ic->led, 0);  /* red */
        }

        /* RX/TX rates */
        format_rate(iface->rates.rx_bytes_per_sec, buf, sizeof(buf));
        lv_label_set_text(ic->lbl_rx_rate, buf);

        format_rate(iface->rates.tx_bytes_per_sec, buf, sizeof(buf));
        lv_label_set_text(ic->lbl_tx_rate, buf);

        /* Update sparkline from short history */
        const net_history_sample_t *hist;
        int count, head, capacity;
        net_get_short_history(i, &hist, &count, &head, &capacity);

        if (hist && count > 0) {
            lv_coord_t max_val = 1;
            for (int j = 0; j < count; j++) {
                int idx = (head - count + j + capacity) % capacity;
                lv_coord_t rx = (lv_coord_t)(hist[idx].rx_bps / 1024.0f);
                lv_coord_t tx = (lv_coord_t)(hist[idx].tx_bps / 1024.0f);
                if (rx > max_val) max_val = rx;
                if (tx > max_val) max_val = tx;
            }
            lv_chart_set_range(ic->chart, LV_CHART_AXIS_PRIMARY_Y,
                               0, max_val + max_val / 10 + 1);

            lv_chart_set_all_value(ic->chart, ic->ser_rx, 0);
            lv_chart_set_all_value(ic->chart, ic->ser_tx, 0);

            for (int j = 0; j < count; j++) {
                int idx = (head - count + j + capacity) % capacity;
                lv_coord_t rx = (lv_coord_t)(hist[idx].rx_bps / 1024.0f);
                lv_coord_t tx = (lv_coord_t)(hist[idx].tx_bps / 1024.0f);
                lv_chart_set_next_value(ic->chart, ic->ser_rx, rx);
                lv_chart_set_next_value(ic->chart, ic->ser_tx, tx);
            }
        }
    }

    /* Update connectivity panel */
    if (state->connectivity.gateway_reachable) {
        ui_set_status_led(g_gw_led, 1);
        snprintf(buf, sizeof(buf), "%.0f ms",
                 state->connectivity.gateway_latency_ms);
        lv_label_set_text(g_lbl_gw_latency, buf);
    } else {
        ui_set_status_led(g_gw_led, 0);
        lv_label_set_text(g_lbl_gw_latency, "N/A");
    }

    ui_set_status_led(g_dns_led,
                      state->connectivity.dns_reachable ? 1 : 0);

    /* Uptime */
    format_duration(state->uptime_sec, buf, sizeof(buf));
    lv_label_set_text(g_lbl_uptime, buf);

    /* Memory */
    if (state->mem_total_kb > 0) {
        int pct = (int)(100 - ((uint64_t)state->mem_available_kb * 100 /
                                state->mem_total_kb));
        snprintf(buf, sizeof(buf), "%d%%", pct);
        lv_label_set_text(g_lbl_mem, buf);
    }

    /* Update infrastructure panel */
    for (int i = 0; i < g_infra_count && i < state->num_infra; i++) {
        const infra_device_t *dev = &state->infra[i];
        if (!dev->enabled) continue;

        if (dev->reachable) {
            if (dev->latency_ms > 100.0) {
                ui_set_status_led(g_infra_leds[i], 2); /* yellow */
            } else {
                ui_set_status_led(g_infra_leds[i], 1); /* green */
            }
            snprintf(buf, sizeof(buf), "%.0f ms", dev->latency_ms);
        } else {
            ui_set_status_led(g_infra_leds[i], 0); /* red */
            snprintf(buf, sizeof(buf), "N/A");
        }
        lv_label_set_text(g_infra_latencies[i], buf);
    }

    /* Update alert badge */
    ui_manager_set_alert_count(alerts_active_count());
}
