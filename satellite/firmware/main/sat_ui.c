/*
 * Round cockpit display UI manager
 */

#include "sat_ui.h"
#include "sat_gauges.h"
#include "sat_data.h"
#include "sat_protocol.h"

#include "lvgl.h"
#include <stdio.h>
#include <string.h>

/* Theme colors */
#define COL_BG         lv_color_hex(0x0d0d1a)
#define COL_TEXT       lv_color_hex(0xddddf0)
#define COL_TEXT_DIM   lv_color_hex(0x8888aa)
#define COL_TEXT_BRIGHT lv_color_hex(0xffffff)
#define COL_GREEN      lv_color_hex(0x00cc66)
#define COL_AMBER      lv_color_hex(0xffaa00)
#define COL_RED        lv_color_hex(0xff3333)
#define COL_GRAY       lv_color_hex(0x555577)
#define COL_PANEL      lv_color_hex(0x15152a)

/* Scale macro for resolution independence */
static int s_disp;
#define S(x) ((x) * s_disp / 360)

/* Max interfaces shown in center panel */
#define MAX_VISIBLE_IFACES 4

/* UI elements */
static lv_obj_t *s_screen;
static sat_gauge_t s_rx_gauge;
static sat_gauge_t s_tx_gauge;

/* Header */
static lv_obj_t *s_title_label;

/* Status indicator dots */
static lv_obj_t *s_dot_gw;
static lv_obj_t *s_dot_dns;
static lv_obj_t *s_dot_wan;
static lv_obj_t *s_dot_mem;
static lv_obj_t *s_label_gw;
static lv_obj_t *s_label_dns;
static lv_obj_t *s_label_wan;
static lv_obj_t *s_label_mem;

/* Center data panel */
static lv_obj_t *s_latency_labels[3];  /* GW, DNS, WAN latency */
static lv_obj_t *s_iface_labels[MAX_VISIBLE_IFACES];

/* Bottom alert bar */
static lv_obj_t *s_alert_label;

/* Connection indicator */
static lv_obj_t *s_conn_label;

/* LVGL timer for periodic updates */
static lv_timer_t *s_update_timer;

static lv_obj_t *create_status_dot(lv_obj_t *parent, int x, int y)
{
    lv_obj_t *dot = lv_obj_create(parent);
    int r = S(6);
    lv_obj_set_size(dot, r * 2, r * 2);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot, COL_GRAY, 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(dot, 0, 0);
    lv_obj_align(dot, LV_ALIGN_CENTER, x, y);
    lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
    return dot;
}

static lv_obj_t *create_label(lv_obj_t *parent, const char *text,
                              const lv_font_t *font, lv_color_t color,
                              int x, int y)
{
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, color, 0);
    lv_obj_set_style_text_font(lbl, font, 0);
    lv_obj_align(lbl, LV_ALIGN_CENTER, x, y);
    return lbl;
}

static void set_dot_color(lv_obj_t *dot, bool ok)
{
    lv_obj_set_style_bg_color(dot, ok ? COL_GREEN : COL_RED, 0);
}

static void format_rate_short(uint32_t bytes_per_sec, char *buf, int len)
{
    double bps = bytes_per_sec * 8.0;
    if (bps >= 1e9)
        snprintf(buf, len, "%.1fG", bps / 1e9);
    else if (bps >= 1e6)
        snprintf(buf, len, "%.1fM", bps / 1e6);
    else if (bps >= 1e3)
        snprintf(buf, len, "%.0fK", bps / 1e3);
    else
        snprintf(buf, len, "%.0fb", bps);
}

static void format_duration_short(uint32_t seconds, char *buf, int len)
{
    unsigned s = (unsigned)seconds;
    if (s >= 86400)
        snprintf(buf, len, "%ud %uh", s / 86400, (s % 86400) / 3600);
    else if (s >= 3600)
        snprintf(buf, len, "%uh %um", s / 3600, (s % 3600) / 60);
    else if (s >= 60)
        snprintf(buf, len, "%um %us", s / 60, s % 60);
    else
        snprintf(buf, len, "%us", s);
}

static void update_callback(lv_timer_t *timer)
{
    (void)timer;
    sat_ui_update();
}

void sat_ui_init(int disp_size)
{
    s_disp = disp_size;

    /* Create main screen with dark background */
    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, COL_BG, 0);
    lv_obj_set_style_bg_opa(s_screen, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_screen, 0, 0);
    lv_obj_clear_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);

    /* Title at top center */
    s_title_label = create_label(s_screen, "NETWORK",
                                 &lv_font_montserrat_16, COL_TEXT_BRIGHT,
                                 0, S(-155));

    /* Status indicator dots - two rows of two */
    int dot_y1 = S(-130);
    int dot_y2 = S(-110);
    int dot_x1 = S(-35);
    int dot_x2 = S(35);
    int lbl_off = S(18);

    s_dot_gw  = create_status_dot(s_screen, dot_x1, dot_y1);
    s_label_gw = create_label(s_screen, "GW", &lv_font_montserrat_10,
                              COL_TEXT_DIM, dot_x1 + lbl_off, dot_y1);

    s_dot_dns = create_status_dot(s_screen, dot_x2, dot_y1);
    s_label_dns = create_label(s_screen, "DNS", &lv_font_montserrat_10,
                               COL_TEXT_DIM, dot_x2 + lbl_off, dot_y1);

    s_dot_wan = create_status_dot(s_screen, dot_x1, dot_y2);
    s_label_wan = create_label(s_screen, "WAN", &lv_font_montserrat_10,
                               COL_TEXT_DIM, dot_x1 + lbl_off, dot_y2);

    s_dot_mem = create_status_dot(s_screen, dot_x2, dot_y2);
    s_label_mem = create_label(s_screen, "MEM", &lv_font_montserrat_10,
                               COL_TEXT_DIM, dot_x2 + lbl_off, dot_y2);

    /* Latency readouts - center section */
    const char *lat_labels[] = {"GW  --", "DNS --", "WAN --"};
    for (int i = 0; i < 3; i++) {
        s_latency_labels[i] = create_label(
            s_screen, lat_labels[i],
            &lv_font_montserrat_14, COL_TEXT,
            0, S(-80) + S(22) * i);
    }

    /* Separator line */
    lv_obj_t *sep = lv_obj_create(s_screen);
    lv_obj_set_size(sep, S(160), 1);
    lv_obj_set_style_bg_color(sep, COL_GRAY, 0);
    lv_obj_set_style_bg_opa(sep, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(sep, 0, 0);
    lv_obj_align(sep, LV_ALIGN_CENTER, 0, S(-15));
    lv_obj_clear_flag(sep, LV_OBJ_FLAG_SCROLLABLE);

    /* Interface summary lines */
    for (int i = 0; i < MAX_VISIBLE_IFACES; i++) {
        s_iface_labels[i] = create_label(
            s_screen, "",
            &lv_font_montserrat_12, COL_TEXT,
            0, S(5) + S(20) * i);
        lv_obj_add_flag(s_iface_labels[i], LV_OBJ_FLAG_HIDDEN);
    }

    /* Alert bar at bottom */
    s_alert_label = create_label(s_screen, "NO ALERTS",
                                 &lv_font_montserrat_12, COL_GREEN,
                                 0, S(100));

    /* Connection status at very bottom */
    s_conn_label = create_label(s_screen, "CONNECTING...",
                                &lv_font_montserrat_10, COL_AMBER,
                                0, S(125));

    /* Bandwidth gauge arcs */
    sat_gauge_create(&s_rx_gauge, s_screen, 0, disp_size);
    sat_gauge_create(&s_tx_gauge, s_screen, 1, disp_size);

    /* Load screen */
    lv_scr_load(s_screen);

    /* Create update timer (100ms = 10 Hz) */
    s_update_timer = lv_timer_create(update_callback, 100, NULL);
}

void sat_ui_update(void)
{
    const sat_data_t *d = sat_data_get();
    char buf[64];

    /* Connection status */
    if (!d->connected) {
        lv_label_set_text(s_conn_label, "NO LINK");
        lv_obj_set_style_text_color(s_conn_label, COL_RED, 0);

        /* Gray out gauges when disconnected */
        sat_gauge_set_value(&s_rx_gauge, 0, "--");
        sat_gauge_set_value(&s_tx_gauge, 0, "--");
        return;
    }

    /* Uptime in connection label */
    format_duration_short(d->uptime_sec, buf, sizeof(buf));
    lv_label_set_text_fmt(s_conn_label, "UP %s", buf);
    lv_obj_set_style_text_color(s_conn_label, COL_TEXT_DIM, 0);

    /* Status dots */
    set_dot_color(s_dot_gw,  d->gw_up);
    set_dot_color(s_dot_dns, d->dns_up && d->dns_resolves);
    set_dot_color(s_dot_wan, d->wan_up);
    set_dot_color(s_dot_mem, d->mem_used_pct < 90);

    /* Latency readouts */
    if (d->gw_up)
        snprintf(buf, sizeof(buf), "GW   %5.1f ms", d->gw_latency_ms);
    else
        snprintf(buf, sizeof(buf), "GW   DOWN");
    lv_label_set_text(s_latency_labels[0], buf);
    lv_obj_set_style_text_color(s_latency_labels[0],
                                d->gw_up ? COL_TEXT : COL_RED, 0);

    if (d->dns_up)
        snprintf(buf, sizeof(buf), "DNS  %5.1f ms", d->dns_latency_ms);
    else
        snprintf(buf, sizeof(buf), "DNS  DOWN");
    lv_label_set_text(s_latency_labels[1], buf);
    lv_obj_set_style_text_color(s_latency_labels[1],
                                d->dns_up ? COL_TEXT : COL_RED, 0);

    if (d->wan_up)
        snprintf(buf, sizeof(buf), "WAN  %5.1f ms", d->wan_latency_ms);
    else
        snprintf(buf, sizeof(buf), "WAN  DOWN");
    lv_label_set_text(s_latency_labels[2], buf);
    lv_obj_set_style_text_color(s_latency_labels[2],
                                d->wan_up ? COL_TEXT : COL_RED, 0);

    /* Interface summary */
    int shown = 0;
    for (int i = 0; i < d->num_ifaces && shown < MAX_VISIBLE_IFACES; i++) {
        const sat_iface_t *iface = &d->ifaces[i];
        if (!iface->up)
            continue;

        char rx_str[16], tx_str[16];
        format_rate_short(iface->rx_rate, rx_str, sizeof(rx_str));
        format_rate_short(iface->tx_rate, tx_str, sizeof(tx_str));

        /* Show interface with directional arrows for rx/tx */
        snprintf(buf, sizeof(buf), "%-6s %s%s %s%s",
                 iface->name,
                 LV_SYMBOL_DOWN, rx_str,
                 LV_SYMBOL_UP, tx_str);
        lv_label_set_text(s_iface_labels[shown], buf);
        lv_obj_clear_flag(s_iface_labels[shown], LV_OBJ_FLAG_HIDDEN);

        /* Color: green if running, amber if up but not running */
        lv_obj_set_style_text_color(s_iface_labels[shown],
                                    iface->running ? COL_TEXT : COL_AMBER, 0);
        shown++;
    }

    /* Hide unused interface lines */
    for (int i = shown; i < MAX_VISIBLE_IFACES; i++)
        lv_obj_add_flag(s_iface_labels[i], LV_OBJ_FLAG_HIDDEN);

    /* Alert bar */
    if (d->active_alerts == 0) {
        lv_label_set_text(s_alert_label, "NO ALERTS");
        lv_obj_set_style_text_color(s_alert_label, COL_GREEN, 0);
    } else {
        lv_label_set_text_fmt(s_alert_label, "%s %d ALERT%s",
                              d->max_severity >= SAT_SEVERITY_CRITICAL ?
                                  LV_SYMBOL_WARNING : LV_SYMBOL_BELL,
                              d->active_alerts,
                              d->active_alerts > 1 ? "S" : "");

        lv_color_t alert_col = COL_GREEN;
        if (d->max_severity >= SAT_SEVERITY_CRITICAL)
            alert_col = COL_RED;
        else if (d->max_severity >= SAT_SEVERITY_WARNING)
            alert_col = COL_AMBER;
        else if (d->max_severity >= SAT_SEVERITY_INFO)
            alert_col = COL_TEXT;
        lv_obj_set_style_text_color(s_alert_label, alert_col, 0);
    }

    /* Bandwidth gauges */
    uint32_t max_bps = d->max_link_bps;
    if (max_bps == 0) max_bps = 100000000u;

    /* Convert byte rates to bit rates for percentage calculation */
    uint64_t rx_bps = (uint64_t)d->total_rx_rate * 8;
    uint64_t tx_bps = (uint64_t)d->total_tx_rate * 8;
    int rx_pct = (int)(rx_bps * 100 / max_bps);
    int tx_pct = (int)(tx_bps * 100 / max_bps);

    char rx_str[16], tx_str[16];
    format_rate_short(d->total_rx_rate, rx_str, sizeof(rx_str));
    format_rate_short(d->total_tx_rate, tx_str, sizeof(tx_str));

    sat_gauge_set_value(&s_rx_gauge, rx_pct, rx_str);
    sat_gauge_set_value(&s_tx_gauge, tx_pct, tx_str);
}
