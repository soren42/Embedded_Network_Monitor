#ifndef API_ROUTES_H
#define API_ROUTES_H

#include "mongoose.h"

void api_routes_handle(struct mg_connection *c, struct mg_http_message *hm);

#endif /* API_ROUTES_H */
