/**
 * @file ui_layout.c
 * @brief Runtime display geometry and scaling abstraction
 */

#include "ui_layout.h"
#include "lvgl.h"

/* Baseline resolution (design target) */
#define BASELINE_RES  720

/* Cached values computed once at init */
static int g_hor_res;
static int g_ver_res;
static int g_status_bar_h;
static int g_content_y;
static int g_content_h;
static int g_content_w;
static int g_card_w_half;
static int g_pad_normal;
static int g_pad_small;

void ui_layout_init(void)
{
    lv_disp_t *disp = lv_disp_get_default();
    g_hor_res = (int)lv_disp_get_hor_res(disp);
    g_ver_res = (int)lv_disp_get_ver_res(disp);

    g_status_bar_h = g_ver_res * 7 / 100;        /* ~50px at 720 */
    if (g_status_bar_h < 30) g_status_bar_h = 30; /* minimum usable */

    g_content_y = g_status_bar_h;
    g_content_h = g_ver_res - g_status_bar_h;
    g_content_w = g_hor_res;

    /* Half-width card: (hor_res - 3*padding) / 2 */
    g_pad_normal = ui_scale(10);
    g_pad_small  = ui_scale(6);
    g_card_w_half = (g_hor_res - 3 * g_pad_normal) / 2;
}

int ui_get_hor_res(void)  { return g_hor_res; }
int ui_get_ver_res(void)  { return g_ver_res; }

int ui_scale(int px)
{
    return px * g_hor_res / BASELINE_RES;
}

int ui_scale_v(int px)
{
    return px * g_ver_res / BASELINE_RES;
}

int ui_status_bar_h(void) { return g_status_bar_h; }
int ui_content_y(void)    { return g_content_y; }
int ui_content_h(void)    { return g_content_h; }
int ui_content_w(void)    { return g_content_w; }
int ui_card_w_half(void)  { return g_card_w_half; }
int ui_pad_normal(void)   { return g_pad_normal; }
int ui_pad_small(void)    { return g_pad_small; }
