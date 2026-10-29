/*
 * CS631 - Advanced Programming in the UNIX Environment
 * Group Project: Simple Web Server (sws)
 * Resource Resolution, Directory Indexing & MIME Identification
 */

#include "sws.h"
#include "resource.h"

int resource_init(ServerConfig *config) {
    config->magic_cookie = magic_open(MAGIC_MIME_TYPE);
    if (config->magic_cookie == NULL) {
        fprintf(stderr, "sws: warning: unable to initialize libmagic\n");
        return 0;
    }

    if (magic_load(config->magic_cookie, NULL) != 0) {
        fprintf(stderr, "sws: warning: failed to load default magic database: %s\n",
                magic_error(config->magic_cookie));
        magic_close(config->magic_cookie);
        config->magic_cookie = NULL;
    }
    return 0;
}

void resource_cleanup(ServerConfig *config) {
    if (config->magic_cookie != NULL) {
        magic_close(config->magic_cookie);
        config->magic_cookie = NULL;
    }
}

void resource_get_mime_type(const ServerConfig *config, const char *file_path, char *mime_buf, size_t buf_len) {
    if (config->magic_cookie != NULL) {
        const char *detected = magic_file(config->magic_cookie, file_path);
        if (detected != NULL && detected[0] != '\0') {
            snprintf(mime_buf, buf_len, "%s", detected);
            return;
        }
    }

    /* Fallback heuristic based on common extensions */
    const char *dot = strrchr(file_path, '.');
    if (dot != NULL) {
        if (strcasecmp(dot, ".html") == 0 || strcasecmp(dot, ".htm") == 0) {
            snprintf(mime_buf, buf_len, "text/html; charset=utf-8");
            return;
        } else if (strcasecmp(dot, ".txt") == 0 || strcasecmp(dot, ".c") == 0 || strcasecmp(dot, ".h") == 0) {
            snprintf(mime_buf, buf_len, "text/plain; charset=utf-8");
            return;
        } else if (strcasecmp(dot, ".css") == 0) {
            snprintf(mime_buf, buf_len, "text/css");
            return;
        } else if (strcasecmp(dot, ".js") == 0) {
            snprintf(mime_buf, buf_len, "application/javascript");
            return;
        } else if (strcasecmp(dot, ".png") == 0) {
            snprintf(mime_buf, buf_len, "image/png");
            return;
        } else if (strcasecmp(dot, ".jpg") == 0 || strcasecmp(dot, ".jpeg") == 0) {
            snprintf(mime_buf, buf_len, "image/jpeg");
            return;
        } else if (strcasecmp(dot, ".gif") == 0) {
            snprintf(mime_buf, buf_len, "image/gif");
            return;
        }
    }

    snprintf(mime_buf, buf_len, "application/octet-stream");
}

int resource_resolve(const ServerConfig *config, const char *raw_uri, char *resolved_path, size_t max_len, int *is_dir, int *is_cgi) {
    *is_dir = 0;
    *is_cgi = 0;

    char uri[URI_MAX_LEN];
    snprintf(uri, sizeof(uri), "%s", raw_uri);

    /* Strip query string if present */
    char *qmark = strchr(uri, '?');
    if (qmark != NULL) {
        *qmark = '\0';
    }

    char base_dir[PATH_MAX];
    char target_path[PATH_MAX];

    /* 1. Check for CGI execution request (/cgi-bin/...) */
    if (strncmp(uri, "/cgi-bin", 8) == 0 && config->cgi_dir != NULL) {
        *is_cgi = 1;
        const char *subpath = uri + 8;
        if (*subpath == '/') {
            subpath++;
        }
        snprintf(target_path, sizeof(target_path), "%s/%s", config->cgi_dir, subpath);
        
        if (realpath(config->cgi_dir, base_dir) == NULL) {
            return 500;
        }
    }
    /* 2. Check for User Directory (/~<user>/...) */
    else if (strncmp(uri, "/~", 2) == 0) {
        const char *user_start = uri + 2;
        const char *slash = strchr(user_start, '/');
        char username[128];
        const char *subpath = "";

        if (slash != NULL) {
            size_t ulen = (size_t)(slash - user_start);
            if (ulen >= sizeof(username)) ulen = sizeof(username) - 1;
            strncpy(username, user_start, ulen);
            username[ulen] = '\0';
            subpath = slash + 1;
        } else {
            snprintf(username, sizeof(username), "%s", user_start);
        }

        struct passwd *pw = getpwnam(username);
        if (pw == NULL) {
            return 404;
        }

        snprintf(base_dir, sizeof(base_dir), "%s/sws", pw->pw_dir);
        if (subpath[0] != '\0') {
            snprintf(target_path, sizeof(target_path), "%s/%s", base_dir, subpath);
        } else {
            snprintf(target_path, sizeof(target_path), "%s", base_dir);
        }
    }
    /* 3. Regular Document Root request */
    else {
        if (realpath(config->doc_root, base_dir) == NULL) {
            return 500;
        }
        const char *subpath = uri;
        if (*subpath == '/') {
            subpath++;
        }
        if (*subpath != '\0') {
            snprintf(target_path, sizeof(target_path), "%s/%s", base_dir, subpath);
        } else {
            snprintf(target_path, sizeof(target_path), "%s", base_dir);
        }
    }

    /* Verify existence and security path boundaries */
    char canonical_target[PATH_MAX];
    if (realpath(target_path, canonical_target) == NULL) {
        if (errno == ENOENT || errno == ENOTDIR) {
            return 404;
        } else if (errno == EACCES) {
            return 403;
        }
        return 404;
    }

    char canonical_base[PATH_MAX];
    if (realpath(base_dir, canonical_base) == NULL) {
        return 500;
    }

    size_t base_len = strlen(canonical_base);
    if (strncmp(canonical_target, canonical_base, base_len) != 0 ||
        (canonical_target[base_len] != '\0' && canonical_target[base_len] != '/')) {
        /* Path Traversal attempt caught */
        return 403;
    }

    struct stat st;
    if (stat(canonical_target, &st) < 0) {
        return 404;
    }

    if (S_ISDIR(st.st_mode)) {
        /* Check if index.html exists within this directory */
        char index_candidate[PATH_MAX];
        snprintf(index_candidate, sizeof(index_candidate), "%s/index.html", canonical_target);
        struct stat ist;
        if (stat(index_candidate, &ist) == 0 && S_ISREG(ist.st_mode)) {
            snprintf(resolved_path, max_len, "%s", index_candidate);
            *is_dir = 0;
            return 200;
        }

        /* No index.html, will generate directory listing */
        snprintf(resolved_path, max_len, "%s", canonical_target);
        *is_dir = 1;
        return 200;
    }

    if (S_ISREG(st.st_mode)) {
        snprintf(resolved_path, max_len, "%s", canonical_target);
        return 200;
    }

    return 403;
}

int resource_generate_dir_index(int client_fd, const char *dir_path, const char *req_uri, off_t *bytes_sent) {
    struct dirent **namelist;
    int n = scandir(dir_path, &namelist, NULL, alphasort);
    if (n < 0) {
        return -1;
    }

    char header_buf[BUFFER_SIZE];
    int hlen = snprintf(header_buf, sizeof(header_buf),
                        "<!DOCTYPE html>\n<html>\n<head>\n"
                        "<meta charset=\"utf-8\">\n"
                        "<title>Index of %s</title>\n"
                        "<style>body { font-family: monospace; padding: 20px; }</style>\n"
                        "</head>\n<body>\n"
                        "<h1>Index of %s</h1>\n<hr>\n<pre>\n",
                        req_uri, req_uri);

    ssize_t w = write(client_fd, header_buf, (size_t)hlen);
    if (w > 0) *bytes_sent += w;

    for (int i = 0; i < n; i++) {
        /* Ignore files starting with a dot (.) as required by RFC/manpage */
        if (namelist[i]->d_name[0] != '.') {
            char entry_path[PATH_MAX];
            snprintf(entry_path, sizeof(entry_path), "%s/%s", dir_path, namelist[i]->d_name);
            struct stat st;
            int is_subdir = (stat(entry_path, &st) == 0 && S_ISDIR(st.st_mode));

            char line_buf[1024];
            int llen = snprintf(line_buf, sizeof(line_buf),
                                "<a href=\"%s%s\">%s%s</a>\n",
                                namelist[i]->d_name, is_subdir ? "/" : "",
                                namelist[i]->d_name, is_subdir ? "/" : "");
            w = write(client_fd, line_buf, (size_t)llen);
            if (w > 0) *bytes_sent += w;
        }
        free(namelist[i]);
    }
    free(namelist);

    char footer_buf[512];
    int flen = snprintf(footer_buf, sizeof(footer_buf),
                        "</pre>\n<hr>\n<address>%s</address>\n</body>\n</html>\n",
                        SERVER_NAME);
    w = write(client_fd, footer_buf, (size_t)flen);
    if (w > 0) *bytes_sent += w;

    return 0;
}
