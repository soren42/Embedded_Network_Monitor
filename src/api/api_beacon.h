#ifndef API_BEACON_H
#define API_BEACON_H

#include "config.h"

int  api_beacon_init(config_t *cfg);
void api_beacon_tick(void);
void api_beacon_shutdown(void);

#endif /* API_BEACON_H */
