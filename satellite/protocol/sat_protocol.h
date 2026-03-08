/*
 * Satellite Display Protocol
 * Embedded Network Monitor - Satellite Subsystem
 *
 * Binary protocol for streaming network state snapshots from a host
 * (running the Embedded Network Monitor) to satellite display devices
 * over USB CDC serial.
 *
 * Frame format:
 *   [header 8 bytes][payload N bytes][crc16 2 bytes]
 *
 * All multi-byte values are little-endian.
 */

#ifndef SAT_PROTOCOL_H
#define SAT_PROTOCOL_H

#include <stdint.h>

#define SAT_PROTO_MAGIC        0x4E4D  /* "NM" */
#define SAT_PROTO_VERSION      1

#define SAT_MAX_INTERFACES     8
#define SAT_IFACE_NAME_LEN    8
#define SAT_BAUD_RATE          115200
#define SAT_UPDATE_INTERVAL_MS 1000

#define SAT_MAX_FRAME_SIZE     256

/* Message types */
typedef enum {
    SAT_MSG_SNAPSHOT    = 0x01,
    SAT_MSG_HEARTBEAT   = 0x02,
    SAT_MSG_ALERT       = 0x03,
    SAT_MSG_ACK         = 0x80,
    SAT_MSG_NACK        = 0x81,
} sat_msg_type_t;

/* Connectivity flags (bitfield) */
#define SAT_FLAG_GATEWAY_UP    (1 << 0)
#define SAT_FLAG_DNS_UP        (1 << 1)
#define SAT_FLAG_DNS_RESOLVES  (1 << 2)
#define SAT_FLAG_WAN_UP        (1 << 3)

/* Interface flags (bitfield) */
#define SAT_IFLAG_UP           (1 << 0)
#define SAT_IFLAG_RUNNING      (1 << 1)
#define SAT_IFLAG_WIRELESS     (1 << 2)

/* Alert severity */
typedef enum {
    SAT_SEVERITY_NONE     = 0,
    SAT_SEVERITY_INFO     = 1,
    SAT_SEVERITY_WARNING  = 2,
    SAT_SEVERITY_CRITICAL = 3,
} sat_severity_t;

/* Protocol header (8 bytes) */
typedef struct __attribute__((packed)) {
    uint16_t magic;
    uint8_t  version;
    uint8_t  msg_type;
    uint16_t payload_len;
    uint16_t sequence;
} sat_header_t;

/* Per-interface data (20 bytes each) */
typedef struct __attribute__((packed)) {
    char     name[SAT_IFACE_NAME_LEN];
    uint8_t  flags;
    uint8_t  error_rate;     /* errors/sec, capped at 255 */
    uint32_t rx_rate;        /* bytes/sec */
    uint32_t tx_rate;        /* bytes/sec */
    uint16_t link_speed;     /* Mbps */
    int8_t   signal_dbm;     /* WiFi signal or 0 */
    uint8_t  _reserved;
} sat_iface_data_t;

/* Snapshot payload (18 + 20*N bytes) */
typedef struct __attribute__((packed)) {
    uint32_t timestamp;
    uint32_t uptime_sec;
    uint8_t  mem_used_pct;    /* 0-100 */
    uint8_t  num_ifaces;
    uint8_t  conn_flags;      /* SAT_FLAG_* */
    uint8_t  active_alerts;
    uint8_t  max_severity;    /* sat_severity_t */
    uint8_t  wan_loss_pct;    /* 0-100 */
    uint16_t gw_latency;      /* ms * 10 */
    uint16_t dns_latency;     /* ms * 10 */
    uint16_t wan_latency;     /* ms * 10 */
    uint16_t wan_jitter;      /* ms * 10 */
    sat_iface_data_t ifaces[];
} sat_snapshot_t;

/* Heartbeat payload (4 bytes) */
typedef struct __attribute__((packed)) {
    uint32_t timestamp;
} sat_heartbeat_t;

/* CRC-16/CCITT (polynomial 0x1021, init 0xFFFF) */
static inline uint16_t sat_crc16(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }
    return crc;
}

/*
 * Frame layout:
 *   Offset  Size  Field
 *   0       8     sat_header_t
 *   8       N     payload (sat_snapshot_t, sat_heartbeat_t, etc.)
 *   8+N     2     CRC-16 over bytes 0..(8+N-1)
 *
 * Total frame size for snapshot with K interfaces:
 *   8 + (18 + 20*K) + 2 = 28 + 20*K
 *   K=1: 48 bytes,  K=4: 108 bytes,  K=8: 188 bytes
 */

#endif /* SAT_PROTOCOL_H */
