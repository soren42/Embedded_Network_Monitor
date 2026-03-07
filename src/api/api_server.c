/**
 * @file api_server.c
 * @brief Mongoose HTTP server for REST API
 */

#include "api_server.h"
#include "api_routes.h"
#include "api_beacon.h"
#include "mongoose.h"
#include "config.h"
#include "log.h"
#include <stdio.h>

#define API_DEFAULT_PORT 8080

static struct mg_mgr s_mgr;
static int s_initialized;

static void ev_handler(struct mg_connection *c, int ev, void *ev_data)
{
    if (ev == MG_EV_HTTP_MSG) {
        struct mg_http_message *hm = (struct mg_http_message *)ev_data;
        api_routes_handle(c, hm);
    }
}

int api_server_init(config_t *cfg)
{
    int port = config_get_int(cfg, "api_port", API_DEFAULT_PORT);

    mg_mgr_init(&s_mgr);

    char url[64];
    snprintf(url, sizeof(url), "http://0.0.0.0:%d", port);

    struct mg_connection *c = mg_http_listen(&s_mgr, url, ev_handler, NULL);
    if (!c) {
        LOG_ERROR("API: failed to bind on port %d", port);
        return -1;
    }

    s_initialized = 1;
    LOG_INFO("API server listening on port %d", port);

    api_beacon_init(cfg);

    return 0;
}

void api_server_poll(void)
{
    if (!s_initialized) return;
    mg_mgr_poll(&s_mgr, 0);
    api_beacon_tick();
}

void api_server_shutdown(void)
{
    if (!s_initialized) return;
    api_beacon_shutdown();
    mg_mgr_free(&s_mgr);
    s_initialized = 0;
    LOG_INFO("API server shut down");
}
