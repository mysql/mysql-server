/* -*- mode: C; c-basic-offset: 4 -*- */

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

#ident "Copyright (c) 2010-2013 Tokutek Inc.  All rights reserved."

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>

#include <toku_portability.h>
#include <toku_assert.h>
#include <portability/toku_fair_rwlock.h>
#include <portability/toku_pthread.h>
#include <portability/toku_time.h>
#include <util/frwlock.h>
#include <util/rwlock.h>
#include "rwlock_condvar.h"

toku_mutex_t mutex;
toku::frwlock w;

static void grab_write_lock(bool expensive) {
    toku_mutex_lock(&mutex);
    w.write_lock(expensive);
    toku_mutex_unlock(&mutex);
}

static void release_write_lock(void) {
    toku_mutex_lock(&mutex);
    w.write_unlock();
    toku_mutex_unlock(&mutex);
}

static void grab_read_lock(void) {
    toku_mutex_lock(&mutex);
    w.read_lock();
    toku_mutex_unlock(&mutex);
}

static void release_read_lock(void) {
    toku_mutex_lock(&mutex);
    w.read_unlock();
    toku_mutex_unlock(&mutex);
}

static void *do_cheap_wait(void *arg) {
    grab_write_lock(false);
    release_write_lock();
    return arg;
}

static void *do_expensive_wait(void *arg) {
    grab_write_lock(true);
    release_write_lock();
    return arg;
}

static void *do_read_wait(void *arg) {
    grab_read_lock();
    release_read_lock();
    return arg;
}

static void launch_cheap_waiter(void) {
    toku_pthread_t tid;
    int r = toku_pthread_create(&tid, NULL, do_cheap_wait, NULL); 
    assert_zero(r);
    toku_pthread_detach(tid);
    sleep(1);
}

static void launch_expensive_waiter(void) {
    toku_pthread_t tid;
    int r = toku_pthread_create(&tid, NULL, do_expensive_wait, NULL); 
    assert_zero(r);
    toku_pthread_detach(tid);
    sleep(1);
}

static void launch_reader(void) {
    toku_pthread_t tid;
    int r = toku_pthread_create(&tid, NULL, do_read_wait, NULL); 
    assert_zero(r);
    toku_pthread_detach(tid);
    sleep(1);
}

static bool locks_are_expensive(void) {
    toku_mutex_lock(&mutex);
    assert(w.write_lock_is_expensive() == w.read_lock_is_expensive());
    bool is_expensive = w.write_lock_is_expensive();
    toku_mutex_unlock(&mutex);
    return is_expensive;
}

static void test_write_cheapness(void) {
    toku_mutex_init(&mutex, NULL);    
    w.init(&mutex);

    // single expensive write lock
    grab_write_lock(true);
    assert(locks_are_expensive());
    release_write_lock();
    assert(!locks_are_expensive());

    // single cheap write lock
    grab_write_lock(false);
    assert(!locks_are_expensive());
    release_write_lock();
    assert(!locks_are_expensive());

    // multiple read locks
    grab_read_lock();
    assert(!locks_are_expensive());
    grab_read_lock();
    grab_read_lock();
    assert(!locks_are_expensive());
    release_read_lock();
    release_read_lock();
    release_read_lock();
    assert(!locks_are_expensive());

    // expensive write lock and cheap writers waiting
    grab_write_lock(true);
    launch_cheap_waiter();
    assert(locks_are_expensive());
    launch_cheap_waiter();
    launch_cheap_waiter();
    assert(locks_are_expensive());
    release_write_lock();
    sleep(1);
    assert(!locks_are_expensive());

    // cheap write lock and expensive writer waiter
    grab_write_lock(false);
    launch_expensive_waiter();
    assert(locks_are_expensive());
    release_write_lock();
    sleep(1);

    // expensive write lock and expensive waiter
    grab_write_lock(true);
    launch_expensive_waiter();
    assert(locks_are_expensive());
    release_write_lock();
    sleep(1);

    // cheap write lock and cheap waiter
    grab_write_lock(false);
    launch_cheap_waiter();
    assert(!locks_are_expensive());
    release_write_lock();
    sleep(1);

    // read lock held and cheap waiter
    grab_read_lock();
    launch_cheap_waiter();
    assert(!locks_are_expensive());
    // add expensive waiter
    launch_expensive_waiter();
    assert(locks_are_expensive());
    release_read_lock();
    sleep(1);

    // read lock held and expensive waiter
    grab_read_lock();
    launch_expensive_waiter();
    assert(locks_are_expensive());
    // add expensive waiter
    launch_cheap_waiter();
    assert(locks_are_expensive());
    release_read_lock();
    sleep(1);

    // cheap write lock held and waiting read
    grab_write_lock(false);
    launch_reader();
    assert(!locks_are_expensive());
    launch_expensive_waiter();
    toku_mutex_lock(&mutex);
    assert(w.write_lock_is_expensive());
    // tricky case here, because we have a launched reader
    // that should be in the queue, a new read lock
    // should piggy back off that
    assert(!w.read_lock_is_expensive());
    toku_mutex_unlock(&mutex);
    release_write_lock();
    sleep(1);

    // expensive write lock held and waiting read
    grab_write_lock(true);
    launch_reader();
    assert(locks_are_expensive());
    launch_cheap_waiter();
    assert(locks_are_expensive());
    release_write_lock();
    sleep(1);

    w.deinit();
    toku_mutex_destroy(&mutex);
}

int main (int UU(argc), const char* UU(argv[])) {
    test_write_cheapness();
    return 0;
}
