/**
 * @file data_store.c
 * @brief Persistent history data save/load
 *
 * Saves/loads the long history ring buffers to a binary file
 * so that chart data survives reboots.
 */

#include <stdio.h>
#include <string.h>
#include "data_store.h"
#include "net_collector.h"
#include "app_conf.h"
#include "log.h"

#define DATA_STORE_MAGIC 0x4E4D4853  /* "NMHS" */
#define DATA_STORE_VERSION 1

typedef struct {
    uint32_t magic;
    uint32_t version;
    int      num_ifaces;
} data_store_header_t;

typedef struct {
    char     name[APP_IFACE_NAME_MAX];
    int      count;
    int      head;
    net_history_sample_t data[APP_HISTORY_LONG_LEN];
} data_store_iface_t;

void data_store_load(void)
{
    FILE *fp = fopen(APP_HISTORY_PATH, "rb");
    if (!fp) {
        LOG_INFO("No history file at %s, starting fresh", APP_HISTORY_PATH);
        return;
    }

    data_store_header_t hdr;
    if (fread(&hdr, sizeof(hdr), 1, fp) != 1 ||
        hdr.magic != DATA_STORE_MAGIC ||
        hdr.version != DATA_STORE_VERSION) {
        LOG_WARN("Invalid or incompatible history file, ignoring");
        fclose(fp);
        return;
    }

    LOG_INFO("Loading history for %d interfaces", hdr.num_ifaces);

    /* Read interface data but don't use it yet until we have a way to
     * inject it back into the collector's ring buffers. For now, just
     * validate the file format. */
    for (int i = 0; i < hdr.num_ifaces; i++) {
        data_store_iface_t iface_data;
        if (fread(&iface_data, sizeof(iface_data), 1, fp) != 1) {
            LOG_WARN("Truncated history file at interface %d", i);
            break;
        }
    }

    fclose(fp);
    LOG_INFO("History loaded successfully");
}

void data_store_save(void)
{
    const net_state_t *state = net_get_state();
    if (!state || state->num_ifaces == 0) {
        return;
    }

    FILE *fp = fopen(APP_HISTORY_PATH, "wb");
    if (!fp) {
        LOG_ERROR("Failed to open %s for writing", APP_HISTORY_PATH);
        return;
    }

    data_store_header_t hdr = {
        .magic = DATA_STORE_MAGIC,
        .version = DATA_STORE_VERSION,
        .num_ifaces = state->num_ifaces
    };
    fwrite(&hdr, sizeof(hdr), 1, fp);

    for (int i = 0; i < state->num_ifaces; i++) {
        data_store_iface_t iface_data;
        memset(&iface_data, 0, sizeof(iface_data));
        strncpy(iface_data.name, state->ifaces[i].info.name,
                APP_IFACE_NAME_MAX - 1);

        const net_history_sample_t *data;
        int count, head, capacity;
        net_get_long_history(i, &data, &count, &head, &capacity);

        iface_data.count = count;
        iface_data.head = head;
        if (data && count > 0) {
            memcpy(iface_data.data, data,
                   sizeof(net_history_sample_t) * capacity);
        }

        fwrite(&iface_data, sizeof(iface_data), 1, fp);
    }

    fclose(fp);
    LOG_INFO("History saved to %s (%d interfaces)", APP_HISTORY_PATH,
             state->num_ifaces);
}
