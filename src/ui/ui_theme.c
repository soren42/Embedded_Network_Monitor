#include "ui_theme.h"
#include "ui_layout.h"

/* ── Static style objects ───────────────────────────────────────────── */
static lv_style_t style_screen;
static lv_style_t style_card;
static lv_style_t style_title;
static lv_style_t style_value_large;
static lv_style_t style_value_small;
static lv_style_t style_nav_btn;
static lv_style_t style_nav_active;
static lv_style_t style_status_ok;
static lv_style_t style_status_warn;
static lv_style_t style_status_err;

/* Font tier (selected at init based on display size) */
static const lv_font_t *g_font_title;
static const lv_font_t *g_font_normal;
static const lv_font_t *g_font_small;

/* ── Initialisation ─────────────────────────────────────────────────── */

void ui_theme_init(void)
{
    /* Select font tier based on shorter screen dimension */
    int min_dim = ui_get_hor_res();
    int vr = ui_get_ver_res();
    if (vr < min_dim) min_dim = vr;

    if (min_dim <= 400) {
        g_font_small  = &lv_font_montserrat_10;
        g_font_normal = &lv_font_montserrat_14;
        g_font_title  = &lv_font_montserrat_18;
    } else if (min_dim <= 600) {
        g_font_small  = &lv_font_montserrat_12;
        g_font_normal = &lv_font_montserrat_16;
        g_font_title  = &lv_font_montserrat_22;
    } else {
        g_font_small  = &lv_font_montserrat_14;
        g_font_normal = &lv_font_montserrat_20;
        g_font_title  = &lv_font_montserrat_28;
    }

    /* Screen background */
    lv_style_init(&style_screen);
    lv_style_set_bg_color(&style_screen, COLOR_BG);
    lv_style_set_bg_opa(&style_screen, LV_OPA_COVER);

    /* Card / panel */
    lv_style_init(&style_card);
    lv_style_set_bg_color(&style_card, COLOR_CARD);
    lv_style_set_bg_opa(&style_card, LV_OPA_COVER);
    lv_style_set_radius(&style_card, 12);
    lv_style_set_border_color(&style_card, COLOR_CARD_BORDER);
    lv_style_set_border_width(&style_card, 1);
    lv_style_set_pad_all(&style_card, ui_pad_normal());

    /* Title label */
    lv_style_init(&style_title);
    lv_style_set_text_font(&style_title, g_font_title);
    lv_style_set_text_color(&style_title, COLOR_TEXT_PRIMARY);

    /* Large value label */
    lv_style_init(&style_value_large);
    lv_style_set_text_font(&style_value_large, g_font_normal);
    lv_style_set_text_color(&style_value_large, COLOR_TEXT_PRIMARY);

    /* Small / secondary value label */
    lv_style_init(&style_value_small);
    lv_style_set_text_font(&style_value_small, g_font_small);
    lv_style_set_text_color(&style_value_small, COLOR_TEXT_SECONDARY);

    /* Nav-bar button (inactive) */
    lv_style_init(&style_nav_btn);
    lv_style_set_bg_opa(&style_nav_btn, LV_OPA_TRANSP);
    lv_style_set_text_color(&style_nav_btn, COLOR_TEXT_SECONDARY);

    /* Nav-bar button (active) */
    lv_style_init(&style_nav_active);
    lv_style_set_text_color(&style_nav_active, COLOR_PRIMARY);

    /* Status LED - OK / green */
    lv_style_init(&style_status_ok);
    lv_style_set_bg_color(&style_status_ok, COLOR_SUCCESS);
    lv_style_set_bg_opa(&style_status_ok, LV_OPA_COVER);

    /* Status LED - warning / amber */
    lv_style_init(&style_status_warn);
    lv_style_set_bg_color(&style_status_warn, COLOR_WARNING);
    lv_style_set_bg_opa(&style_status_warn, LV_OPA_COVER);

    /* Status LED - error / red */
    lv_style_init(&style_status_err);
    lv_style_set_bg_color(&style_status_err, COLOR_DANGER);
    lv_style_set_bg_opa(&style_status_err, LV_OPA_COVER);
}

/* ── Style getters ──────────────────────────────────────────────────── */

const lv_style_t *ui_get_style_screen(void)      { return &style_screen;      }
const lv_style_t *ui_get_style_card(void)         { return &style_card;        }
const lv_style_t *ui_get_style_title(void)        { return &style_title;       }
const lv_style_t *ui_get_style_value_large(void)  { return &style_value_large; }
const lv_style_t *ui_get_style_value_small(void)  { return &style_value_small; }
const lv_style_t *ui_get_style_nav_btn(void)      { return &style_nav_btn;     }
const lv_style_t *ui_get_style_nav_active(void)   { return &style_nav_active;  }
const lv_style_t *ui_get_style_status_ok(void)    { return &style_status_ok;   }
const lv_style_t *ui_get_style_status_warn(void)  { return &style_status_warn; }
const lv_style_t *ui_get_style_status_err(void)   { return &style_status_err;  }

/* ── Font getters ───────────────────────────────────────────────────── */

const lv_font_t *ui_font_title(void)  { return g_font_title;  }
const lv_font_t *ui_font_normal(void) { return g_font_normal; }
const lv_font_t *ui_font_small(void)  { return g_font_small;  }
