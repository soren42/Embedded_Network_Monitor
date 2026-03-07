#include "ui_manager.h"
#include "ui_theme.h"
#include "ui_layout.h"
#include "ui_status.h"
#include "ui_dashboard.h"
#include "ui_interface.h"
#include "ui_traffic.h"
#include "ui_alerts.h"
#include "ui_connections.h"
#include "ui_settings.h"
#include "app_conf.h"

#include <stdio.h>
#include <time.h>
#include <string.h>

/* ── Forward declarations ────────────────────────────────────────────── */
static void clock_timer_cb(lv_timer_t *timer);
static void hamburger_btn_cb(lv_event_t *e);
static void menu_item_cb(lv_event_t *e);
static void backdrop_cb(lv_event_t *e);

/* ── Module state ───────────────────────────────────────────────────── */
static lv_obj_t *scr_root;                  /* root screen object        */
static lv_obj_t *status_bar;                /* top status bar            */
static lv_obj_t *content_area;              /* main scrollable region    */

static lv_obj_t *lbl_title;                 /* "EmbedMon"                */
static lv_obj_t *lbl_clock;                 /* HH:MM:SS                  */
static lv_obj_t *alert_badge;              /* red circle with number     */
static lv_obj_t *alert_badge_label;        /* number inside badge        */

/* Hamburger menu overlay */
static lv_obj_t *menu_backdrop;            /* semi-transparent overlay   */
static lv_obj_t *menu_panel;               /* slide-in side panel        */
static int menu_open;                      /* 0 = closed, 1 = open      */

static lv_obj_t *screen_objs[SCREEN_COUNT]; /* cached content per screen */

static screen_id_t current_screen = SCREEN_STATUS;
static screen_id_t previous_screen = SCREEN_STATUS;

static int detail_iface_idx;                /* interface index for detail */

/* ── Menu items ──────────────────────────────────────────────────────── */
typedef struct {
    const char *label;
    const char *icon;
    screen_id_t screen;
} menu_entry_t;

static const menu_entry_t menu_entries[] = {
    { "Status",      LV_SYMBOL_EYE_OPEN, SCREEN_STATUS      },
    { "Interfaces",  LV_SYMBOL_HOME,     SCREEN_DASHBOARD   },
    { "Traffic",     LV_SYMBOL_CHARGE,   SCREEN_TRAFFIC     },
    { "Alerts",      LV_SYMBOL_BELL,     SCREEN_ALERTS      },
    { "Connections", LV_SYMBOL_LIST,     SCREEN_CONNECTIONS  },
    { "Settings",    LV_SYMBOL_SETTINGS, SCREEN_SETTINGS     },
};
#define MENU_ENTRY_COUNT (sizeof(menu_entries) / sizeof(menu_entries[0]))

/* ── Status bar ─────────────────────────────────────────────────────── */

static void create_status_bar(void)
{
    int sb_h = ui_status_bar_h();

    status_bar = lv_obj_create(scr_root);
    lv_obj_remove_style_all(status_bar);
    lv_obj_set_size(status_bar, ui_get_hor_res(), sb_h);
    lv_obj_set_pos(status_bar, 0, 0);
    lv_obj_set_style_bg_color(status_bar, COLOR_NAV_BG, 0);
    lv_obj_set_style_bg_opa(status_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_left(status_bar, ui_scale(8), 0);
    lv_obj_set_style_pad_right(status_bar, ui_scale(8), 0);
    lv_obj_clear_flag(status_bar, LV_OBJ_FLAG_SCROLLABLE);

    /* Left: hamburger button */
    lv_obj_t *ham_btn = lv_btn_create(status_bar);
    lv_obj_remove_style_all(ham_btn);
    lv_obj_set_size(ham_btn, ui_scale(44), sb_h - ui_scale(4));
    lv_obj_set_style_bg_opa(ham_btn, LV_OPA_TRANSP, 0);
    lv_obj_align(ham_btn, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t *ham_lbl = lv_label_create(ham_btn);
    lv_label_set_text(ham_lbl, LV_SYMBOL_LIST);
    lv_obj_set_style_text_font(ham_lbl, ui_font_normal(), 0);
    lv_obj_set_style_text_color(ham_lbl, COLOR_TEXT_PRIMARY, 0);
    lv_obj_center(ham_lbl);

    lv_obj_add_event_cb(ham_btn, hamburger_btn_cb, LV_EVENT_CLICKED, NULL);

    /* Centre: title */
    lbl_title = lv_label_create(status_bar);
    lv_label_set_text(lbl_title, "EmbedMon");
    lv_obj_set_style_text_font(lbl_title, ui_font_normal(), 0);
    lv_obj_set_style_text_color(lbl_title, COLOR_TEXT_PRIMARY, 0);
    lv_obj_align(lbl_title, LV_ALIGN_CENTER, 0, 0);

    /* Right side: clock then alert badge */
    lbl_clock = lv_label_create(status_bar);
    lv_label_set_text(lbl_clock, "00:00:00");
    lv_obj_set_style_text_font(lbl_clock, ui_font_small(), 0);
    lv_obj_set_style_text_color(lbl_clock, COLOR_TEXT_PRIMARY, 0);
    lv_obj_align(lbl_clock, LV_ALIGN_RIGHT_MID, -ui_scale(36), 0);

    /* Alert badge */
    int badge_sz = ui_scale(26);
    alert_badge = lv_obj_create(status_bar);
    lv_obj_remove_style_all(alert_badge);
    lv_obj_set_size(alert_badge, badge_sz, badge_sz);
    lv_obj_set_style_bg_color(alert_badge, COLOR_DANGER, 0);
    lv_obj_set_style_bg_opa(alert_badge, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(alert_badge, LV_RADIUS_CIRCLE, 0);
    lv_obj_align(alert_badge, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_clear_flag(alert_badge, LV_OBJ_FLAG_SCROLLABLE);

    alert_badge_label = lv_label_create(alert_badge);
    lv_label_set_text(alert_badge_label, "0");
    lv_obj_set_style_text_font(alert_badge_label, ui_font_small(), 0);
    lv_obj_set_style_text_color(alert_badge_label, COLOR_TEXT_PRIMARY, 0);
    lv_obj_center(alert_badge_label);

    lv_obj_add_flag(alert_badge, LV_OBJ_FLAG_HIDDEN);

    /* 1-second timer to update the clock */
    lv_timer_create(clock_timer_cb, 1000, NULL);
}

/* ── Hamburger menu overlay ──────────────────────────────────────────── */

static void create_menu_overlay(void)
{
    int hor = ui_get_hor_res();
    int ver = ui_get_ver_res();
    int panel_w = hor * 60 / 100;  /* 60% of screen width */

    /* Backdrop: full-screen semi-transparent black */
    menu_backdrop = lv_obj_create(scr_root);
    lv_obj_remove_style_all(menu_backdrop);
    lv_obj_set_size(menu_backdrop, hor, ver);
    lv_obj_set_pos(menu_backdrop, 0, 0);
    lv_obj_set_style_bg_color(menu_backdrop, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(menu_backdrop, LV_OPA_50, 0);
    lv_obj_clear_flag(menu_backdrop, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(menu_backdrop, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(menu_backdrop, backdrop_cb, LV_EVENT_CLICKED, NULL);

    /* Side panel: left-aligned */
    menu_panel = lv_obj_create(menu_backdrop);
    lv_obj_remove_style_all(menu_panel);
    lv_obj_set_size(menu_panel, panel_w, ver);
    lv_obj_set_pos(menu_panel, 0, 0);
    lv_obj_set_style_bg_color(menu_panel, COLOR_NAV_BG, 0);
    lv_obj_set_style_bg_opa(menu_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_top(menu_panel, ui_status_bar_h() + ui_scale(16), 0);
    lv_obj_set_style_pad_left(menu_panel, ui_scale(16), 0);
    lv_obj_set_style_pad_right(menu_panel, ui_scale(16), 0);
    lv_obj_set_style_pad_row(menu_panel, ui_scale(4), 0);
    lv_obj_set_flex_flow(menu_panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(menu_panel, LV_OBJ_FLAG_SCROLLABLE);

    /* Menu title */
    lv_obj_t *menu_title = lv_label_create(menu_panel);
    lv_label_set_text(menu_title, "Navigation");
    lv_obj_set_style_text_font(menu_title, ui_font_small(), 0);
    lv_obj_set_style_text_color(menu_title, COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_pad_bottom(menu_title, ui_scale(8), 0);

    /* Menu buttons */
    for (int i = 0; i < (int)MENU_ENTRY_COUNT; i++) {
        lv_obj_t *btn = lv_btn_create(menu_panel);
        lv_obj_set_size(btn, LV_PCT(100), ui_scale(48));
        lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
        lv_obj_set_style_bg_color(btn, COLOR_PRIMARY, LV_STATE_PRESSED);
        lv_obj_set_style_bg_opa(btn, LV_OPA_20, LV_STATE_PRESSED);
        lv_obj_set_style_radius(btn, ui_scale(8), 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);
        lv_obj_set_style_pad_left(btn, ui_scale(8), 0);

        lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_START,
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(btn, ui_scale(12), 0);

        lv_obj_t *icon = lv_label_create(btn);
        lv_label_set_text(icon, menu_entries[i].icon);
        lv_obj_set_style_text_font(icon, ui_font_normal(), 0);
        lv_obj_set_style_text_color(icon, COLOR_PRIMARY, 0);

        lv_obj_t *label = lv_label_create(btn);
        lv_label_set_text(label, menu_entries[i].label);
        lv_obj_set_style_text_font(label, ui_font_normal(), 0);
        lv_obj_set_style_text_color(label, COLOR_TEXT_PRIMARY, 0);

        lv_obj_add_event_cb(btn, menu_item_cb, LV_EVENT_CLICKED,
                             (void *)(intptr_t)i);
    }

    /* Start hidden */
    lv_obj_add_flag(menu_backdrop, LV_OBJ_FLAG_HIDDEN);
    menu_open = 0;
}

static void menu_show(void)
{
    lv_obj_clear_flag(menu_backdrop, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(menu_backdrop);
    menu_open = 1;
}

static void menu_hide(void)
{
    lv_obj_add_flag(menu_backdrop, LV_OBJ_FLAG_HIDDEN);
    menu_open = 0;
}

/* ── Content area ───────────────────────────────────────────────────── */

static void create_content_area(void)
{
    content_area = lv_obj_create(scr_root);
    lv_obj_remove_style_all(content_area);
    lv_obj_set_size(content_area, ui_content_w(), ui_content_h());
    lv_obj_set_pos(content_area, 0, ui_content_y());
    lv_obj_set_style_bg_color(content_area, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(content_area, LV_OPA_COVER, 0);
    lv_obj_clear_flag(content_area, LV_OBJ_FLAG_SCROLLABLE);
}

/* ── Callbacks ──────────────────────────────────────────────────────── */

static void clock_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d",
             tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
    lv_label_set_text(lbl_clock, buf);
}

static void hamburger_btn_cb(lv_event_t *e)
{
    (void)e;
    if (menu_open) {
        menu_hide();
    } else {
        menu_show();
    }
}

static void menu_item_cb(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx >= 0 && idx < (int)MENU_ENTRY_COUNT) {
        menu_hide();
        ui_manager_show(menu_entries[idx].screen);
    }
}

static void backdrop_cb(lv_event_t *e)
{
    /* Only close if the backdrop itself was tapped, not the panel */
    lv_obj_t *target = lv_event_get_target(e);
    if (target == menu_backdrop) {
        menu_hide();
    }
}

/* ── Hide / show helpers ────────────────────────────────────────────── */

static void hide_all_screens(void)
{
    for (int i = 0; i < SCREEN_COUNT; i++) {
        if (screen_objs[i] != NULL) {
            lv_obj_add_flag(screen_objs[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static lv_obj_t *ensure_screen_obj(screen_id_t id)
{
    if (screen_objs[id] == NULL) {
        screen_objs[id] = lv_obj_create(content_area);
        lv_obj_remove_style_all(screen_objs[id]);
        lv_obj_set_size(screen_objs[id], ui_content_w(), ui_content_h());
        lv_obj_set_pos(screen_objs[id], 0, 0);
        lv_obj_set_style_bg_opa(screen_objs[id], LV_OPA_TRANSP, 0);
        lv_obj_set_style_pad_all(screen_objs[id], ui_pad_normal(), 0);
        lv_obj_set_flex_flow(screen_objs[id], LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(screen_objs[id],
                              LV_FLEX_ALIGN_START,
                              LV_FLEX_ALIGN_START,
                              LV_FLEX_ALIGN_START);

        /* Call the screen-specific builder */
        switch (id) {
            case SCREEN_STATUS:       ui_status_create(screen_objs[id]);       break;
            case SCREEN_DASHBOARD:    ui_dashboard_create(screen_objs[id]);    break;
            case SCREEN_IFACE_DETAIL: ui_interface_create(screen_objs[id]);    break;
            case SCREEN_TRAFFIC:      ui_traffic_create(screen_objs[id]);      break;
            case SCREEN_ALERTS:       ui_alerts_create(screen_objs[id]);       break;
            case SCREEN_CONNECTIONS:  ui_connections_create(screen_objs[id]);   break;
            case SCREEN_SETTINGS:     ui_settings_create(screen_objs[id]);     break;
            case SCREEN_MORE:         ui_settings_create(screen_objs[id]);     break;
            default: break;
        }
    }
    return screen_objs[id];
}

/* ── Public API ─────────────────────────────────────────────────────── */

void ui_manager_init(void)
{
    memset(screen_objs, 0, sizeof(screen_objs));

    scr_root = lv_obj_create(NULL);
    lv_obj_add_style(scr_root, (lv_style_t *)ui_get_style_screen(), 0);
    lv_obj_clear_flag(scr_root, LV_OBJ_FLAG_SCROLLABLE);

    create_status_bar();
    create_content_area();
    create_menu_overlay();

    lv_scr_load(scr_root);

    ui_manager_show(SCREEN_STATUS);
}

void ui_manager_show(screen_id_t screen)
{
    if (screen >= SCREEN_COUNT) return;

    previous_screen = current_screen;
    current_screen  = screen;

    hide_all_screens();
    lv_obj_t *obj = ensure_screen_obj(screen);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

void ui_manager_show_iface_detail(int iface_idx)
{
    detail_iface_idx = iface_idx;
    ui_interface_set_iface(iface_idx);
    ui_manager_show(SCREEN_IFACE_DETAIL);
}

void ui_manager_back(void)
{
    ui_manager_show(previous_screen);
}

screen_id_t ui_manager_current(void)
{
    return current_screen;
}

void ui_manager_set_alert_count(int count)
{
    if (count <= 0) {
        lv_obj_add_flag(alert_badge, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    char buf[8];
    snprintf(buf, sizeof(buf), "%d", count);
    lv_label_set_text(alert_badge_label, buf);
    lv_obj_clear_flag(alert_badge, LV_OBJ_FLAG_HIDDEN);
}

lv_obj_t *ui_manager_get_content(void)
{
    return content_area;
}
