#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

int open_socket(uint16_t port) {
    const int fd = socket(PF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("Can't create a socket");
        return -1;
    }

    const int val = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(int)) < 0) {
        perror("Can't set socket option");
        return EXIT_FAILURE;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("Can't bind socket");
        return -1;
    }

    return fd;
}

#define READ_CHUNK_SIZE 1024

ssize_t read_to_str(int fd, char **str) {
    fd_set set;
    struct timeval timeout;
    int status;

    char buf[READ_CHUNK_SIZE];
    ssize_t byte_count = 0;
    size_t all_byte_count = 0;
    char *str_grow = NULL;

    do {
        FD_ZERO(&set);
        FD_SET(fd, &set);
        timeout.tv_sec = 0;
        timeout.tv_usec = 100;

        status = select(fd + 1, &set, NULL, NULL, &timeout);
        if (status < 0) {
            if (str_grow != NULL) {
                free(str_grow);
            }
            return -1;
        } else if (status == 0) {
            break;
        }

        byte_count = read(fd, buf, READ_CHUNK_SIZE);
        if (byte_count < 0) {
            if (str_grow != NULL) {
                free(str_grow);
            }
            return -1;
        }

        str_grow = realloc(str_grow, all_byte_count + byte_count + 1);
        if (str_grow == NULL) {
            exit(EXIT_FAILURE);
        }

        memcpy(str_grow + all_byte_count, buf, byte_count);
        all_byte_count += byte_count;
    } while (byte_count == READ_CHUNK_SIZE);

    str_grow[all_byte_count] = '\0';
    *str = str_grow;
    return all_byte_count + 1;
}

ssize_t write_all(int fd, const void *buf, size_t n) {
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

void expand_ip(int ip[4], in_addr_t ip_val) {
    ip[0] = ip_val & 0xFF;
    ip[1] = (ip_val >> 8) & 0xFF;
    ip[2] = (ip_val >> 16) & 0xFF;
    ip[3] = (ip_val >> 24) & 0xFF;
}

int main(void) {
    printf("Creating socket...\n");
    int socket = open_socket(8080);
    if (socket < 0) {
        return EXIT_FAILURE;
    }

    if (listen(socket, 10) < 0) {
        return EXIT_FAILURE;
    }

    struct sockaddr_in addr;
    socklen_t addr_len;
    while (1) {
        addr_len = sizeof(addr);
        int new_socket = accept(socket, (struct sockaddr *)&addr, &addr_len);
        if (new_socket < 0) {
            perror("Can't accept");
            return EXIT_FAILURE;
        }

        int ip[4];
        expand_ip(ip, addr.sin_addr.s_addr);
        printf("%d.%d.%d.%d\n", ip[0], ip[1], ip[2], ip[3]);

        char *str;
        ssize_t bytes_read = read_to_str(new_socket, &str);
        if (bytes_read < 0) {
            perror("Can't read bytes");
            return EXIT_FAILURE;
        }

        printf("%s\n", str);
        free(str);

        char buf[] = "HTTP/1.1 200\r\n\r\n:(";
        ssize_t bytes_written = write_all(new_socket, buf, sizeof(buf));
        if (bytes_written < 0) {
            perror("Can't write");
            return EXIT_FAILURE;
        }

        printf("Closing new socket...\n");
        if (close(new_socket) < 0) {
            perror("Can't close new socket");
            return EXIT_FAILURE;
        }
    }
}
