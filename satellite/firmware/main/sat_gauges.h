/*
 * Cockpit-style arc gauge widgets for round displays
 *
 * Creates semicircular bar graph indicators that hug the left and right
 * curved edges of the circular display, inspired by glass cockpit
 * instruments in modern jet aircraft.
 *
 * Left arc:  RX bandwidth utilization (fills bottom-to-top)
 * Right arc: TX bandwidth utilization (fills bottom-to-top)
 *
 * Color coding: green (0-60%) -> amber (60-85%) -> red (85-100%)
 */

#ifndef SAT_GAUGES_H
#define SAT_GAUGES_H

#include "lvgl.h"

typedef struct {
    lv_obj_t *arc_bg;       /* Background arc (track) */
    lv_obj_t *arc_fill;     /* Filled indicator arc */
    lv_obj_t *label_title;  /* "RX" or "TX" label */
    lv_obj_t *label_value;  /* Rate value text */
    int       value;        /* Current 0-100 */
} sat_gauge_t;

/* Create a gauge arc on the given parent (should be full-screen).
 * side: 0 = left (RX), 1 = right (TX)
 * disp_size: display resolution (e.g. 240, 360, 412) */
void sat_gauge_create(sat_gauge_t *gauge, lv_obj_t *parent,
                      int side, int disp_size);

/* Update gauge value (0-100) and rate text */
void sat_gauge_set_value(sat_gauge_t *gauge, int value, const char *rate_text);

#endif /* SAT_GAUGES_H */
