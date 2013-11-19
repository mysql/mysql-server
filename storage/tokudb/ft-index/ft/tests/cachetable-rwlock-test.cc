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



#include "test.h"

// test create and destroy

static void
test_create_destroy (void) {
    struct rwlock the_rwlock, *rwlock = &the_rwlock;

    rwlock_init(rwlock);
    rwlock_destroy(rwlock);
}

// test read lock and unlock with no writers

static void
test_simple_read_lock (int n) {
    struct rwlock the_rwlock, *rwlock = &the_rwlock;

    rwlock_init(rwlock);
    assert(rwlock_readers(rwlock) == 0);
    int i;
    for (i=1; i<=n; i++) {
        rwlock_read_lock(rwlock, 0);
        assert(rwlock_readers(rwlock) == i);
        assert(rwlock_users(rwlock) == i);
    }
    for (i=n-1; i>=0; i--) {
        rwlock_read_unlock(rwlock);
        assert(rwlock_readers(rwlock) == i);
        assert(rwlock_users(rwlock) == i);
    }
    rwlock_destroy(rwlock);
}

// test write lock and unlock with no readers

static void
test_simple_write_lock (void) {
    struct rwlock the_rwlock, *rwlock = &the_rwlock;

    rwlock_init(rwlock);
    assert(rwlock_users(rwlock) == 0);
    rwlock_write_lock(rwlock, 0);
    assert(rwlock_writers(rwlock) == 1);
    assert(rwlock_users(rwlock) == 1);
    rwlock_write_unlock(rwlock);
    assert(rwlock_users(rwlock) == 0);
    rwlock_destroy(rwlock);
}

struct rw_event {
    int e;
    struct rwlock the_rwlock;
    toku_mutex_t mutex;
};

static void
rw_event_init (struct rw_event *rwe) {
    rwe->e = 0;
    rwlock_init(&rwe->the_rwlock);
    toku_mutex_init(&rwe->mutex, 0);
}

static void
rw_event_destroy (struct rw_event *rwe) {
    rwlock_destroy(&rwe->the_rwlock);
    toku_mutex_destroy(&rwe->mutex);
}

static void *
test_writer_priority_thread (void *arg) {
    struct rw_event *CAST_FROM_VOIDP(rwe, arg);

    toku_mutex_lock(&rwe->mutex);
    rwlock_write_lock(&rwe->the_rwlock, &rwe->mutex);
    rwe->e++; assert(rwe->e == 3);
    toku_mutex_unlock(&rwe->mutex);
    sleep(1);
    toku_mutex_lock(&rwe->mutex);
    rwe->e++; assert(rwe->e == 4);
    rwlock_write_unlock(&rwe->the_rwlock);
    toku_mutex_unlock(&rwe->mutex);
    
    return arg;
}

// test writer priority over new readers

static void
test_writer_priority (void) {
    struct rw_event rw_event, *rwe = &rw_event;
    ZERO_STRUCT(rw_event);
    int r;

    rw_event_init(rwe);
    toku_mutex_lock(&rwe->mutex);
    rwlock_read_lock(&rwe->the_rwlock, &rwe->mutex);
    sleep(1);
    rwe->e++; assert(rwe->e == 1);
    toku_mutex_unlock(&rwe->mutex);

    toku_pthread_t tid;
    r = toku_pthread_create(&tid, 0, test_writer_priority_thread, rwe);
    sleep(1);
    toku_mutex_lock(&rwe->mutex);
    rwe->e++; assert(rwe->e == 2);
    toku_mutex_unlock(&rwe->mutex);

    sleep(1);
    toku_mutex_lock(&rwe->mutex);
    rwlock_read_unlock(&rwe->the_rwlock);
    toku_mutex_unlock(&rwe->mutex);
    sleep(1);
    toku_mutex_lock(&rwe->mutex);
    rwlock_read_lock(&rwe->the_rwlock, &rwe->mutex);
    rwe->e++; assert(rwe->e == 5);
    toku_mutex_unlock(&rwe->mutex);
    sleep(1);
    toku_mutex_lock(&rwe->mutex);
    rwlock_read_unlock(&rwe->the_rwlock);
    toku_mutex_unlock(&rwe->mutex);

    void *ret;
    r = toku_pthread_join(tid, &ret); assert(r == 0);

    rw_event_destroy(rwe);
}

// test single writer

static void *
test_single_writer_thread (void *arg) {
    struct rw_event *CAST_FROM_VOIDP(rwe, arg);

    toku_mutex_lock(&rwe->mutex);
    rwlock_write_lock(&rwe->the_rwlock, &rwe->mutex);
    rwe->e++; assert(rwe->e == 3);
    assert(rwlock_writers(&rwe->the_rwlock) == 1);
    rwlock_write_unlock(&rwe->the_rwlock);
    toku_mutex_unlock(&rwe->mutex);
    
    return arg;
}

static void
test_single_writer (void) {
    struct rw_event rw_event, *rwe = &rw_event;
    ZERO_STRUCT(rw_event);
    int r;

    rw_event_init(rwe);
    assert(rwlock_writers(&rwe->the_rwlock) == 0);
    toku_mutex_lock(&rwe->mutex);
    rwlock_write_lock(&rwe->the_rwlock, &rwe->mutex);
    assert(rwlock_writers(&rwe->the_rwlock) == 1);
    sleep(1);
    rwe->e++; assert(rwe->e == 1);
    toku_mutex_unlock(&rwe->mutex);

    toku_pthread_t tid;
    r = toku_pthread_create(&tid, 0, test_single_writer_thread, rwe);
    sleep(1);
    toku_mutex_lock(&rwe->mutex);
    rwe->e++; assert(rwe->e == 2);
    assert(rwlock_writers(&rwe->the_rwlock) == 1);
    assert(rwlock_users(&rwe->the_rwlock) == 2);
    rwlock_write_unlock(&rwe->the_rwlock);
    toku_mutex_unlock(&rwe->mutex);

    void *ret;
    r = toku_pthread_join(tid, &ret); assert(r == 0);

    assert(rwlock_writers(&rwe->the_rwlock) == 0);
    rw_event_destroy(rwe);
}

int
test_main(int argc, const char *argv[]) {
    default_parse_args(argc, argv);
    test_create_destroy();
    test_simple_read_lock(0);
    test_simple_read_lock(42);
    test_simple_write_lock();
    test_writer_priority();
    test_single_writer();
    
    return 0;
}
