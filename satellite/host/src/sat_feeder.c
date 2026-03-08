/*
 * Satellite Display Feeder
 * Embedded Network Monitor - Host-side daemon
 *
 * Fetches network state from the Embedded Network Monitor REST API,
 * encodes it as a compact binary frame, and streams it to a satellite
 * display device over USB serial.
 *
 * Usage: sat_feeder [-d /dev/ttyACM0] [-u http://localhost:8080] [-b 115200]
 */

/* Feature macros set via CMake compile options */

#include "sat_serial.h"
#include "sat_encoder.h"
#include "../../protocol/sat_protocol.h"
#include "cJSON.h"

#include <errno.h>
#include <getopt.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define DEFAULT_SERIAL   "/dev/ttyACM0"
#define DEFAULT_API_URL  "http://localhost:8080"
#define DEFAULT_BAUD     SAT_BAUD_RATE
#define HTTP_BUF_SIZE    8192
#define MAX_CONSECUTIVE_FAILURES 10

static volatile int g_running = 1;

static void signal_handler(int sig)
{
    (void)sig;
    g_running = 0;
}

/* Minimal HTTP GET - returns malloc'd response body or NULL */
static char *http_get(const char *host, int port, const char *path)
{
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    char port_str[8];
    snprintf(port_str, sizeof(port_str), "%d", port);

    if (getaddrinfo(host, port_str, &hints, &res) != 0)
        return NULL;

    int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock < 0) {
        freeaddrinfo(res);
        return NULL;
    }

    /* Set socket timeout */
    struct timeval tv = { .tv_sec = 5, .tv_usec = 0 };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    if (connect(sock, res->ai_addr, res->ai_addrlen) < 0) {
        close(sock);
        freeaddrinfo(res);
        return NULL;
    }
    freeaddrinfo(res);

    /* Send HTTP request */
    char req[256];
    int req_len = snprintf(req, sizeof(req),
        "GET %s HTTP/1.0\r\n"
        "Host: %s:%d\r\n"
        "Connection: close\r\n"
        "\r\n", path, host, port);

    if (write(sock, req, (size_t)req_len) != req_len) {
        close(sock);
        return NULL;
    }

    /* Read response */
    char *buf = malloc(HTTP_BUF_SIZE);
    if (!buf) {
        close(sock);
        return NULL;
    }

    size_t total = 0;
    ssize_t n;
    while ((n = read(sock, buf + total, HTTP_BUF_SIZE - total - 1)) > 0) {
        total += (size_t)n;
        if (total >= HTTP_BUF_SIZE - 1)
            break;
    }
    close(sock);
    buf[total] = '\0';

    /* Find body after \r\n\r\n */
    char *body = strstr(buf, "\r\n\r\n");
    if (!body) {
        free(buf);
        return NULL;
    }
    body += 4;

    /* Move body to start of buffer */
    size_t body_len = total - (size_t)(body - buf);
    memmove(buf, body, body_len + 1);

    return buf;
}

/* Parse URL into host, port, path components */
static int parse_url(const char *url, char *host, size_t host_len,
                     int *port, char *path, size_t path_len)
{
    const char *p = url;

    /* Skip scheme */
    if (strncmp(p, "http://", 7) == 0)
        p += 7;
    else if (strncmp(p, "https://", 8) == 0) {
        fprintf(stderr, "HTTPS not supported, use HTTP\n");
        return -1;
    }

    /* Extract host:port */
    const char *slash = strchr(p, '/');
    const char *colon = strchr(p, ':');

    if (colon && (!slash || colon < slash)) {
        size_t hlen = (size_t)(colon - p);
        if (hlen >= host_len) hlen = host_len - 1;
        memcpy(host, p, hlen);
        host[hlen] = '\0';
        *port = atoi(colon + 1);
    } else {
        size_t hlen = slash ? (size_t)(slash - p) : strlen(p);
        if (hlen >= host_len) hlen = host_len - 1;
        memcpy(host, p, hlen);
        host[hlen] = '\0';
        *port = 80;
    }

    if (slash)
        snprintf(path, path_len, "%s", slash);
    else
        snprintf(path, path_len, "/");

    return 0;
}

static void print_usage(const char *prog)
{
    fprintf(stderr,
        "Usage: %s [options]\n"
        "\n"
        "Options:\n"
        "  -d DEVICE   Serial device (default: %s)\n"
        "  -u URL      API base URL (default: %s)\n"
        "  -b BAUD     Baud rate (default: %d)\n"
        "  -i MSEC     Update interval ms (default: %d)\n"
        "  -v          Verbose output\n"
        "  -h          Show this help\n",
        prog, DEFAULT_SERIAL, DEFAULT_API_URL, DEFAULT_BAUD,
        SAT_UPDATE_INTERVAL_MS);
}

int main(int argc, char *argv[])
{
    const char *serial_dev = DEFAULT_SERIAL;
    const char *api_url = DEFAULT_API_URL;
    int baud = DEFAULT_BAUD;
    int interval_ms = SAT_UPDATE_INTERVAL_MS;
    int verbose = 0;

    int opt;
    while ((opt = getopt(argc, argv, "d:u:b:i:vh")) != -1) {
        switch (opt) {
        case 'd': serial_dev = optarg; break;
        case 'u': api_url = optarg; break;
        case 'b': baud = atoi(optarg); break;
        case 'i': interval_ms = atoi(optarg); break;
        case 'v': verbose = 1; break;
        case 'h':
            print_usage(argv[0]);
            return 0;
        default:
            print_usage(argv[0]);
            return 1;
        }
    }

    /* Parse API URL */
    char api_host[128], api_path[128];
    int api_port;
    if (parse_url(api_url, api_host, sizeof(api_host),
                  &api_port, api_path, sizeof(api_path)) < 0)
        return 1;

    /* Build full API path */
    char status_path[256];
    snprintf(status_path, sizeof(status_path), "%s/api/v1/status",
             strcmp(api_path, "/") == 0 ? "" : api_path);

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    printf("Satellite feeder starting\n");
    printf("  Serial: %s @ %d baud\n", serial_dev, baud);
    printf("  API:    %s:%d%s\n", api_host, api_port, status_path);
    printf("  Interval: %d ms\n", interval_ms);

    int serial_fd = serial_open(serial_dev, baud);
    if (serial_fd < 0) {
        fprintf(stderr, "Failed to open serial port %s\n", serial_dev);
        return 1;
    }

    uint16_t seq = 0;
    uint8_t frame_buf[SAT_MAX_FRAME_SIZE];
    int consecutive_failures = 0;
    int heartbeat_counter = 0;

    while (g_running) {
        /* Fetch status from API */
        char *json_str = http_get(api_host, api_port, status_path);

        if (json_str) {
            cJSON *root = cJSON_Parse(json_str);
            if (root) {
                int frame_len = encoder_json_to_frame(root, frame_buf,
                                                      sizeof(frame_buf), &seq);
                if (frame_len > 0) {
                    if (serial_write(serial_fd, frame_buf, (size_t)frame_len) > 0) {
                        consecutive_failures = 0;
                        if (verbose)
                            printf("TX snapshot seq=%u len=%d\n",
                                   (unsigned)(seq - 1), frame_len);
                    } else {
                        consecutive_failures++;
                        fprintf(stderr, "Serial write failed\n");
                    }
                }
                cJSON_Delete(root);
            } else {
                consecutive_failures++;
                if (verbose)
                    fprintf(stderr, "JSON parse error\n");
            }
            free(json_str);
        } else {
            consecutive_failures++;
            heartbeat_counter++;

            /* Send heartbeat every 5 failures to keep link alive */
            if (heartbeat_counter >= 5) {
                int hb_len = encoder_heartbeat_frame(frame_buf,
                                                     sizeof(frame_buf), &seq);
                if (hb_len > 0)
                    serial_write(serial_fd, frame_buf, (size_t)hb_len);
                heartbeat_counter = 0;
            }

            if (verbose)
                fprintf(stderr, "API fetch failed (attempt %d)\n",
                        consecutive_failures);
        }

        if (consecutive_failures >= MAX_CONSECUTIVE_FAILURES) {
            fprintf(stderr, "Too many consecutive failures, exiting\n");
            break;
        }

        /* Sleep for interval */
        struct timespec ts = {
            .tv_sec  = interval_ms / 1000,
            .tv_nsec = (interval_ms % 1000) * 1000000L
        };
        nanosleep(&ts, NULL);
    }

    serial_close(serial_fd);
    printf("Satellite feeder stopped\n");
    return 0;
}
