/**
 * @file net_infra.h
 * @brief Infrastructure device monitoring (router, switch, AP)
 */

#ifndef NET_INFRA_H
#define NET_INFRA_H

#include "app_conf.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    INFRA_TYPE_ROUTER = 0,
    INFRA_TYPE_SWITCH,
    INFRA_TYPE_AP,
    INFRA_TYPE_OTHER
} infra_device_type_t;

typedef struct {
    char     name[32];
    char     ip_str[16];          /* "10.0.0.1" */
    infra_device_type_t type;
    int      enabled;
    int      reachable;           /* last probe result */
    double   latency_ms;          /* last probe latency */
    int      consecutive_fails;   /* for tracking flapping */
} infra_device_t;

/**
 * Parse infrastructure device type string.
 * @return INFRA_TYPE_ROUTER, INFRA_TYPE_SWITCH, INFRA_TYPE_AP, or INFRA_TYPE_OTHER
 */
infra_device_type_t infra_parse_type(const char *type_str);

#ifdef __cplusplus
}
#endif

#endif /* NET_INFRA_H */
