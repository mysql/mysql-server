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

// This test verifies that small txn's do not get stalled for a long time by lock escalation.
// Two lock trees are used by the test: a big lock tree and a small lock tree.
// One big txn grabs lots of write locks on the big lock tree. 
// Several small txn's grab a single write lock on the small lock tree.
// None of the locks conflict.
// Eventually, the locks for the big txn consume all of the lock tree memory, so lock escalation runs.
// The test measures the lock acquisition time and makes sure that the small txn's are not blocked for 

// locktree_escalation_stalls -v --stalls 10
// verify that only big txn's get tagged with > 1 second stalls

#include <stdio.h>
#include "locktree.h"
#include "test.h"

using namespace toku;

static int verbose = 0;
static int killed = 0;

static void locktree_release_lock(locktree *lt, TXNID txn_id, int64_t left_k, int64_t right_k) {
    range_buffer buffer;
    buffer.create();
    DBT left; toku_fill_dbt(&left, &left_k, sizeof left_k);
    DBT right; toku_fill_dbt(&right, &right_k, sizeof right_k);
    buffer.append(&left, &right);
    lt->release_locks(txn_id, &buffer);
    buffer.destroy();
}

// grab a write range lock on int64 keys bounded by left_k and right_k
static int locktree_write_lock(locktree *lt, TXNID txn_id, int64_t left_k, int64_t right_k, bool big_txn) {
    DBT left; toku_fill_dbt(&left, &left_k, sizeof left_k);
    DBT right; toku_fill_dbt(&right, &right_k, sizeof right_k);
    return lt->acquire_write_lock(txn_id, &left, &right, nullptr, big_txn);
}

static void run_big_txn(locktree::manager *mgr UU(), locktree *lt, TXNID txn_id) {
    int64_t last_i = -1;
    for (int64_t i = 0; !killed; i++) {
        uint64_t t_start = toku_current_time_microsec();
        int r = locktree_write_lock(lt, txn_id, i, i, true);
        assert(r == 0);
        last_i = i;
        uint64_t t_end = toku_current_time_microsec();
        uint64_t t_duration = t_end - t_start;
        if (t_duration > 100000) {
            printf("%u %s %" PRId64 " %" PRIu64 "\n", toku_os_gettid(), __FUNCTION__, i, t_duration);
        }
        toku_pthread_yield();
    }
    if (last_i != -1)
        locktree_release_lock(lt, txn_id, 0, last_i); // release the range 0 .. last_i
}

static void run_small_txn(locktree::manager *mgr UU(), locktree *lt, TXNID txn_id, int64_t k) {
    for (int64_t i = 0; !killed; i++) {
        uint64_t t_start = toku_current_time_microsec();
        int r = locktree_write_lock(lt, txn_id, k, k, false);
        assert(r == 0);
        uint64_t t_end = toku_current_time_microsec();
        uint64_t t_duration = t_end - t_start;
        if (t_duration > 100000) {
            printf("%u %s %" PRId64 " %" PRIu64 "\n", toku_os_gettid(), __FUNCTION__, i, t_duration);
        }
        locktree_release_lock(lt, txn_id, k, k);
        toku_pthread_yield();
    }
}

struct arg {
    locktree::manager *mgr;
    locktree *lt;
    TXNID txn_id;
    int64_t k;
};

static void *big_f(void *_arg) {
    struct arg *arg = (struct arg *) _arg;
    run_big_txn(arg->mgr, arg->lt, arg->txn_id);
    return arg;
}

static void *small_f(void *_arg) {
    struct arg *arg = (struct arg *) _arg;
    run_small_txn(arg->mgr, arg->lt, arg->txn_id, arg->k);
    return arg;
}

static void e_callback(TXNID txnid, const locktree *lt, const range_buffer &buffer, void *extra) {
    if (verbose)
        printf("%u %s %" PRIu64 " %p %d %p\n", toku_os_gettid(), __FUNCTION__, txnid, lt, buffer.get_num_ranges(), extra);
}

static uint64_t get_escalation_count(locktree::manager &mgr) {
    LTM_STATUS_S ltm_status;
    mgr.get_status(&ltm_status);

    TOKU_ENGINE_STATUS_ROW key_status = NULL;
    // lookup keyname in status
    for (int i = 0; ; i++) {
        TOKU_ENGINE_STATUS_ROW status = &ltm_status.status[i];
        if (status->keyname == NULL)
            break;
        if (strcmp(status->keyname, "LTM_ESCALATION_COUNT") == 0) {
            key_status = status;
            break;
        }
    }
    assert(key_status);
    return key_status->value.num;
}

int main(int argc, const char *argv[]) {
    uint64_t stalls = 0;
    uint64_t max_lock_memory = 1000000000;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose++;
            continue;
        }
        if (strcmp(argv[i], "--stalls") == 0 && i+1 < argc) {
            stalls = atoll(argv[++i]);
            continue;
        }
        if (strcmp(argv[i], "--max_lock_memory") == 0 && i+1 < argc) {
            max_lock_memory = atoll(argv[++i]);
            continue;
        }
    }

    int r;

    // create a manager
    locktree::manager mgr;
    mgr.create(nullptr, nullptr, e_callback, nullptr);
    mgr.set_max_lock_memory(max_lock_memory);

    // create lock trees
    DESCRIPTOR desc_0 = nullptr;
    DICTIONARY_ID dict_id_0 = { 1 };
    locktree *lt_0 = mgr.get_lt(dict_id_0, desc_0, compare_dbts, nullptr);

    DESCRIPTOR desc_1 = nullptr;
    DICTIONARY_ID dict_id_1 = { 2 };
    locktree *lt_1 = mgr.get_lt(dict_id_1, desc_1, compare_dbts, nullptr);

    // create the worker threads
    struct arg big_arg = { &mgr, lt_0, 1000 };
    pthread_t big_id;
    r = toku_pthread_create(&big_id, nullptr, big_f, &big_arg);
    assert(r == 0);

    const int n_small = 7;
    pthread_t small_ids[n_small];
    struct arg small_args[n_small];

    for (int i = 0; i < n_small; i++) {
        small_args[i] = { &mgr, lt_1, (TXNID)(2000+i), i };
        r = toku_pthread_create(&small_ids[i], nullptr, small_f, &small_args[i]);
        assert(r == 0);
    }

    // wait for some escalations to occur
    while (get_escalation_count(mgr) < stalls) {
        sleep(1);
    }
    killed = 1;

    // cleanup
    void *ret;
    r = toku_pthread_join(big_id, &ret);
    assert(r == 0);

    for (int i = 0; i < n_small; i++) {
        r = toku_pthread_join(small_ids[i], &ret);
        assert(r == 0);
    }

    mgr.release_lt(lt_0);
    mgr.release_lt(lt_1);
    mgr.destroy();

    return 0;
}
