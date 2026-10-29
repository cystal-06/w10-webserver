/*
 * CS631 - Advanced Programming in the UNIX Environment
 * Group Project: Simple Web Server (sws)
 * CGI Engine Implementation (RFC 3875)
 */

#include "sws.h"
#include "cgi.h"

extern char **environ;

int cgi_execute(int client_fd, const HttpRequest *req, const char *script_path, const ServerConfig *config, off_t *bytes_sent) {
    if (access(script_path, X_OK) != 0) {
        return 403;
    }

    int pipe_fd[2];
    if (pipe(pipe_fd) < 0) {
        perror("sws: cgi pipe");
        return 500;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("sws: cgi fork");
        close(pipe_fd[0]);
        close(pipe_fd[1]);
        return 500;
    }

    if (pid == 0) {
        /* Child Process */
        close(pipe_fd[0]);
        if (dup2(pipe_fd[1], STDOUT_FILENO) < 0) {
            _exit(1);
        }
        if (dup2(pipe_fd[1], STDERR_FILENO) < 0) {
            _exit(1);
        }
        close(pipe_fd[1]);
        close(client_fd);

        char port_str[16];
        snprintf(port_str, sizeof(port_str), "%d", config->port);

        setenv("GATEWAY_INTERFACE", "CGI/1.1", 1);
        setenv("SERVER_PROTOCOL", "HTTP/1.0", 1);
        setenv("SERVER_SOFTWARE", SERVER_NAME, 1);
        setenv("REQUEST_METHOD", req->raw_method, 1);
        setenv("SCRIPT_NAME", req->uri, 1);
        setenv("QUERY_STRING", req->query_string, 1);
        setenv("REMOTE_ADDR", req->client_ip, 1);
        setenv("SERVER_PORT", port_str, 1);

        char *argv[] = { (char *)script_path, NULL };
        execve(script_path, argv, environ);

        _exit(127);
    }

    /* Parent Process */
    close(pipe_fd[1]);

    time_t now = time(NULL);
    struct tm gm_time;
    char date_str[64];
    if (gmtime_r(&now, &gm_time) != NULL) {
        strftime(date_str, sizeof(date_str), "%a, %d %b %Y %H:%M:%S GMT", &gm_time);
    } else {
        snprintf(date_str, sizeof(date_str), "Thu, 01 Jan 1970 00:00:00 GMT");
    }

    char initial_header[512];
    int hlen = snprintf(initial_header, sizeof(initial_header),
                       "HTTP/1.0 200 OK\r\n"
                       "Date: %s\r\n"
                       "Server: %s\r\n",
                       date_str, SERVER_NAME);

    ssize_t w = write(client_fd, initial_header, (size_t)hlen);
    if (w > 0) *bytes_sent += w;

    char buffer[BUFFER_SIZE];
    ssize_t n;
    while ((n = read(pipe_fd[0], buffer, sizeof(buffer))) > 0) {
        if (req->method == METHOD_GET) {
            ssize_t total_written = 0;
            while (total_written < n) {
                ssize_t written = write(client_fd, buffer + total_written, (size_t)(n - total_written));
                if (written < 0) {
                    if (errno == EINTR) continue;
                    break;
                }
                total_written += written;
                *bytes_sent += written;
            }
        }
    }

    close(pipe_fd[0]);
    int status;
    waitpid(pid, &status, 0);

    return 200;
}
