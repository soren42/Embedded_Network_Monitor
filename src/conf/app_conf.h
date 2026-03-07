#ifndef APP_CONF_H
#define APP_CONF_H

/* Display */
#define APP_DISP_HOR_RES        720
#define APP_DISP_VER_RES        720
#define APP_FB_DEVICE           "/dev/fb0"
#define APP_TOUCH_DEVICE        "/dev/input/event0"

/* Timing (milliseconds) */
#define APP_TICK_PERIOD_MS      5
#define APP_FAST_TIMER_MS       1000
#define APP_MEDIUM_TIMER_MS     5000
#define APP_SLOW_TIMER_MS       30000
#define APP_UI_REFRESH_MS       500

/* Data limits */
#define APP_MAX_INTERFACES      8
#define APP_MAX_CONNECTIONS     256
#define APP_MAX_ALERTS          64
#define APP_HISTORY_SHORT_LEN   60      /* 60 seconds at 1s resolution */
#define APP_HISTORY_LONG_LEN    1440    /* 24 hours at 1-min resolution */
#define APP_MAX_ARP_ENTRIES     64
#define APP_MAX_INFRA_DEVICES   8

/* Network */
#define APP_IFACE_NAME_MAX      16
#define APP_DEFAULT_PING_TARGET "10.0.0.1"
#define APP_DEFAULT_DNS_TARGET  "8.8.8.8"
#define APP_DEFAULT_DNS_HOST    "google.com"

/* WiFi defaults */
#define APP_DEFAULT_WIFI_SSID       "solarian"
#define APP_DEFAULT_WIFI_PASSWORD   "S0l@r1@n"

/* Alert defaults */
#define APP_ALERT_BW_WARN_PCT   80
#define APP_ALERT_BW_CRIT_PCT   95
#define APP_ALERT_PING_WARN_MS  100
#define APP_ALERT_PING_CRIT_MS  500
#define APP_ALERT_ERR_THRESHOLD 10
#define APP_ALERT_HYSTERESIS    3

/* Paths */
#define APP_CONFIG_PATH         "/etc/netmon.conf"
#define APP_HISTORY_PATH        "/tmp/netmon_history.dat"

/* UI Layout (compile-time defaults, used by HAL framebuffer init) */
#define APP_STATUS_BAR_H        50
#define APP_CONTENT_Y           APP_STATUS_BAR_H
#define APP_CONTENT_H           (APP_DISP_VER_RES - APP_STATUS_BAR_H)

#endif /* APP_CONF_H */
