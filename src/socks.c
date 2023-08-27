#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/select.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include "socks.h"

// Create a socket, bind an address to it and listen
// RETURN:
//      A file descriptor for the socket.
//      On error, -1
static int open_socket(uint16_t port) {
    const int fd = socket(PF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }

    const int val = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(int)) < 0) {
        return -1;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        return -1;
    }

    if (listen(fd, 10) < 0) {
        return -1;
    }

    return fd;
}

// Await and accept a connection.
// PARAMS:
//      fd - server socket file descriptor
//      ip - client socket IPv4 addr
// RETURN:
//      A file descriptor for the client socket.
//      On error, -1
static int accept_connection(int fd, char ip[INET_ADDRSTRLEN]) {
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    fd = accept(fd, (struct sockaddr *)&addr, &addr_len);
    ip = inet_ntoa(addr.sin_addr);
    return fd;
}

// Read n bytes from file associated with the file
// descriptor to buf.
// Note: if no bytes are available for 100ms, the
// function will terminate successfully.
// RETURN:
//      Number of bytes read.
//      On error, -1
static ssize_t readn(int fd, char *buf, size_t n) {
    ssize_t byte_count;
    size_t all_byte_count = 0;
    fd_set is_ready;
    struct timeval tiemout;
    int status;

    while (all_byte_count < n) {
        FD_ZERO(&is_ready);
        FD_SET(fd, &is_ready);
        tiemout.tv_sec = 0;
        tiemout.tv_usec = 100;

        status = select(fd + 1, &is_ready, NULL, NULL, &tiemout);

        if (status < 0) {
            return -1;
        } else if (status == 0) {
            return all_byte_count;
        } else {
            byte_count = read(fd, buf + all_byte_count, n - all_byte_count);
            if (byte_count <= 0) {
                return -1;
            }
            all_byte_count += byte_count;
        }
    }

    return all_byte_count;
}

// Write n bytes from buf to file associated with the
// file descriptor.
// RETURN:
//      Number of bytes written.
//      On error, -1
static ssize_t write_all(int fd, const void *buf, size_t n) {
    size_t bytes_written = 0;
    ssize_t count_or_err = 0;

    while (bytes_written != n) {
        count_or_err = write(fd, buf + bytes_written, n - bytes_written);
        if (count_or_err < 0) {
            return -1;
        }

        bytes_written += count_or_err;
    }

    return bytes_written;
}

ssize_t first_solid(const char *str) {
    size_t i = 0;
    while (isspace(str[i])) {
        i++;
    }
    return str[i] == '\0' ? -1 : i;
}

ssize_t last_solid(const char *str, size_t len) {
    ssize_t i = len - 1;
    while (i >= 0 && isspace(str[i])) {
        i--;
    }
    return i;
}

char *trim(char *str) {
    if (str == NULL) {
        return NULL;
    }

    const ssize_t s = first_solid(str);
    if (s < 0) {
        str[0] = '\0';
        return str;
    }

    const size_t len = strlen(str + s) + s;
    const ssize_t e = last_solid(str, len);
    str[e + 1] = '\0';

    return str + s;
}

ssize_t strnfind(const char *str, size_t str_len, const char *pat) {
    size_t pat_len = strlen(pat);
    if (str_len < pat_len) {
        return -1;
    }

    for (size_t i = 0; i <= str_len - pat_len; i++) {
        if (strncmp(str + i, pat, pat_len) == 0) {
            return i;
        }
    }
    return -1;
}

// Read content from req->fd, parse it and
// put the results in req.
// Note: this function uses strtok.
// PARAMS:
//      req - you must assign the req client socket file descriptor
//      before passing it, you can optionally also assign the ip.
//      Rest of fields will be stored here. req->content needs to
//      be free'd.
// ERRORS:
//      errno will be set to EPROTONOSUPPORT on any parsing errors
// RETURN:
//      0 if no error, -1 otherwise
static int parse_http_req(struct http_req *req) {
    char buf[1024];

    ssize_t buf_len = readn(req->fd, buf, sizeof(buf));
    if (buf_len < 0) {
        return -1;
    }

    ssize_t split_index = strnfind(buf, buf_len, "\r\n");
    if (split_index < 0) {
        errno = EPROTONOSUPPORT;
        return -1;
    }

    const char *verb = strtok(buf, " ");
    const char *url = strtok(NULL, " ");
    if (verb == NULL || url == NULL) {
        errno = EPROTONOSUPPORT;
        return -1;
    }
    memcpy(req->verb, verb, sizeof(req->verb));
    memcpy(req->url, url, sizeof(req->url));
    req->content = NULL;
    req->content_len = 0;
    req->was_handled = 0;
#ifndef NDEBUG
    printf("%s %s\n", verb, url);
#endif /* NDEBUG */

    while (split_index > 0) {
        split_index += 2;
        memmove(buf, buf + split_index, buf_len);
        buf_len -= split_index;

        split_index = strnfind(buf, buf_len, "\r\n");
        if (split_index < 0) {
            const ssize_t bytes_read = readn(req->fd, buf + buf_len, sizeof(buf) - buf_len);
            if (bytes_read < 0) {
                return -1;
            }
            buf_len += bytes_read;

            split_index = strnfind(buf, buf_len, "\r\n");
            if (split_index < 0) {
                errno = EPROTONOSUPPORT;
                return -1;
            }
        }

        buf[split_index] = '\0';
        const char *key = trim(strtok(buf, ":"));
        const char *val = trim(strtok(NULL, "\0"));
        if (key != NULL && strcmp(key, "Content-Length") == 0) {
            req->content_len = atoi(val);
        }
#ifndef NDEBUG
        printf("%s %s\n", key, val);
#endif /* NDEBUG */
    }

    if (req->content_len == 0) {
        return 0;
    }

    req->content = malloc(req->content_len + 1);
    if (req->content == NULL) {
        return -1;
    }

    memcpy(req->content, buf + split_index + 2, buf_len - 2);
    req->content[req->content_len] = '\0';

    if (readn(req->fd, req->content + buf_len, req->content_len - buf_len) < 0) {
        free(req->content);
        errno = EPROTONOSUPPORT;
        return -1;
    }
    return 0;
}

void http_resn(struct http_req *req, uint32_t status, char *body, size_t body_len) {
    req->was_handled = 1;
    char *buf = malloc(17 + body_len);
    if (buf == NULL) {
        close(req->fd);
        return;
    }

    int header_len = sprintf(buf, "HTTP/1.0 %d\r\n\r\n", status);
    if (body != NULL) {
        memcpy(buf + header_len, body, body_len);
    }

    if (write_all(req->fd, buf, header_len + body_len) < 0) {
        perror("write_all");
    }

#ifndef NDEBUG
    printf("RESPONDED WITH %d\n", status);
#endif /* NDEBUG */

    free(buf);
    close(req->fd);
}

void http_res(struct http_req *req, uint32_t status, char *body) {
    http_resn(req, status, body, body == NULL ? 0 : strlen(body));
}

// TODO docs
static void file_req_handler(struct http_req *req) {
    FILE *fp = fopen(req->path, "r");
    if (fp == NULL) {
        http_res(req, 500, NULL);
        return;
    }

    long file_size;
    if (fseek(fp, 0L, SEEK_END) < 0 || (file_size = ftell(fp)) < 0) {
        fclose(fp);
        http_res(req, 500, NULL);
        return;
    }
    rewind(fp);

    char *content = malloc(file_size);
    if (content == NULL) {
        fclose(fp);
        http_res(req, 500, NULL);
        return;
    }

    if (fread(content, 1, file_size, fp) != file_size) {
        fclose(fp);
        http_res(req, 500, NULL);
        return;
    }
    fclose(fp);

    http_resn(req, 200, content, file_size);
    free(content);
}

int gen_file_routes(const char *root, size_t root_len, struct route *routes, size_t *file_count) {
    DIR *dirp = opendir(root);
    if (dirp == NULL) {
        return -1;
    }

    char *subroot;
    struct dirent *item;
    while ((item = readdir(dirp))) {
        if (strcmp(item->d_name, ".") == 0 || strcmp(item->d_name, "..") == 0) {
            continue;
        }

        if (item->d_type == DT_REG) {
            sprintf(routes[*file_count].url, "%s/%s", root + root_len, item->d_name);
            sprintf(routes[*file_count].path, "%s/%s", root, item->d_name);
#ifndef NDEBUG
            printf("%s\n", routes[*file_count].path);
#endif /* NDEBUG */
            strcpy(routes[*file_count].verb, "GET");
            routes[*file_count].handler = file_req_handler;
            (*file_count)++;

            if (strcmp(item->d_name, "index.html") == 0) {
                sprintf(routes[*file_count].url, "%s/", root + root_len);
                sprintf(routes[*file_count].path, "%s/%s", root, item->d_name);
                strcpy(routes[*file_count].verb, "GET");
                routes[*file_count].handler = file_req_handler;
                (*file_count)++;
            }
        } else if (item->d_type == DT_DIR) {
            subroot = malloc(strlen(root) + strlen(item->d_name) + 2);
            if (subroot == NULL) {
                closedir(dirp);
                return -1;
            }
            sprintf(subroot, "%s/%s", root, item->d_name);
            if (gen_file_routes(subroot, root_len, routes, file_count) < 0) {
                free(subroot);
                closedir(dirp);
                return -1;
            }
            free(subroot);
        }
    }

    closedir(dirp);
    return 0;
}

// Check if url matches template.
// Template syntax:
//      * -> checks any set of chars until '/'
// Example:
//      does_url_match("/*", "/anything") -> 1
//      does_url_match("/*", "/anything/") -> 0
//      does_url_match("/*/", "/anything/") -> 1
//      does_url_match("/hello/*", "/hello/world") -> 1
// RETURN:
//      1 if matches, 0 otherwise
static int does_url_match(const char *template, const char *url) {
    if (strcmp(template, url) == 0) {
        return 1;
    }

    size_t i = 0;
    size_t j = 0;
    while (template[i] && url[j]) {
        if (template[i] == url[j]) {
            i++;
            j++;
        } else if (template[i] == '*') {
            i++;
            while (url[j] && url[j] != '/') {
                j++;
            }
        } else {
            break;
        }
    }

    return template[i] == url[j] || template[i] == '*';
}

void socks_run(uint16_t port, struct route *routes, size_t routes_len) {
    const int server_fd = open_socket(port);
    if (server_fd < 0) {
        perror("open_socket");
        exit(EXIT_FAILURE);
    }

    fd_set sock_set;
    fd_set sock_set_copy;
    FD_ZERO(&sock_set);
    FD_SET(server_fd, &sock_set);

    char ip[INET_ADDRSTRLEN] = {0};
    struct http_req req;
    while (1) {
#ifndef NDEBUG
        printf("\nWaiting for a client.\n");
#endif /* NDEBUG */

        sock_set_copy = sock_set;
        select(FD_SETSIZE, &sock_set_copy, NULL, NULL, NULL);

        for (size_t i = 0; i < FD_SETSIZE; i++) {
            if (!FD_ISSET(i, &sock_set_copy)) {
                continue;
            }

            if (i == server_fd) {
                const int client_fd = accept_connection(server_fd, ip);
                if (client_fd < 0) {
                    perror("accept_connection");
                    continue;
                }
                FD_SET(client_fd, &sock_set);
                continue;
            }

            req.fd = i;
            memcpy(req.ip, ip, sizeof(ip));
            if (parse_http_req(&req) < 0) {
                perror("parse_http_req");
                http_res(&req, 400, NULL);
                continue;
            }

            for (size_t i = 0; i < routes_len; i++) {
                if (strcmp(routes[i].verb, req.verb) == 0 && does_url_match(routes[i].url, req.url)) {
                    memcpy(req.path, routes[i].path, sizeof(req.path));
                    routes[i].handler(&req);
                    break;
                }
            }

            if (!req.was_handled) {
                http_res(&req, 404, NULL);
            }
            FD_CLR(i, &sock_set);
        }
    }
}
