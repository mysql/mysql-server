/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
#include <stdio.h>
#include <sys/types.h>
#include <toku_assert.h>
#include <toku_time.h>

int main(void) {
    int r;
    struct timeval tv;
    struct timezone tz;

    r = gettimeofday(&tv, &tz);
    assert(r == 0);
    
    return 0;
}
