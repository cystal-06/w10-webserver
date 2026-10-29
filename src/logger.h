#ifndef SWS_LOGGER_H
#define SWS_LOGGER_H

#include "sws.h"

int logger_init(ServerConfig *config);
void logger_close(ServerConfig *config);
void logger_log(const ServerConfig *config, const HttpRequest *req, int status_code, off_t bytes_sent);

#endif /* SWS_LOGGER_H */
