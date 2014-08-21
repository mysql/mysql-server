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

#ident "Copyright (c) 2009-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

// This test ensures that lock escalation occurs on the big transaction thread.
// locktree_escalation_stalls --max_i 1000000000 --n_small 16 --verbose

#include "test.h"
#include <db.h>
#include "toku_time.h"
#include "toku_pthread.h"

// from #include "threaded_stress_test_helpers.h"
// For each line of engine status output, look for lines that contain substrings
// that match any of the strings in the pattern string. The pattern string contains
// 0 or more strings separated by the '|' character, kind of like a regex.
static void print_matching_engine_status_rows(DB_ENV *env, const char *pattern) {
    uint64_t num_rows;
    env->get_engine_status_num_rows(env, &num_rows);
    uint64_t buf_size = num_rows * 128;
    const char *row;
    char *row_r;

    char *pattern_copy = toku_xstrdup(pattern);
    int num_patterns = 1;
    for (char *p = pattern_copy; *p != '\0'; p++) {
        if (*p == '|') {
            *p = '\0';
            num_patterns++;
        }
    }

    char *XMALLOC_N(buf_size, buf);
    int r = env->get_engine_status_text(env, buf, buf_size);
    invariant_zero(r);

    for (row = strtok_r(buf, "\n", &row_r); row != nullptr; row = strtok_r(nullptr, "\n", &row_r)) {
        const char *p = pattern_copy;
        for (int i = 0; i < num_patterns; i++, p += strlen(p) + 1) {
            if (strstr(row, p) != nullptr) {
                fprintf(stderr, "%s\n", row);
            }
        }
    }

    toku_free(pattern_copy);
    toku_free(buf);
    fflush(stderr);
}

static volatile int killed = 0;

// in a big transaction, insert a bunch of rows.
static void big_test(DB_ENV *env, DB *db, uint64_t max_i) {
    if (verbose)
        fprintf(stderr, "%u %s\n", toku_os_gettid(), __FUNCTION__);
    int r;

    DB_TXN *txn = NULL;
    r = env->txn_begin(env, NULL, &txn, 0);
    assert(r == 0);

    for (uint64_t i = 0; !killed && i < max_i; i++) {
        uint64_t k = htonl(i);
        DBT key = { .data = &k, .size = sizeof k };
        DBT val = { .data = &i, .size = sizeof i };
        uint64_t t_start = toku_current_time_microsec();
        r = db->put(db, txn, &key, &val, 0);
        assert(r == 0);
        uint64_t t_end = toku_current_time_microsec();
        uint64_t t_delta = t_end - t_start;
        if (t_delta >= 1000000) {
            fprintf(stderr, "%u %s i=%" PRIu64 " %" PRIu64 "\n", toku_os_gettid(), __FUNCTION__, i, t_delta);
            if (verbose) 
                print_matching_engine_status_rows(env, "locktree");
        }

        toku_pthread_yield();
    }

    r = txn->commit(txn, 0);
    assert(r == 0);
}

// insert a row in a single transaction.
static void small_test(DB_ENV *env, DB *db, uint64_t max_i) {
    if (verbose)
        fprintf(stderr, "%u %s\n", toku_os_gettid(), __FUNCTION__);
    int r;
    uint64_t k = toku_os_gettid(); // get a unique number
    for (uint64_t i = 0; !killed && i < max_i; i++) {
        DB_TXN *txn = NULL;
        r = env->txn_begin(env, NULL, &txn, 0);
        assert(r == 0);

        DBT key = { .data = &k, .size = sizeof k };
        DBT val = { .data = &i, .size = sizeof i };
        uint64_t t_start = toku_current_time_microsec();
        r = db->put(db, txn, &key, &val, 0);
        assert(r == 0);
        uint64_t t_end = toku_current_time_microsec();
        uint64_t t_delta = t_end - t_start;
        if (t_delta >= 1000000) {
            fprintf(stderr, "%u %s  %" PRIu64 "\n", toku_os_gettid(), __FUNCTION__, t_delta);
            assert(0);
        }
        
        r = txn->commit(txn, 0);
        assert(r == 0);

        toku_pthread_yield();
    }
}

struct test_args {
    DB_ENV *env;
    DB *db;
    uint64_t max_i;
    void (*test_f)(DB_ENV *env, DB *db, uint64_t max_i);
};

static void *test_f(void *args) {
    struct test_args *test_args = (struct test_args *) args;
    test_args->test_f(test_args->env, test_args->db, test_args->max_i);
    return args;
}

static void run_test(uint64_t max_i, int n_small) {
    int r;

    DB_ENV *env = NULL;
    r = db_env_create(&env, 0);
    assert(r == 0);
    env->set_errfile(env, stderr);
    r = env->set_cachesize(env, 8, 0, 1);
    assert(r == 0);
    r = env->set_lk_max_memory(env, 1000000000);
    assert(r == 0);
    r = env->open(env, TOKU_TEST_FILENAME, DB_INIT_LOCK+DB_INIT_MPOOL+DB_INIT_TXN+DB_INIT_LOG + DB_CREATE + DB_PRIVATE, S_IRWXU+S_IRWXG+S_IRWXO);
    assert(r == 0);
    
    DB *big_db = NULL;
    r = db_create(&big_db, env, 0);
    assert(r == 0);

    r = big_db->open(big_db, NULL, "big", NULL, DB_BTREE, DB_AUTO_COMMIT+DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO);
    assert(r == 0);

    DB *small_db = NULL;
    r = db_create(&small_db, env, 0);
    assert(r == 0);

    r = small_db->open(small_db, NULL, "small", NULL, DB_BTREE, DB_AUTO_COMMIT+DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO);
    assert(r == 0);

    struct test_args big_test_args = {
        env, big_db, max_i, big_test,
    };
    toku_pthread_t big_id;
    r = toku_pthread_create(&big_id, NULL, test_f, &big_test_args);
    assert(r == 0);

    struct test_args small_test_args[n_small];
    toku_pthread_t small_id[n_small];
    for (int i = 0; i < n_small; i++) {
        small_test_args[i] = { env, small_db, max_i, small_test };
        r = toku_pthread_create(&small_id[i], NULL, test_f, &small_test_args[i]);
        assert(r == 0);
    }
    
    void *big_ret;
    r = toku_pthread_join(big_id, &big_ret);
    assert(r == 0);

    killed = 1;

    for (int i = 0; i < n_small; i++) {
        void *small_ret;
        r = toku_pthread_join(small_id[i], &small_ret);
        assert(r == 0);
    }

    r = small_db->close(small_db, 0);
    assert(r == 0);

    r = big_db->close(big_db, 0);
    assert(r == 0);

    r = env->close(env, 0);
    assert(r == 0);
}

int test_main (int argc, char * const argv[]) {
    int r;
    uint64_t max_i = 10000;
    int n_small = 1;

    // parse_args(argc, argv);
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose++;
            continue;
        }
        if (strcmp(argv[i], "-q") == 0) {
            if (verbose > 0) verbose--;
            continue;
        }
        if (strcmp(argv[i], "--max_i") == 0 && i+1 < argc) {
            max_i = atoll(argv[++i]);
            continue;
        }
        if (strcmp(argv[i], "--n_small") == 0 && i+1 < argc) {
            n_small = atoi(argv[++i]);
            continue;
        }
    }

    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    r = toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO);
    assert(r == 0);
    
    run_test(max_i, n_small);

    return 0;
}
