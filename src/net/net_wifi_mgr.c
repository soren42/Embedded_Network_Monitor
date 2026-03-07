/**
 * @file net_wifi_mgr.c
 * @brief WiFi network management via wpa_supplicant CLI and iw
 *
 * Controls wireless interfaces using system commands:
 * - wpa_supplicant for WPA/WPA2 authentication
 * - iw / iwlist for scanning
 * - ip link for interface up/down
 */

#include "net_wifi_mgr.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>

static wifi_mgr_state_t g_wifi_state;
static int g_initialized;

/**
 * Detect the first wireless interface by checking /sys/class/net/xxx/wireless.
 */
static int detect_wifi_iface(char *iface_buf, int buf_len)
{
    DIR *dir = opendir("/sys/class/net");
    if (!dir) return -1;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        if (strcmp(entry->d_name, "lo") == 0) continue;

        char path[256];
        snprintf(path, sizeof(path), "/sys/class/net/%s/wireless",
                 entry->d_name);

        if (access(path, F_OK) == 0) {
            snprintf(iface_buf, buf_len, "%s", entry->d_name);
            closedir(dir);
            return 0;
        }
    }

    closedir(dir);
    return -1;
}

/**
 * Run a shell command and capture stdout into buf.
 */
static int run_cmd(const char *cmd, char *buf, int buf_len)
{
    if (buf && buf_len > 0) buf[0] = '\0';

    FILE *fp = popen(cmd, "r");
    if (!fp) return -1;

    if (buf && buf_len > 0) {
        int total = 0;
        while (total < buf_len - 1) {
            int n = fread(buf + total, 1, buf_len - 1 - total, fp);
            if (n <= 0) break;
            total += n;
        }
        buf[total] = '\0';
    }

    return pclose(fp);
}

/**
 * Run a shell command, no output capture.
 */
static int run_cmd_silent(const char *cmd)
{
    return run_cmd(cmd, NULL, 0);
}

int wifi_mgr_init(void)
{
    memset(&g_wifi_state, 0, sizeof(g_wifi_state));

    if (detect_wifi_iface(g_wifi_state.iface,
                          sizeof(g_wifi_state.iface)) < 0) {
        LOG_WARN("No wireless interface detected");
        g_initialized = 0;
        return -1;
    }

    LOG_INFO("WiFi interface detected: %s", g_wifi_state.iface);
    g_wifi_state.enabled = 1;
    g_initialized = 1;

    /* Check if already connected */
    wifi_mgr_update();

    return 0;
}

const wifi_mgr_state_t *wifi_mgr_get_state(void)
{
    return &g_wifi_state;
}

const char *wifi_mgr_get_iface(void)
{
    return g_wifi_state.iface;
}

int wifi_mgr_set_enabled(int enable)
{
    if (!g_initialized) return -1;

    char cmd[128];
    if (enable) {
        snprintf(cmd, sizeof(cmd), "ip link set %s up 2>/dev/null",
                 g_wifi_state.iface);
    } else {
        snprintf(cmd, sizeof(cmd), "ip link set %s down 2>/dev/null",
                 g_wifi_state.iface);
    }

    int ret = run_cmd_silent(cmd);
    if (ret == 0) {
        g_wifi_state.enabled = enable;
        if (!enable) {
            g_wifi_state.connected = 0;
            g_wifi_state.current_ssid[0] = '\0';
        }
        LOG_INFO("WiFi %s: %s", enable ? "enabled" : "disabled",
                 g_wifi_state.iface);
    }
    return ret;
}

int wifi_mgr_scan(void)
{
    if (!g_initialized || !g_wifi_state.enabled) return -1;

    char cmd[128];
    char buf[4096];

    /* Trigger scan */
    snprintf(cmd, sizeof(cmd), "iw dev %s scan 2>/dev/null",
             g_wifi_state.iface);
    if (run_cmd(cmd, buf, sizeof(buf)) != 0) {
        /* Fallback to iwlist */
        snprintf(cmd, sizeof(cmd), "iwlist %s scan 2>/dev/null",
                 g_wifi_state.iface);
        if (run_cmd(cmd, buf, sizeof(buf)) != 0)
            return -1;
    }

    /* Parse scan results - look for SSID and signal lines */
    g_wifi_state.num_scan_results = 0;
    char *line = strtok(buf, "\n");
    wifi_scan_result_t *cur = NULL;

    while (line && g_wifi_state.num_scan_results < WIFI_MAX_NETWORKS) {
        /* iw output format */
        char *ssid_pos = strstr(line, "SSID: ");
        if (ssid_pos) {
            if (cur && cur->ssid[0] != '\0')
                g_wifi_state.num_scan_results++;
            if (g_wifi_state.num_scan_results < WIFI_MAX_NETWORKS) {
                cur = &g_wifi_state.scan_results[g_wifi_state.num_scan_results];
                memset(cur, 0, sizeof(*cur));
                ssid_pos += 6;
                strncpy(cur->ssid, ssid_pos, WIFI_SSID_MAX - 1);
                /* Trim trailing whitespace */
                int len = strlen(cur->ssid);
                while (len > 0 && (cur->ssid[len-1] == '\n' ||
                       cur->ssid[len-1] == '\r' ||
                       cur->ssid[len-1] == ' ')) {
                    cur->ssid[--len] = '\0';
                }
            }
        }

        char *signal_pos = strstr(line, "signal: ");
        if (signal_pos && cur) {
            cur->signal_dbm = atoi(signal_pos + 8);
        }

        char *freq_pos = strstr(line, "freq: ");
        if (freq_pos && cur) {
            cur->frequency_mhz = atoi(freq_pos + 6);
        }

        line = strtok(NULL, "\n");
    }

    /* Count the last entry if valid */
    if (cur && cur->ssid[0] != '\0')
        g_wifi_state.num_scan_results++;

    LOG_INFO("WiFi scan found %d networks", g_wifi_state.num_scan_results);
    return 0;
}

int wifi_mgr_connect(const char *ssid, const char *password)
{
    if (!g_initialized || !g_wifi_state.enabled) return -1;
    if (!ssid || ssid[0] == '\0') return -1;

    char cmd[512];

    /* Kill any existing wpa_supplicant for this interface */
    snprintf(cmd, sizeof(cmd),
             "killall wpa_supplicant 2>/dev/null; sleep 1");
    run_cmd_silent(cmd);

    if (password && password[0] != '\0') {
        /* WPA/WPA2 - generate config and connect */
        snprintf(cmd, sizeof(cmd),
                 "wpa_passphrase '%s' '%s' > /tmp/wpa_%s.conf 2>/dev/null",
                 ssid, password, g_wifi_state.iface);
        if (run_cmd_silent(cmd) != 0) {
            LOG_ERROR("Failed to generate wpa_supplicant config for %s", ssid);
            return -1;
        }

        snprintf(cmd, sizeof(cmd),
                 "wpa_supplicant -B -i %s -c /tmp/wpa_%s.conf 2>/dev/null",
                 g_wifi_state.iface, g_wifi_state.iface);
    } else {
        /* Open network */
        snprintf(cmd, sizeof(cmd),
                 "printf 'network={\\n  ssid=\"%s\"\\n  key_mgmt=NONE\\n}\\n'"
                 " > /tmp/wpa_%s.conf",
                 ssid, g_wifi_state.iface);
        run_cmd_silent(cmd);

        snprintf(cmd, sizeof(cmd),
                 "wpa_supplicant -B -i %s -c /tmp/wpa_%s.conf 2>/dev/null",
                 g_wifi_state.iface, g_wifi_state.iface);
    }

    if (run_cmd_silent(cmd) != 0) {
        LOG_ERROR("Failed to start wpa_supplicant for %s", ssid);
        return -1;
    }

    /* Request DHCP */
    snprintf(cmd, sizeof(cmd),
             "udhcpc -i %s -n -q 2>/dev/null &", g_wifi_state.iface);
    run_cmd_silent(cmd);

    strncpy(g_wifi_state.current_ssid, ssid,
            sizeof(g_wifi_state.current_ssid) - 1);
    g_wifi_state.connected = 1;

    LOG_INFO("WiFi connecting to %s", ssid);
    return 0;
}

int wifi_mgr_disconnect(void)
{
    if (!g_initialized) return -1;

    char cmd[128];
    snprintf(cmd, sizeof(cmd),
             "wpa_cli -i %s disconnect 2>/dev/null", g_wifi_state.iface);
    run_cmd_silent(cmd);

    g_wifi_state.connected = 0;
    g_wifi_state.current_ssid[0] = '\0';

    LOG_INFO("WiFi disconnected");
    return 0;
}

void wifi_mgr_update(void)
{
    if (!g_initialized) return;

    /* Check if interface is up */
    char path[128];
    snprintf(path, sizeof(path), "/sys/class/net/%s/operstate",
             g_wifi_state.iface);

    FILE *fp = fopen(path, "r");
    if (fp) {
        char state[32] = {0};
        if (fgets(state, sizeof(state), fp)) {
            /* Trim newline */
            int len = strlen(state);
            if (len > 0 && state[len-1] == '\n') state[len-1] = '\0';
            g_wifi_state.enabled = (strcmp(state, "down") != 0);
        }
        fclose(fp);
    }

    /* Check current SSID via iw */
    char cmd[128], buf[256];
    snprintf(cmd, sizeof(cmd),
             "iw dev %s link 2>/dev/null", g_wifi_state.iface);

    if (run_cmd(cmd, buf, sizeof(buf)) == 0) {
        if (strstr(buf, "Not connected")) {
            g_wifi_state.connected = 0;
            g_wifi_state.current_ssid[0] = '\0';
        } else {
            char *ssid_line = strstr(buf, "SSID: ");
            if (ssid_line) {
                ssid_line += 6;
                char *nl = strchr(ssid_line, '\n');
                if (nl) *nl = '\0';
                strncpy(g_wifi_state.current_ssid, ssid_line,
                        sizeof(g_wifi_state.current_ssid) - 1);
                g_wifi_state.connected = 1;
            }
        }
    }
}
