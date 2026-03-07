/**
 * @file ui_settings.c
 * @brief Interactive settings screen with IP editors, steppers, and infra config
 */

#include "ui_settings.h"
#include "ui_theme.h"
#include "ui_widgets.h"
#include "ui_layout.h"
#include "ui_edit_widgets.h"
#include "ui_manager.h"
#include "net_collector.h"
#include "net_infra.h"
#include "net_wifi_mgr.h"
#include "config.h"
#include "app_conf.h"
#include "log.h"

#include <stdio.h>
#include <string.h>

/* ── Widget references ─────────────────────────────────────────────── */

/* Network section */
static lv_obj_t *g_ip_ping;
static lv_obj_t *g_ip_dns;
static lv_obj_t *g_dd_dns_host;

/* Infrastructure devices */
static lv_obj_t *g_infra_cards[APP_MAX_INFRA_DEVICES];
static lv_obj_t *g_infra_ip_editors[APP_MAX_INFRA_DEVICES];
static lv_obj_t *g_infra_name_lbls[APP_MAX_INFRA_DEVICES];
static lv_obj_t *g_infra_type_dds[APP_MAX_INFRA_DEVICES];
static lv_obj_t *g_infra_switches[APP_MAX_INFRA_DEVICES];
static int g_infra_count;
static lv_obj_t *g_add_device_btn;

/* Alert thresholds */
static lv_obj_t *g_step_bw_warn;
static lv_obj_t *g_step_bw_crit;
static lv_obj_t *g_step_ping_warn;
static lv_obj_t *g_step_ping_crit;
static lv_obj_t *g_step_err_thresh;

/* WiFi section */
static lv_obj_t *g_wifi_switch;
static lv_obj_t *g_wifi_ssid_dd;
static lv_obj_t *g_wifi_password_ta;
static lv_obj_t *g_wifi_connect_btn;
static lv_obj_t *g_wifi_status_lbl;
static lv_obj_t *g_wifi_scan_btn;

/* Save button */
static lv_obj_t *g_save_btn;
static lv_obj_t *g_save_lbl;
static lv_timer_t *g_save_feedback_timer;

/* ── Helper: section title ─────────────────────────────────────────── */

static lv_obj_t *create_section(lv_obj_t *parent, const char *title)
{
    lv_obj_t *card = ui_create_card(parent, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(card, ui_scale(12), 0);
    lv_obj_set_style_pad_row(card, ui_scale(8), 0);

    lv_obj_t *lbl = lv_label_create(card);
    lv_label_set_text(lbl, title);
    lv_obj_set_style_text_font(lbl, ui_font_normal(), 0);
    lv_obj_set_style_text_color(lbl, COLOR_PRIMARY, 0);

    return card;
}

/* ── Helper: labeled row ───────────────────────────────────────────── */

static lv_obj_t *create_labeled_row(lv_obj_t *parent, const char *label)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl = lv_label_create(row);
    lv_label_set_text(lbl, label);
    lv_obj_set_style_text_font(lbl, ui_font_small(), 0);
    lv_obj_set_style_text_color(lbl, COLOR_TEXT_SECONDARY, 0);

    return row;
}

/* ── WiFi callbacks ───────────────────────────────────────────────── */

static void wifi_switch_cb(lv_event_t *e)
{
    (void)e;
    int enabled = lv_obj_has_state(g_wifi_switch, LV_STATE_CHECKED) ? 1 : 0;
    wifi_mgr_set_enabled(enabled);
    if (g_wifi_status_lbl) {
        lv_label_set_text(g_wifi_status_lbl,
                          enabled ? "WiFi enabled" : "WiFi disabled");
    }
}

static void wifi_scan_cb(lv_event_t *e)
{
    (void)e;
    if (g_wifi_status_lbl)
        lv_label_set_text(g_wifi_status_lbl, "Scanning...");
    wifi_mgr_scan();

    /* Populate dropdown with scan results */
    const wifi_mgr_state_t *ws = wifi_mgr_get_state();
    if (ws->num_scan_results > 0 && g_wifi_ssid_dd) {
        char opts[1024] = {0};
        int pos = 0;
        for (int i = 0; i < ws->num_scan_results && i < WIFI_MAX_NETWORKS; i++) {
            if (i > 0 && pos < (int)sizeof(opts) - 1)
                opts[pos++] = '\n';
            int remaining = (int)sizeof(opts) - pos - 1;
            if (remaining <= 0) break;
            int n = snprintf(opts + pos, remaining, "%s", ws->scan_results[i].ssid);
            if (n > 0) pos += n;
        }
        lv_dropdown_set_options(g_wifi_ssid_dd, opts);
        if (g_wifi_status_lbl) {
            char buf[64];
            snprintf(buf, sizeof(buf), "Found %d networks",
                     ws->num_scan_results);
            lv_label_set_text(g_wifi_status_lbl, buf);
        }
    } else {
        if (g_wifi_status_lbl)
            lv_label_set_text(g_wifi_status_lbl, "No networks found");
    }
}

static void wifi_connect_cb(lv_event_t *e)
{
    (void)e;
    if (!g_wifi_ssid_dd || !g_wifi_password_ta) return;

    char ssid[WIFI_SSID_MAX];
    lv_dropdown_get_selected_str(g_wifi_ssid_dd, ssid, sizeof(ssid));
    const char *pass = lv_textarea_get_text(g_wifi_password_ta);

    if (ssid[0] == '\0') return;

    if (g_wifi_status_lbl) {
        char buf[128];
        snprintf(buf, sizeof(buf), "Connecting to %s...", ssid);
        lv_label_set_text(g_wifi_status_lbl, buf);
    }

    if (wifi_mgr_connect(ssid, pass) == 0) {
        /* Save to config */
        config_t *cfg = net_get_config();
        if (cfg) {
            config_set_str(cfg, "wifi_ssid", ssid);
            config_set_str(cfg, "wifi_password", pass);
        }
    }
}

/* ── Save feedback timer ───────────────────────────────────────────── */

static void save_feedback_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    lv_obj_set_style_bg_color(g_save_btn, COLOR_PRIMARY, 0);
    lv_label_set_text(g_save_lbl, "Save");
    lv_timer_del(g_save_feedback_timer);
    g_save_feedback_timer = NULL;
}

/* ── Save button callback ──────────────────────────────────────────── */

static void save_btn_cb(lv_event_t *e)
{
    (void)e;
    config_t *cfg = net_get_config();
    if (!cfg) return;

    char buf[64];

    /* Save network settings */
    ui_ip_editor_get_value(g_ip_ping, buf, sizeof(buf));
    config_set_str(cfg, "ping_target", buf);

    ui_ip_editor_get_value(g_ip_dns, buf, sizeof(buf));
    config_set_str(cfg, "dns_target", buf);

    /* DNS hostname from dropdown */
    char dns_host[64];
    lv_dropdown_get_selected_str(g_dd_dns_host, dns_host, sizeof(dns_host));
    config_set_str(cfg, "dns_test_hostname", dns_host);

    /* Save alert thresholds */
    config_set_int(cfg, "alert_bw_warn_pct", ui_stepper_get_value(g_step_bw_warn));
    config_set_int(cfg, "alert_bw_crit_pct", ui_stepper_get_value(g_step_bw_crit));
    config_set_int(cfg, "alert_ping_warn_ms", ui_stepper_get_value(g_step_ping_warn));
    config_set_int(cfg, "alert_ping_crit_ms", ui_stepper_get_value(g_step_ping_crit));
    config_set_int(cfg, "alert_err_threshold", ui_stepper_get_value(g_step_err_thresh));

    /* Save WiFi settings */
    if (g_wifi_switch) {
        int wifi_en = lv_obj_has_state(g_wifi_switch, LV_STATE_CHECKED) ? 1 : 0;
        config_set_int(cfg, "wifi_enabled", wifi_en);
    }
    if (g_wifi_ssid_dd) {
        char ssid[WIFI_SSID_MAX];
        lv_dropdown_get_selected_str(g_wifi_ssid_dd, ssid, sizeof(ssid));
        if (ssid[0] != '\0')
            config_set_str(cfg, "wifi_ssid", ssid);
    }
    if (g_wifi_password_ta) {
        const char *pass = lv_textarea_get_text(g_wifi_password_ta);
        if (pass)
            config_set_str(cfg, "wifi_password", pass);
    }

    /* Save infrastructure devices */
    net_state_t *state = net_get_state_mut();
    config_set_int(cfg, "infra_count", g_infra_count);

    for (int i = 0; i < g_infra_count; i++) {
        char key[64];
        infra_device_t *dev = &state->infra[i];

        /* Get IP from editor */
        snprintf(key, sizeof(key), "infra_%d_ip", i);
        ui_ip_editor_get_value(g_infra_ip_editors[i], buf, sizeof(buf));
        config_set_str(cfg, key, buf);
        strncpy(dev->ip_str, buf, sizeof(dev->ip_str) - 1);

        /* Get type from dropdown */
        snprintf(key, sizeof(key), "infra_%d_type", i);
        char type_str[16];
        lv_dropdown_get_selected_str(g_infra_type_dds[i], type_str, sizeof(type_str));
        /* Convert display text to config key */
        if (strcmp(type_str, "Router") == 0)      config_set_str(cfg, key, "router");
        else if (strcmp(type_str, "Switch") == 0)  config_set_str(cfg, key, "switch");
        else if (strcmp(type_str, "AP") == 0)      config_set_str(cfg, key, "ap");
        else                                       config_set_str(cfg, key, "other");

        /* Get enabled from switch */
        dev->enabled = lv_obj_has_state(g_infra_switches[i], LV_STATE_CHECKED) ? 1 : 0;
    }
    state->num_infra = g_infra_count;

    /* Write to file */
    if (config_save(cfg, APP_CONFIG_PATH) == 0) {
        LOG_INFO("Configuration saved to %s", APP_CONFIG_PATH);
        /* Visual feedback: turn button green */
        lv_obj_set_style_bg_color(g_save_btn, COLOR_SUCCESS, 0);
        lv_label_set_text(g_save_lbl, "Saved!");
        if (g_save_feedback_timer) lv_timer_del(g_save_feedback_timer);
        g_save_feedback_timer = lv_timer_create(save_feedback_timer_cb, 1500, NULL);
        lv_timer_set_repeat_count(g_save_feedback_timer, 1);
    } else {
        LOG_ERROR("Failed to save configuration");
        lv_obj_set_style_bg_color(g_save_btn, COLOR_DANGER, 0);
        lv_label_set_text(g_save_lbl, "Error!");
        if (g_save_feedback_timer) lv_timer_del(g_save_feedback_timer);
        g_save_feedback_timer = lv_timer_create(save_feedback_timer_cb, 2000, NULL);
        lv_timer_set_repeat_count(g_save_feedback_timer, 1);
    }
}

/* ── Add device callback ───────────────────────────────────────────── */

static void create_infra_device_card(lv_obj_t *section, int idx);

static void add_device_cb(lv_event_t *e)
{
    (void)e;
    if (g_infra_count >= APP_MAX_INFRA_DEVICES) return;

    net_state_t *state = net_get_state_mut();
    infra_device_t *dev = &state->infra[g_infra_count];
    snprintf(dev->name, sizeof(dev->name), "Device %d", g_infra_count);
    dev->ip_str[0] = '\0';
    dev->type = INFRA_TYPE_OTHER;
    dev->enabled = 1;
    dev->reachable = 0;
    dev->latency_ms = 0;
    dev->consecutive_fails = 0;

    /* Get the section card (parent of add button) */
    lv_obj_t *section = lv_obj_get_parent(g_add_device_btn);
    create_infra_device_card(section, g_infra_count);
    g_infra_count++;
    state->num_infra = g_infra_count;

    /* Hide add button when max reached */
    if (g_infra_count >= APP_MAX_INFRA_DEVICES) {
        lv_obj_add_flag(g_add_device_btn, LV_OBJ_FLAG_HIDDEN);
    }
}

/* ── Create a single infra device card ─────────────────────────────── */

static void create_infra_device_card(lv_obj_t *section, int idx)
{
    const net_state_t *state = net_get_state();
    const infra_device_t *dev = &state->infra[idx];

    lv_obj_t *card = lv_obj_create(section);
    lv_obj_remove_style_all(card);
    lv_obj_set_size(card, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x1A1A3E), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, ui_scale(8), 0);
    lv_obj_set_style_pad_all(card, ui_scale(8), 0);
    lv_obj_set_style_pad_row(card, ui_scale(4), 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    g_infra_cards[idx] = card;

    /* Top row: name + type dropdown + enabled switch */
    lv_obj_t *top = lv_obj_create(card);
    lv_obj_remove_style_all(top);
    lv_obj_set_size(top, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(top, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(top, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(top, ui_scale(8), 0);
    lv_obj_clear_flag(top, LV_OBJ_FLAG_SCROLLABLE);

    g_infra_name_lbls[idx] = lv_label_create(top);
    lv_label_set_text(g_infra_name_lbls[idx], dev->name);
    lv_obj_set_style_text_font(g_infra_name_lbls[idx], ui_font_normal(), 0);
    lv_obj_set_style_text_color(g_infra_name_lbls[idx], COLOR_TEXT_PRIMARY, 0);

    /* Type dropdown */
    g_infra_type_dds[idx] = lv_dropdown_create(top);
    lv_dropdown_set_options(g_infra_type_dds[idx],
                             "Router\nSwitch\nAP\nOther");
    lv_obj_set_width(g_infra_type_dds[idx], ui_scale(100));
    lv_obj_set_style_bg_color(g_infra_type_dds[idx], COLOR_CARD, 0);
    lv_obj_set_style_bg_opa(g_infra_type_dds[idx], LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(g_infra_type_dds[idx], COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_text_font(g_infra_type_dds[idx], ui_font_small(), 0);
    lv_obj_set_style_border_color(g_infra_type_dds[idx], COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(g_infra_type_dds[idx], 1, 0);
    lv_obj_set_style_pad_all(g_infra_type_dds[idx], ui_scale(4), 0);
    lv_dropdown_set_selected(g_infra_type_dds[idx], (uint16_t)dev->type);

    /* Enabled switch */
    g_infra_switches[idx] = lv_switch_create(top);
    lv_obj_set_size(g_infra_switches[idx], ui_scale(44), ui_scale(24));
    if (dev->enabled) {
        lv_obj_add_state(g_infra_switches[idx], LV_STATE_CHECKED);
    }

    /* IP editor row */
    lv_obj_t *ip_label = lv_label_create(card);
    lv_label_set_text(ip_label, "IP Address:");
    lv_obj_set_style_text_font(ip_label, ui_font_small(), 0);
    lv_obj_set_style_text_color(ip_label, COLOR_TEXT_SECONDARY, 0);

    g_infra_ip_editors[idx] = ui_create_ip_editor(card, dev->ip_str);
}

/* ── Connections button callback ────────────────────────────────────── */

static void connections_btn_cb(lv_event_t *e)
{
    (void)e;
    ui_manager_show(SCREEN_CONNECTIONS);
}

/* ── Public API ────────────────────────────────────────────────────── */

void ui_settings_create(lv_obj_t *parent)
{
    const net_state_t *state = net_get_state();
    config_t *cfg = net_get_config();

    lv_obj_add_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    /* Section header */
    ui_create_section_header(parent, "Settings");

    /* ── Section 1: Network ───────────────────────────────────────── */
    lv_obj_t *net_sec = create_section(parent, "Network");

    lv_obj_t *ping_row = create_labeled_row(net_sec, "Ping Target");
    (void)ping_row;
    g_ip_ping = ui_create_ip_editor(net_sec,
        config_get_str(cfg, "ping_target", APP_DEFAULT_PING_TARGET));

    lv_obj_t *dns_row = create_labeled_row(net_sec, "DNS Server");
    (void)dns_row;
    g_ip_dns = ui_create_ip_editor(net_sec,
        config_get_str(cfg, "dns_target", APP_DEFAULT_DNS_TARGET));

    lv_obj_t *host_row = create_labeled_row(net_sec, "DNS Test Host");
    g_dd_dns_host = lv_dropdown_create(host_row);
    lv_dropdown_set_options(g_dd_dns_host,
        "google.com\ncloudflare.com\none.one.one.one");
    lv_obj_set_width(g_dd_dns_host, ui_scale(180));
    lv_obj_set_style_bg_color(g_dd_dns_host, COLOR_CARD, 0);
    lv_obj_set_style_bg_opa(g_dd_dns_host, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(g_dd_dns_host, COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_text_font(g_dd_dns_host, ui_font_small(), 0);
    lv_obj_set_style_border_color(g_dd_dns_host, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(g_dd_dns_host, 1, 0);
    lv_obj_set_style_pad_all(g_dd_dns_host, ui_scale(4), 0);

    /* Select current value */
    const char *cur_host = config_get_str(cfg, "dns_test_hostname",
                                           APP_DEFAULT_DNS_HOST);
    if (strcmp(cur_host, "cloudflare.com") == 0)
        lv_dropdown_set_selected(g_dd_dns_host, 1);
    else if (strcmp(cur_host, "one.one.one.one") == 0)
        lv_dropdown_set_selected(g_dd_dns_host, 2);
    else
        lv_dropdown_set_selected(g_dd_dns_host, 0);

    /* ── Section 2: WiFi ─────────────────────────────────────────── */
    {
        lv_obj_t *wifi_sec = create_section(parent, "WiFi");
        const wifi_mgr_state_t *ws = wifi_mgr_get_state();
        config_t *wifi_cfg = net_get_config();

        /* Enable/disable switch row */
        lv_obj_t *en_row = create_labeled_row(wifi_sec, "WiFi Enabled");
        g_wifi_switch = lv_switch_create(en_row);
        lv_obj_set_size(g_wifi_switch, ui_scale(44), ui_scale(24));
        int wifi_en = config_get_int(wifi_cfg, "wifi_enabled", 1);
        if (wifi_en && ws->iface[0] != '\0')
            lv_obj_add_state(g_wifi_switch, LV_STATE_CHECKED);
        lv_obj_add_event_cb(g_wifi_switch, wifi_switch_cb, LV_EVENT_VALUE_CHANGED, NULL);

        /* Status label */
        g_wifi_status_lbl = lv_label_create(wifi_sec);
        if (ws->connected) {
            char status_buf[128];
            snprintf(status_buf, sizeof(status_buf), "Connected: %s",
                     ws->current_ssid);
            lv_label_set_text(g_wifi_status_lbl, status_buf);
        } else if (ws->iface[0] != '\0') {
            lv_label_set_text(g_wifi_status_lbl, "Not connected");
        } else {
            lv_label_set_text(g_wifi_status_lbl, "No WiFi adapter found");
        }
        lv_obj_set_style_text_font(g_wifi_status_lbl, ui_font_small(), 0);
        lv_obj_set_style_text_color(g_wifi_status_lbl, COLOR_TEXT_SECONDARY, 0);

        /* SSID dropdown */
        lv_obj_t *ssid_label = lv_label_create(wifi_sec);
        lv_label_set_text(ssid_label, "Network:");
        lv_obj_set_style_text_font(ssid_label, ui_font_small(), 0);
        lv_obj_set_style_text_color(ssid_label, COLOR_TEXT_SECONDARY, 0);

        g_wifi_ssid_dd = lv_dropdown_create(wifi_sec);
        /* Pre-populate with configured SSID */
        const char *saved_ssid = config_get_str(wifi_cfg, "wifi_ssid",
                                                 APP_DEFAULT_WIFI_SSID);
        lv_dropdown_set_options(g_wifi_ssid_dd, saved_ssid);
        lv_obj_set_width(g_wifi_ssid_dd, LV_PCT(100));
        lv_obj_set_style_bg_color(g_wifi_ssid_dd, COLOR_CARD, 0);
        lv_obj_set_style_bg_opa(g_wifi_ssid_dd, LV_OPA_COVER, 0);
        lv_obj_set_style_text_color(g_wifi_ssid_dd, COLOR_TEXT_PRIMARY, 0);
        lv_obj_set_style_text_font(g_wifi_ssid_dd, ui_font_small(), 0);
        lv_obj_set_style_border_color(g_wifi_ssid_dd, COLOR_CARD_BORDER, 0);
        lv_obj_set_style_border_width(g_wifi_ssid_dd, 1, 0);
        lv_obj_set_style_pad_all(g_wifi_ssid_dd, ui_scale(4), 0);

        /* Scan button */
        g_wifi_scan_btn = lv_btn_create(wifi_sec);
        lv_obj_set_size(g_wifi_scan_btn, LV_PCT(100), ui_scale(36));
        lv_obj_set_style_bg_color(g_wifi_scan_btn, COLOR_CARD_BORDER, 0);
        lv_obj_set_style_bg_opa(g_wifi_scan_btn, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(g_wifi_scan_btn, ui_scale(8), 0);
        lv_obj_set_style_shadow_width(g_wifi_scan_btn, 0, 0);
        lv_obj_t *scan_lbl = lv_label_create(g_wifi_scan_btn);
        lv_label_set_text(scan_lbl, LV_SYMBOL_REFRESH " Scan Networks");
        lv_obj_set_style_text_font(scan_lbl, ui_font_small(), 0);
        lv_obj_set_style_text_color(scan_lbl, COLOR_TEXT_PRIMARY, 0);
        lv_obj_center(scan_lbl);
        lv_obj_add_event_cb(g_wifi_scan_btn, wifi_scan_cb, LV_EVENT_CLICKED, NULL);

        /* Password field */
        lv_obj_t *pass_label = lv_label_create(wifi_sec);
        lv_label_set_text(pass_label, "Password:");
        lv_obj_set_style_text_font(pass_label, ui_font_small(), 0);
        lv_obj_set_style_text_color(pass_label, COLOR_TEXT_SECONDARY, 0);

        g_wifi_password_ta = lv_textarea_create(wifi_sec);
        lv_obj_set_width(g_wifi_password_ta, LV_PCT(100));
        lv_textarea_set_one_line(g_wifi_password_ta, true);
        lv_textarea_set_password_mode(g_wifi_password_ta, true);
        lv_textarea_set_placeholder_text(g_wifi_password_ta, "Enter password");
        /* Pre-fill with saved password */
        const char *saved_pass = config_get_str(wifi_cfg, "wifi_password",
                                                 APP_DEFAULT_WIFI_PASSWORD);
        if (saved_pass[0] != '\0')
            lv_textarea_set_text(g_wifi_password_ta, saved_pass);
        lv_obj_set_style_bg_color(g_wifi_password_ta, COLOR_CARD, 0);
        lv_obj_set_style_bg_opa(g_wifi_password_ta, LV_OPA_COVER, 0);
        lv_obj_set_style_text_color(g_wifi_password_ta, COLOR_TEXT_PRIMARY, 0);
        lv_obj_set_style_text_font(g_wifi_password_ta, ui_font_small(), 0);
        lv_obj_set_style_border_color(g_wifi_password_ta, COLOR_CARD_BORDER, 0);
        lv_obj_set_style_border_width(g_wifi_password_ta, 1, 0);

        /* Connect button */
        g_wifi_connect_btn = lv_btn_create(wifi_sec);
        lv_obj_set_size(g_wifi_connect_btn, LV_PCT(100), ui_scale(40));
        lv_obj_set_style_bg_color(g_wifi_connect_btn, COLOR_PRIMARY, 0);
        lv_obj_set_style_bg_opa(g_wifi_connect_btn, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(g_wifi_connect_btn, ui_scale(8), 0);
        lv_obj_set_style_shadow_width(g_wifi_connect_btn, 0, 0);
        lv_obj_t *conn_lbl = lv_label_create(g_wifi_connect_btn);
        lv_label_set_text(conn_lbl, LV_SYMBOL_WIFI " Connect");
        lv_obj_set_style_text_font(conn_lbl, ui_font_small(), 0);
        lv_obj_set_style_text_color(conn_lbl, COLOR_TEXT_PRIMARY, 0);
        lv_obj_center(conn_lbl);
        lv_obj_add_event_cb(g_wifi_connect_btn, wifi_connect_cb, LV_EVENT_CLICKED, NULL);
    }

    /* ── Section 3: Infrastructure Devices ────────────────────────── */
    lv_obj_t *infra_sec = create_section(parent, "Infrastructure Devices");

    g_infra_count = state->num_infra;
    if (g_infra_count > APP_MAX_INFRA_DEVICES)
        g_infra_count = APP_MAX_INFRA_DEVICES;

    for (int i = 0; i < g_infra_count; i++) {
        create_infra_device_card(infra_sec, i);
    }

    /* Add Device button */
    g_add_device_btn = lv_btn_create(infra_sec);
    lv_obj_set_size(g_add_device_btn, LV_PCT(100), ui_scale(40));
    lv_obj_set_style_bg_color(g_add_device_btn, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_bg_opa(g_add_device_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(g_add_device_btn, ui_scale(8), 0);
    lv_obj_set_style_shadow_width(g_add_device_btn, 0, 0);

    lv_obj_t *add_lbl = lv_label_create(g_add_device_btn);
    lv_label_set_text(add_lbl, LV_SYMBOL_PLUS " Add Device");
    lv_obj_set_style_text_font(add_lbl, ui_font_small(), 0);
    lv_obj_set_style_text_color(add_lbl, COLOR_TEXT_PRIMARY, 0);
    lv_obj_center(add_lbl);

    lv_obj_add_event_cb(g_add_device_btn, add_device_cb, LV_EVENT_CLICKED, NULL);

    if (g_infra_count >= APP_MAX_INFRA_DEVICES) {
        lv_obj_add_flag(g_add_device_btn, LV_OBJ_FLAG_HIDDEN);
    }

    /* ── Section 4: Alert Thresholds ──────────────────────────────── */
    lv_obj_t *alert_sec = create_section(parent, "Alert Thresholds");

    lv_obj_t *bw_warn_row = create_labeled_row(alert_sec, "BW Warning");
    g_step_bw_warn = ui_create_stepper(bw_warn_row, 0, 100, 5,
        config_get_int(cfg, "alert_bw_warn_pct", APP_ALERT_BW_WARN_PCT), "%d%%");

    lv_obj_t *bw_crit_row = create_labeled_row(alert_sec, "BW Critical");
    g_step_bw_crit = ui_create_stepper(bw_crit_row, 0, 100, 5,
        config_get_int(cfg, "alert_bw_crit_pct", APP_ALERT_BW_CRIT_PCT), "%d%%");

    lv_obj_t *ping_warn_row = create_labeled_row(alert_sec, "Ping Warning");
    g_step_ping_warn = ui_create_stepper(ping_warn_row, 10, 1000, 10,
        config_get_int(cfg, "alert_ping_warn_ms", APP_ALERT_PING_WARN_MS), "%d ms");

    lv_obj_t *ping_crit_row = create_labeled_row(alert_sec, "Ping Critical");
    g_step_ping_crit = ui_create_stepper(ping_crit_row, 100, 5000, 100,
        config_get_int(cfg, "alert_ping_crit_ms", APP_ALERT_PING_CRIT_MS), "%d ms");

    lv_obj_t *err_row = create_labeled_row(alert_sec, "Error Threshold");
    g_step_err_thresh = ui_create_stepper(err_row, 1, 100, 1,
        config_get_int(cfg, "alert_err_threshold", APP_ALERT_ERR_THRESHOLD), "%d");

    /* ── Section 5: About ─────────────────────────────────────────── */
    lv_obj_t *about_sec = create_section(parent, "About");

    lv_obj_t *lbl_app = lv_label_create(about_sec);
    lv_label_set_text(lbl_app, "Embedded Network Monitor");
    lv_obj_set_style_text_font(lbl_app, ui_font_normal(), 0);
    lv_obj_set_style_text_color(lbl_app, COLOR_TEXT_PRIMARY, 0);

    lv_obj_t *lbl_ver = lv_label_create(about_sec);
    lv_label_set_text(lbl_ver, "Version 3.0.0");
    lv_obj_set_style_text_font(lbl_ver, ui_font_small(), 0);
    lv_obj_set_style_text_color(lbl_ver, COLOR_TEXT_SECONDARY, 0);

    lv_obj_t *lbl_host = lv_label_create(about_sec);
    lv_label_set_text(lbl_host, "neutrino.akoria.net");
    lv_obj_set_style_text_font(lbl_host, ui_font_small(), 0);
    lv_obj_set_style_text_color(lbl_host, COLOR_TEXT_SECONDARY, 0);

    /* Navigation: Connections button */
    lv_obj_t *nav_sec = create_section(parent, "Views");

    lv_obj_t *conn_btn = lv_btn_create(nav_sec);
    lv_obj_set_size(conn_btn, LV_PCT(100), ui_scale(44));
    lv_obj_set_style_bg_color(conn_btn, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_bg_opa(conn_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(conn_btn, ui_scale(8), 0);
    lv_obj_set_style_shadow_width(conn_btn, 0, 0);

    lv_obj_t *conn_lbl = lv_label_create(conn_btn);
    lv_label_set_text(conn_lbl, "Connections");
    lv_obj_set_style_text_font(conn_lbl, ui_font_normal(), 0);
    lv_obj_set_style_text_color(conn_lbl, COLOR_TEXT_PRIMARY, 0);
    lv_obj_center(conn_lbl);

    lv_obj_add_event_cb(conn_btn, connections_btn_cb, LV_EVENT_CLICKED, NULL);

    /* ── Save button ──────────────────────────────────────────────── */
    g_save_btn = lv_btn_create(parent);
    lv_obj_set_size(g_save_btn, LV_PCT(100), ui_scale(50));
    lv_obj_set_style_bg_color(g_save_btn, COLOR_PRIMARY, 0);
    lv_obj_set_style_bg_opa(g_save_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(g_save_btn, ui_scale(10), 0);
    lv_obj_set_style_shadow_width(g_save_btn, 0, 0);

    g_save_lbl = lv_label_create(g_save_btn);
    lv_label_set_text(g_save_lbl, "Save");
    lv_obj_set_style_text_font(g_save_lbl, ui_font_normal(), 0);
    lv_obj_set_style_text_color(g_save_lbl, COLOR_TEXT_PRIMARY, 0);
    lv_obj_center(g_save_lbl);

    lv_obj_add_event_cb(g_save_btn, save_btn_cb, LV_EVENT_CLICKED, NULL);
    g_save_feedback_timer = NULL;
}
