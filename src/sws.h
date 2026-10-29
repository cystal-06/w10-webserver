/*
 * CS631 - Advanced Programming in the UNIX Environment
 * Group Project: Simple Web Server (sws)
 * Core System Definitions and Header
 */

#ifndef SWS_H
#define SWS_H

#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <limits.h>
#include <pwd.h>
#include <dirent.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <magic.h>

#define DEFAULT_PORT       8080
#define SERVER_NAME        "sws/1.0 (UNIX)"
#define BUFFER_SIZE        8192
#define URI_MAX_LEN        2048
#define HEADER_MAX_LEN     8192
#define LINE_MAX_LEN       4096

/* HTTP Request Methods supported by sws */
typedef enum {
    METHOD_UNSUPPORTED = 0,
    METHOD_GET,
    METHOD_HEAD
} HttpMethod;

/* Server Configuration State */
typedef struct {
    int port;                          /* Listening TCP port (-p) */
    char *bind_addr;                   /* Specific IPv4/IPv6 address (-i) */
    char *doc_root;                    /* Document root path (mandatory) */
    char *cgi_dir;                     /* CGI root path (-c) */
    char *log_file;                    /* Log file path (-l) */
    int log_fd;                        /* File descriptor for log */
    int debug_mode;                    /* Debug mode flag (-d) */
    int sock_v4;                       /* IPv4 listener socket */
    int sock_v6;                       /* IPv6 listener socket */
    magic_t magic_cookie;              /* libmagic handle for MIME detection */
} ServerConfig;

/* Parsed HTTP Request */
typedef struct {
    HttpMethod method;                 /* GET, HEAD, or UNSUPPORTED */
    char raw_method[16];               /* Original method string */
    char uri[URI_MAX_LEN];             /* Requested URI */
    char query_string[URI_MAX_LEN];     /* Query string after '?' for CGI */
    char http_version[16];             /* "HTTP/1.0" or "HTTP/1.1" */
    char first_line[LINE_MAX_LEN];     /* Unaltered first request line for logging */
    time_t if_modified_since;          /* Parsed If-Modified-Since header (-1 if absent) */
    char client_ip[INET6_ADDRSTRLEN];  /* Remote client IP string */
    int client_port;                   /* Remote client port */
} HttpRequest;

/* HTTP Response Metadata */
typedef struct {
    int status_code;                   /* 200, 304, 400, 403, 404, 500, 501 */
    const char *status_phrase;         /* "OK", "Not Found", etc. */
    char content_type[128];            /* MIME type string */
    off_t content_length;              /* Response body size in bytes */
    time_t last_modified;              /* File modification timestamp */
    char file_path[PATH_MAX];          /* Resolved real file path */
    int is_cgi;                        /* 1 if request is CGI, 0 otherwise */
} HttpResponse;

/* Global server termination flag */
extern volatile sig_atomic_t g_server_running;

#endif /* SWS_H */
