/*
 * CS631 - Advanced Programming in the UNIX Environment
 * Group Project: Simple Web Server (sws)
 * Dual-Stack IPv4 / IPv6 Network Implementation
 */

#include "sws.h"
#include "network.h"
#include <poll.h>

static int create_and_bind_socket(int domain, const struct sockaddr *addr, socklen_t addrlen) {
    int sfd = socket(domain, SOCK_STREAM, 0);
    if (sfd < 0) {
        return -1;
    }

    int opt = 1;
    if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("sws: setsockopt SO_REUSEADDR");
    }

    if (domain == AF_INET6) {
        int v6only = 1;
        if (setsockopt(sfd, IPPROTO_IPV6, IPV6_V6ONLY, &v6only, sizeof(v6only)) < 0) {
            /* Some platforms may not support or require this */
        }
    }

    if (bind(sfd, addr, addrlen) < 0) {
        close(sfd);
        return -1;
    }

    if (listen(sfd, 128) < 0) {
        close(sfd);
        return -1;
    }

    return sfd;
}

int network_init(ServerConfig *config) {
    config->sock_v4 = -1;
    config->sock_v6 = -1;

    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", config->port);

    if (config->bind_addr != NULL) {
        /* User specified a concrete IPv4 or IPv6 address */
        struct addrinfo hints, *res, *rp;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE;

        int status = getaddrinfo(config->bind_addr, port_str, &hints, &res);
        if (status != 0) {
            fprintf(stderr, "sws: getaddrinfo(%s): %s\n", config->bind_addr, gai_strerror(status));
            return -1;
        }

        for (rp = res; rp != NULL; rp = rp->ai_next) {
            int s = create_and_bind_socket(rp->ai_family, rp->ai_addr, rp->ai_addrlen);
            if (s >= 0) {
                if (rp->ai_family == AF_INET6) {
                    config->sock_v6 = s;
                } else {
                    config->sock_v4 = s;
                }
                break;
            }
        }
        freeaddrinfo(res);

        if (config->sock_v4 < 0 && config->sock_v6 < 0) {
            fprintf(stderr, "sws: unable to bind to %s:%d\n", config->bind_addr, config->port);
            return -1;
        }
    } else {
        /* Default: Listen on all IPv6 and IPv4 addresses */
        struct sockaddr_in6 sa6;
        memset(&sa6, 0, sizeof(sa6));
        sa6.sin6_family = AF_INET6;
        sa6.sin6_addr = in6addr_any;
        sa6.sin6_port = htons((uint16_t)config->port);
        config->sock_v6 = create_and_bind_socket(AF_INET6, (struct sockaddr *)&sa6, sizeof(sa6));

        struct sockaddr_in sa4;
        memset(&sa4, 0, sizeof(sa4));
        sa4.sin_family = AF_INET;
        sa4.sin_addr.s_addr = htonl(INADDR_ANY);
        sa4.sin_port = htons((uint16_t)config->port);
        config->sock_v4 = create_and_bind_socket(AF_INET, (struct sockaddr *)&sa4, sizeof(sa4));

        if (config->sock_v4 < 0 && config->sock_v6 < 0) {
            fprintf(stderr, "sws: failed to bind to port %d on IPv4 and IPv6\n", config->port);
            return -1;
        }
    }

    return 0;
}

void network_close(ServerConfig *config) {
    if (config->sock_v4 >= 0) {
        close(config->sock_v4);
        config->sock_v4 = -1;
    }
    if (config->sock_v6 >= 0) {
        close(config->sock_v6);
        config->sock_v6 = -1;
    }
}

int network_accept(const ServerConfig *config, HttpRequest *req) {
    struct pollfd pfds[2];
    int nfds = 0;

    if (config->sock_v4 >= 0) {
        pfds[nfds].fd = config->sock_v4;
        pfds[nfds].events = POLLIN;
        nfds++;
    }
    if (config->sock_v6 >= 0) {
        pfds[nfds].fd = config->sock_v6;
        pfds[nfds].events = POLLIN;
        nfds++;
    }

    if (nfds == 0) {
        return -1;
    }

    while (g_server_running) {
        int ret = poll(pfds, (nfds_t)nfds, 1000);
        if (ret < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("sws: poll");
            return -1;
        }
        if (ret == 0) {
            continue; /* Timeout, check g_server_running flag */
        }

        for (int i = 0; i < nfds; i++) {
            if (pfds[i].revents & POLLIN) {
                struct sockaddr_storage client_addr;
                socklen_t addr_len = sizeof(client_addr);
                int client_fd = accept(pfds[i].fd, (struct sockaddr *)&client_addr, &addr_len);
                if (client_fd < 0) {
                    if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
                        continue;
                    }
                    perror("sws: accept");
                    return -1;
                }

                /* Extract client IP string */
                req->client_ip[0] = '\0';
                if (client_addr.ss_family == AF_INET) {
                    struct sockaddr_in *s = (struct sockaddr_in *)&client_addr;
                    inet_ntop(AF_INET, &s->sin_addr, req->client_ip, sizeof(req->client_ip));
                    req->client_port = ntohs(s->sin_port);
                } else if (client_addr.ss_family == AF_INET6) {
                    struct sockaddr_in6 *s = (struct sockaddr_in6 *)&client_addr;
                    inet_ntop(AF_INET6, &s->sin6_addr, req->client_ip, sizeof(req->client_ip));
                    req->client_port = ntohs(s->sin6_port);
                }

                return client_fd;
            }
        }
    }

    return -1;
}
