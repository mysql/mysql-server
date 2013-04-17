/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
#include <stdio.h>
#include <sys/types.h>
#include <toku_assert.h>
#include <unistd.h>
#include <toku_time.h>

int main(void) {
    int r;
    struct timespec ts;

    r = toku_clock_gettime(CLOCK_REALTIME, &ts);
    assert(r == 0);
    sleep(10);
    r = toku_clock_gettime(CLOCK_REALTIME, &ts);
    assert(r == 0);
    
    return 0;
}
