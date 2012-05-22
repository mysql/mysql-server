/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
#include <test.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

int test_main(int argc, char *const argv[]) {
    int i;
    for (i=1; i<argc; i++) {
        int fd = open(argv[i], O_RDONLY);
        printf("%s: %d %d\n", argv[i], fd, errno);
        if (fd >= 0) close(fd);
    }
    return 0;
}
