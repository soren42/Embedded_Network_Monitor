/**
 * @file net_infra.c
 * @brief Infrastructure device monitoring helpers
 */

#include "net_infra.h"
#include <string.h>

infra_device_type_t infra_parse_type(const char *type_str)
{
    if (!type_str) return INFRA_TYPE_OTHER;

    if (strcmp(type_str, "router") == 0) return INFRA_TYPE_ROUTER;
    if (strcmp(type_str, "switch") == 0) return INFRA_TYPE_SWITCH;
    if (strcmp(type_str, "ap") == 0)     return INFRA_TYPE_AP;

    return INFRA_TYPE_OTHER;
}
