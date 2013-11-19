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
#include <pthread.h>

// verify that a commit of a big txn does not block the commits of other txn's
// commit writer (0) happens before bigtxn commit (1) happens before checkpoint (2)
static int test_state = 0;

static void *checkpoint_thread(void *arg) {
    sleep(1);
    DB_ENV *env = (DB_ENV *) arg;
    printf("%s start\n", __FUNCTION__);
    int r = env->txn_checkpoint(env, 0, 0, 0);
    assert(r == 0);
    printf("%s done\n", __FUNCTION__);
    int old_state = toku_sync_fetch_and_add(&test_state, 1);
    assert(old_state == 2);
    return arg;
}

struct writer_arg {
    DB_ENV *env;
    DB *db;
    int k;
};

static void *w_thread(void *arg) {
    sleep(2);
    struct writer_arg *warg = (struct writer_arg *) arg;
    DB_ENV *env = warg->env;
    DB *db = warg->db;
    int k = warg->k;
    printf("%s start\n", __FUNCTION__);
    int r;
    DB_TXN *txn;
    r = env->txn_begin(env, NULL, &txn, 0);
    assert(r == 0);
    if (1) {
        DBT key = { .data = &k, .size = sizeof k };
        DBT val = { .data = &k, .size = sizeof k };
        r = db->put(db, txn, &key, &val, 0);
        assert(r == 0);
    }
    r = txn->commit(txn, 0);
    assert(r == 0);
    printf("%s done\n", __FUNCTION__);
    int old_state = toku_sync_fetch_and_add(&test_state, 1);
    assert(old_state == 0);
    return arg;
}

static void bigtxn_progress(TOKU_TXN_PROGRESS progress, void *extra) {
    printf("%s %" PRIu64 " %" PRIu64 " %p\n", __FUNCTION__, progress->entries_processed, progress->entries_total, extra);
    sleep(1);
}

int test_main (int argc, char *const argv[]) {
    int r;
    int N = 25000;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--N") == 0 && i+1 < argc) {
            N = atoi(argv[++i]);
            continue;
        }
    }

    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    r = toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO);
    assert(r == 0);

    DB_ENV *env;
    r = db_env_create(&env, 0);
    assert(r == 0);

    // avoid locktree escalation by picking a big enough lock tree
    r = env->set_lk_max_memory(env, 128*1024*1024);
    assert(r == 0);

    r = env->open(env, TOKU_TEST_FILENAME, DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN|DB_PRIVATE, S_IRWXU+S_IRWXG+S_IRWXO);
    assert(r == 0);

    DB *db = NULL;
    r = db_create(&db, env, 0);
    assert(r == 0);

    r = db->open(db, NULL, "testit", NULL, DB_BTREE, DB_AUTO_COMMIT+DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO);
    assert(r == 0);

    DB_TXN *bigtxn = NULL;
    r = env->txn_begin(env, NULL, &bigtxn, 0); 
    assert(r == 0);

    // use a big key so that the rollback log spills
    char k[1024]; memset(k, 0, sizeof k);
    char v[8]; memset(v, 0, sizeof v);

    for (int i = 0; i < N; i++) {
        memcpy(k, &i, sizeof i);
        memcpy(v, &i, sizeof i);
        DBT key = { .data = k, .size = sizeof k };
        DBT val = { .data = v, .size = sizeof v };
        r = db->put(db, bigtxn, &key, &val, 0);
        assert(r == 0);
        if ((i % 10000) == 0)
            printf("put %d\n", i);
    }

    pthread_t checkpoint_tid = 0;
    r = pthread_create(&checkpoint_tid, NULL, checkpoint_thread, env);
    assert(r == 0);

    pthread_t w_tid = 0;
    struct writer_arg w_arg = { env, db, N };
    r = pthread_create(&w_tid, NULL, w_thread, &w_arg);
    assert(r == 0);

    r = bigtxn->commit_with_progress(bigtxn, 0, bigtxn_progress, NULL);
    assert(r == 0);
    int old_state = toku_sync_fetch_and_add(&test_state, 1);
    assert(old_state == 1);

    void *ret;
    r = pthread_join(w_tid, &ret);
    assert(r == 0);
    r = pthread_join(checkpoint_tid, &ret);
    assert(r == 0);

    r = db->close(db, 0);
    assert(r == 0);

    r = env->close(env, 0);
    assert(r == 0);

    return 0;
}
