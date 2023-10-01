#include "common.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>

static int32_t query(int fd, const char *text) {
    uint32_t len = (uint32_t)strlen(text);
    if (len > k_max) {
        return -1;
    }

    char wbuf[4 + k_max];

    memcpy(wbuf, &len, 4);
    memcpy(&wbuf[4], text, len);
    if (int32_t err = write_all(fd, wbuf, 4 + len)) {
        return err;
    }

    // 4 byte header 

    char rbuf[4 + k_max + 1];
    errno = 0;
    int32_t err = read_full(fd, rbuf, 4);
    if (err) {
        if (errno == 0) {
            msg("EOF");
        } else {
            msg("read err");
        }
        return err;
    }

    memcpy(&len, rbuf, 4);
    if (len > k_max) {
        msg("too large");
        return -1;
    }

    err = read_full(fd, &rbuf[4], len);
    if (err) {
        msg("read err");
        return err;
    }

    rbuf[4 + len] = '\0';
    printf("server said: %s\n", &rbuf[4]);
    return 0;
}

int main() {
    int fd = socket(AF_INET, SOCK_STREAM, 0); // ipv4, tcp, default protocol
    if (fd < 0) {
        die("socket");
    }

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET; // ipv4
    addr.sin_port = htons(6379); // port
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); //localhost
    int rv = connect(fd, (const sockaddr *)&addr, sizeof(addr));
    if (rv < 0) {
        die("connect");
    }

    int32_t err = query(fd, "hello1");
    if (err) {
        goto L_DONE;
    }

    err = query(fd, "hello2");
    if (err) {
        goto L_DONE;
    }

    err = query(fd, "hello3");
    if (err) {
        goto L_DONE;
    }

L_DONE:
    close(fd);
    return 0;
}
