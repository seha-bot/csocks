#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

int open_socket(uint16_t port) {
    int fd = socket(PF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("Can't create a socket");
        return -1;
    }

    int val = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(int)) < 0) {
        perror("Can't set socket option");
        return EXIT_FAILURE;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("Can't bind socket");
        return -1;
    }

    return fd;
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
        int new_socket = accept(socket, (struct sockaddr*)&addr, &addr_len);
        if (new_socket < 0) {
            perror("Can't accept");
            return EXIT_FAILURE;
        }

        int ip[4];
        ip[0] = addr.sin_addr.s_addr & 0xFF;
        ip[1] = (addr.sin_addr.s_addr >> 8) & 0xFF;
        ip[2] = (addr.sin_addr.s_addr >> 16) & 0xFF;
        ip[3] = (addr.sin_addr.s_addr >> 24) & 0xFF;

        printf("%d.%d.%d.%d\n", ip[0], ip[1], ip[2], ip[3]);

        char buf[1024];
        ssize_t bytes_read = read(new_socket, buf, 1024);

        if (bytes_read < 0) {
            perror("Can't read");
            return EXIT_FAILURE;
        }

        printf("%s\n", buf);
        int n = sprintf(buf, "HTTP/1.1 200\r\n\r\n:(");

        ssize_t bytes_written = write_all(new_socket, buf, n);
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
