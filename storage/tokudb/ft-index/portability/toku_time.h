/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
/*
COPYING CONDITIONS NOTICE:

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation, and provided that the
  following conditions are met:

      * Redistributions of source code must retain this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below).

      * Redistributions in binary form must reproduce this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below) in the documentation and/or other materials
        provided with the distribution.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
  02110-1301, USA.

COPYRIGHT NOTICE:

  TokuFT, Tokutek Fractal Tree Indexing Library.
  Copyright (C) 2007-2013 Tokutek, Inc.

DISCLAIMER:

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

UNIVERSITY PATENT NOTICE:

  The technology is licensed by the Massachusetts Institute of
  Technology, Rutgers State University of New Jersey, and the Research
  Foundation of State University of New York at Stony Brook under
  United States of America Serial No. 11/760379 and to the patents
  and/or patent applications resulting from it.

PATENT MARKING NOTICE:

  This software is covered by US Patent No. 8,185,551.
  This software is covered by US Patent No. 8,489,638.

PATENT RIGHTS GRANT:

  "THIS IMPLEMENTATION" means the copyrightable works distributed by
  Tokutek as part of the Fractal Tree project.

  "PATENT CLAIMS" means the claims of patents that are owned or
  licensable by Tokutek, both currently or in the future; and that in
  the absence of this license would be infringed by THIS
  IMPLEMENTATION or by using or running THIS IMPLEMENTATION.

  "PATENT CHALLENGE" shall mean a challenge to the validity,
  patentability, enforceability and/or non-infringement of any of the
  PATENT CLAIMS or otherwise opposing any of the PATENT CLAIMS.

  Tokutek hereby grants to you, for the term and geographical scope of
  the PATENT CLAIMS, a non-exclusive, no-charge, royalty-free,
  irrevocable (except as stated in this section) patent license to
  make, have made, use, offer to sell, sell, import, transfer, and
  otherwise run, modify, and propagate the contents of THIS
  IMPLEMENTATION, where such license applies only to the PATENT
  CLAIMS.  This grant does not include claims that would be infringed
  only as a consequence of further modifications of THIS
  IMPLEMENTATION.  If you or your agent or licensee institute or order
  or agree to the institution of patent litigation against any entity
  (including a cross-claim or counterclaim in a lawsuit) alleging that
  THIS IMPLEMENTATION constitutes direct or contributory patent
  infringement, or inducement of patent infringement, then any rights
  granted to you under this License shall terminate as of the date
  such litigation is filed.  If you or your agent or exclusive
  licensee institute or order or agree to the institution of a PATENT
  CHALLENGE, then Tokutek may terminate any rights granted to you
  under this License.
*/

#pragma once

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."

#include "toku_config.h"

#include <time.h>
#include <sys/time.h>
#include <stdint.h>

static inline float toku_tdiff (struct timeval *a, struct timeval *b) {
    return (float)((a->tv_sec - b->tv_sec) + 1e-6 * (a->tv_usec - b->tv_usec));
}

#if !defined(HAVE_CLOCK_REALTIME)
// OS X does not have clock_gettime, we fake clockid_t for the interface, and we'll implement it with clock_get_time.
typedef int clockid_t;
// just something bogus, it doesn't matter, we just want to make sure we're
// only supporting this mode because we're not sure we can support other modes
// without a real clock_gettime()
#define CLOCK_REALTIME 0x01867234
#endif
int toku_clock_gettime(clockid_t clk_id, struct timespec *ts) __attribute__((__visibility__("default")));

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

// Get the value of tokutime for right now.  We want this to be fast, so we expose the implementation as RDTSC.
static inline tokutime_t toku_time_now(void) {
    uint32_t lo, hi;
    __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
    return (uint64_t)hi << 32 | lo;
}

static inline uint64_t toku_current_time_microsec(void) {
    struct timeval t;
    gettimeofday(&t, NULL);
    return t.tv_sec * (1UL * 1000 * 1000) + t.tv_usec;
}
