/*
 * Round cockpit display UI manager
 *
 * Layout inspired by glass cockpit Primary Flight Displays:
 *
 *          ╭────────────────╮
 *        ╱     NETWORK       ╲
 *      ╱   ● GW  ● DNS       ╲
 *     │    ● WAN  ● MEM        │
 *   R │                        │ T
 *   X │    GW   1.2 ms         │ X
 *   ▓ │    DNS 15.3 ms         │ ▓
 *   ▓ │    WAN 25.1 ms         │ ▓
 *   ▓ │                        │ ▓
 *     │    eth0     ↓1.5M      │
 *      ╲   wlan0    ↓800K    ╱
 *        ╲   ⚠ 0 ALERTS   ╱
 *          ╰────────────────╯
 *
 * Left/right arcs are bandwidth utilization gauges.
 */

#ifndef SAT_UI_H
#define SAT_UI_H

/* Initialize the UI (call after LVGL and display are initialized) */
void sat_ui_init(int disp_size);

/* Update the UI from current data model (call periodically, ~10 Hz) */
void sat_ui_update(void);

#endif /* SAT_UI_H */
