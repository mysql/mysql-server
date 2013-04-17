/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
#include <test.h>
#include <stdio.h>
#include <stdlib.h>
#include <toku_assert.h>
#include <string.h>
#include <unistd.h>

int verbose;

int test_main(int argc, char *const argv[]) {
    int i;
    int n = 1;

    for (i=1; i<argc; i++) {
        char *arg = argv[i];
        if (strcmp(arg, "-v") == 0 || strcmp(arg, "--verbose") == 0)
            verbose++;
        n = atoi(arg);
    }

    for (i=0; i<1000; i++) {
        if (verbose) {
            printf("usleep %d\n", i); fflush(stdout);
        }
        usleep(n);
    }

    return 0;
}
