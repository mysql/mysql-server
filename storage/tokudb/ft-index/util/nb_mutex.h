/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ifndef UTIL_NB_MUTEX_H
#define UTIL_NB_MUTEX_H
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

#include "rwlock.h"

//Use case:
// General purpose non blocking mutex with properties:
// 1. one writer at a time

// An external mutex must be locked when using these functions.  An alternate
// design would bury a mutex into the nb_mutex itself.  While this may
// increase parallelism at the expense of single thread performance, we
// are experimenting with a single higher level lock.

typedef struct nb_mutex *NB_MUTEX;
struct nb_mutex {
    struct rwlock lock;
};

// initialize an nb mutex
static __attribute__((__unused__))
void
nb_mutex_init(NB_MUTEX nb_mutex) {
    rwlock_init(&nb_mutex->lock);
}

// destroy a read write lock
static __attribute__((__unused__))
void
nb_mutex_destroy(NB_MUTEX nb_mutex) {
    rwlock_destroy(&nb_mutex->lock);
}

// obtain a write lock
// expects: mutex is locked
static inline void nb_mutex_lock(NB_MUTEX nb_mutex, toku_mutex_t *mutex) {
    rwlock_write_lock(&nb_mutex->lock, mutex);
}

// release a write lock
// expects: mutex is locked

static inline void nb_mutex_unlock(NB_MUTEX nb_mutex) {
    rwlock_write_unlock(&nb_mutex->lock);
}

static inline void nb_mutex_wait_for_users(NB_MUTEX nb_mutex, toku_mutex_t *mutex) {
    rwlock_wait_for_users(&nb_mutex->lock, mutex);
}

// returns: the number of writers who are waiting for the lock

static inline int nb_mutex_blocked_writers(NB_MUTEX nb_mutex) {
    return rwlock_blocked_writers(&nb_mutex->lock);
}

// returns: the number of writers

static inline int nb_mutex_writers(NB_MUTEX nb_mutex) {
    return rwlock_writers(&nb_mutex->lock);
}

// returns: the sum of the number of readers, pending readers,
// writers, and pending writers
static inline int nb_mutex_users(NB_MUTEX nb_mutex) {
    return rwlock_users(&nb_mutex->lock);
}

#endif // UTIL_NB_MUTEX_H
