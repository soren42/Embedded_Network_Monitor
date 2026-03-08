/*
 * USB CDC serial receiver and protocol decoder
 *
 * Runs as a FreeRTOS task that reads bytes from USB CDC, assembles frames,
 * validates CRC, and updates the shared data model.
 */

#include "sat_receiver.h"
#include "sat_data.h"
#include "sat_protocol.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tinyusb.h"
#include "tusb_cdc_acm.h"

static const char *TAG = "sat_rx";

#define RX_BUF_SIZE    512
#define TASK_STACK_SIZE 4096

typedef enum {
    RX_STATE_SYNC,
    RX_STATE_HEADER,
    RX_STATE_PAYLOAD,
    RX_STATE_CRC,
} rx_state_t;

static uint8_t s_rx_buf[RX_BUF_SIZE];
static uint8_t s_frame_buf[SAT_MAX_FRAME_SIZE];
static int s_frame_pos;
static rx_state_t s_state;
static sat_header_t s_header;

static uint32_t millis(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static void process_frame(void)
{
    /* Validate CRC */
    uint16_t expected_len = sizeof(sat_header_t) + s_header.payload_len;
    uint16_t crc_rx = s_frame_buf[expected_len] |
                      ((uint16_t)s_frame_buf[expected_len + 1] << 8);
    uint16_t crc_calc = sat_crc16(s_frame_buf, expected_len);

    if (crc_rx != crc_calc) {
        ESP_LOGW(TAG, "CRC mismatch: rx=0x%04x calc=0x%04x", crc_rx, crc_calc);
        return;
    }

    sat_data_mark_alive(millis(), s_header.sequence);

    switch (s_header.msg_type) {
    case SAT_MSG_SNAPSHOT: {
        sat_snapshot_t *snap = (sat_snapshot_t *)(s_frame_buf + sizeof(sat_header_t));
        sat_data_update_snapshot(snap);
        ESP_LOGD(TAG, "snapshot seq=%u ifaces=%d", s_header.sequence, snap->num_ifaces);
        break;
    }
    case SAT_MSG_HEARTBEAT:
        ESP_LOGD(TAG, "heartbeat seq=%u", s_header.sequence);
        break;
    default:
        ESP_LOGW(TAG, "unknown msg_type=0x%02x", s_header.msg_type);
        break;
    }
}

static void feed_byte(uint8_t b)
{
    switch (s_state) {
    case RX_STATE_SYNC:
        if (s_frame_pos == 0 && b == (SAT_PROTO_MAGIC & 0xFF)) {
            s_frame_buf[0] = b;
            s_frame_pos = 1;
        } else if (s_frame_pos == 1 && b == (SAT_PROTO_MAGIC >> 8)) {
            s_frame_buf[1] = b;
            s_frame_pos = 2;
            s_state = RX_STATE_HEADER;
        } else {
            s_frame_pos = 0;
        }
        break;

    case RX_STATE_HEADER:
        s_frame_buf[s_frame_pos++] = b;
        if (s_frame_pos >= (int)sizeof(sat_header_t)) {
            memcpy(&s_header, s_frame_buf, sizeof(sat_header_t));
            if (s_header.version != SAT_PROTO_VERSION ||
                s_header.payload_len > SAT_MAX_FRAME_SIZE - sizeof(sat_header_t) - 2) {
                ESP_LOGW(TAG, "bad header: ver=%d plen=%d",
                         s_header.version, s_header.payload_len);
                s_state = RX_STATE_SYNC;
                s_frame_pos = 0;
            } else if (s_header.payload_len == 0) {
                s_state = RX_STATE_CRC;
            } else {
                s_state = RX_STATE_PAYLOAD;
            }
        }
        break;

    case RX_STATE_PAYLOAD:
        s_frame_buf[s_frame_pos++] = b;
        if (s_frame_pos >= (int)(sizeof(sat_header_t) + s_header.payload_len)) {
            s_state = RX_STATE_CRC;
        }
        break;

    case RX_STATE_CRC:
        s_frame_buf[s_frame_pos++] = b;
        if (s_frame_pos >= (int)(sizeof(sat_header_t) + s_header.payload_len + 2)) {
            process_frame();
            s_state = RX_STATE_SYNC;
            s_frame_pos = 0;
        }
        break;
    }
}

static void receiver_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "receiver task started");

    while (1) {
        size_t rx_size = 0;
        esp_err_t ret = tinyusb_cdcacm_read(TINYUSB_CDC_ACM_0,
                                             s_rx_buf, sizeof(s_rx_buf),
                                             &rx_size);
        if (ret == ESP_OK && rx_size > 0) {
            for (size_t i = 0; i < rx_size; i++)
                feed_byte(s_rx_buf[i]);
        } else {
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        sat_data_check_connection(millis());
    }
}

void sat_receiver_init(void)
{
    /* Initialize TinyUSB */
    const tinyusb_config_t tusb_cfg = {
        .device_descriptor = NULL,
        .string_descriptor = NULL,
        .external_phy = false,
    };
    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));

    tinyusb_config_cdcacm_t acm_cfg = {
        .usb_dev = TINYUSB_CDC_ACM_0,
        .cdc_port = TINYUSB_CDC_ACM_0,
        .rx_unread_buf_sz = 256,
        .callback_rx = NULL,
        .callback_rx_wanted_char = NULL,
        .callback_line_state_changed = NULL,
        .callback_line_coding_changed = NULL,
    };
    ESP_ERROR_CHECK(tusb_cdc_acm_init(&acm_cfg));

    s_state = RX_STATE_SYNC;
    s_frame_pos = 0;

    xTaskCreate(receiver_task, "sat_rx", TASK_STACK_SIZE, NULL, 5, NULL);
}
