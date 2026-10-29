/*
 * CS631 - Advanced Programming in the UNIX Environment
 * Group Project: Simple Web Server (sws)
 * Main Entry Point, CLI Parser & Daemon Lifecycle
 */

#include "sws.h"
#include "network.h"
#include "http.h"
#include "resource.h"
#include "logger.h"

volatile sig_atomic_t g_server_running = 1;

static void handle_sigchld(int sig) {
    (void)sig;
    int saved_errno = errno;
    while (waitpid(-1, NULL, WNOHANG) > 0) {
        /* Reap all terminated child processes */
    }
    errno = saved_errno;
}

static void handle_sigterm(int sig) {
    (void)sig;
    g_server_running = 0;
}

static void print_usage(const char *prog_name) {
    printf("Usage: %s [-dh] [-c dir] [-i address] [-l file] [-p port] dir\n\n", prog_name);
    printf("Options:\n");
    printf("  -c dir       Allow execution of CGIs from the given directory\n");
    printf("  -d           Enter debugging mode (no daemon, 1 connection at a time, log stdout)\n");
    printf("  -h           Print this short usage summary and exit\n");
    printf("  -i address   Bind to the given IPv4 or IPv6 address (default: all addresses)\n");
    printf("  -l file      Log all requests to the given file\n");
    printf("  -p port      Listen on the given port (default: 8080)\n");
    printf("  dir          Document root directory to serve files from\n");
}

static int daemonize_server(void) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("sws: fork daemon failed");
        return -1;
    }
    if (pid > 0) {
        /* Parent exits */
        _exit(0);
    }

    /* Child creates a new session and becomes process group leader */
    if (setsid() < 0) {
        perror("sws: setsid failed");
        return -1;
    }

    /* Ignore terminal signals */
    signal(SIGHUP, SIG_IGN);

    /* Second fork to ensure process cannot reacquire a controlling terminal */
    pid = fork();
    if (pid < 0) {
        perror("sws: second fork failed");
        return -1;
    }
    if (pid > 0) {
        _exit(0);
    }

    umask(027);

    /* Redirect standard file descriptors to /dev/null */
    int dev_null = open("/dev/null", O_RDWR);
    if (dev_null >= 0) {
        dup2(dev_null, STDIN_FILENO);
        dup2(dev_null, STDOUT_FILENO);
        dup2(dev_null, STDERR_FILENO);
        if (dev_null > STDERR_FILENO) {
            close(dev_null);
        }
    }

    return 0;
}

int main(int argc, char *argv[]) {
    ServerConfig config;
    memset(&config, 0, sizeof(config));
    config.port = DEFAULT_PORT;
    config.debug_mode = 0;
    config.log_fd = -1;

    int opt;
    while ((opt = getopt(argc, argv, "c:dhi:l:p:")) != -1) {
        switch (opt) {
            case 'c':
                config.cgi_dir = optarg;
                break;
            case 'd':
                config.debug_mode = 1;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            case 'i':
                config.bind_addr = optarg;
                break;
            case 'l':
                config.log_file = optarg;
                break;
            case 'p': {
                char *endptr = NULL;
                long p = strtol(optarg, &endptr, 10);
                if (*endptr != '\0' || p <= 0 || p > 65535) {
                    fprintf(stderr, "sws: invalid port number '%s'\n", optarg);
                    return 1;
                }
                config.port = (int)p;
                break;
            }
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    if (optind >= argc) {
        fprintf(stderr, "sws: missing mandatory document root directory\n");
        print_usage(argv[0]);
        return 1;
    }

    config.doc_root = argv[optind];

    /* Verify document root existence and readability */
    struct stat st;
    if (stat(config.doc_root, &st) < 0 || !S_ISDIR(st.st_mode) || access(config.doc_root, R_OK) != 0) {
        fprintf(stderr, "sws: document root '%s' is not an accessible directory\n", config.doc_root);
        return 1;
    }

    /* Verify CGI root if specified */
    if (config.cgi_dir != NULL) {
        if (stat(config.cgi_dir, &st) < 0 || !S_ISDIR(st.st_mode) || access(config.cgi_dir, R_OK) != 0) {
            fprintf(stderr, "sws: CGI directory '%s' is not an accessible directory\n", config.cgi_dir);
            return 1;
        }
    }

    /* Initialize subsystems */
    if (logger_init(&config) < 0) {
        return 1;
    }

    if (resource_init(&config) < 0) {
        logger_close(&config);
        return 1;
    }

    if (network_init(&config) < 0) {
        resource_cleanup(&config);
        logger_close(&config);
        return 1;
    }

    /* Setup Signal Handlers */
    struct sigaction sa_chld;
    memset(&sa_chld, 0, sizeof(sa_chld));
    sa_chld.sa_handler = handle_sigchld;
    sigemptyset(&sa_chld.sa_mask);
    sa_chld.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa_chld, NULL);

    struct sigaction sa_term;
    memset(&sa_term, 0, sizeof(sa_term));
    sa_term.sa_handler = handle_sigterm;
    sigemptyset(&sa_term.sa_mask);
    sigaction(SIGINT, &sa_term, NULL);
    sigaction(SIGTERM, &sa_term, NULL);

    /* Daemonize if not in debug mode (-d) */
    if (!config.debug_mode) {
        if (daemonize_server() < 0) {
            network_close(&config);
            resource_cleanup(&config);
            logger_close(&config);
            return 1;
        }
    }

    /* Main Event Loop */
    while (g_server_running) {
        HttpRequest req;
        int client_fd = network_accept(&config, &req);
        if (client_fd < 0) {
            continue;
        }

        if (config.debug_mode) {
            /* Debug mode: Only accept one connection at a time */
            http_handle_connection(client_fd, &config, &req);
            close(client_fd);
        } else {
            /* Daemon mode: Fork child worker to handle concurrent connection */
            pid_t pid = fork();
            if (pid == 0) {
                /* Child Worker */
                network_close(&config);
                http_handle_connection(client_fd, &config, &req);
                close(client_fd);
                resource_cleanup(&config);
                logger_close(&config);
                _exit(0);
            }
            close(client_fd);
        }
    }

    /* Clean Shutdown */
    network_close(&config);
    resource_cleanup(&config);
    logger_close(&config);
    return 0;
}
