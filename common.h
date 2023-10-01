#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

const size_t k_max = 4096;

static void die(const char *s) {
    int err = errno;
    fprintf(stderr, "[%d]: %s\n", err, s);
    abort();
}

static void msg(const char *s) {
    fprintf(stderr, "%s\n", s);
}

// Set fb to non-blocking
static void fd_set_nb(int fd) {
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
