/**
 * @file api_routes.c
 * @brief REST API route handlers
 */

#include "api_routes.h"
#include "api_json.h"
#include "mongoose.h"
#include "cJSON.h"
#include "net_collector.h"
#include "config.h"
#include "alerts.h"
#include "log.h"
#include "app_conf.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/fb.h>

static void send_json(struct mg_connection *c, cJSON *json)
{
    char *str = cJSON_PrintUnformatted(json);
    mg_http_reply(c, 200,
                  "Content-Type: application/json\r\n"
                  "Access-Control-Allow-Origin: *\r\n",
                  "%s", str);
    cJSON_free(str);
    cJSON_Delete(json);
}

static void send_404(struct mg_connection *c)
{
    mg_http_reply(c, 404,
                  "Content-Type: application/json\r\n"
                  "Access-Control-Allow-Origin: *\r\n",
                  "{\"error\":\"not found\"}");
}

static void handle_get_status(struct mg_connection *c)
{
    send_json(c, api_json_status(net_get_state()));
}

static void handle_get_interfaces(struct mg_connection *c)
{
    send_json(c, api_json_interfaces(net_get_state()));
}

static void handle_get_connectivity(struct mg_connection *c)
{
    send_json(c, api_json_connectivity(net_get_state()));
}

static void handle_get_connections(struct mg_connection *c)
{
    send_json(c, api_json_connections(net_get_state()));
}

static void handle_get_alerts(struct mg_connection *c)
{
    send_json(c, api_json_alerts());
}

static void handle_get_infra(struct mg_connection *c)
{
    send_json(c, api_json_infra(net_get_state()));
}

static void handle_get_wan(struct mg_connection *c)
{
    send_json(c, api_json_wan(net_get_wan_discovery()));
}

static void handle_get_config(struct mg_connection *c)
{
    send_json(c, api_json_config(net_get_config()));
}

static void handle_post_config(struct mg_connection *c,
                                struct mg_http_message *hm)
{
    cJSON *body = cJSON_ParseWithLength(hm->body.buf, hm->body.len);
    if (!body || !cJSON_IsObject(body)) {
        mg_http_reply(c, 400,
                      "Content-Type: application/json\r\n"
                      "Access-Control-Allow-Origin: *\r\n",
                      "{\"error\":\"invalid JSON\"}");
        if (body) cJSON_Delete(body);
        return;
    }

    config_t *cfg = net_get_config();
    cJSON *item;
    cJSON_ArrayForEach(item, body) {
        if (cJSON_IsString(item) && item->string) {
            config_set_str(cfg, item->string, item->valuestring);
        }
    }
    cJSON_Delete(body);

    config_save(cfg, APP_CONFIG_PATH);

    send_json(c, api_json_config(cfg));
}

static void handle_get_screenshot(struct mg_connection *c)
{
    int fd = open(APP_FB_DEVICE, O_RDONLY);
    if (fd < 0) {
        mg_http_reply(c, 500,
                      "Content-Type: application/json\r\n"
                      "Access-Control-Allow-Origin: *\r\n",
                      "{\"error\":\"cannot open framebuffer\"}");
        return;
    }

    struct fb_var_screeninfo vinfo;
    if (ioctl(fd, FBIOGET_VSCREENINFO, &vinfo) < 0) {
        close(fd);
        mg_http_reply(c, 500,
                      "Content-Type: application/json\r\n"
                      "Access-Control-Allow-Origin: *\r\n",
                      "{\"error\":\"cannot get fb info\"}");
        return;
    }

    int w = (int)vinfo.xres;
    int h = (int)vinfo.yres;
    int bpp = (int)vinfo.bits_per_pixel;
    size_t fb_size = (size_t)(w * h * (bpp / 8));

    char *buf = (char *)malloc(fb_size);
    if (!buf) {
        close(fd);
        mg_http_reply(c, 500, "", "{\"error\":\"out of memory\"}");
        return;
    }

    lseek(fd, 0, SEEK_SET);
    size_t total = 0;
    while (total < fb_size) {
        ssize_t n = read(fd, buf + total, fb_size - total);
        if (n <= 0) break;
        total += (size_t)n;
    }
    close(fd);

    /* Build raw HTTP response for binary data */
    mg_printf(c,
              "HTTP/1.1 200 OK\r\n"
              "Content-Type: application/octet-stream\r\n"
              "Content-Length: %lu\r\n"
              "Access-Control-Allow-Origin: *\r\n"
              "X-FB-Width: %d\r\n"
              "X-FB-Height: %d\r\n"
              "X-FB-BPP: %d\r\n"
              "\r\n",
              (unsigned long)total, w, h, bpp);
    mg_send(c, buf, total);

    free(buf);
}

static void handle_get_info(struct mg_connection *c)
{
    send_json(c, api_json_info(net_get_state()));
}

void api_routes_handle(struct mg_connection *c, struct mg_http_message *hm)
{
    /* Handle CORS preflight */
    if (mg_match(hm->method, mg_str("OPTIONS"), NULL)) {
        mg_http_reply(c, 204,
                      "Access-Control-Allow-Origin: *\r\n"
                      "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                      "Access-Control-Allow-Headers: Content-Type\r\n",
                      "");
        return;
    }

    struct mg_str *uri = &hm->uri;

    if (mg_match(hm->method, mg_str("GET"), NULL)) {
        if (mg_match(*uri, mg_str("/api/v1/status"), NULL))
            handle_get_status(c);
        else if (mg_match(*uri, mg_str("/api/v1/interfaces"), NULL))
            handle_get_interfaces(c);
        else if (mg_match(*uri, mg_str("/api/v1/connectivity"), NULL))
            handle_get_connectivity(c);
        else if (mg_match(*uri, mg_str("/api/v1/connections"), NULL))
            handle_get_connections(c);
        else if (mg_match(*uri, mg_str("/api/v1/alerts"), NULL))
            handle_get_alerts(c);
        else if (mg_match(*uri, mg_str("/api/v1/infra"), NULL))
            handle_get_infra(c);
        else if (mg_match(*uri, mg_str("/api/v1/wan"), NULL))
            handle_get_wan(c);
        else if (mg_match(*uri, mg_str("/api/v1/config"), NULL))
            handle_get_config(c);
        else if (mg_match(*uri, mg_str("/api/v1/screenshot"), NULL))
            handle_get_screenshot(c);
        else if (mg_match(*uri, mg_str("/api/v1/info"), NULL))
            handle_get_info(c);
        else
            send_404(c);
    } else if (mg_match(hm->method, mg_str("POST"), NULL)) {
        if (mg_match(*uri, mg_str("/api/v1/config"), NULL))
            handle_post_config(c, hm);
        else
            send_404(c);
    } else {
        send_404(c);
    }
}
