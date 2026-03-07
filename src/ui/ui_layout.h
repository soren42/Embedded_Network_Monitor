/**
 * @file ui_layout.h
 * @brief Runtime display geometry and scaling abstraction
 *
 * Provides resolution-independent layout values based on the actual
 * display dimensions.  All pixel values are scaled relative to a
 * 720×720 baseline so the UI adapts to different screen sizes.
 */

#ifndef UI_LAYOUT_H
#define UI_LAYOUT_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize layout values from the LVGL display driver.
 * Call once after hal_display_init() and lv_init().
 */
void ui_layout_init(void);

/* ── Display dimensions ────────────────────────────────────────────── */

int ui_get_hor_res(void);
int ui_get_ver_res(void);

/* ── Scaling ───────────────────────────────────────────────────────── */

/** Scale a horizontal pixel value relative to 720px baseline. */
int ui_scale(int px);

/** Scale a vertical pixel value relative to 720px baseline. */
int ui_scale_v(int px);

/* ── Common layout values (calculated at init) ─────────────────────── */

int ui_status_bar_h(void);   /* ~7% of ver_res                      */
int ui_content_y(void);      /* = status_bar_h                       */
int ui_content_h(void);      /* = ver_res - status_bar_h             */
int ui_content_w(void);      /* = hor_res                            */
int ui_card_w_half(void);    /* half-width card for dashboard grid   */
int ui_pad_normal(void);     /* standard padding (~10px at 720)      */
int ui_pad_small(void);      /* compact padding (~6px at 720)        */

#ifdef __cplusplus
}
#endif

#endif /* UI_LAYOUT_H */
