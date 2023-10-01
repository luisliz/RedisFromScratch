#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include "common.h"

// Set fb to non-blocking
static void fb_set_nb(int fd) {
    errno = 0;
    int flags = fcntl(fd, F_GETFL, 0);
    if (errno) {
        die("fcntl");
        return;
    }

    // Convert to non blocking
    flags |= O_NONBLOCK;

    errno = 0;
    (void)fcntl(fd, F_SETFL, flags);
    if (errno) {
        die("fcntl");
    }
}

static int32_t single_request(int connfd) {
    /// 4 bytes header
    char rbuf[4 + k_max + 1];
    errno = 0;
    int32_t err = read_full(connfd, rbuf, 4);
    if (err) {
        if (errno == 0) {
            msg("EOF");
        } else {
            msg("read err");
        }
        return err;
    }

    uint32_t len = 0;
    memcpy(&len, rbuf, 4);
    if (len > k_max) {
        msg("too large");
        return -1;
    }

    // Request body
    err = read_full(connfd, &rbuf[4], len);
    if (err) {
        msg("read err");
        return err;
    }

    rbuf[4 + len] = '\0';
    printf("client sayd: %s\n", &rbuf[4]);

    // reply 
    const char reply[] = "world";
    char wbuf[4 + sizeof(reply)];
    len = (uint32_t)strlen(reply);
    memcpy(wbuf, &len, 4);
    memcpy(&wbuf[4], reply, len);
    return write_all(connfd, wbuf, 4 + len); 
}


int main() {
    int val = 1;

    /* Create a socket
    AF_INET -> IPv4
    SOCK_STREAM -> TCP
    0 -> Protocol (0 = default) */
    int fd = socket(AF_INET, SOCK_STREAM, 0);

    /* Socket options 
    SOL_SOCKET -> Socket level
    SO_REUSEADDR -> Reuse address */
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

    struct sockaddr_in addr;
    addr.sin_family = AF_INET; // ipv4 
    addr.sin_port = htons(6379); // port
    addr.sin_addr.s_addr = ntohl(0); // 0.0.0.0 addr 
    int rv = bind(fd, (const sockaddr *)&addr, sizeof(addr));
    if (rv) {
        die("bind");
    }

    rv = listen(fd, SOMAXCONN);
    if (rv) {
        die("listen");
    }

    while (true) {
        struct sockaddr_in client_addr = {};
        socklen_t socklen = sizeof(client_addr);
        int connfd = accept(fd, (struct sockaddr *)&client_addr, &socklen);
        if (connfd < 0) {
            continue; // skip errors 
        }

        while(true) {
            int32_t err = single_request(connfd);
            if (err) {
                break;
            }
        }
        close(connfd);
    }

    return 0;
}