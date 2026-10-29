#ifndef SWS_HTTP_H
#define SWS_HTTP_H

#include "sws.h"

int http_parse_request(int client_fd, HttpRequest *req);
void http_handle_connection(int client_fd, const ServerConfig *config, HttpRequest *req);
void http_send_error(int client_fd, int status_code, const HttpRequest *req, const ServerConfig *config);

#endif /* SWS_HTTP_H */
