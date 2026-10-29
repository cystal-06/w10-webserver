/*
 * CS631 - Advanced Programming in the UNIX Environment
 * Group Project: Simple Web Server (sws)
 * HTTP/1.0 Protocol Engine (RFC 1945)
 */

#include "sws.h"
#include "http.h"
#include "resource.h"
#include "cgi.h"
#include "logger.h"
#include <inttypes.h>

static time_t parse_http_date(const char *date_str) {
    struct tm tm_buf;
    memset(&tm_buf, 0, sizeof(tm_buf));

    /* RFC 1123 format: Sun, 06 Nov 1994 08:49:37 GMT */
    if (strptime(date_str, "%a, %d %b %Y %H:%M:%S GMT", &tm_buf) != NULL) {
        return timegm(&tm_buf);
    }
    /* RFC 850 format: Sunday, 06-Nov-94 08:49:37 GMT */
    if (strptime(date_str, "%a, %d-%b-%y %H:%M:%S GMT", &tm_buf) != NULL) {
        return timegm(&tm_buf);
    }
    /* asctime format: Sun Nov  6 08:49:37 1994 */
    if (strptime(date_str, "%a %b %e %H:%M:%S %Y", &tm_buf) != NULL) {
        return timegm(&tm_buf);
    }
    return (time_t)-1;
}

static const char *get_status_phrase(int status_code) {
    switch (status_code) {
        case 200: return "OK";
        case 304: return "Not Modified";
        case 400: return "Bad Request";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        default:  return "Unknown Status";
    }
}

int http_parse_request(int client_fd, HttpRequest *req) {
    char buffer[HEADER_MAX_LEN];
    ssize_t total_read = 0;
    bool header_ended = false;

    memset(req->raw_method, 0, sizeof(req->raw_method));
    memset(req->uri, 0, sizeof(req->uri));
    memset(req->query_string, 0, sizeof(req->query_string));
    memset(req->http_version, 0, sizeof(req->http_version));
    memset(req->first_line, 0, sizeof(req->first_line));
    req->if_modified_since = (time_t)-1;
    req->method = METHOD_UNSUPPORTED;

    while (total_read < (ssize_t)(sizeof(buffer) - 1)) {
        ssize_t n = read(client_fd, buffer + total_read, 1);
        if (n <= 0) {
            break;
        }
        total_read += n;
        buffer[total_read] = '\0';

        if (strstr(buffer, "\r\n\r\n") != NULL || strstr(buffer, "\n\n") != NULL) {
            header_ended = true;
            break;
        }
    }

    if (total_read == 0) {
        return -1;
    }

    char *saveptr = NULL;
    char *line = strtok_r(buffer, "\r\n", &saveptr);
    if (line == NULL) {
        return 400;
    }

    snprintf(req->first_line, sizeof(req->first_line), "%s", line);

    char method_str[32], uri_str[URI_MAX_LEN], ver_str[32];
    int parsed = sscanf(line, "%31s %2047s %31s", method_str, uri_str, ver_str);
    if (parsed < 2) {
        return 400;
    }

    snprintf(req->raw_method, sizeof(req->raw_method), "%s", method_str);
    if (strcmp(method_str, "GET") == 0) {
        req->method = METHOD_GET;
    } else if (strcmp(method_str, "HEAD") == 0) {
        req->method = METHOD_HEAD;
    } else {
        req->method = METHOD_UNSUPPORTED;
        return 501;
    }

    /* Enforce Relative URI only (RFC 1945, Section 3.2.1) */
    if (uri_str[0] != '/') {
        return 400;
    }

    char *qmark = strchr(uri_str, '?');
    if (qmark != NULL) {
        *qmark = '\0';
        snprintf(req->query_string, sizeof(req->query_string), "%s", qmark + 1);
    }
    snprintf(req->uri, sizeof(req->uri), "%s", uri_str);

    if (parsed >= 3) {
        snprintf(req->http_version, sizeof(req->http_version), "%s", ver_str);
    } else {
        snprintf(req->http_version, sizeof(req->http_version), "HTTP/1.0");
    }

    /* Parse remaining headers */
    while ((line = strtok_r(NULL, "\r\n", &saveptr)) != NULL) {
        if (strncasecmp(line, "If-Modified-Since:", 18) == 0) {
            const char *val = line + 18;
            while (*val == ' ' || *val == '\t') val++;
            req->if_modified_since = parse_http_date(val);
        }
    }

    (void)header_ended;
    return 200;
}

void http_send_error(int client_fd, int status_code, const HttpRequest *req, const ServerConfig *config) {
    const char *phrase = get_status_phrase(status_code);
    time_t now = time(NULL);
    struct tm gm_time;
    char date_str[64];
    gmtime_r(&now, &gm_time);
    strftime(date_str, sizeof(date_str), "%a, %d %b %Y %H:%M:%S GMT", &gm_time);

    char body[1024];
    int body_len = snprintf(body, sizeof(body),
                           "<!DOCTYPE html>\n<html>\n<head><title>%d %s</title></head>\n"
                           "<body>\n<center><h1>%d %s</h1></center>\n"
                           "<hr><center>%s</center>\n</body>\n</html>\n",
                           status_code, phrase, status_code, phrase, SERVER_NAME);

    char header[512];
    int hdr_len = snprintf(header, sizeof(header),
                          "HTTP/1.0 %d %s\r\n"
                          "Date: %s\r\n"
                          "Server: %s\r\n"
                          "Content-Type: text/html; charset=utf-8\r\n"
                          "Content-Length: %d\r\n"
                          "Connection: close\r\n\r\n",
                          status_code, phrase, date_str, SERVER_NAME, body_len);

    ssize_t w1 = write(client_fd, header, (size_t)hdr_len);
    ssize_t w2 = 0;
    if (req == NULL || req->method != METHOD_HEAD) {
        w2 = write(client_fd, body, (size_t)body_len);
    }

    off_t total_sent = (w1 > 0 ? w1 : 0) + (w2 > 0 ? w2 : 0);
    logger_log(config, req, status_code, total_sent);
}

void http_handle_connection(int client_fd, const ServerConfig *config, HttpRequest *req) {
    int parse_status = http_parse_request(client_fd, req);
    if (parse_status != 200) {
        if (parse_status > 0) {
            http_send_error(client_fd, parse_status, req, config);
        }
        return;
    }

    char resolved_path[PATH_MAX];
    int is_dir = 0, is_cgi = 0;
    int res_status = resource_resolve(config, req->uri, resolved_path, sizeof(resolved_path), &is_dir, &is_cgi);

    if (res_status != 200) {
        http_send_error(client_fd, res_status, req, config);
        return;
    }

    off_t bytes_sent = 0;

    /* Handle CGI execution */
    if (is_cgi) {
        int cgi_status = cgi_execute(client_fd, req, resolved_path, config, &bytes_sent);
        logger_log(config, req, cgi_status, bytes_sent);
        return;
    }

    /* Handle Directory Indexing */
    if (is_dir) {
        time_t now = time(NULL);
        struct tm gm_time;
        char date_str[64];
        gmtime_r(&now, &gm_time);
        strftime(date_str, sizeof(date_str), "%a, %d %b %Y %H:%M:%S GMT", &gm_time);

        char hdr[512];
        int hlen = snprintf(hdr, sizeof(hdr),
                            "HTTP/1.0 200 OK\r\n"
                            "Date: %s\r\n"
                            "Server: %s\r\n"
                            "Content-Type: text/html; charset=utf-8\r\n"
                            "Connection: close\r\n\r\n",
                            date_str, SERVER_NAME);
        ssize_t w = write(client_fd, hdr, (size_t)hlen);
        if (w > 0) bytes_sent += w;

        if (req->method == METHOD_GET) {
            resource_generate_dir_index(client_fd, resolved_path, req->uri, &bytes_sent);
        }

        logger_log(config, req, 200, bytes_sent);
        return;
    }

    /* Handle Regular Static File */
    struct stat st;
    if (stat(resolved_path, &st) < 0) {
        http_send_error(client_fd, 404, req, config);
        return;
    }

    /* Check If-Modified-Since header */
    if (req->if_modified_since != (time_t)-1 && st.st_mtime <= req->if_modified_since) {
        time_t now = time(NULL);
        struct tm gm_time, m_time;
        char date_str[64], mod_str[64];
        gmtime_r(&now, &gm_time);
        strftime(date_str, sizeof(date_str), "%a, %d %b %Y %H:%M:%S GMT", &gm_time);
        gmtime_r(&st.st_mtime, &m_time);
        strftime(mod_str, sizeof(mod_str), "%a, %d %b %Y %H:%M:%S GMT", &m_time);

        char hdr[512];
        int hlen = snprintf(hdr, sizeof(hdr),
                            "HTTP/1.0 304 Not Modified\r\n"
                            "Date: %s\r\n"
                            "Server: %s\r\n"
                            "Last-Modified: %s\r\n"
                            "Connection: close\r\n\r\n",
                            date_str, SERVER_NAME, mod_str);
        ssize_t w = write(client_fd, hdr, (size_t)hlen);
        if (w > 0) bytes_sent += w;

        logger_log(config, req, 304, bytes_sent);
        return;
    }

    /* Serve File with 200 OK */
    char mime_type[128];
    resource_get_mime_type(config, resolved_path, mime_type, sizeof(mime_type));

    time_t now = time(NULL);
    struct tm gm_time, m_time;
    char date_str[64], mod_str[64];
    gmtime_r(&now, &gm_time);
    strftime(date_str, sizeof(date_str), "%a, %d %b %Y %H:%M:%S GMT", &gm_time);
    gmtime_r(&st.st_mtime, &m_time);
    strftime(mod_str, sizeof(mod_str), "%a, %d %b %Y %H:%M:%S GMT", &m_time);

    char hdr[512];
    int hlen = snprintf(hdr, sizeof(hdr),
                        "HTTP/1.0 200 OK\r\n"
                        "Date: %s\r\n"
                        "Server: %s\r\n"
                        "Last-Modified: %s\r\n"
                        "Content-Type: %s\r\n"
                        "Content-Length: %" PRIdMAX "\r\n"
                        "Connection: close\r\n\r\n",
                        date_str, SERVER_NAME, mod_str, mime_type, (intmax_t)st.st_size);

    ssize_t w = write(client_fd, hdr, (size_t)hlen);
    if (w > 0) bytes_sent += w;

    if (req->method == METHOD_GET) {
        int file_fd = open(resolved_path, O_RDONLY);
        if (file_fd >= 0) {
            char fbuf[BUFFER_SIZE];
            ssize_t nr;
            while ((nr = read(file_fd, fbuf, sizeof(fbuf))) > 0) {
                ssize_t nw = write(client_fd, fbuf, (size_t)nr);
                if (nw > 0) bytes_sent += nw;
            }
            close(file_fd);
        }
    }

    logger_log(config, req, 200, bytes_sent);
}
