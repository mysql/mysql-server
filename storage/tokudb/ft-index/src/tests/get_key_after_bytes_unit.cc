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
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "test.h"
#include <db.h>
#include <algorithm>

// Unit test for db->get_key_after_bytes.

static const int num_keys = 1<<10;

static void setup(DB_ENV **envp, DB **dbp, uint32_t nodesize, uint32_t basementnodesize) {
    int r;
    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    r = toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU|S_IRWXG|S_IRWXO);
    CKERR(r);
    r = db_env_create(envp, 0);
    CKERR(r);
    DB_ENV *env = *envp;
    r = env->set_default_bt_compare(env, int_dbt_cmp);
    CKERR(r);
    env->set_errfile(env, stderr);
    r = env->open(env, TOKU_TEST_FILENAME, DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_MPOOL|DB_INIT_TXN|DB_CREATE|DB_PRIVATE, S_IRWXU|S_IRWXG|S_IRWXO);
    CKERR(r);
    r = db_create(dbp, env, 0);
    CKERR(r);
    DB *db = *dbp;
    {
        r = db->set_pagesize(db, nodesize);
        CKERR(r);
        r = db->set_readpagesize(db, basementnodesize);
        CKERR(r);
        DB_TXN *txn;
        r = env->txn_begin(env, 0, &txn, 0);
        CKERR(r);
        r = db->open(db, txn, "foo.db", 0, DB_BTREE, DB_CREATE, S_IRWXU|S_IRWXG|S_IRWXO);
        CKERR(r);
        r = txn->commit(txn, 0);
        CKERR(r);
    }
}

static void fill(DB_ENV *env, DB *db) {
    int r;
    DB_TXN *txn;
    r = env->txn_begin(env, 0, &txn, 0);
    CKERR(r);
    int k, v;
    DBT key, val;
    dbt_init(&key, &k, sizeof k);
    dbt_init(&val, &v, sizeof v);
    for (int i = 0; i < num_keys; ++i) {
        k = i;
        v = i;
        r = db->put(db, txn, &key, &val, 0);
        CKERR(r);
    }
    r = txn->commit(txn, 0);
    CKERR(r);
}

struct check_extra {
    int start_key;
    uint64_t skip_len;
    bool filled;
    bool exact;
};

static void check_callback(const DBT *end_key, uint64_t actually_skipped, void *extra) {
    struct check_extra *CAST_FROM_VOIDP(e, extra);

    int real_start_key = std::min(std::max(e->start_key, 0), num_keys);
    int expected_key = std::min(real_start_key + (e->skip_len / (2 * sizeof(int))), (uint64_t) num_keys);

    if (e->exact) {
        if (!e->filled || expected_key >= num_keys) {
            expected_key = -1;
        }
        assert(actually_skipped <= e->skip_len);
        if (expected_key == -1) {
            assert(end_key == nullptr);
        } else {
            assert(e->skip_len - actually_skipped < 2 * (int) sizeof(int));
            assert(end_key != nullptr);
            assert(end_key->size == sizeof expected_key);
            assert((*(int *) end_key->data) == expected_key);
        }
    } else {
        // no sense in doing an inexact check if the table's empty
        assert(e->filled);
        int found;
        if (end_key == nullptr) {
            found = num_keys;
        } else {
            assert(end_key->size == sizeof found);
            found = *(int *) end_key->data;
        }
        // These are just guesses.  I don't have a good reason but they
        // seem like alright bounds.
        double skipped_portion = (double) e->skip_len / (num_keys * 2 * sizeof(int));
        int key_slack = num_keys * std::max(std::min(skipped_portion, 0.25), 0.01);
        int size_slack = key_slack * 2 * sizeof(int);
        assert(found <= expected_key + key_slack);
        assert(found >= expected_key - key_slack);
        assert(actually_skipped <= e->skip_len + size_slack);
        if (end_key != nullptr) {
            // if we hit the end of the table, this definitely won't hold up
            assert((int) actually_skipped >= (int) e->skip_len - size_slack);
        }
    }
}

static void check(DB_ENV *env, DB *db, int start_key, uint64_t skip_len, bool filled, bool exact) {
    int r;
    DB_TXN *txn;
    r = env->txn_begin(env, 0, &txn, 0);
    CKERR(r);

    DBT start_dbt, end_key;
    dbt_init(&start_dbt, &start_key, sizeof start_key);
    dbt_init(&end_key, nullptr, 0);

    struct check_extra extra = {start_key, skip_len, filled, exact};
    r = db->get_key_after_bytes(db, txn, (start_key == -2 ? nullptr : &start_dbt), skip_len, check_callback, &extra, 0);
    CKERR(r);

    r = txn->commit(txn, 0);
    CKERR(r);
}

static void teardown(DB_ENV *env, DB *db) {
    int r;
    r = db->close(db, 0);
    CKERR(r);
    r = env->close(env, 0);
    CKERR(r);
}

int test_main(int argc, char * const argv[]) {
    int r;
    default_parse_args(argc, argv);

    DB_ENV *env;
    DB *db;

    setup(&env, &db, 4<<20, 64<<10);

    // if the table is empty, always say DB_NOTFOUND
    for (int start_key = -2; start_key <= 1; ++start_key) {
        for (int skip_len = 0; skip_len < 2; ++skip_len) {
            check(env, db, start_key, skip_len, false, true);
        }
    }

    fill(env, db);

    // if start_key is bigger than any key, assert that we get DB_NOTFOUND
    for (int extra_key = 0; extra_key < 10; extra_key += 5) {
        for (int skip_len = 0; skip_len < 24; ++skip_len) {
            check(env, db, num_keys + extra_key, skip_len, true, true);
        }
    }

    // if start_key is nullptr or the first key or before the first key, we start at the beginning
    for (int start_key = -2; start_key <= 0; ++start_key) {
        for (int skip_len = 0; skip_len < 48; ++skip_len) {
            check(env, db, start_key, skip_len, true, true);
        }
    }

    // check a bunch of places in the middle too (use prime increments to get a good distribution of stuff)
    for (int start_key = 0; start_key <= num_keys; start_key += 31) {
        for (int skip_len = 0; skip_len < (num_keys + 1 - start_key) * (2 * (int) sizeof(int)); skip_len += 67) {
            check(env, db, start_key, skip_len, true, true);
        }
    }

    // TODO: test mvcc stuff (check that we only look at the latest val, which is the current behavior)

    teardown(env, db);

    // Try many bn and nodesizes
    for (int basementnodesize = 1<<10; basementnodesize <= 64<<10; basementnodesize <<= 1) {
        for (int nodesize = basementnodesize; nodesize <= 128<<10; nodesize <<= 2) {
            setup(&env, &db, nodesize, basementnodesize);
            fill(env, db);
            // forces a rebalance of the root, to get multiple bns
            r = env->txn_checkpoint(env, 0, 0, 0);
            CKERR(r);
            // near the beginning
            for (int start_key = -2; start_key <= 1; ++start_key) {
                for (int skip_len = 0; skip_len <= (num_keys + 1 - start_key) * (2 * (int) sizeof(int)); skip_len += 41) {
                    check(env, db, start_key, skip_len, true, false);
                }
            }
            // near the end
            for (int start_key = num_keys - 1; start_key <= num_keys + 1; ++start_key) {
                for (int skip_len = 0; skip_len <= (num_keys + 1 - start_key) * (2 * (int) sizeof(int)); skip_len += 41) {
                    check(env, db, start_key, skip_len, true, false);
                }
            }
            for (int start_key = 0; start_key <= num_keys; start_key += 17) {
                for (int skip_len = 0; skip_len <= (num_keys + 1 - start_key) * (2 * (int) sizeof(int)); skip_len += 31) {
                    check(env, db, start_key, skip_len, true, false);
                }
            }
            teardown(env, db);
        }
    }

    return 0;
}
