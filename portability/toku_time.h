/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."

#ifndef TOKU_TIME_H
#define TOKU_TIME_H

#include "config.h"
#include <time.h>
#include <sys/time.h>


static inline float toku_tdiff (struct timeval *a, struct timeval *b) {
    return (float)((a->tv_sec - b->tv_sec) + 1e-6 * (a->tv_usec - b->tv_usec));
}

#if !defined(HAVE_CLOCK_REALTIME) // OS X does not have clock_gettime, use clock_get_time
# include <errno.h>
# include <mach/clock.h>
# include <mach/mach.h>
typedef int clockid_t;
# define CLOCK_REALTIME 0x01867234  // just something bogus, it doesn't matter
#endif

static inline int
toku_clock_gettime(clockid_t clk_id, struct timespec *ts)
{
#if !defined(HAVE_CLOCK_REALTIME)
    if (clk_id != CLOCK_REALTIME) {
        // dunno how to fake any of the other ones on osx
        return EINVAL;
    }
    clock_serv_t cclock;
    mach_timespec_t mts;
    host_get_clock_service(mach_host_self(), REALTIME_CLOCK, &cclock);
    clock_get_time(cclock, &mts);
    mach_port_deallocate(mach_task_self(), cclock);
    ts->tv_sec = mts.tv_sec;
    ts->tv_nsec = mts.tv_nsec;
    return 0;
#else
    return clock_gettime(clk_id, ts);
#endif
}

// *************** Performance timers ************************
// What do you really want from a performance timer:
//  (1) Can determine actual time of day from the performance time.
//  (2) Time goes forward, never backward.
//  (3) Same time on different processors (or even different machines).
//  (4) Time goes forward at a constant rate (doesn't get faster and slower)
//  (5) Portable.
//  (6) Getting the time is cheap.
// Unfortuately it seems tough to get Properties 1-5.  So we go for Property 6,, but we abstract it.
// We offer a type tokutime_t which can hold the time.
// This type can be subtracted to get a time difference.
// We can get the present time cheaply.
// We can convert this type to seconds (but that can be expensive).
// The implementation is to use RDTSC (hence we lose property 3: not portable).
// Recent machines have constant_tsc in which case we get property (4).
// Recent OSs on recent machines (that have RDTSCP) fix the per-processor clock skew, so we get property (3).
// We get property 2 with RDTSC (as long as there's not any skew).
// We don't even try to get propety 1, since we don't need it.
// The decision here is that these times are really accurate only on modern machines with modern OSs.
typedef uint64_t tokutime_t;             // Time type used in by tokutek timers.

// The value of tokutime_t is not specified here. 
// It might be microseconds since 1/1/1970 (if gettimeofday() is
// used), or clock cycles since boot (if rdtsc is used).  Or something
// else.
// Two tokutime_t values can be subtracted to get a time difference.
// Use tokutime_to_seconds to that convert difference  to seconds.
// We want get_tokutime() to be fast, but don't care so much about tokutime_to_seconds();
//
// For accurate time calculations do the subtraction in the right order:
//   Right:  tokutime_to_seconds(t1-t2);
//   Wrong   tokutime_to_seconds(t1)-toku_time_to_seconds(t2);
// Doing it the wrong way is likely to result in loss of precision.
// A double can hold numbers up to about 53 bits.  RDTSC which uses about 33 bits every second, so that leaves
// 2^20 seconds from booting (about 2 weeks) before the RDTSC value cannot be represented accurately as a double.
//
double tokutime_to_seconds(tokutime_t)  __attribute__((__visibility__("default"))); // Convert tokutime to seconds.

// Get tokutime.  We want this to be fast, so we expose the implementation as RDTSC.
static inline tokutime_t get_tokutime (void) {
    uint32_t lo, hi;
    __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
    return (uint64_t)hi << 32 | lo;
}

static inline uint64_t toku_current_time_usec(void) {
    struct timeval t;
    gettimeofday(&t, NULL);
    return t.tv_sec * 1000000UL + t.tv_usec;
}

#endif
