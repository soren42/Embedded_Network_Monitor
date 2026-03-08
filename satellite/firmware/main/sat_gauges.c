/*
 * Cockpit-style arc gauge widgets for round displays
 */

#include "sat_gauges.h"

/* Theme colors */
#define COL_BG_DARK    lv_color_hex(0x1a1a2e)
#define COL_TRACK      lv_color_hex(0x2a2a3e)
#define COL_GREEN      lv_color_hex(0x00cc66)
#define COL_AMBER      lv_color_hex(0xffaa00)
#define COL_RED        lv_color_hex(0xff3333)
#define COL_TEXT       lv_color_hex(0xddddf0)
#define COL_TEXT_DIM   lv_color_hex(0x8888aa)

/* Scale a pixel value relative to 360px reference */
#define S(disp, x) ((x) * (disp) / 360)

/*
 * Arc geometry:
 * Left gauge:  120° to 240° (LVGL angles, 0°=3 o'clock, clockwise)
 *              This traces the left edge from ~7 o'clock to ~11 o'clock
 * Right gauge: 300° to 60° (wrapping through 0°)
 *              This traces the right edge from ~1 o'clock to ~5 o'clock
 */
#define LEFT_ARC_START   120
#define LEFT_ARC_END     240
#define RIGHT_ARC_START  300
#define RIGHT_ARC_END     60

static lv_color_t value_color(int pct)
{
    if (pct >= 85) return COL_RED;
    if (pct >= 60) return COL_AMBER;
    return COL_GREEN;
}

static lv_obj_t *create_arc_obj(lv_obj_t *parent, int disp_size,
                                int start_angle, int end_angle,
                                lv_color_t color, int arc_width,
                                bool is_indicator)
{
    lv_obj_t *arc = lv_arc_create(parent);

    /* Size: full display minus small margin so arc sits at edge */
    int margin = S(disp_size, 4);
    int size = disp_size - margin * 2;
    lv_obj_set_size(arc, size, size);
    lv_obj_center(arc);

    lv_arc_set_bg_angles(arc, start_angle, end_angle);
    lv_arc_set_range(arc, 0, 100);
    lv_arc_set_value(arc, is_indicator ? 0 : 100);

    if (is_indicator) {
        lv_arc_set_mode(arc, LV_ARC_MODE_NORMAL);
    }

    /* Style the background arc (track) */
    int w = S(disp_size, arc_width);
    lv_obj_set_style_arc_width(arc, w, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, is_indicator ? lv_color_hex(0x000000) : color,
                               LV_PART_MAIN);
    lv_obj_set_style_arc_opa(arc, is_indicator ? LV_OPA_TRANSP : LV_OPA_COVER,
                             LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_MAIN);

    /* Style the indicator arc */
    lv_obj_set_style_arc_width(arc, w, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, color, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_INDICATOR);

    /* Hide the knob */
    lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_pad_all(arc, 0, LV_PART_KNOB);

    /* Not interactive */
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);

    /* Transparent background */
    lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(arc, 0, LV_PART_MAIN);

    return arc;
}

void sat_gauge_create(sat_gauge_t *gauge, lv_obj_t *parent,
                      int side, int disp_size)
{
    int start_angle, end_angle;
    const char *title;
    int label_x_offset;

    if (side == 0) {
        /* Left = RX */
        start_angle = LEFT_ARC_START;
        end_angle   = LEFT_ARC_END;
        title = "RX";
        label_x_offset = S(disp_size, -120);
    } else {
        /* Right = TX */
        start_angle = RIGHT_ARC_START;
        end_angle   = RIGHT_ARC_END;
        title = "TX";
        label_x_offset = S(disp_size, 120);
    }

    int arc_width = 20;

    /* Background track */
    gauge->arc_bg = create_arc_obj(parent, disp_size,
                                   start_angle, end_angle,
                                   COL_TRACK, arc_width, false);

    /* Indicator (filled portion) */
    gauge->arc_fill = create_arc_obj(parent, disp_size,
                                     start_angle, end_angle,
                                     COL_GREEN, arc_width, true);

    /* For right-side arc, use REVERSE mode so it fills bottom-to-top */
    if (side == 1) {
        lv_arc_set_mode(gauge->arc_fill, LV_ARC_MODE_REVERSE);
    }

    /* Title label ("RX" / "TX") positioned near the arc midpoint */
    gauge->label_title = lv_label_create(parent);
    lv_label_set_text(gauge->label_title, title);
    lv_obj_set_style_text_color(gauge->label_title, COL_TEXT_DIM, 0);
    lv_obj_set_style_text_font(gauge->label_title,
                               &lv_font_montserrat_14, 0);
    lv_obj_align(gauge->label_title, LV_ALIGN_CENTER,
                 label_x_offset, S(disp_size, -50));

    /* Value label below title */
    gauge->label_value = lv_label_create(parent);
    lv_label_set_text(gauge->label_value, "--");
    lv_obj_set_style_text_color(gauge->label_value, COL_TEXT, 0);
    lv_obj_set_style_text_font(gauge->label_value,
                               &lv_font_montserrat_12, 0);
    lv_obj_align(gauge->label_value, LV_ALIGN_CENTER,
                 label_x_offset, S(disp_size, -30));

    gauge->value = 0;
}

void sat_gauge_set_value(sat_gauge_t *gauge, int value, const char *rate_text)
{
    if (value < 0) value = 0;
    if (value > 100) value = 100;

    gauge->value = value;
    lv_arc_set_value(gauge->arc_fill, value);

    /* Update arc color based on value */
    lv_color_t col = value_color(value);
    lv_obj_set_style_arc_color(gauge->arc_fill, col, LV_PART_INDICATOR);

    /* Update rate text */
    if (rate_text)
        lv_label_set_text(gauge->label_value, rate_text);
}
