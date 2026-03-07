/**
 * @file ui_status.c
 * @brief Network Status Dashboard - at-a-glance network health overview
 *
 * The default screen showing overall network health at a glance:
 * - Large central health indicator (OK / DEGRADED / DOWN)
 * - Router reachability with latency
 * - DNS functionality
 * - WAN link quality (next hop, jitter, packet loss)
 * - WiFi connection status
 * - Infrastructure device summary
 */

#include "ui_status.h"
#include "ui_theme.h"
#include "ui_widgets.h"
#include "ui_layout.h"
#include "ui_manager.h"
#include "net_collector.h"
#include "net_discovery.h"
#include "net_wifi_mgr.h"
#include "alerts.h"
#include "utils.h"
#include "app_conf.h"

#include <stdio.h>
#include <string.h>

/* ── Widget references ────────────────────────────────────────────── */

/* Overall health */
static lv_obj_t *g_health_card;
static lv_obj_t *g_health_led;
static lv_obj_t *g_health_label;
static lv_obj_t *g_health_detail;

/* Service status indicators */
static lv_obj_t *g_router_led;
static lv_obj_t *g_router_label;
static lv_obj_t *g_dns_led;
static lv_obj_t *g_dns_label;
static lv_obj_t *g_wan_led;
static lv_obj_t *g_wan_label;
static lv_obj_t *g_wifi_led;
static lv_obj_t *g_wifi_label;

/* WAN detail panel */
static lv_obj_t *g_wan_detail_card;
static lv_obj_t *g_lbl_wan_hop;
static lv_obj_t *g_lbl_wan_latency;
static lv_obj_t *g_lbl_wan_jitter;
static lv_obj_t *g_lbl_wan_loss;

/* Infrastructure summary */
static lv_obj_t *g_infra_card;
static lv_obj_t *g_lbl_infra_summary;
static lv_obj_t *g_infra_leds[APP_MAX_INFRA_DEVICES];
static lv_obj_t *g_infra_names[APP_MAX_INFRA_DEVICES];
static int g_infra_count;

/* System info */
static lv_obj_t *g_lbl_uptime;
static lv_obj_t *g_lbl_mem;
static lv_obj_t *g_lbl_alerts;

static lv_timer_t *g_update_timer;

/* ── Health assessment ────────────────────────────────────────────── */

typedef enum {
    HEALTH_OK = 0,
    HEALTH_DEGRADED,
    HEALTH_DOWN
} health_level_t;

static health_level_t assess_health(const net_state_t *state,
                                     const wan_discovery_t *wan)
{
    if (!state->connectivity.gateway_reachable)
        return HEALTH_DOWN;
    if (!state->connectivity.dns_reachable)
        return HEALTH_DEGRADED;
    if (wan && wan->wan_packet_loss_pct > 50)
        return HEALTH_DEGRADED;
    if (state->connectivity.gateway_latency_ms > 200.0)
        return HEALTH_DEGRADED;
    if (alerts_active_count() > 2)
        return HEALTH_DEGRADED;
    return HEALTH_OK;
}

/* ── Create a status row (LED + label pair) ───────────────────────── */

static void create_status_row(lv_obj_t *parent, const char *title,
                               lv_obj_t **led_out, lv_obj_t **label_out)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, ui_scale(10), 0);
    lv_obj_set_style_pad_top(row, ui_scale(4), 0);
    lv_obj_set_style_pad_bottom(row, ui_scale(4), 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    *led_out = ui_create_status_led(row, ui_scale(18));

    lv_obj_t *text_col = lv_obj_create(row);
    lv_obj_remove_style_all(text_col);
    lv_obj_set_size(text_col, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(text_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(text_col, 0, 0);
    lv_obj_clear_flag(text_col, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *name = lv_label_create(text_col);
    lv_label_set_text(name, title);
    lv_obj_set_style_text_font(name, ui_font_normal(), 0);
    lv_obj_set_style_text_color(name, COLOR_TEXT_PRIMARY, 0);

    *label_out = lv_label_create(text_col);
    lv_label_set_text(*label_out, "Checking...");
    lv_obj_set_style_text_font(*label_out, ui_font_small(), 0);
    lv_obj_set_style_text_color(*label_out, COLOR_TEXT_SECONDARY, 0);
}

/* ── Update timer callback ────────────────────────────────────────── */

static void update_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if (ui_manager_current() != SCREEN_STATUS) return;
    ui_status_update();
}

/* ── Public API ───────────────────────────────────────────────────── */

void ui_status_create(lv_obj_t *parent)
{
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(parent, ui_pad_small(), 0);
    lv_obj_add_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    /* ── Overall Health Card ────────────────────────────────────── */
    g_health_card = ui_create_card(parent, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(g_health_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(g_health_card, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(g_health_card, ui_scale(16), 0);
    lv_obj_set_style_pad_row(g_health_card, ui_scale(6), 0);

    g_health_led = ui_create_status_led(g_health_card, ui_scale(40));

    g_health_label = lv_label_create(g_health_card);
    lv_label_set_text(g_health_label, "CHECKING...");
    lv_obj_set_style_text_font(g_health_label, ui_font_title(), 0);
    lv_obj_set_style_text_color(g_health_label, COLOR_TEXT_PRIMARY, 0);

    g_health_detail = lv_label_create(g_health_card);
    lv_label_set_text(g_health_detail, "Initializing network checks...");
    lv_obj_set_style_text_font(g_health_detail, ui_font_small(), 0);
    lv_obj_set_style_text_color(g_health_detail, COLOR_TEXT_SECONDARY, 0);

    /* ── Service Status Grid (2x2) ──────────────────────────────── */
    lv_obj_t *svc_grid = lv_obj_create(parent);
    lv_obj_remove_style_all(svc_grid);
    lv_obj_set_size(svc_grid, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(svc_grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(svc_grid, LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(svc_grid, ui_pad_small(), 0);
    lv_obj_set_style_pad_column(svc_grid, ui_pad_small(), 0);
    lv_obj_clear_flag(svc_grid, LV_OBJ_FLAG_SCROLLABLE);

    /* Router card */
    lv_obj_t *router_card = ui_create_card(svc_grid, ui_card_w_half(),
                                            LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(router_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(router_card, ui_scale(10), 0);
    create_status_row(router_card, "Router", &g_router_led, &g_router_label);

    /* DNS card */
    lv_obj_t *dns_card = ui_create_card(svc_grid, ui_card_w_half(),
                                         LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(dns_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(dns_card, ui_scale(10), 0);
    create_status_row(dns_card, "DNS", &g_dns_led, &g_dns_label);

    /* WAN card */
    lv_obj_t *wan_card = ui_create_card(svc_grid, ui_card_w_half(),
                                         LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(wan_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(wan_card, ui_scale(10), 0);
    create_status_row(wan_card, "WAN Link", &g_wan_led, &g_wan_label);

    /* WiFi card */
    lv_obj_t *wifi_card = ui_create_card(svc_grid, ui_card_w_half(),
                                          LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(wifi_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(wifi_card, ui_scale(10), 0);
    create_status_row(wifi_card, "WiFi", &g_wifi_led, &g_wifi_label);

    /* ── WAN Detail Card ────────────────────────────────────────── */
    g_wan_detail_card = ui_create_card(parent, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(g_wan_detail_card, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(g_wan_detail_card, LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(g_wan_detail_card, ui_scale(10), 0);

    lv_obj_t *hop_col = lv_obj_create(g_wan_detail_card);
    lv_obj_remove_style_all(hop_col);
    lv_obj_set_size(hop_col, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(hop_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(hop_col, 0, 0);
    lv_obj_clear_flag(hop_col, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *hop_title = lv_label_create(hop_col);
    lv_label_set_text(hop_title, "Next Hop");
    lv_obj_set_style_text_font(hop_title, ui_font_small(), 0);
    lv_obj_set_style_text_color(hop_title, COLOR_TEXT_SECONDARY, 0);
    g_lbl_wan_hop = lv_label_create(hop_col);
    lv_label_set_text(g_lbl_wan_hop, "--");
    lv_obj_set_style_text_font(g_lbl_wan_hop, ui_font_small(), 0);
    lv_obj_set_style_text_color(g_lbl_wan_hop, COLOR_TEXT_PRIMARY, 0);

    lv_obj_t *lat_col = lv_obj_create(g_wan_detail_card);
    lv_obj_remove_style_all(lat_col);
    lv_obj_set_size(lat_col, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(lat_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(lat_col, 0, 0);
    lv_obj_clear_flag(lat_col, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lat_title = lv_label_create(lat_col);
    lv_label_set_text(lat_title, "Latency");
    lv_obj_set_style_text_font(lat_title, ui_font_small(), 0);
    lv_obj_set_style_text_color(lat_title, COLOR_TEXT_SECONDARY, 0);
    g_lbl_wan_latency = lv_label_create(lat_col);
    lv_label_set_text(g_lbl_wan_latency, "-- ms");
    lv_obj_set_style_text_font(g_lbl_wan_latency, ui_font_small(), 0);
    lv_obj_set_style_text_color(g_lbl_wan_latency, COLOR_TEXT_PRIMARY, 0);

    lv_obj_t *jit_col = lv_obj_create(g_wan_detail_card);
    lv_obj_remove_style_all(jit_col);
    lv_obj_set_size(jit_col, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(jit_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(jit_col, 0, 0);
    lv_obj_clear_flag(jit_col, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *jit_title = lv_label_create(jit_col);
    lv_label_set_text(jit_title, "Jitter");
    lv_obj_set_style_text_font(jit_title, ui_font_small(), 0);
    lv_obj_set_style_text_color(jit_title, COLOR_TEXT_SECONDARY, 0);
    g_lbl_wan_jitter = lv_label_create(jit_col);
    lv_label_set_text(g_lbl_wan_jitter, "-- ms");
    lv_obj_set_style_text_font(g_lbl_wan_jitter, ui_font_small(), 0);
    lv_obj_set_style_text_color(g_lbl_wan_jitter, COLOR_TEXT_PRIMARY, 0);

    lv_obj_t *loss_col = lv_obj_create(g_wan_detail_card);
    lv_obj_remove_style_all(loss_col);
    lv_obj_set_size(loss_col, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(loss_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(loss_col, 0, 0);
    lv_obj_clear_flag(loss_col, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *loss_title = lv_label_create(loss_col);
    lv_label_set_text(loss_title, "Loss");
    lv_obj_set_style_text_font(loss_title, ui_font_small(), 0);
    lv_obj_set_style_text_color(loss_title, COLOR_TEXT_SECONDARY, 0);
    g_lbl_wan_loss = lv_label_create(loss_col);
    lv_label_set_text(g_lbl_wan_loss, "--%");
    lv_obj_set_style_text_font(g_lbl_wan_loss, ui_font_small(), 0);
    lv_obj_set_style_text_color(g_lbl_wan_loss, COLOR_TEXT_PRIMARY, 0);

    /* ── Infrastructure Summary Card ────────────────────────────── */
    const net_state_t *state = net_get_state();
    if (state->num_infra > 0) {
        g_infra_card = ui_create_card(parent, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(g_infra_card, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_all(g_infra_card, ui_scale(10), 0);
        lv_obj_set_style_pad_row(g_infra_card, ui_scale(4), 0);

        lv_obj_t *infra_title = lv_label_create(g_infra_card);
        lv_label_set_text(infra_title, "Infrastructure");
        lv_obj_set_style_text_font(infra_title, ui_font_small(), 0);
        lv_obj_set_style_text_color(infra_title, COLOR_PRIMARY, 0);

        lv_obj_t *infra_grid = lv_obj_create(g_infra_card);
        lv_obj_remove_style_all(infra_grid);
        lv_obj_set_size(infra_grid, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(infra_grid, LV_FLEX_FLOW_ROW_WRAP);
        lv_obj_set_style_pad_column(infra_grid, ui_scale(16), 0);
        lv_obj_set_style_pad_row(infra_grid, ui_scale(4), 0);
        lv_obj_clear_flag(infra_grid, LV_OBJ_FLAG_SCROLLABLE);

        g_infra_count = state->num_infra;
        if (g_infra_count > APP_MAX_INFRA_DEVICES)
            g_infra_count = APP_MAX_INFRA_DEVICES;

        for (int i = 0; i < g_infra_count; i++) {
            lv_obj_t *dev_row = lv_obj_create(infra_grid);
            lv_obj_remove_style_all(dev_row);
            lv_obj_set_size(dev_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_flex_flow(dev_row, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(dev_row, LV_FLEX_ALIGN_START,
                                  LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            lv_obj_set_style_pad_column(dev_row, ui_scale(4), 0);
            lv_obj_clear_flag(dev_row, LV_OBJ_FLAG_SCROLLABLE);

            g_infra_leds[i] = ui_create_status_led(dev_row, ui_scale(10));

            g_infra_names[i] = lv_label_create(dev_row);
            lv_label_set_text(g_infra_names[i], state->infra[i].name);
            lv_obj_set_style_text_font(g_infra_names[i], ui_font_small(), 0);
            lv_obj_set_style_text_color(g_infra_names[i],
                                         COLOR_TEXT_PRIMARY, 0);
        }

        g_lbl_infra_summary = lv_label_create(g_infra_card);
        lv_label_set_text(g_lbl_infra_summary, "");
        lv_obj_set_style_text_font(g_lbl_infra_summary, ui_font_small(), 0);
        lv_obj_set_style_text_color(g_lbl_infra_summary,
                                     COLOR_TEXT_SECONDARY, 0);
    }

    /* ── System info row ────────────────────────────────────────── */
    lv_obj_t *sys_card = ui_create_card(parent, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(sys_card, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(sys_card, LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(sys_card, ui_scale(8), 0);

    /* Uptime */
    lv_obj_t *up_col = lv_obj_create(sys_card);
    lv_obj_remove_style_all(up_col);
    lv_obj_set_size(up_col, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(up_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(up_col, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(up_col, 0, 0);
    lv_obj_clear_flag(up_col, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *up_title = lv_label_create(up_col);
    lv_label_set_text(up_title, "Uptime");
    lv_obj_set_style_text_font(up_title, ui_font_small(), 0);
    lv_obj_set_style_text_color(up_title, COLOR_TEXT_SECONDARY, 0);
    g_lbl_uptime = lv_label_create(up_col);
    lv_label_set_text(g_lbl_uptime, "--");
    lv_obj_set_style_text_font(g_lbl_uptime, ui_font_small(), 0);
    lv_obj_set_style_text_color(g_lbl_uptime, COLOR_TEXT_PRIMARY, 0);

    /* Memory */
    lv_obj_t *mem_col = lv_obj_create(sys_card);
    lv_obj_remove_style_all(mem_col);
    lv_obj_set_size(mem_col, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(mem_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(mem_col, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(mem_col, 0, 0);
    lv_obj_clear_flag(mem_col, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *mem_title = lv_label_create(mem_col);
    lv_label_set_text(mem_title, "Memory");
    lv_obj_set_style_text_font(mem_title, ui_font_small(), 0);
    lv_obj_set_style_text_color(mem_title, COLOR_TEXT_SECONDARY, 0);
    g_lbl_mem = lv_label_create(mem_col);
    lv_label_set_text(g_lbl_mem, "--");
    lv_obj_set_style_text_font(g_lbl_mem, ui_font_small(), 0);
    lv_obj_set_style_text_color(g_lbl_mem, COLOR_TEXT_PRIMARY, 0);

    /* Alerts */
    lv_obj_t *alert_col = lv_obj_create(sys_card);
    lv_obj_remove_style_all(alert_col);
    lv_obj_set_size(alert_col, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(alert_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(alert_col, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(alert_col, 0, 0);
    lv_obj_clear_flag(alert_col, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *alert_title = lv_label_create(alert_col);
    lv_label_set_text(alert_title, "Alerts");
    lv_obj_set_style_text_font(alert_title, ui_font_small(), 0);
    lv_obj_set_style_text_color(alert_title, COLOR_TEXT_SECONDARY, 0);
    g_lbl_alerts = lv_label_create(alert_col);
    lv_label_set_text(g_lbl_alerts, "0");
    lv_obj_set_style_text_font(g_lbl_alerts, ui_font_small(), 0);
    lv_obj_set_style_text_color(g_lbl_alerts, COLOR_TEXT_PRIMARY, 0);

    /* Register periodic update timer */
    g_update_timer = lv_timer_create(update_timer_cb, APP_UI_REFRESH_MS, NULL);

    /* Initial update */
    ui_status_update();
}

void ui_status_update(void)
{
    const net_state_t *state = net_get_state();
    const wan_discovery_t *wan = net_get_wan_discovery();
    char buf[64];

    /* ── Router ──────────────────────────────────────────────────── */
    if (state->connectivity.gateway_reachable) {
        ui_set_status_led(g_router_led, 1);
        snprintf(buf, sizeof(buf), "%.0f ms",
                 state->connectivity.gateway_latency_ms);
        lv_label_set_text(g_router_label, buf);
    } else {
        ui_set_status_led(g_router_led, 0);
        lv_label_set_text(g_router_label, "Unreachable");
    }

    /* ── DNS ─────────────────────────────────────────────────────── */
    if (state->connectivity.dns_reachable) {
        ui_set_status_led(g_dns_led, 1);
        lv_label_set_text(g_dns_label, "Resolving OK");
    } else {
        ui_set_status_led(g_dns_led, 0);
        lv_label_set_text(g_dns_label, "Failed");
    }

    /* ── WAN Link ────────────────────────────────────────────────── */
    if (wan && wan->wan_reachable) {
        if (wan->wan_packet_loss_pct > 20 || wan->wan_jitter_ms > 50.0) {
            ui_set_status_led(g_wan_led, 2); /* yellow */
            lv_label_set_text(g_wan_label, "Degraded");
        } else {
            ui_set_status_led(g_wan_led, 1); /* green */
            lv_label_set_text(g_wan_label, "Healthy");
        }

        /* Update detail card */
        if (wan->num_hops >= 2 && wan->hops[1].reachable) {
            lv_label_set_text(g_lbl_wan_hop, wan->hops[1].ip_str);
        } else if (wan->num_hops >= 1 && wan->hops[0].reachable) {
            lv_label_set_text(g_lbl_wan_hop, wan->hops[0].ip_str);
        } else {
            lv_label_set_text(g_lbl_wan_hop, "--");
        }

        snprintf(buf, sizeof(buf), "%.0f ms", wan->wan_latency_ms);
        lv_label_set_text(g_lbl_wan_latency, buf);

        snprintf(buf, sizeof(buf), "%.1f ms", wan->wan_jitter_ms);
        lv_label_set_text(g_lbl_wan_jitter, buf);

        snprintf(buf, sizeof(buf), "%d%%", wan->wan_packet_loss_pct);
        lv_label_set_text(g_lbl_wan_loss, buf);
    } else {
        ui_set_status_led(g_wan_led, 0);
        lv_label_set_text(g_wan_label, "No upstream");
        lv_label_set_text(g_lbl_wan_hop, "--");
        lv_label_set_text(g_lbl_wan_latency, "-- ms");
        lv_label_set_text(g_lbl_wan_jitter, "-- ms");
        lv_label_set_text(g_lbl_wan_loss, "--%");
    }

    /* ── WiFi ────────────────────────────────────────────────────── */
    const wifi_mgr_state_t *wifi = wifi_mgr_get_state();
    if (wifi->iface[0] == '\0') {
        ui_set_status_led(g_wifi_led, 2); /* yellow - no iface */
        lv_label_set_text(g_wifi_label, "No adapter");
    } else if (!wifi->enabled) {
        ui_set_status_led(g_wifi_led, 0);
        lv_label_set_text(g_wifi_label, "Disabled");
    } else if (wifi->connected) {
        ui_set_status_led(g_wifi_led, 1);
        snprintf(buf, sizeof(buf), "%s", wifi->current_ssid);
        lv_label_set_text(g_wifi_label, buf);
    } else {
        ui_set_status_led(g_wifi_led, 2);
        lv_label_set_text(g_wifi_label, "Not connected");
    }

    /* ── Overall Health ──────────────────────────────────────────── */
    health_level_t health = assess_health(state, wan);
    switch (health) {
        case HEALTH_OK:
            ui_set_status_led(g_health_led, 1);
            lv_label_set_text(g_health_label, "NETWORK OK");
            lv_obj_set_style_text_color(g_health_label, COLOR_SUCCESS, 0);
            lv_label_set_text(g_health_detail,
                              "All services operational");
            break;
        case HEALTH_DEGRADED:
            ui_set_status_led(g_health_led, 2);
            lv_label_set_text(g_health_label, "DEGRADED");
            lv_obj_set_style_text_color(g_health_label, COLOR_WARNING, 0);
            if (!state->connectivity.dns_reachable)
                lv_label_set_text(g_health_detail, "DNS resolution failing");
            else if (wan && wan->wan_packet_loss_pct > 50)
                lv_label_set_text(g_health_detail, "High WAN packet loss");
            else if (state->connectivity.gateway_latency_ms > 200.0)
                lv_label_set_text(g_health_detail, "High gateway latency");
            else
                lv_label_set_text(g_health_detail, "Multiple active alerts");
            break;
        case HEALTH_DOWN:
            ui_set_status_led(g_health_led, 0);
            lv_label_set_text(g_health_label, "NETWORK DOWN");
            lv_obj_set_style_text_color(g_health_label, COLOR_DANGER, 0);
            lv_label_set_text(g_health_detail,
                              "Gateway unreachable");
            break;
    }

    /* ── Infrastructure ──────────────────────────────────────────── */
    int infra_up = 0, infra_total = 0;
    for (int i = 0; i < g_infra_count && i < state->num_infra; i++) {
        const infra_device_t *dev = &state->infra[i];
        if (!dev->enabled) continue;
        infra_total++;

        if (dev->reachable) {
            if (dev->latency_ms > 100.0)
                ui_set_status_led(g_infra_leds[i], 2);
            else
                ui_set_status_led(g_infra_leds[i], 1);
            infra_up++;
        } else {
            ui_set_status_led(g_infra_leds[i], 0);
        }
    }

    if (g_lbl_infra_summary && infra_total > 0) {
        snprintf(buf, sizeof(buf), "%d/%d devices reachable",
                 infra_up, infra_total);
        lv_label_set_text(g_lbl_infra_summary, buf);
    }

    /* ── System info ─────────────────────────────────────────────── */
    format_duration(state->uptime_sec, buf, sizeof(buf));
    lv_label_set_text(g_lbl_uptime, buf);

    if (state->mem_total_kb > 0) {
        int pct = (int)(100 - ((uint64_t)state->mem_available_kb * 100 /
                                state->mem_total_kb));
        snprintf(buf, sizeof(buf), "%d%%", pct);
        lv_label_set_text(g_lbl_mem, buf);
    }

    int alert_count = alerts_active_count();
    snprintf(buf, sizeof(buf), "%d", alert_count);
    lv_label_set_text(g_lbl_alerts, buf);
    if (alert_count > 0)
        lv_obj_set_style_text_color(g_lbl_alerts, COLOR_WARNING, 0);
    else
        lv_obj_set_style_text_color(g_lbl_alerts, COLOR_SUCCESS, 0);

    /* Update status bar badge */
    ui_manager_set_alert_count(alert_count);
}
