#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <sys/socket.h>

static void msg(const char *s) {
    fprintf(stderr, "%s\n", s);
}

static void die(const char *s) {
    int err = errno; 
    fprintf(stderr, "[%d]: %s\n", err, s);
    abort();
}


static void handle_conn(int connfd) {
    char rbuf[64] = {};
    ssize_t rlen = read(connfd, rbuf, sizeof(rbuf) - 1);
    if (rlen < 0) {
        msg("read err");
        return;
    }
    printf("client sayd: %s\n", rbuf);

    char wbuf[] = "world";
    write(connfd, wbuf, strlen(wbuf));
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
            continue;
        }

        // Handle connection 
        handle_conn(connfd);

        close(connfd);
    }

    return 0;
}