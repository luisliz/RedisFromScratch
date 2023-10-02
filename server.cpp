#include "conn.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <vector>
#include <fcntl.h>
#include <poll.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <sys/socket.h>

int main() {
    int val = 1;

    /* Create a socket
    AF_INET -> IPv4
    SOCK_STREAM -> TCP
    0 -> Protocol (0 = default) */
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        die("socket");
    }

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

    // Map of client connections 
    std::vector<Conn*> fd2conn;
    fd_set_nb(fd); // Set to non-blocking

    std::vector<struct pollfd> poll_args;
    while (true) {
        poll_args.clear();

        // Set listener as first position
        struct pollfd pfd = {fd, POLLIN, 0};
        poll_args.push_back(pfd);
        for (Conn *conn : fd2conn) {
            if (!conn) {
                continue;
            }
            struct pollfd pfd = {};
            pfd.fd = conn->fd;
            // POLLIN -> Readable
            // POLLOUT -> Writable
            pfd.events = (conn->state == STATE_REQ) ? POLLIN : POLLOUT;
            pfd.events = pfd.events | POLLERR;
            poll_args.push_back(pfd);
        }

        int rv = poll(poll_args.data(), (nfds_t)poll_args.size(), 1000);
        if (rv < 0) {
            die("poll");
        }

        // Process the active connections 
        for (size_t i = 1; i < poll_args.size(); ++i) {
            if (poll_args[i].revents) {
                Conn *conn = fd2conn[poll_args[i].fd];
                connection_io(conn);
                if (conn->state == STATE_END) {
                    fd2conn[conn->fd] = NULL;
                    (void)close(conn->fd);
                    free(conn);
                }
            }
        }

        if (poll_args[0].revents) {
            (void)accept_new_conn(fd2conn, fd);
        }
    }
    return 0;
}