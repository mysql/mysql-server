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
#ident "$Id$"

#ifndef TOKU_MINICRON_H
#define TOKU_MINICRON_H

#include <toku_pthread.h>
#include <toku_time.h>
#include "fttypes.h"


// Specification:
// A minicron is a miniature cron job for executing a job periodically inside a pthread.
// To create a minicron,
//   1) allocate a "struct minicron" somewhere.
//      Rationale:  This struct can be stored inside another struct (such as the cachetable), avoiding a malloc/free pair.
//   2) call toku_minicron_setup, specifying a period (in milliseconds), a function, and some arguments.
//      If the period is positive then the function is called periodically (with the period specified)
//      Note: The period is measured from when the previous call to f finishes to when the new call starts.
//            Thus, if the period is 5 minutes, and it takes 8 minutes to run f, then the actual periodicity is 13 minutes.
//      Rationale:  If f always takes longer than f to run, then it will get "behind".  This module makes getting behind explicit.
//   3) When finished, call toku_minicron_shutdown.
//   4) If you want to change the period, then call toku_minicron_change_period.    The time since f finished is applied to the new period
//      and the call is rescheduled.  (If the time since f finished is more than the new period, then f is called immediately).

struct minicron {
    toku_pthread_t thread;
    toku_timespec_t time_of_last_call_to_f;
    toku_mutex_t mutex;
    toku_cond_t  condvar;
    int (*f)(void*);
    void *arg;
    uint32_t period_in_ms;
    bool      do_shutdown;
};

int toku_minicron_setup (struct minicron *s, uint32_t period_in_ms, int(*f)(void *), void *arg);
void toku_minicron_change_period(struct minicron *p, uint32_t new_period);
uint32_t toku_minicron_get_period_in_seconds_unlocked(struct minicron *p);
uint32_t toku_minicron_get_period_in_ms_unlocked(struct minicron *p);
int toku_minicron_shutdown(struct minicron *p);
bool toku_minicron_has_been_shutdown(struct minicron *p);


#endif
