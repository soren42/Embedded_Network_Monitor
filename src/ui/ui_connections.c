/**
 * @file ui_connections.c
 * @brief Connections screen - active TCP/UDP connection table
 */

#include "ui_connections.h"
#include "ui_theme.h"
#include "ui_widgets.h"
#include "ui_layout.h"
#include "ui_manager.h"
#include "net_collector.h"
#include "utils.h"
#include "app_conf.h"

#include <stdio.h>
#include <string.h>

/* ── TCP state mapping (hex values from /proc/net/tcp) ──────────────── */

static const char *tcp_state_str(uint8_t state)
{
    switch (state) {
        case 0x01: return "ESTABLISHED";
        case 0x02: return "SYN_SENT";
        case 0x03: return "SYN_RECV";
        case 0x04: return "FIN_WAIT1";
        case 0x05: return "FIN_WAIT2";
        case 0x06: return "TIME_WAIT";
        case 0x07: return "CLOSE";
        case 0x08: return "CLOSE_WAIT";
        case 0x09: return "LAST_ACK";
        case 0x0A: return "LISTEN";
        case 0x0B: return "CLOSING";
        default:   return "UNKNOWN";
    }
}

/* ── Module state ───────────────────────────────────────────────────── */

static lv_obj_t *g_table;
static lv_timer_t *g_update_timer;

/* ── Table header style ─────────────────────────────────────────────── */

static void draw_event_cb(lv_event_t *e)
{
    lv_obj_draw_part_dsc_t *dsc = lv_event_get_draw_part_dsc(e);
    if (dsc->part != LV_PART_ITEMS) return;

    uint32_t row = dsc->id / lv_table_get_col_cnt(g_table);
    if (row == 0) {
        /* Header row: darker background */
        dsc->rect_dsc->bg_color = lv_color_hex(0x0F3460);
        dsc->rect_dsc->bg_opa   = LV_OPA_COVER;
    }
}

/* ── Update timer callback ─────────────────────────────────────────── */

static void update_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if (ui_manager_current() != SCREEN_CONNECTIONS) return;
    ui_connections_update();
}

/* ── Public API ────────────────────────────────────────────────────── */

void ui_connections_create(lv_obj_t *parent)
{
    /* Back button */
    ui_create_back_btn(parent);

    /* Section header */
    ui_create_section_header(parent, "Active Connections");

    /* Table */
    g_table = lv_table_create(parent);
    lv_obj_set_size(g_table, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_max_height(g_table, ui_content_h() - ui_scale(80), 0);
    lv_obj_add_flag(g_table, LV_OBJ_FLAG_SCROLLABLE);

    /* Column count and widths */
    lv_table_set_col_cnt(g_table, 4);
    lv_table_set_col_width(g_table, 0, ui_scale(80));   /* Protocol  */
    lv_table_set_col_width(g_table, 1, ui_scale(230));  /* Local     */
    lv_table_set_col_width(g_table, 2, ui_scale(230));  /* Remote    */
    lv_table_set_col_width(g_table, 3, ui_scale(140));  /* State     */

    /* Header row */
    lv_table_set_cell_value(g_table, 0, 0, "Proto");
    lv_table_set_cell_value(g_table, 0, 1, "Local");
    lv_table_set_cell_value(g_table, 0, 2, "Remote");
    lv_table_set_cell_value(g_table, 0, 3, "State");

    /* Style: dark background, light text */
    lv_obj_set_style_bg_color(g_table, COLOR_CARD, 0);
    lv_obj_set_style_bg_opa(g_table, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(g_table, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(g_table, 1, 0);
    lv_obj_set_style_radius(g_table, 8, 0);
    lv_obj_set_style_pad_all(g_table, 0, 0);

    /* Cell styling */
    lv_obj_set_style_text_color(g_table, COLOR_TEXT_PRIMARY, LV_PART_ITEMS);
    lv_obj_set_style_text_font(g_table, ui_font_small(), LV_PART_ITEMS);
    lv_obj_set_style_border_color(g_table, COLOR_CARD_BORDER, LV_PART_ITEMS);
    lv_obj_set_style_border_width(g_table, 1, LV_PART_ITEMS);
    lv_obj_set_style_border_side(g_table, LV_BORDER_SIDE_BOTTOM, LV_PART_ITEMS);
    lv_obj_set_style_pad_top(g_table, 6, LV_PART_ITEMS);
    lv_obj_set_style_pad_bottom(g_table, 6, LV_PART_ITEMS);
    lv_obj_set_style_pad_left(g_table, 6, LV_PART_ITEMS);
    lv_obj_set_style_pad_right(g_table, 6, LV_PART_ITEMS);

    /* Custom draw callback for header row styling */
    lv_obj_add_event_cb(g_table, draw_event_cb, LV_EVENT_DRAW_PART_BEGIN, NULL);

    /* Register periodic update timer */
    g_update_timer = lv_timer_create(update_timer_cb, 2000, NULL);

    /* Initial update */
    ui_connections_update();
}

void ui_connections_update(void)
{
    const net_state_t *state = net_get_state();
    int n = state->num_connections;
    char local_buf[48];
    char remote_buf[48];
    char ip_buf[20];

    /* Row 0 is the header -- data starts at row 1 */
    lv_table_set_row_cnt(g_table, (uint16_t)(n + 1));

    for (int i = 0; i < n; i++) {
        const net_connection_t *c = &state->connections[i];
        int row = i + 1;

        /* Protocol */
        lv_table_set_cell_value(g_table, row, 0,
                                c->protocol == 6 ? "TCP" : "UDP");

        /* Local Addr:Port */
        format_ip(c->local_addr, ip_buf, sizeof(ip_buf));
        snprintf(local_buf, sizeof(local_buf), "%s:%u", ip_buf, c->local_port);
        lv_table_set_cell_value(g_table, row, 1, local_buf);

        /* Remote Addr:Port */
        format_ip(c->remote_addr, ip_buf, sizeof(ip_buf));
        snprintf(remote_buf, sizeof(remote_buf), "%s:%u", ip_buf, c->remote_port);
        lv_table_set_cell_value(g_table, row, 2, remote_buf);

        /* State */
        if (c->protocol == 6) {
            lv_table_set_cell_value(g_table, row, 3, tcp_state_str(c->state));
        } else {
            lv_table_set_cell_value(g_table, row, 3, "--");
        }
    }
}
