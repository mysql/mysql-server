/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
#include <test.h>
#include <stdio.h>
#include <toku_assert.h>
#include <toku_time.h>

int test_main(int argc, char *const argv[]) {
    int r;
    struct timeval tv;
    struct timezone tz;

    r = gettimeofday(&tv, 0);
    assert(r == 0);
    
    return 0;
}
