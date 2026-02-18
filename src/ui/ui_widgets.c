#include "ui_widgets.h"
#include "ui_manager.h"

/* ── Internal: back button event handler ────────────────────────────── */
static void back_btn_cb(lv_event_t *e)
{
    (void)e;
    ui_manager_back();
}

/* ── Card panel ─────────────────────────────────────────────────────── */

lv_obj_t *ui_create_card(lv_obj_t *parent, lv_coord_t w, lv_coord_t h)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_remove_style_all(card);
    lv_obj_add_style(card, (lv_style_t *)ui_get_style_card(), 0);
    lv_obj_set_size(card, w, h);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    return card;
}

/* ── Stat label ─────────────────────────────────────────────────────── */

/*
 * Layout: a small transparent container holding two labels stacked
 * vertically -- the title in small/secondary style and the value in
 * the large/primary style.
 *
 * The value label is stored as the second child (index 1) so that
 * ui_update_stat_label() can retrieve it.
 */

lv_obj_t *ui_create_stat_label(lv_obj_t *parent, const char *title)
{
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_remove_style_all(cont);
    lv_obj_set_size(cont, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(cont, 2, 0);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);

    /* Title (child 0) */
    lv_obj_t *lbl_title = lv_label_create(cont);
    lv_label_set_text(lbl_title, title);
    lv_obj_add_style(lbl_title, (lv_style_t *)ui_get_style_value_small(), 0);

    /* Value (child 1) */
    lv_obj_t *lbl_value = lv_label_create(cont);
    lv_label_set_text(lbl_value, "--");
    lv_obj_add_style(lbl_value, (lv_style_t *)ui_get_style_value_large(), 0);

    return cont;
}

void ui_update_stat_label(lv_obj_t *stat_obj, const char *value)
{
    /* The value label is the second child (index 1) */
    lv_obj_t *lbl_value = lv_obj_get_child(stat_obj, 1);
    if (lbl_value != NULL) {
        lv_label_set_text(lbl_value, value);
    }
}

/* ── Status LED ─────────────────────────────────────────────────────── */

lv_obj_t *ui_create_status_led(lv_obj_t *parent, lv_coord_t size)
{
    lv_obj_t *led = lv_obj_create(parent);
    lv_obj_remove_style_all(led);
    lv_obj_set_size(led, size, size);
    lv_obj_set_style_radius(led, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(led, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(led, COLOR_DANGER, 0);   /* default: red */
    lv_obj_clear_flag(led, LV_OBJ_FLAG_SCROLLABLE);
    return led;
}

void ui_set_status_led(lv_obj_t *led, int status)
{
    switch (status) {
        case 0:  /* red / down */
            lv_obj_set_style_bg_color(led, COLOR_DANGER, 0);
            break;
        case 1:  /* green / up */
            lv_obj_set_style_bg_color(led, COLOR_SUCCESS, 0);
            break;
        case 2:  /* yellow / warn */
            lv_obj_set_style_bg_color(led, COLOR_WARNING, 0);
            break;
        default:
            lv_obj_set_style_bg_color(led, COLOR_TEXT_SECONDARY, 0);
            break;
    }
}

/* ── Section header ─────────────────────────────────────────────────── */

lv_obj_t *ui_create_section_header(lv_obj_t *parent, const char *text)
{
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text);
    lv_obj_add_style(lbl, (lv_style_t *)ui_get_style_title(), 0);
    return lbl;
}

/* ── Back button ────────────────────────────────────────────────────── */

lv_obj_t *ui_create_back_btn(lv_obj_t *parent)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_remove_style_all(btn);
    lv_obj_set_size(btn, 48, 40);
    lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);

    /* Use a left-arrow symbol as the icon */
    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_font(lbl, ui_font_normal(), 0);
    lv_obj_set_style_text_color(lbl, COLOR_PRIMARY, 0);
    lv_obj_center(lbl);

    /* Position in top-left of parent */
    lv_obj_align(btn, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_add_event_cb(btn, back_btn_cb, LV_EVENT_CLICKED, NULL);

    return btn;
}
