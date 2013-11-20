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

  TokuDB, Tokutek Fractal Tree Indexing Library.
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

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
/* Time {m,l,s}fence vs.xchgl for a memory barrier. */

/* Timing numbers:
 * Intel T2500 2GHZ

do1       9.0ns/loop
mfence:  29.0ns/loop  (marginal cost=  20.0ns)
sfence:  17.3ns/loop  (marginal cost=   8.3ns)
lfence:  23.6ns/loop  (marginal cost=  14.6ns)
 xchgl:  35.8ns/loop  (marginal cost=  26.8ns)

* AMD Athlon 64 X2 Dual Core Processor 4200+
  Timings are more crazy

do1      20.6ns/loop
mfence:  12.9ns/loop  (marginal cost=  -7.6ns)
sfence:   8.4ns/loop  (marginal cost= -12.1ns)
lfence:  20.2ns/loop  (marginal cost=  -0.3ns)
 xchgl:  16.6ns/loop  (marginal cost=  -3.9ns)

do1      13.0ns/loop
mfence:  25.6ns/loop  (marginal cost=  12.6ns)
sfence:  21.0ns/loop  (marginal cost=   8.1ns)
lfence:  12.9ns/loop  (marginal cost=  -0.1ns)
 xchgl:  29.3ns/loop  (marginal cost=  16.3ns)

*/


#include <sys/time.h>
#include <stdio.h>
#include <portability/toku_atomic.h>

enum { COUNT = 100000000 };

static inline void xchgl (void) {
    {
	/*
	 * According to the Intel Architecture Software Developer's
	 * Manual, Volume 3: System Programming Guide
	 * (http://www.intel.com/design/pro/manuals/243192.htm), page
	 * 7-6, "For the P6 family processors, locked operations
	 * serialize all outstanding load and store operations (that
	 * is, wait for them to complete)."  
	 * Since xchg is locked by default, it is one way to do membar.
	 */
	int x=0, y;
	asm volatile ("xchgl %0,%1" :"=r" (x) :"m" (y), "0" (x) :"memory");
   }
}

static inline void mfence (void) {
    asm volatile ("mfence":::"memory");
}

static inline void lfence (void) {
    asm volatile ("lfence":::"memory");
}

static inline void sfence (void) {
    asm volatile ("sfence":::"memory");
}

int lock_for_lock_and_unlock;
static inline void lock_and_unlock (void) {
    (void)toku_sync_lock_test_and_set(&lock_for_lock_and_unlock, 1);
    toku_sync_lock_release(&lock_for_lock_and_unlock);
}


double tdiff (struct timeval *start, struct timeval *end) {
    return ((end->tv_sec-start->tv_sec + 1e-6*(end->tv_usec + start->tv_usec))/COUNT)*1e9;
}

double nop_cost;

void do1 (volatile int *x) {
    int i;
    struct timeval start, end;
    gettimeofday(&start, 0);
    for (i=0; i<COUNT; i++) {
	x[0]++;
	x[1]++;
	x[2]++;
	x[3]++;
    }
    gettimeofday(&end, 0);
    printf("do1    %6.1fns/loop\n", nop_cost=tdiff(&start, &end));
}

#define doit(name) void do ##name (volatile int *x) { \
    int i;                      \
    struct timeval start, end;  \
    gettimeofday(&start, 0);    \
    for (i=0; i<COUNT; i++) {   \
	x[0]++;                 \
	x[1]++;                 \
	name();                 \
	x[2]++;                 \
	x[3]++;                 \
    }                           \
    gettimeofday(&end, 0);      \
    double this_cost = tdiff(&start, &end); \
    printf("%15s:%6.1fns/loop  (marginal cost=%6.1fns)\n",  #name, this_cost, this_cost-nop_cost); \
}


doit(mfence)
doit(lfence)
doit(sfence)
doit(xchgl)
doit(lock_and_unlock);

int main (int argc __attribute__((__unused__)), 
	  char *argv[] __attribute__((__unused__))) {
    int x[4];
    int i;
    for (i=0; i<4; i++) {
	do1(x);
	domfence(x);
	dosfence(x);
	dolfence(x);
	doxchgl(x);
	dolock_and_unlock(x);
    }
    return 0;
}
