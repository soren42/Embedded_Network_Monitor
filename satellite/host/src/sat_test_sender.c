/*
 * Test sender - sends a synthetic snapshot frame to verify display
 *
 * Build: gcc -o sat_test_sender sat_test_sender.c sat_serial.c -I../../protocol
 * Usage: ./sat_test_sender /dev/ttyACM0
 */

#include "sat_serial.h"
#include "sat_protocol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <math.h>

static int build_test_frame(uint8_t *buf, size_t buf_size, uint16_t *seq,
                            int iteration)
{
    /* Build a synthetic snapshot */
    uint8_t payload[sizeof(sat_snapshot_t) + 2 * sizeof(sat_iface_data_t)];
    memset(payload, 0, sizeof(payload));

    sat_snapshot_t *snap = (sat_snapshot_t *)payload;
    snap->timestamp   = (uint32_t)time(NULL);
    snap->uptime_sec  = (uint32_t)(iteration);
    snap->mem_used_pct = 42;
    snap->num_ifaces  = 2;
    snap->conn_flags  = SAT_FLAG_GATEWAY_UP | SAT_FLAG_DNS_UP |
                        SAT_FLAG_DNS_RESOLVES | SAT_FLAG_WAN_UP;
    snap->active_alerts = 0;
    snap->max_severity  = SAT_SEVERITY_NONE;
    snap->wan_loss_pct  = 0;
    snap->gw_latency    = 12;    /* 1.2 ms */
    snap->dns_latency   = 153;   /* 15.3 ms */
    snap->wan_latency   = 251;   /* 25.1 ms */
    snap->wan_jitter    = 32;    /* 3.2 ms */

    /* Simulated varying traffic */
    double t = (double)iteration;
    uint32_t rx = (uint32_t)(500000 + 400000 * sin(t * 0.1));
    uint32_t tx = (uint32_t)(200000 + 150000 * sin(t * 0.15 + 1.0));

    sat_iface_data_t *ifaces = (sat_iface_data_t *)(payload + sizeof(sat_snapshot_t));

    /* eth0 */
    strncpy(ifaces[0].name, "eth0", SAT_IFACE_NAME_LEN);
    ifaces[0].flags      = SAT_IFLAG_UP | SAT_IFLAG_RUNNING;
    ifaces[0].rx_rate    = rx;
    ifaces[0].tx_rate    = tx;
    ifaces[0].link_speed = 1000;
    ifaces[0].signal_dbm = 0;

    /* wlan0 */
    strncpy(ifaces[1].name, "wlan0", SAT_IFACE_NAME_LEN);
    ifaces[1].flags      = SAT_IFLAG_UP | SAT_IFLAG_RUNNING | SAT_IFLAG_WIRELESS;
    ifaces[1].rx_rate    = rx / 3;
    ifaces[1].tx_rate    = tx / 4;
    ifaces[1].link_speed = 300;
    ifaces[1].signal_dbm = -52;

    uint16_t payload_len = (uint16_t)(sizeof(sat_snapshot_t) +
                           2 * sizeof(sat_iface_data_t));

    /* Build frame */
    sat_header_t *hdr = (sat_header_t *)buf;
    hdr->magic       = SAT_PROTO_MAGIC;
    hdr->version     = SAT_PROTO_VERSION;
    hdr->msg_type    = SAT_MSG_SNAPSHOT;
    hdr->payload_len = payload_len;
    hdr->sequence    = (*seq)++;

    size_t total = sizeof(sat_header_t) + payload_len + 2;
    if (total > buf_size)
        return -1;

    memcpy(buf + sizeof(sat_header_t), payload, payload_len);

    uint16_t crc = sat_crc16(buf, sizeof(sat_header_t) + payload_len);
    buf[sizeof(sat_header_t) + payload_len]     = crc & 0xFF;
    buf[sizeof(sat_header_t) + payload_len + 1] = (crc >> 8) & 0xFF;

    return (int)total;
}

int main(int argc, char *argv[])
{
    const char *dev = argc > 1 ? argv[1] : "/dev/ttyACM0";
    int count = argc > 2 ? atoi(argv[2]) : 30;

    printf("Test sender: %s, %d frames\n", dev, count);

    int fd = serial_open(dev, SAT_BAUD_RATE);
    if (fd < 0) {
        fprintf(stderr, "Failed to open %s\n", dev);
        return 1;
    }

    uint16_t seq = 0;
    uint8_t frame[SAT_MAX_FRAME_SIZE];

    for (int i = 0; i < count; i++) {
        int len = build_test_frame(frame, sizeof(frame), &seq, i);
        if (len > 0) {
            serial_write(fd, frame, (size_t)len);
            printf("  TX seq=%u len=%d\n", (unsigned)(seq - 1), len);
        }

        struct timespec ts = { .tv_sec = 1, .tv_nsec = 0 };
        nanosleep(&ts, NULL);
    }

    serial_close(fd);
    printf("Done\n");
    return 0;
}
