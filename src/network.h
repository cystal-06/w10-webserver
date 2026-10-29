#ifndef SWS_NETWORK_H
#define SWS_NETWORK_H

#include "sws.h"

int network_init(ServerConfig *config);
void network_close(ServerConfig *config);
int network_accept(const ServerConfig *config, HttpRequest *req);

#endif /* SWS_NETWORK_H */
