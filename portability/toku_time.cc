/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."

#include "toku_time.h"

#if !defined(HAVE_CLOCK_REALTIME)

#include <errno.h>
#include <mach/clock.h>
#include <mach/mach.h>

int toku_clock_gettime(clockid_t clk_id, struct timespec *ts) {
    if (clk_id != CLOCK_REALTIME) {
        // dunno how to fake any of the other types of clock on osx
        return EINVAL;
    }
    // We may want to share access to cclock for performance, but that requires
    // initialization and destruction that's more complex than it's worth for
    // OSX right now.  Some day we'll probably just use pthread_once or
    // library constructors.
    clock_serv_t cclock;
    mach_timespec_t mts;
    host_get_clock_service(mach_host_self(), REALTIME_CLOCK, &cclock);
    clock_get_time(cclock, &mts);
    mach_port_deallocate(mach_task_self(), cclock);
    ts->tv_sec = mts.tv_sec;
    ts->tv_nsec = mts.tv_nsec;
    return 0;
}
#else // defined(HAVE_CLOCK_REALTIME)

#include <time.h>
int toku_clock_gettime(clockid_t clk_id, struct timespec *ts) {
    return clock_gettime(clk_id, ts);
}

#endif
