#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>

static void die(const char *s) {
    int err = errno;
    fprintf(stderr, "[%d]: %s\n", err, s);
    abort();
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

    char msg[] = "hello";
    write(fd, msg, strlen(msg));

    char rbuf[64] = {};
    ssize_t rlen = read(fd, rbuf, sizeof(rbuf) - 1);
    if (rlen < 0) {
        die("read");
    }

    printf("server said: %s\n", rbuf);
    close(fd);
    return 0;
}
