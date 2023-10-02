// Connection handler for the server
#include "req.h"
#include <vector>
#include <string>
#include <cstring>
#include <stdlib.h>
#include <stdint.h>
#include <netinet/ip.h>

enum {
    STATE_REQ = 0,
    STATE_RESP = 1,
    STATE_END = 2, // Finished delete 
};

struct Conn {
    int fd = -1;
    uint32_t state = STATE_REQ;
    size_t rbuf_size = 0;
    uint8_t rbuf[4 + k_max]; // 4 byte header + 4096 bytes body
    size_t wbuf_size = 0;
    size_t wbuf_sent = 0;
    uint8_t wbuf[4 + k_max]; // 4 byte header + 4096 bytes body
};

static void state_req(Conn *conn);
static void state_resp(Conn *conn);

// ======================== 
// Reader State Machine 
// ========================
static bool try_one_request(Conn *conn) {
    if (conn->rbuf_size < 4) {
        // Only have 4 bytes, not enough even for header
        return false;
    }

    uint32_t len = 0;
    memcpy(&len, &conn->rbuf[0], 4);
    if (len > k_max) {
        msg("too large");
        conn->state = STATE_END;
        return false;
    }

    if(4+len > conn->rbuf_size) {
        // Not enough data for the full request
        return false;
    }

    uint32_t rescode = 0;
    uint32_t wlen = 0;
    int32_t err = do_request(
        &conn->rbuf[4], len,
        &rescode, &conn->wbuf[4 + 4], &wlen
    );
    if (err) {
        conn->state = STATE_END;
        return false;
    }

    wlen += 4;
    memcpy(&conn->wbuf[0], &len, 4);
    memcpy(&conn->wbuf[4], &conn->rbuf[4], len); // move to read buf
    conn->wbuf_size = 4 + len;

    size_t remain = conn->rbuf_size - (4 + len);
    if (remain) { 
        // TODO: Replace memmove
        memmove(&conn->rbuf[0], &conn->rbuf[4 + len], remain);
    }
    conn->rbuf_size = remain;

    conn->state = STATE_RESP;
    state_resp(conn);
    msg("response sent");

    // continue outer loop if request was fully processed (try_fill_buffer?)
    return (conn->state == STATE_REQ);
}

// Fill the buffer into the connection
static bool try_fill_buffer(Conn *conn) {
    assert(conn->rbuf_size < sizeof(conn->rbuf));
    ssize_t rv = 0; 
    do {
        // Get difference between buffer size and current size?
        size_t cap = sizeof(conn->rbuf) - conn->rbuf_size;
        rv = read(conn->fd, &conn->rbuf[conn->rbuf_size], cap);
    } while(rv < 0 && errno == EAGAIN); // RETRY if EAGAIN
    if (rv <0 && errno != EAGAIN) {
        return false;
    }
    if (rv<0) {
        msg("Read error");
        conn->state = STATE_END;
        return false;
    }

    if (rv == 0) {
        if (conn->rbuf_size > 0) {
            msg("EOF, Unexpected");
        } else {
            msg("EOF");
        } 
        conn->state = STATE_END;
        return false;
    }

    conn->rbuf_size += (size_t)rv;
    assert(conn->rbuf_size <= sizeof(conn->rbuf));

    while (try_one_request(conn)) {}
    return (conn->state == STATE_REQ);
}

static void state_req(Conn *conn) {
    while(try_fill_buffer(conn)) {}
}

// ======================== 
// Writer State Machine 
// ========================
static bool try_flush_buffer(Conn *conn) {
    ssize_t rv = 0;
    do {
        size_t remain = conn->wbuf_size - conn->wbuf_sent;
        rv = write(conn->fd, &conn->wbuf[conn->wbuf_sent], remain);
    } while(rv < 0 && errno == EAGAIN); 

    if (rv<0 && errno == EAGAIN) {
        return false;
    }

    if (rv<0) {
        msg("Write error");
        conn->state = STATE_END;
        return false;
    }
    conn->wbuf_sent += (size_t)rv;
    assert(conn->wbuf_sent <= conn->wbuf_size);
    if (conn->wbuf_sent == conn->wbuf_size) {
        // response was sent change to request state
        conn->state = STATE_REQ;
        conn->wbuf_size = 0;
        conn->wbuf_sent = 0;
        return false;
    }

    return true;
}

static void state_resp(Conn *conn) {
    while(try_flush_buffer(conn)) {}
}

// ======================== 
// connection handler 
// ========================
static void connection_io(Conn *conn) {
    if(conn->state == STATE_REQ) {
        state_req(conn);
    } else if(conn->state == STATE_REQ) {
        state_resp(conn);
    } else {
        assert(0); // unexpected error
    }
}
static void conn_put(std::vector<Conn *> &fd2conn, struct Conn *conn) {
    if(fd2conn.size() <= (size_t)conn->fd) {
        fd2conn.resize(conn->fd + 1);
    }
    msg("new connection");
    fd2conn[conn->fd] = conn;
}

static int32_t accept_new_conn(std::vector<Conn *> &fd2conn, int fd) {
    // Accept 
    struct sockaddr_in client_addr = {};
    socklen_t socklen = sizeof(client_addr);
    int connfd = accept(fd, (struct sockaddr *)&client_addr, &socklen);
    if (connfd < 0) {
        msg("accept error");
        return -1;
    }

    // change new connection to nonblocking mode 
    fd_set_nb(connfd);

    // Create new connection struct 
    struct Conn *conn = (struct Conn *)malloc(sizeof(struct Conn)); // allocate a new Conn
    if (!conn) {
        close(connfd);
        return -1;
    }

    conn->fd = connfd;
    conn->state = STATE_REQ;
    conn->rbuf_size = 0;
    conn->wbuf_size = 0;
    conn->wbuf_sent = 0;
    conn_put(fd2conn, conn);
    return 0;
}