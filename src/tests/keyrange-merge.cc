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
#include "test.h"

// verify that key_range64 returns reasonable results after leaf merges

// create a tree with at least 2 child nodes and large rows.
// replace the rows with small rows.
// this should cause a leaf node merge.
// verify stats after the merge.

#include <db.h>
#include <unistd.h>
#include <sys/stat.h>

static DB_ENV *env = NULL;
static DB_TXN *txn = NULL;
static DB *db = NULL;
static uint32_t db_page_size = 4096;
static uint32_t db_basement_size = 4096;
static const char *envdir = TOKU_TEST_FILENAME;
static uint64_t nrows = 0;

static uint64_t 
max64(uint64_t a, uint64_t b) {
    return a < b ? b : a;
}

static void 
run_test(void) {
    if (verbose) printf("%s %" PRIu64 "\n", __FUNCTION__, nrows);

    // create a tree with 2 children
    uint32_t key_size = 9;
    uint32_t val_size = db_basement_size / 32;
    size_t est_row_size_with_overhead = 8 + key_size + 4 + val_size + 4; // xid + key + key_len + val + val)len
    size_t rows_per_basement = db_basement_size / est_row_size_with_overhead;

    if (nrows == 0)
        nrows = 2 * (db_page_size / est_row_size_with_overhead);

    int r;
    r = db_env_create(&env, 0); CKERR(r);
    env->set_errfile(env, stderr);
    r = env->set_redzone(env, 0); CKERR(r);
    r = env->open(env, envdir, DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_MPOOL|DB_INIT_TXN|DB_CREATE|DB_PRIVATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);

    r = db_create(&db, env, 0); CKERR(r);
    r = db->set_pagesize(db, db_page_size);
    r = env->txn_begin(env, 0, &txn, 0); CKERR(r);
    r = db->open(db, txn, "foo.db", 0, DB_BTREE, DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
    r = txn->commit(txn, 0);    CKERR(r);

    // insert keys 1, 3, 5, ... 2*(nrows-1) + 1
    r = env->txn_begin(env, 0, &txn, 0); CKERR(r);
    for (uint64_t i=0; i<nrows; i++) {
        char key[100];
        snprintf(key, sizeof key, "%08llu", (unsigned long long)2*i+1);
        char val[val_size];
        memset(val, 0, val_size);
        DBT k = { .data = key, .size = key_size };
        DBT v = { .data = val, .size = val_size };
        r = db->put(db, txn, &k, &v, 0); CKERR(r);
    }

    DB_BTREE_STAT64 s64;
    r = db->stat64(db, txn, &s64); CKERR(r);
    if (verbose) 
        printf("stats %" PRId64 " %" PRId64 "\n", s64.bt_nkeys, s64.bt_dsize);
    assert(0 < s64.bt_nkeys && s64.bt_nkeys <= nrows);
    assert(0 < s64.bt_dsize && s64.bt_dsize <= nrows * (key_size + val_size));

    r = txn->commit(txn, 0);    CKERR(r);

    // lose the seqinsert bit by flushing the tree from the cache table
    r = db->close(db, 0);     CKERR(r);
    r = db_create(&db, env, 0); CKERR(r);
    r = env->txn_begin(env, 0, &txn, 0); CKERR(r);
    r = db->open(db, txn, "foo.db", 0, DB_BTREE, 0, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
    r = txn->commit(txn, 0);    CKERR(r);

    // replace the rows with small values.  this should shrink the leaf node and induce merging.
    // do this until a leaf node merge occurs.
    int t;
    for (t = 0; t<100; t++) {
        r = env->txn_begin(env, 0, &txn, 0); CKERR(r);
        // replace in reverse order to disable the sequential insertion code
        for (uint64_t i=nrows; i>0; i--) {
            char key[100];
            snprintf(key, sizeof key, "%08llu", (unsigned long long)2*(i-1)+1);
            assert(1+strlen(key) == key_size);
            DBT k;
            dbt_init(&k, key, 1+strlen(key));
            DBT v;
            dbt_init(&v, NULL, 0);
            r = db->put(db, txn, &k, &v, 0); CKERR(r);
        }
        r = txn->commit(txn, 0); CKERR(r);

        uint64_t merge_leaf = get_engine_status_val(env, "FT_FLUSHER_MERGE_LEAF");
        if (merge_leaf > 0) {
            if (verbose) printf("t=%d\n", t);
            break;
        }
    }
    assert(t < 100); // if this asserts, then no leaf merge occurred

    // verify key_range for keys that exist in the tree
    r = env->txn_begin(env, 0, &txn, 0); CKERR(r);
    for (uint64_t i=0; i<nrows; i++) {
	char key[100];
	snprintf(key, 100, "%08llu", (unsigned long long)2*i+1);
	DBT k;
	uint64_t less,equal,greater;
	int is_exact;
	r = db->key_range64(db, txn, dbt_init(&k, key, 1+strlen(key)), &less, &equal, &greater, &is_exact); CKERR(r);
	if (verbose)
            printf("key %llu/%llu %llu %llu %llu %llu\n", (unsigned long long)2*i, (unsigned long long)2*nrows, (unsigned long long)less, (unsigned long long)equal, (unsigned long long)greater,
                   (unsigned long long)(less+equal+greater));
        assert(is_exact == 0);
        assert(0 < less + equal + greater);
        assert(less + equal + greater < 2*nrows);
        assert(equal == 1);
        uint64_t est_i = max64(i, i + rows_per_basement/2);
        assert(less <= est_i + est_i / 1);
        assert(greater <= nrows - i + rows_per_basement/2);
    }
    r = txn->commit(txn, 0);    CKERR(r);

    // verify key range for keys that do not exist in the tree
    r = env->txn_begin(env, 0, &txn, 0); CKERR(r);
    for (uint64_t i=0; i<1+nrows; i++) {
	char key[100];
	snprintf(key, 100, "%08llu", (unsigned long long)2*i);
	DBT k;
	uint64_t less,equal,greater;
	int is_exact;
	r = db->key_range64(db, txn, dbt_init(&k, key, 1+strlen(key)), &less, &equal, &greater, &is_exact); CKERR(r);
	if (verbose) 
            printf("key %llu/%llu %llu %llu %llu %llu\n", (unsigned long long)2*i, (unsigned long long)2*nrows, (unsigned long long)less, (unsigned long long)equal, (unsigned long long)greater,
                   (unsigned long long)(less+equal+greater));
        assert(is_exact == 0);
        assert(0 < less + equal + greater);
        assert(less + equal + greater < 2*nrows);
        assert(equal == 0);
        uint64_t est_i = max64(i, i + rows_per_basement/2);
        assert(less <= est_i + est_i / 1);
        assert(greater <= nrows - i + rows_per_basement/2);
    }
    r = txn->commit(txn, 0);    CKERR(r);

    r = db->close(db, 0);     CKERR(r);
    r = env->close(env, 0);   CKERR(r);
}

static int 
usage(void) {
    fprintf(stderr, "-v (verbose)\n");
    fprintf(stderr, "-q (quiet)\n");
    fprintf(stderr, "--envdir %s\n", envdir);
    fprintf(stderr, "--nrows %" PRIu64 " (number of rows)\n", nrows);
    fprintf(stderr, "--nrows %" PRIu64 " (number of rows)\n", nrows);
    return 1;
}

int
test_main (int argc , char * const argv[]) {
    for (int i = 1 ; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose++;
            continue;
        }
        if (strcmp(argv[i], "-q") == 0) {
            if (verbose > 0)
                verbose--;
            continue;
        }
        if (strcmp(argv[i], "--envdir") == 0 && i+1 < argc) {
            envdir = argv[++i];
            continue;
        }
        if (strcmp(argv[i], "--nrows") == 0 && i+1 < argc) {
            nrows = atoll(argv[++i]);
            continue;
        }
        return usage();
    }

    char rmcmd[32 + strlen(envdir)]; 
    snprintf(rmcmd, sizeof rmcmd, "rm -rf %s", envdir);
    int r;
    r = system(rmcmd); CKERR(r);
    r = toku_os_mkdir(envdir, S_IRWXU+S_IRWXG+S_IRWXO);       CKERR(r);

    run_test();

    return 0;
}
