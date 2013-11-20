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
// verify that the BDB locker can detect deadlocks on the fly and allow
// the deadlock to be unwound by the deadlocked threads.  the main thread
// polls for deadlocks with the lock_detect function.
//
// A write locks L
// B write locks M
// A tries to write lock M && B tries to write lock L
// One of A or B gets the DEADLOCK error, the other waits
// A and B release their locks

#include "test.h"
#include "toku_pthread.h"
#include <portability/toku_atomic.h>

struct test_seq {
    int state;
    toku_mutex_t lock;
    toku_cond_t cv;
};

static void test_seq_init(struct test_seq *seq) {
    seq->state = 0;
    toku_mutex_init(&seq->lock, NULL);
    toku_cond_init(&seq->cv, NULL);
}

static void test_seq_destroy(struct test_seq *seq) {
    toku_mutex_destroy(&seq->lock);
    toku_cond_destroy(&seq->cv);
}

static void test_seq_sleep(struct test_seq *seq, int new_state) {
    toku_mutex_lock(&seq->lock);
    while (seq->state != new_state) {
        toku_cond_wait(&seq->cv, &seq->lock);
    }
    toku_mutex_unlock(&seq->lock);
}

static void test_seq_next_state(struct test_seq *seq) {
    toku_mutex_lock(&seq->lock);
    seq->state++;
    toku_cond_broadcast(&seq->cv);
    toku_mutex_unlock(&seq->lock);
}

struct locker_args {
    DB_ENV *db_env;
    struct test_seq *test_seq;
    int *deadlock_count;
};

static void *run_locker_a(void *arg) {
    struct locker_args *locker_args = (struct locker_args *) arg;
    DB_ENV *db_env = locker_args->db_env;
    struct test_seq *test_seq = locker_args->test_seq;
    int r;

    uint32_t locker_a;
    r = db_env->lock_id(db_env, &locker_a); assert(r == 0);

    DBT object_l = { .data = (char *) "L", .size = 1 };
    DBT object_m = { .data = (char *) "M", .size = 1 };

    test_seq_sleep(test_seq, 0);
    DB_LOCK lock_a_l;
    r = db_env->lock_get(db_env, locker_a, DB_LOCK_NOWAIT, &object_l, DB_LOCK_WRITE, &lock_a_l); assert(r == 0);
    test_seq_next_state(test_seq);

    test_seq_sleep(test_seq, 2);
    DB_LOCK lock_a_m;
    bool m_locked = false;
    r = db_env->lock_get(db_env, locker_a, 0, &object_m, DB_LOCK_WRITE, &lock_a_m); 
    assert(r == 0 || r == DB_LOCK_DEADLOCK);
    if (r == 0)
        m_locked = true;

    r = db_env->lock_put(db_env, &lock_a_l); assert(r == 0);

    if (m_locked) {
        r = db_env->lock_put(db_env, &lock_a_m); assert(r == 0);
    } else {
        (void) toku_sync_fetch_and_add(locker_args->deadlock_count, 1);
        if (verbose) printf("%s:%u m deadlock\n", __FUNCTION__, __LINE__);
    }

    r = db_env->lock_id_free(db_env, locker_a); assert(r == 0);

    return arg;
}

static void *run_locker_b(void *arg) {
    struct locker_args *locker_args = (struct locker_args *) arg;
    DB_ENV *db_env = locker_args->db_env;
    struct test_seq *test_seq = locker_args->test_seq;
    int r;

    uint32_t locker_b;
    r = db_env->lock_id(db_env, &locker_b); assert(r == 0);

    DBT object_l = { .data = (char *) "L", .size = 1 };
    DBT object_m = { .data = (char *) "M", .size = 1 };

    test_seq_sleep(test_seq, 1);
    DB_LOCK lock_b_m;
    r = db_env->lock_get(db_env, locker_b, DB_LOCK_NOWAIT, &object_m, DB_LOCK_WRITE, &lock_b_m); assert(r == 0);
    test_seq_next_state(test_seq);

    test_seq_sleep(test_seq, 2);
    DB_LOCK lock_b_l;
    bool l_locked = false;
    r = db_env->lock_get(db_env, locker_b, 0, &object_l, DB_LOCK_WRITE, &lock_b_l); 
    assert(r == 0 || r == DB_LOCK_DEADLOCK);
    if (r == 0)
        l_locked = true;

    r = db_env->lock_put(db_env, &lock_b_m); assert(r == 0);

    if (l_locked) {
        r = db_env->lock_put(db_env, &lock_b_l); assert(r == 0);
    } else {
        (void) toku_sync_fetch_and_add(locker_args->deadlock_count, 1);
        if (verbose) printf("%s:%u l deadlock\n", __FUNCTION__, __LINE__);
    }

    r = db_env->lock_id_free(db_env, locker_b); assert(r == 0);

    return arg;
}

static void simple_deadlock(DB_ENV *db_env) {
    int r;

    struct test_seq test_seq; ZERO_STRUCT(test_seq); test_seq_init(&test_seq);

    int deadlock_count = 0 ;

    toku_pthread_t tid_a;
    struct locker_args args_a = { db_env, &test_seq, &deadlock_count };
    r = toku_pthread_create(&tid_a, NULL, run_locker_a, &args_a); assert(r == 0);

    toku_pthread_t tid_b;
    struct locker_args args_b = { db_env, &test_seq, &deadlock_count };
    r = toku_pthread_create(&tid_b, NULL, run_locker_b, &args_b); assert(r == 0);

    while (1) {
        sleep(10);
        int rejected = 0;
        r = db_env->lock_detect(db_env, 0, DB_LOCK_YOUNGEST, &rejected); assert(r == 0);
        if (verbose)
            printf("%s %d\n", __FUNCTION__, rejected);
        if (rejected == 0)
            break;
    }

    void *ret = NULL;
    r = toku_pthread_join(tid_a, &ret); assert(r == 0);
    r = toku_pthread_join(tid_b, &ret); assert(r == 0);

    assert(deadlock_count == 1);

    test_seq_destroy(&test_seq);
}

int test_main(int argc, char * const argv[]) {
    uint64_t cachesize = 0;
    int do_txn = 1;
    const char *db_env_dir = TOKU_TEST_FILENAME;
    int db_env_open_flags = DB_CREATE | DB_PRIVATE | DB_INIT_MPOOL | DB_INIT_TXN | DB_INIT_LOCK | DB_INIT_LOG | DB_THREAD;

    // parse_args(argc, argv);
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose++;
            continue;
        }
        if (strcmp(argv[i], "-q") == 0 || strcmp(argv[i], "--quiet") == 0) {
            if (verbose > 0)
                verbose--;
            continue;
        }
        assert(0);
    }

    // setup env
    int r;
    char rm_cmd[strlen(db_env_dir) + strlen("rm -rf ") + 1];
    snprintf(rm_cmd, sizeof(rm_cmd), "rm -rf %s", db_env_dir);
    r = system(rm_cmd); assert(r == 0);

    r = toku_os_mkdir(db_env_dir, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH); assert(r == 0);

    DB_ENV *db_env = NULL;
    r = db_env_create(&db_env, 0); assert(r == 0);
    if (cachesize) {
        const uint64_t gig = 1 << 30;
        r = db_env->set_cachesize(db_env, cachesize / gig, cachesize % gig, 1); assert(r == 0);
    }
    if (!do_txn)
        db_env_open_flags &= ~(DB_INIT_TXN | DB_INIT_LOG);
    r = db_env->open(db_env, db_env_dir, db_env_open_flags, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH); assert(r == 0);

    // run test
    simple_deadlock(db_env);

    // close env
    r = db_env->close(db_env, 0); assert(r == 0); db_env = NULL;

    return 0;
}
