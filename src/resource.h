#ifndef SWS_RESOURCE_H
#define SWS_RESOURCE_H

#include "sws.h"

int resource_init(ServerConfig *config);
void resource_cleanup(ServerConfig *config);
int resource_resolve(const ServerConfig *config, const char *raw_uri, char *resolved_path, size_t max_len, int *is_dir, int *is_cgi);
void resource_get_mime_type(const ServerConfig *config, const char *file_path, char *mime_buf, size_t buf_len);
int resource_generate_dir_index(int client_fd, const char *dir_path, const char *req_uri, off_t *bytes_sent);

#endif /* SWS_RESOURCE_H */
