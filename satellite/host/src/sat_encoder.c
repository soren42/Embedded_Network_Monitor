/*
 * Protocol encoder - converts JSON API response to binary satellite frames
 */

#include "sat_encoder.h"
#include "../../protocol/sat_protocol.h"
#include "cJSON.h"

#include <string.h>
#include <time.h>

static int build_frame(uint8_t msg_type, const uint8_t *payload,
                       uint16_t payload_len, uint8_t *buf,
                       size_t buf_size, uint16_t *seq)
{
    size_t total = sizeof(sat_header_t) + payload_len + 2;
    if (total > buf_size)
        return -1;

    sat_header_t *hdr = (sat_header_t *)buf;
    hdr->magic       = SAT_PROTO_MAGIC;
    hdr->version     = SAT_PROTO_VERSION;
    hdr->msg_type    = msg_type;
    hdr->payload_len = payload_len;
    hdr->sequence    = (*seq)++;

    if (payload_len > 0)
        memcpy(buf + sizeof(sat_header_t), payload, payload_len);

    uint16_t crc = sat_crc16(buf, sizeof(sat_header_t) + payload_len);
    buf[sizeof(sat_header_t) + payload_len]     = crc & 0xFF;
    buf[sizeof(sat_header_t) + payload_len + 1] = (crc >> 8) & 0xFF;

    return (int)total;
}

int encoder_json_to_frame(const cJSON *root, uint8_t *buf,
                          size_t buf_size, uint16_t *seq)
{
    /* Parse JSON structure matching /api/v1/status response */
    const cJSON *info = cJSON_GetObjectItem(root, "info");
    const cJSON *ifaces_arr = cJSON_GetObjectItem(root, "interfaces");
    const cJSON *conn = cJSON_GetObjectItem(root, "connectivity");
    const cJSON *alerts_obj = cJSON_GetObjectItem(root, "alerts");
    const cJSON *wan = cJSON_GetObjectItem(root, "wan");

    int num_ifaces = cJSON_GetArraySize(ifaces_arr);
    if (num_ifaces > SAT_MAX_INTERFACES)
        num_ifaces = SAT_MAX_INTERFACES;

    uint16_t payload_len = (uint16_t)(sizeof(sat_snapshot_t) +
                           (size_t)num_ifaces * sizeof(sat_iface_data_t));

    /* Working buffer for payload */
    uint8_t payload[sizeof(sat_snapshot_t) + SAT_MAX_INTERFACES * sizeof(sat_iface_data_t)];
    memset(payload, 0, sizeof(payload));

    sat_snapshot_t *snap = (sat_snapshot_t *)payload;

    /* Info section */
    if (info) {
        const cJSON *j;
        j = cJSON_GetObjectItem(info, "uptime_sec");
        if (j) snap->uptime_sec = (uint32_t)j->valuedouble;

        j = cJSON_GetObjectItem(info, "mem_total_kb");
        uint32_t mem_total = j ? (uint32_t)j->valuedouble : 1;
        j = cJSON_GetObjectItem(info, "mem_available_kb");
        uint32_t mem_avail = j ? (uint32_t)j->valuedouble : 0;
        if (mem_total > 0)
            snap->mem_used_pct = (uint8_t)(100 - (mem_avail * 100 / mem_total));
    }

    snap->timestamp = (uint32_t)time(NULL);
    snap->num_ifaces = (uint8_t)num_ifaces;

    /* Connectivity */
    if (conn) {
        const cJSON *j;
        uint8_t flags = 0;

        j = cJSON_GetObjectItem(conn, "gateway_reachable");
        if (j && cJSON_IsTrue(j)) flags |= SAT_FLAG_GATEWAY_UP;

        j = cJSON_GetObjectItem(conn, "dns_reachable");
        if (j && cJSON_IsTrue(j)) flags |= SAT_FLAG_DNS_UP;

        j = cJSON_GetObjectItem(conn, "dns_resolves");
        if (j && cJSON_IsTrue(j)) flags |= SAT_FLAG_DNS_RESOLVES;

        snap->conn_flags = flags;

        j = cJSON_GetObjectItem(conn, "gateway_latency_ms");
        if (j) snap->gw_latency = (uint16_t)(j->valuedouble * 10.0);

        j = cJSON_GetObjectItem(conn, "dns_latency_ms");
        if (j) snap->dns_latency = (uint16_t)(j->valuedouble * 10.0);
    }

    /* WAN */
    if (wan) {
        const cJSON *j;

        j = cJSON_GetObjectItem(wan, "wan_reachable");
        if (j && cJSON_IsTrue(j)) snap->conn_flags |= SAT_FLAG_WAN_UP;

        j = cJSON_GetObjectItem(wan, "wan_latency_ms");
        if (j) snap->wan_latency = (uint16_t)(j->valuedouble * 10.0);

        j = cJSON_GetObjectItem(wan, "wan_jitter_ms");
        if (j) snap->wan_jitter = (uint16_t)(j->valuedouble * 10.0);

        j = cJSON_GetObjectItem(wan, "wan_packet_loss_pct");
        if (j) snap->wan_loss_pct = (uint8_t)j->valuedouble;
    }

    /* Alerts */
    if (alerts_obj) {
        const cJSON *j;
        j = cJSON_GetObjectItem(alerts_obj, "active_count");
        if (j) snap->active_alerts = (uint8_t)j->valuedouble;

        /* Find max severity among active alerts */
        uint8_t max_sev = SAT_SEVERITY_NONE;
        const cJSON *entries = cJSON_GetObjectItem(alerts_obj, "entries");
        const cJSON *entry;
        cJSON_ArrayForEach(entry, entries) {
            const cJSON *active = cJSON_GetObjectItem(entry, "active");
            if (active && cJSON_IsTrue(active)) {
                const cJSON *sev = cJSON_GetObjectItem(entry, "severity");
                if (sev && (uint8_t)sev->valuedouble > max_sev)
                    max_sev = (uint8_t)sev->valuedouble;
            }
        }
        snap->max_severity = max_sev;
    }

    /* Interfaces */
    sat_iface_data_t *iface_out = (sat_iface_data_t *)(payload + sizeof(sat_snapshot_t));
    int idx = 0;
    const cJSON *iface;
    cJSON_ArrayForEach(iface, ifaces_arr) {
        if (idx >= num_ifaces)
            break;

        sat_iface_data_t *out = &iface_out[idx];
        memset(out, 0, sizeof(*out));

        const cJSON *j;
        j = cJSON_GetObjectItem(iface, "name");
        if (j && j->valuestring) {
            strncpy(out->name, j->valuestring, SAT_IFACE_NAME_LEN - 1);
        }

        uint8_t flags = 0;
        j = cJSON_GetObjectItem(iface, "up");
        if (j && cJSON_IsTrue(j)) flags |= SAT_IFLAG_UP;
        j = cJSON_GetObjectItem(iface, "running");
        if (j && cJSON_IsTrue(j)) flags |= SAT_IFLAG_RUNNING;
        j = cJSON_GetObjectItem(iface, "wireless");
        if (j && cJSON_IsTrue(j)) flags |= SAT_IFLAG_WIRELESS;
        out->flags = flags;

        j = cJSON_GetObjectItem(iface, "speed_mbps");
        if (j) out->link_speed = (uint16_t)j->valuedouble;

        const cJSON *rates = cJSON_GetObjectItem(iface, "rates");
        if (rates) {
            j = cJSON_GetObjectItem(rates, "rx_bps");
            if (j) out->rx_rate = (uint32_t)j->valuedouble;
            j = cJSON_GetObjectItem(rates, "tx_bps");
            if (j) out->tx_rate = (uint32_t)j->valuedouble;
        }

        const cJSON *wifi = cJSON_GetObjectItem(iface, "wifi");
        if (wifi && !cJSON_IsNull(wifi)) {
            j = cJSON_GetObjectItem(wifi, "signal_dbm");
            if (j) out->signal_dbm = (int8_t)j->valuedouble;
        }

        idx++;
    }

    return build_frame(SAT_MSG_SNAPSHOT, payload, payload_len, buf, buf_size, seq);
}

int encoder_heartbeat_frame(uint8_t *buf, size_t buf_size, uint16_t *seq)
{
    sat_heartbeat_t hb;
    hb.timestamp = (uint32_t)time(NULL);
    return build_frame(SAT_MSG_HEARTBEAT, (uint8_t *)&hb, sizeof(hb),
                       buf, buf_size, seq);
}
