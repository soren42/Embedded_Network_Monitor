/**
 * @file ui_edit_widgets.c
 * @brief Reusable editing controls (IP editor, stepper) for settings screen
 */

#include "ui_edit_widgets.h"
#include "ui_theme.h"
#include "ui_layout.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── IP Editor ─────────────────────────────────────────────────────── */

/* Build a string "0\n1\n2\n...\n255" for roller options (done once) */
static char g_octet_opts[256 * 4]; /* "0\n1\n...\n255" ~1280 chars max */
static int g_octet_opts_built;

static void build_octet_options(void)
{
    if (g_octet_opts_built) return;
    int pos = 0;
    for (int i = 0; i <= 255; i++) {
        if (i > 0) {
            g_octet_opts[pos++] = '\n';
        }
        int written = snprintf(g_octet_opts + pos,
                                sizeof(g_octet_opts) - pos, "%d", i);
        pos += written;
    }
    g_octet_opts_built = 1;
}

static void style_roller(lv_obj_t *roller)
{
    lv_obj_set_style_bg_color(roller, COLOR_CARD, 0);
    lv_obj_set_style_bg_opa(roller, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(roller, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(roller, 1, 0);
    lv_obj_set_style_text_color(roller, COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_text_font(roller, ui_font_small(), 0);

    /* Selected item styling */
    lv_obj_set_style_bg_color(roller, COLOR_PRIMARY, LV_PART_SELECTED);
    lv_obj_set_style_bg_opa(roller, LV_OPA_40, LV_PART_SELECTED);
    lv_obj_set_style_text_color(roller, COLOR_TEXT_PRIMARY, LV_PART_SELECTED);
}

lv_obj_t *ui_create_ip_editor(lv_obj_t *parent, const char *initial_ip)
{
    build_octet_options();

    /* Parse initial IP */
    int octets[4] = {0, 0, 0, 0};
    if (initial_ip) {
        sscanf(initial_ip, "%d.%d.%d.%d",
               &octets[0], &octets[1], &octets[2], &octets[3]);
    }

    /* Container: row of 4 rollers with dot separators */
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_remove_style_all(cont);
    lv_obj_set_size(cont, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(cont, ui_scale(2), 0);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);

    int roller_w = ui_scale(62);
    int roller_h = ui_scale(50);

    for (int i = 0; i < 4; i++) {
        if (i > 0) {
            lv_obj_t *dot = lv_label_create(cont);
            lv_label_set_text(dot, ".");
            lv_obj_set_style_text_font(dot, ui_font_normal(), 0);
            lv_obj_set_style_text_color(dot, COLOR_TEXT_PRIMARY, 0);
        }

        lv_obj_t *roller = lv_roller_create(cont);
        lv_roller_set_options(roller, g_octet_opts, LV_ROLLER_MODE_NORMAL);
        lv_roller_set_visible_row_count(roller, 1);
        lv_obj_set_size(roller, roller_w, roller_h);
        lv_roller_set_selected(roller, octets[i], LV_ANIM_OFF);
        style_roller(roller);
    }

    return cont;
}

void ui_ip_editor_get_value(lv_obj_t *editor, char *buf, int buf_len)
{
    int octets[4] = {0, 0, 0, 0};
    int octet_idx = 0;

    /* Iterate children; rollers are the non-label children */
    uint32_t child_cnt = lv_obj_get_child_cnt(editor);
    for (uint32_t i = 0; i < child_cnt && octet_idx < 4; i++) {
        lv_obj_t *child = lv_obj_get_child(editor, i);
        /* Check if it's a roller by trying to get selected */
        if (lv_obj_check_type(child, &lv_roller_class)) {
            octets[octet_idx++] = lv_roller_get_selected(child);
        }
    }

    snprintf(buf, buf_len, "%d.%d.%d.%d",
             octets[0], octets[1], octets[2], octets[3]);
}

/* ── Stepper ───────────────────────────────────────────────────────── */

/* User data stored in the container */
typedef struct {
    int value;
    int min;
    int max;
    int step;
    char fmt[16];
    lv_obj_t *lbl_value;
} stepper_data_t;

/* We store up to 16 steppers statically */
#define MAX_STEPPERS 16
static stepper_data_t g_stepper_data[MAX_STEPPERS];
static int g_stepper_count;

static void stepper_update_label(stepper_data_t *sd)
{
    char buf[32];
    snprintf(buf, sizeof(buf), sd->fmt, sd->value);
    lv_label_set_text(sd->lbl_value, buf);
}

static void stepper_minus_cb(lv_event_t *e)
{
    stepper_data_t *sd = (stepper_data_t *)lv_event_get_user_data(e);
    sd->value -= sd->step;
    if (sd->value < sd->min) sd->value = sd->min;
    stepper_update_label(sd);
}

static void stepper_plus_cb(lv_event_t *e)
{
    stepper_data_t *sd = (stepper_data_t *)lv_event_get_user_data(e);
    sd->value += sd->step;
    if (sd->value > sd->max) sd->value = sd->max;
    stepper_update_label(sd);
}

lv_obj_t *ui_create_stepper(lv_obj_t *parent, int min, int max,
                             int step, int initial, const char *fmt)
{
    if (g_stepper_count >= MAX_STEPPERS) return NULL;

    stepper_data_t *sd = &g_stepper_data[g_stepper_count++];
    sd->value = initial;
    sd->min = min;
    sd->max = max;
    sd->step = step;
    strncpy(sd->fmt, fmt, sizeof(sd->fmt) - 1);
    sd->fmt[sizeof(sd->fmt) - 1] = '\0';

    /* Container */
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_remove_style_all(cont);
    lv_obj_set_size(cont, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(cont, ui_scale(6), 0);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);

    int btn_sz = ui_scale(36);

    /* Minus button */
    lv_obj_t *btn_minus = lv_btn_create(cont);
    lv_obj_set_size(btn_minus, btn_sz, btn_sz);
    lv_obj_set_style_bg_color(btn_minus, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_bg_opa(btn_minus, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(btn_minus, ui_scale(6), 0);
    lv_obj_set_style_shadow_width(btn_minus, 0, 0);

    lv_obj_t *minus_lbl = lv_label_create(btn_minus);
    lv_label_set_text(minus_lbl, LV_SYMBOL_MINUS);
    lv_obj_set_style_text_font(minus_lbl, ui_font_normal(), 0);
    lv_obj_set_style_text_color(minus_lbl, COLOR_TEXT_PRIMARY, 0);
    lv_obj_center(minus_lbl);

    lv_obj_add_event_cb(btn_minus, stepper_minus_cb, LV_EVENT_CLICKED, sd);

    /* Value label */
    sd->lbl_value = lv_label_create(cont);
    lv_obj_set_style_text_font(sd->lbl_value, ui_font_normal(), 0);
    lv_obj_set_style_text_color(sd->lbl_value, COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_min_width(sd->lbl_value, ui_scale(60), 0);
    lv_obj_set_style_text_align(sd->lbl_value, LV_TEXT_ALIGN_CENTER, 0);
    stepper_update_label(sd);

    /* Plus button */
    lv_obj_t *btn_plus = lv_btn_create(cont);
    lv_obj_set_size(btn_plus, btn_sz, btn_sz);
    lv_obj_set_style_bg_color(btn_plus, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_bg_opa(btn_plus, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(btn_plus, ui_scale(6), 0);
    lv_obj_set_style_shadow_width(btn_plus, 0, 0);

    lv_obj_t *plus_lbl = lv_label_create(btn_plus);
    lv_label_set_text(plus_lbl, LV_SYMBOL_PLUS);
    lv_obj_set_style_text_font(plus_lbl, ui_font_normal(), 0);
    lv_obj_set_style_text_color(plus_lbl, COLOR_TEXT_PRIMARY, 0);
    lv_obj_center(plus_lbl);

    lv_obj_add_event_cb(btn_plus, stepper_plus_cb, LV_EVENT_CLICKED, sd);

    return cont;
}

int ui_stepper_get_value(lv_obj_t *stepper)
{
    /* Find matching stepper_data by checking if the label matches */
    for (int i = 0; i < g_stepper_count; i++) {
        /* Walk children of the stepper container to find the label */
        uint32_t cnt = lv_obj_get_child_cnt(stepper);
        for (uint32_t j = 0; j < cnt; j++) {
            lv_obj_t *child = lv_obj_get_child(stepper, j);
            if (child == g_stepper_data[i].lbl_value) {
                return g_stepper_data[i].value;
            }
        }
    }
    return 0;
}
