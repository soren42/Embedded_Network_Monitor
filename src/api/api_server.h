#ifndef API_SERVER_H
#define API_SERVER_H

#include "config.h"

int  api_server_init(config_t *cfg);
void api_server_poll(void);
void api_server_shutdown(void);

#endif /* API_SERVER_H */
