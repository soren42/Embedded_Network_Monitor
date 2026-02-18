#include "ui_manager.h"
#include "ui_theme.h"
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

/* ── Forward declarations for nav-bar callback ──────────────────────── */
static void nav_btn_cb(lv_event_t *e);
static void clock_timer_cb(lv_timer_t *timer);

/* ── Module state ───────────────────────────────────────────────────── */
static lv_obj_t *scr_root;                  /* root screen object        */
static lv_obj_t *status_bar;                /* top 50 px                 */
static lv_obj_t *nav_bar;                   /* bottom 60 px (btnmatrix)  */
static lv_obj_t *content_area;              /* middle scrollable region  */

static lv_obj_t *lbl_title;                 /* "LP NetMon"               */
static lv_obj_t *lbl_clock;                 /* HH:MM:SS                  */
static lv_obj_t *alert_badge;              /* red circle with number     */
static lv_obj_t *alert_badge_label;        /* number inside badge        */

static lv_obj_t *screen_objs[SCREEN_COUNT]; /* cached content per screen */

static screen_id_t current_screen = SCREEN_DASHBOARD;
static screen_id_t previous_screen = SCREEN_DASHBOARD;

static int detail_iface_idx;                /* interface index for detail */

/* Button-matrix map (4 visible buttons) */
static const char *nav_map[] = {
    "Dashboard", "Traffic", "Alerts", "More", ""
};

/* ── Status bar ─────────────────────────────────────────────────────── */

static void create_status_bar(void)
{
    status_bar = lv_obj_create(scr_root);
    lv_obj_remove_style_all(status_bar);
    lv_obj_set_size(status_bar, APP_DISP_HOR_RES, APP_STATUS_BAR_H);
    lv_obj_set_pos(status_bar, 0, 0);
    lv_obj_set_style_bg_color(status_bar, COLOR_NAV_BG, 0);
    lv_obj_set_style_bg_opa(status_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_left(status_bar, 12, 0);
    lv_obj_set_style_pad_right(status_bar, 12, 0);
    lv_obj_clear_flag(status_bar, LV_OBJ_FLAG_SCROLLABLE);

    /* Left: title */
    lbl_title = lv_label_create(status_bar);
    lv_label_set_text(lbl_title, "LP NetMon");
    lv_obj_set_style_text_font(lbl_title, ui_font_normal(), 0);
    lv_obj_set_style_text_color(lbl_title, COLOR_TEXT_PRIMARY, 0);
    lv_obj_align(lbl_title, LV_ALIGN_LEFT_MID, 0, 0);

    /* Centre: clock */
    lbl_clock = lv_label_create(status_bar);
    lv_label_set_text(lbl_clock, "00:00:00");
    lv_obj_set_style_text_font(lbl_clock, ui_font_normal(), 0);
    lv_obj_set_style_text_color(lbl_clock, COLOR_TEXT_PRIMARY, 0);
    lv_obj_align(lbl_clock, LV_ALIGN_CENTER, 0, 0);

    /* Right: alert badge (hidden by default) */
    alert_badge = lv_obj_create(status_bar);
    lv_obj_remove_style_all(alert_badge);
    lv_obj_set_size(alert_badge, 28, 28);
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

/* ── Navigation bar ─────────────────────────────────────────────────── */

static void create_nav_bar(void)
{
    nav_bar = lv_btnmatrix_create(scr_root);
    lv_btnmatrix_set_map(nav_bar, nav_map);
    lv_obj_set_size(nav_bar, APP_DISP_HOR_RES, APP_NAV_BAR_H);
    lv_obj_set_pos(nav_bar, 0, APP_DISP_VER_RES - APP_NAV_BAR_H);

    /* Remove default styles, then apply dark theme */
    lv_obj_remove_style_all(nav_bar);
    lv_obj_set_style_bg_color(nav_bar, COLOR_NAV_BG, 0);
    lv_obj_set_style_bg_opa(nav_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(nav_bar, 0, 0);
    lv_obj_set_style_pad_all(nav_bar, 4, 0);
    lv_obj_set_style_pad_gap(nav_bar, 2, 0);

    /* Button items: normal state */
    lv_obj_set_style_bg_opa(nav_bar, LV_OPA_TRANSP, LV_PART_ITEMS);
    lv_obj_set_style_text_color(nav_bar, COLOR_TEXT_SECONDARY, LV_PART_ITEMS);
    lv_obj_set_style_text_font(nav_bar, ui_font_small(), LV_PART_ITEMS);
    lv_obj_set_style_border_width(nav_bar, 0, LV_PART_ITEMS);
    lv_obj_set_style_shadow_width(nav_bar, 0, LV_PART_ITEMS);
    lv_obj_set_style_radius(nav_bar, 8, LV_PART_ITEMS);

    /* Checked (active) state */
    lv_obj_set_style_text_color(nav_bar, COLOR_PRIMARY, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_bg_opa(nav_bar, LV_OPA_20, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(nav_bar, COLOR_PRIMARY, LV_PART_ITEMS | LV_STATE_CHECKED);

    /* Allow one-button check */
    lv_btnmatrix_set_btn_ctrl_all(nav_bar, LV_BTNMATRIX_CTRL_CHECKABLE);
    lv_btnmatrix_set_one_checked(nav_bar, true);
    lv_btnmatrix_set_btn_ctrl(nav_bar, 0, LV_BTNMATRIX_CTRL_CHECKED);

    lv_obj_add_event_cb(nav_bar, nav_btn_cb, LV_EVENT_VALUE_CHANGED, NULL);
}

/* ── Content area ───────────────────────────────────────────────────── */

static void create_content_area(void)
{
    content_area = lv_obj_create(scr_root);
    lv_obj_remove_style_all(content_area);
    lv_obj_set_size(content_area, APP_DISP_HOR_RES, APP_CONTENT_H);
    lv_obj_set_pos(content_area, 0, APP_CONTENT_Y);
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

static void nav_btn_cb(lv_event_t *e)
{
    lv_obj_t *btnm = lv_event_get_target(e);
    uint32_t id = lv_btnmatrix_get_selected_btn(btnm);

    /* Map btnmatrix index to screen_id_t */
    screen_id_t target;
    switch (id) {
        case 0:  target = SCREEN_DASHBOARD; break;
        case 1:  target = SCREEN_TRAFFIC;   break;
        case 2:  target = SCREEN_ALERTS;    break;
        case 3:  target = SCREEN_MORE;      break;
        default: return;
    }

    ui_manager_show(target);
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

/**
 * Ensure a container exists for the given screen inside content_area.
 * Each screen gets a full-size child of content_area that individual
 * screen builders can populate.
 */
static lv_obj_t *ensure_screen_obj(screen_id_t id)
{
    if (screen_objs[id] == NULL) {
        screen_objs[id] = lv_obj_create(content_area);
        lv_obj_remove_style_all(screen_objs[id]);
        lv_obj_set_size(screen_objs[id], APP_DISP_HOR_RES, APP_CONTENT_H);
        lv_obj_set_pos(screen_objs[id], 0, 0);
        lv_obj_set_style_bg_opa(screen_objs[id], LV_OPA_TRANSP, 0);
        lv_obj_set_style_pad_all(screen_objs[id], 8, 0);
        lv_obj_set_flex_flow(screen_objs[id], LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(screen_objs[id],
                              LV_FLEX_ALIGN_START,
                              LV_FLEX_ALIGN_START,
                              LV_FLEX_ALIGN_START);

        /* Call the screen-specific builder */
        switch (id) {
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

/* ── Highlight the correct nav button ───────────────────────────────── */

static void sync_nav_highlight(screen_id_t id)
{
    /* Only the first 4 screens have corresponding nav buttons */
    uint32_t btn_idx;
    switch (id) {
        case SCREEN_DASHBOARD:  btn_idx = 0; break;
        case SCREEN_TRAFFIC:    btn_idx = 1; break;
        case SCREEN_ALERTS:     btn_idx = 2; break;
        case SCREEN_MORE:       btn_idx = 3; break;
        default:
            /* Sub-screens (detail, connections, settings) keep the
               parent tab highlighted -- no change needed. */
            return;
    }

    /* Uncheck all, then check the target */
    for (uint32_t i = 0; i < 4; i++) {
        lv_btnmatrix_clear_btn_ctrl(nav_bar, i, LV_BTNMATRIX_CTRL_CHECKED);
    }
    lv_btnmatrix_set_btn_ctrl(nav_bar, btn_idx, LV_BTNMATRIX_CTRL_CHECKED);
}

/* ── Public API ─────────────────────────────────────────────────────── */

void ui_manager_init(void)
{
    memset(screen_objs, 0, sizeof(screen_objs));

    /* Create the root screen and apply the background style */
    scr_root = lv_obj_create(NULL);
    lv_obj_add_style(scr_root, (lv_style_t *)ui_get_style_screen(), 0);
    lv_obj_clear_flag(scr_root, LV_OBJ_FLAG_SCROLLABLE);

    create_status_bar();
    create_content_area();
    create_nav_bar();

    /* Load the root screen */
    lv_scr_load(scr_root);

    /* Show the dashboard by default */
    ui_manager_show(SCREEN_DASHBOARD);
}

void ui_manager_show(screen_id_t screen)
{
    if (screen >= SCREEN_COUNT) return;

    previous_screen = current_screen;
    current_screen  = screen;

    hide_all_screens();
    lv_obj_t *obj = ensure_screen_obj(screen);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);

    sync_nav_highlight(screen);
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
