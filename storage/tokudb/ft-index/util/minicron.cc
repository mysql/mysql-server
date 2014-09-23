/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
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

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#include <toku_portability.h>
#include <errno.h>
#include <string.h>

#include "portability/toku_assert.h"
#include "util/minicron.h"

static void
toku_gettime (toku_timespec_t *a) {
    struct timeval tv;
    gettimeofday(&tv, 0);
    a->tv_sec  = tv.tv_sec;
    a->tv_nsec = tv.tv_usec * 1000LL;
}
    

static int
timespec_compare (toku_timespec_t *a, toku_timespec_t *b) {
    if (a->tv_sec > b->tv_sec) return 1;
    if (a->tv_sec < b->tv_sec) return -1;
    if (a->tv_nsec > b->tv_nsec) return 1;
    if (a->tv_nsec < b->tv_nsec) return -1;
    return 0;
}

// Implementation notes:
//  When calling do_shutdown or change_period, the mutex is obtained, the variables in the minicron struct are modified, and
//  the condition variable is signalled.  Possibly the minicron thread will miss the signal.  To avoid this problem, whenever
//  the minicron thread acquires the mutex, it must check to see what the variables say to do (e.g., should it shut down?).

static void*
minicron_do (void *pv)
{
    struct minicron *CAST_FROM_VOIDP(p, pv);
    toku_mutex_lock(&p->mutex);
    while (1) {
        if (p->do_shutdown) {
            toku_mutex_unlock(&p->mutex);
            return 0;
        }
        if (p->period_in_ms == 0) {
            // if we aren't supposed to do it then just do an untimed wait.
            toku_cond_wait(&p->condvar, &p->mutex);
        } 
        else if (p->period_in_ms <= 1000) {
            toku_mutex_unlock(&p->mutex);
            usleep(p->period_in_ms * 1000);
            toku_mutex_lock(&p->mutex);
        }
        else {
            // Recompute the wakeup time every time (instead of once per call to f) in case the period changges.
            toku_timespec_t wakeup_at = p->time_of_last_call_to_f;
            wakeup_at.tv_sec += (p->period_in_ms/1000);
            wakeup_at.tv_nsec += (p->period_in_ms % 1000) * 1000000;
            toku_timespec_t now;
            toku_gettime(&now);
            int compare = timespec_compare(&wakeup_at, &now);
            // if the time to wakeup has yet to come, then we sleep
            // otherwise, we continue
            if (compare > 0) {
                int r = toku_cond_timedwait(&p->condvar, &p->mutex, &wakeup_at);
                if (r!=0 && r!=ETIMEDOUT) fprintf(stderr, "%s:%d r=%d (%s)", __FILE__, __LINE__, r, strerror(r));
                assert(r==0 || r==ETIMEDOUT);
            }
        }
        // Now we woke up, and we should figure out what to do
        if (p->do_shutdown) {
            toku_mutex_unlock(&p->mutex);
            return 0;
        }
        if (p->period_in_ms > 1000) {
            toku_timespec_t now;
            toku_gettime(&now);
            toku_timespec_t time_to_call = p->time_of_last_call_to_f;
            time_to_call.tv_sec += p->period_in_ms/1000;
            time_to_call.tv_nsec += (p->period_in_ms % 1000) * 1000000;
            int compare = timespec_compare(&time_to_call, &now);
            if (compare <= 0) {
                toku_gettime(&p->time_of_last_call_to_f); // the measured period includes the time to make the call.
                toku_mutex_unlock(&p->mutex);
                int r = p->f(p->arg);
                assert(r==0);
                toku_mutex_lock(&p->mutex);
                
            }
        }
        else if (p->period_in_ms != 0) {
            toku_mutex_unlock(&p->mutex);
            int r = p->f(p->arg);
            assert(r==0);
            toku_mutex_lock(&p->mutex);
        }
    }
}

int
toku_minicron_setup(struct minicron *p, uint32_t period_in_ms, int(*f)(void *), void *arg)
{
    p->f = f;
    p->arg = arg;
    toku_gettime(&p->time_of_last_call_to_f);
    //printf("now=%.6f", p->time_of_last_call_to_f.tv_sec + p->time_of_last_call_to_f.tv_nsec*1e-9);
    p->period_in_ms = period_in_ms; 
    p->do_shutdown = false;
    toku_mutex_init(&p->mutex, 0);
    toku_cond_init (&p->condvar, 0);
    return toku_pthread_create(&p->thread, 0, minicron_do, p);
}
    
void
toku_minicron_change_period(struct minicron *p, uint32_t new_period)
{
    toku_mutex_lock(&p->mutex);
    p->period_in_ms = new_period;
    toku_cond_signal(&p->condvar);
    toku_mutex_unlock(&p->mutex);
}

/* unlocked function for use by engine status which takes no locks */
uint32_t
toku_minicron_get_period_in_seconds_unlocked(struct minicron *p)
{
    uint32_t retval = p->period_in_ms/1000;
    return retval;
}

/* unlocked function for use by engine status which takes no locks */
uint32_t
toku_minicron_get_period_in_ms_unlocked(struct minicron *p)
{
    uint32_t retval = p->period_in_ms;
    return retval;
}

int
toku_minicron_shutdown(struct minicron *p) {
    toku_mutex_lock(&p->mutex);
    assert(!p->do_shutdown);
    p->do_shutdown = true;
    //printf("%s:%d signalling\n", __FILE__, __LINE__);
    toku_cond_signal(&p->condvar);
    toku_mutex_unlock(&p->mutex);
    void *returned_value;
    //printf("%s:%d joining\n", __FILE__, __LINE__);
    int r = toku_pthread_join(p->thread, &returned_value);
    if (r!=0) fprintf(stderr, "%s:%d r=%d (%s)\n", __FILE__, __LINE__, r, strerror(r));
    assert(r==0);  assert(returned_value==0);
    toku_cond_destroy(&p->condvar);
    toku_mutex_destroy(&p->mutex);
    //printf("%s:%d shutdowned\n", __FILE__, __LINE__);
    return 0;
}

bool
toku_minicron_has_been_shutdown(struct minicron *p) {
    return p->do_shutdown;
}
