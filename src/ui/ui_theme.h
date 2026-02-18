#ifndef UI_THEME_H
#define UI_THEME_H

#include "lvgl.h"

/* ── Colour palette ─────────────────────────────────────────────────── */
#define COLOR_BG              lv_color_hex(0x1A1A2E)
#define COLOR_CARD            lv_color_hex(0x16213E)
#define COLOR_CARD_BORDER     lv_color_hex(0x0F3460)
#define COLOR_PRIMARY         lv_color_hex(0x00B4D8)
#define COLOR_SUCCESS         lv_color_hex(0x2ECC71)
#define COLOR_WARNING         lv_color_hex(0xF39C12)
#define COLOR_DANGER          lv_color_hex(0xE74C3C)
#define COLOR_TEXT_PRIMARY    lv_color_hex(0xE8E8E8)
#define COLOR_TEXT_SECONDARY  lv_color_hex(0x8892A0)
#define COLOR_NAV_BG          lv_color_hex(0x0F3460)
#define COLOR_CHART_RX        lv_color_hex(0x3498DB)
#define COLOR_CHART_TX        lv_color_hex(0x2ECC71)

/* ── Public API ─────────────────────────────────────────────────────── */

/**
 * Initialise the global theme styles.  Must be called once after
 * lv_init() and before any UI objects are created.
 */
void ui_theme_init(void);

/* Style getters (return pointers to static styles) */
const lv_style_t *ui_get_style_screen(void);
const lv_style_t *ui_get_style_card(void);
const lv_style_t *ui_get_style_title(void);
const lv_style_t *ui_get_style_value_large(void);
const lv_style_t *ui_get_style_value_small(void);
const lv_style_t *ui_get_style_nav_btn(void);
const lv_style_t *ui_get_style_nav_active(void);
const lv_style_t *ui_get_style_status_ok(void);
const lv_style_t *ui_get_style_status_warn(void);
const lv_style_t *ui_get_style_status_err(void);

/* Font convenience getters */
const lv_font_t *ui_font_title(void);   /* Montserrat 28 */
const lv_font_t *ui_font_normal(void);  /* Montserrat 20 */
const lv_font_t *ui_font_small(void);   /* Montserrat 14 */

#endif /* UI_THEME_H */
