#ifndef SWS_CGI_H
#define SWS_CGI_H

#include "sws.h"

int cgi_execute(int client_fd, const HttpRequest *req, const char *script_path, const ServerConfig *config, off_t *bytes_sent);

#endif /* SWS_CGI_H */
