#ifndef API_JSON_H
#define API_JSON_H

#include "cJSON.h"
#include "net_collector.h"
#include "config.h"
#include "alerts.h"

cJSON *api_json_status(const net_state_t *state);
cJSON *api_json_interfaces(const net_state_t *state);
cJSON *api_json_connectivity(const net_state_t *state);
cJSON *api_json_connections(const net_state_t *state);
cJSON *api_json_alerts(void);
cJSON *api_json_infra(const net_state_t *state);
cJSON *api_json_wan(const wan_discovery_t *wan);
cJSON *api_json_config(const config_t *cfg);
cJSON *api_json_info(const net_state_t *state);

#endif /* API_JSON_H */
