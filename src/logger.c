/*
 * CS631 - Advanced Programming in the UNIX Environment
 * Group Project: Simple Web Server (sws)
 * Logger Implementation (Apache Common Log Format Variant)
 */

#include "sws.h"
#include "logger.h"
#include <inttypes.h>

int logger_init(ServerConfig *config) {
    if (config->log_file == NULL) {
        config->log_fd = -1;
        return 0;
    }

    config->log_fd = open(config->log_file, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (config->log_fd < 0) {
        perror("sws: open log file failed");
        return -1;
    }
    return 0;
}

void logger_close(ServerConfig *config) {
    if (config->log_fd >= 0) {
        close(config->log_fd);
        config->log_fd = -1;
    }
}

void logger_log(const ServerConfig *config, const HttpRequest *req, int status_code, off_t bytes_sent) {
    time_t now = time(NULL);
    struct tm gm_time;
    char time_str[32];
    char log_entry[LINE_MAX_LEN * 2];

    if (gmtime_r(&now, &gm_time) != NULL) {
        strftime(time_str, sizeof(time_str), "%Y-%m-%dT%H:%M:%SZ", &gm_time);
    } else {
        snprintf(time_str, sizeof(time_str), "1970-01-01T00:00:00Z");
    }

    const char *client_ip = (req && req->client_ip[0] != '\0') ? req->client_ip : "-";
    const char *first_line = (req && req->first_line[0] != '\0') ? req->first_line : "-";

    int len = snprintf(log_entry, sizeof(log_entry), "%s %s \"%s\" %d %" PRIdMAX "\n",
                       client_ip,
                       time_str,
                       first_line,
                       status_code,
                       (intmax_t)bytes_sent);

    if (len <= 0) {
        return;
    }

    /* In debug mode (-d), logs are printed directly to stdout */
    if (config->debug_mode) {
        ssize_t written = write(STDOUT_FILENO, log_entry, (size_t)len);
        (void)written;
    }

    /* If a log file was configured, append atomically */
    if (config->log_fd >= 0) {
        ssize_t written = write(config->log_fd, log_entry, (size_t)len);
        (void)written;
    }
}
