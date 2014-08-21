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

// verify that key_range64 returns reasonable results after inserting rows into a tree.
// variations include:
// 1. trickle load versus bulk load
// 2. sequential keys versus random keys
// 3. basements on disk versus basements in memory

#include <db.h>
#include <unistd.h>
#include <sys/stat.h>

static DB_ENV *env = NULL;
static DB_TXN *txn = NULL;
static DB *db = NULL;
static uint32_t db_page_size = 4096;
static uint32_t db_basement_size = 4096;
static const char *envdir = TOKU_TEST_FILENAME;
static uint64_t nrows = 30000;
static bool get_all = true;
static bool use_loader = false;
static bool random_keys = false;

static int 
my_compare(DB *this_db UU(), const DBT *a UU(), const DBT *b UU()) {
    assert(a->size == b->size);
    return memcmp(a->data, b->data, a->size);
}

static int 
my_generate_row(DB *dest_db UU(), DB *src_db UU(), DBT_ARRAY *dest_keys UU(), DBT_ARRAY *dest_vals UU(), const DBT *src_key UU(), const DBT *src_val UU()) {
    toku_dbt_array_resize(dest_keys, 1);
    toku_dbt_array_resize(dest_vals, 1);
    DBT *dest_key = &dest_keys->dbts[0];
    DBT *dest_val = &dest_vals->dbts[0];

    assert(dest_key->flags == DB_DBT_REALLOC);
    dest_key->data = toku_realloc(dest_key->data, src_key->size);
    memcpy(dest_key->data, src_key->data, src_key->size);
    dest_key->size = src_key->size;
    assert(dest_val->flags == DB_DBT_REALLOC);
    dest_val->data = toku_realloc(dest_val->data, src_val->size);
    memcpy(dest_val->data, src_val->data, src_val->size);
    dest_val->size = src_val->size;
    return 0;
}

static void
swap(uint64_t keys[], uint64_t i, uint64_t j) {
    uint64_t t = keys[i]; keys[i] = keys[j]; keys[j] = t;
}

static uint64_t 
max64(uint64_t a, uint64_t b) {
    return a < b ? b : a;
}

static void open_env(void) {
    int r = db_env_create(&env, 0); CKERR(r);
    env->set_errfile(env, stderr);
    r = env->set_redzone(env, 0); CKERR(r);
    r = env->set_generate_row_callback_for_put(env, my_generate_row); CKERR(r);
    r = env->set_default_bt_compare(env, my_compare); CKERR(r);
    r = env->open(env, envdir, DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_MPOOL|DB_INIT_TXN|DB_CREATE|DB_PRIVATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
}

static void 
run_test(void) {
    if (verbose) printf("%s %" PRIu64 "\n", __FUNCTION__, nrows);

    size_t key_size = 9;
    size_t val_size = 9;
    size_t est_row_size_with_overhead = 8 + key_size + 4 + val_size + 4 + 5; // xid + key + key_len + val + val_len + mvcc overhead
    size_t rows_per_basement = db_basement_size / est_row_size_with_overhead;

    open_env();
    int r;
    r = db_create(&db, env, 0); CKERR(r);
    r = db->set_pagesize(db, db_page_size); CKERR(r);
    r = db->set_readpagesize(db, db_basement_size); CKERR(r);
    r = env->txn_begin(env, 0, &txn, 0); CKERR(r);
    r = db->open(db, txn, "foo.db", 0, DB_BTREE, DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
    r = txn->commit(txn, 0);    CKERR(r);

    uint64_t *XMALLOC_N(nrows, keys);
    for (uint64_t i = 0; i < nrows; i++)
        keys[i] = 2*i + 1;

    if (random_keys)
        for (uint64_t i = 0; i < nrows; i++)
            swap(keys, random() % nrows, random() % nrows);

    // insert keys 1, 3, 5, ... 2*(nrows-1) + 1
    r = env->txn_begin(env, 0, &txn, 0); CKERR(r);
    if (use_loader) {
        DB_LOADER *loader = NULL;
        r = env->create_loader(env, txn, &loader, db, 1, &db, NULL, NULL, 0); CKERR(r);
        for (uint64_t i=0; i<nrows; i++) {
            char key[100],val[100];
            snprintf(key, sizeof key, "%08llu", (unsigned long long)keys[i]);
            snprintf(val, sizeof val, "%08llu", (unsigned long long)keys[i]);
	    assert(1+strlen(key) == key_size && 1+strlen(val) == val_size);
            DBT k,v;
            r = loader->put(loader, dbt_init(&k, key, 1+strlen(key)), dbt_init(&v,val, 1+strlen(val))); CKERR(r);
        }
        r = loader->close(loader); CKERR(r);
    } else {
        for (uint64_t i=0; i<nrows; i++) {
            char key[100],val[100];
            snprintf(key, sizeof key, "%08llu", (unsigned long long)keys[i]);
            snprintf(val, sizeof val, "%08llu", (unsigned long long)keys[i]);
	    assert(1+strlen(key) == key_size && 1+strlen(val) == val_size);
            DBT k,v;
            r = db->put(db, txn, dbt_init(&k, key, 1+strlen(key)), dbt_init(&v,val, 1+strlen(val)), 0); CKERR(r);
        }
    }
    r = txn->commit(txn, 0);    CKERR(r);

    // close and reopen to get rid of basements
    r = db->close(db, 0); CKERR(r); // close MUST flush the nodes of this db out of the cache table for this test to be valid
    r = env->close(env, 0);   CKERR(r);
    env = NULL;
    open_env();

    r = db_create(&db, env, 0); CKERR(r);
    r = env->txn_begin(env, 0, &txn, 0); CKERR(r);
    r = db->open(db, txn, "foo.db", 0, DB_BTREE, 0, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
    r = txn->commit(txn, 0); CKERR(r);

    r = env->txn_begin(env, 0, &txn, 0); CKERR(r);

    if (get_all) {
        // read the basements into memory
        for (uint64_t i=0; i<nrows; i++) {
            char key[100];
            snprintf(key, 100, "%08llu", (unsigned long long)2*i+1);
            DBT k,v;
            memset(&v, 0, sizeof(v));
            r = db->get(db, txn, dbt_init(&k, key, 1+strlen(key)), &v, 0); CKERR(r);
        }
    }

    DB_BTREE_STAT64 s64;
    r = db->stat64(db, txn, &s64); CKERR(r);
    if (verbose) 
        printf("stats %" PRId64 " %" PRId64 "\n", s64.bt_nkeys, s64.bt_dsize);
    if (use_loader) {
	assert(s64.bt_nkeys == nrows);
	assert(s64.bt_dsize == nrows * (key_size + val_size));
    } else {
	assert(0 < s64.bt_nkeys && s64.bt_nkeys <= nrows);
	assert(0 < s64.bt_dsize && s64.bt_dsize <= nrows * (key_size + val_size));
    }

    if (0) goto skipit; // debug: just write the tree

    bool last_basement;
    last_basement = false;
    // verify key_range for keys that exist in the tree
    uint64_t random_fudge;
    random_fudge = random_keys ? rows_per_basement + nrows / 10 : 0;
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
        if (use_loader) {
            assert(less + equal + greater <= nrows);
            if (get_all || last_basement) {
                assert(equal == 1);
            } else if (i < nrows - rows_per_basement * 2) {
                assert(equal == 0);
            } else if (i == nrows - 1) {
                assert(equal == 1);
            } else if (equal == 1) {
                last_basement = true;
            }
            assert(less <= max64(i, i + rows_per_basement/2));
            assert(greater <= nrows - less);
        } else {
            assert(less + equal + greater <= nrows + nrows / 8);
            if (get_all || last_basement) {
                assert(equal == 1);
            } else if (i < nrows - rows_per_basement * 2) {
                assert(equal == 0);
            } else if (i == nrows - 1) {
                assert(equal == 1);
            } else if (equal == 1) {
                last_basement = true;
            }
            uint64_t est_i = i * 2 + rows_per_basement;
            assert(less <= est_i + random_fudge);
            assert(greater <= nrows - i + rows_per_basement + random_fudge);
	}
    }

    // verify key range for keys that do not exist in the tree
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
        if (use_loader) {
            assert(less + equal + greater <= nrows);
            assert(equal == 0);
            assert(less <= max64(i, i + rows_per_basement/2));
            assert(greater <= nrows - less);
        } else {
            assert(less + equal + greater <= nrows + nrows / 8);
            assert(equal == 0);
            uint64_t est_i = i * 2 + rows_per_basement;
            assert(less <= est_i + random_fudge);
            assert(greater <= nrows - i + rows_per_basement + random_fudge);
        }
    }

 skipit:
    r = txn->commit(txn, 0);    CKERR(r);
    r = db->close(db, 0);     CKERR(r);
    r = env->close(env, 0);   CKERR(r);
    
    toku_free(keys);
}

static int 
usage(void) {
    fprintf(stderr, "-v (verbose)\n");
    fprintf(stderr, "-q (quiet)\n");
    fprintf(stderr, "--nrows %" PRIu64 " (number of rows)\n", nrows);
    fprintf(stderr, "--nrows %" PRIu64 " (number of rows)\n", nrows);
    fprintf(stderr, "--loader %u (use the loader to load the keys)\n", use_loader);
    fprintf(stderr, "--get %u (get all keys before keyrange)\n", get_all);
    fprintf(stderr, "--random_keys %u\n", random_keys);
    fprintf(stderr, "--page_size %u\n", db_page_size);
    fprintf(stderr, "--basement_size %u\n", db_basement_size);
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
        if (strcmp(argv[i], "--nrows") == 0 && i+1 < argc) {
            nrows = atoll(argv[++i]);
            continue;
        }
        if (strcmp(argv[i], "--get") == 0 && i+1 < argc) {
            get_all = atoi(argv[++i]) != 0;
            continue;
        }
        if (strcmp(argv[i], "--loader") == 0 && i+1 < argc) {
            use_loader = atoi(argv[++i]) != 0;
            continue;
        }
        if (strcmp(argv[i], "--random_keys") == 0 && i+1 < argc) {
            random_keys = atoi(argv[++i]) != 0;
            continue;
        }
        if (strcmp(argv[i], "--page_size") == 0 && i+1 < argc) {
            db_page_size = atoi(argv[++i]);
            continue;
        }
        if (strcmp(argv[i], "--basement_size") == 0 && i+1 < argc) {
            db_basement_size = atoi(argv[++i]);
            continue;
        }
        return usage();
    }

    toku_os_recursive_delete(envdir);
    int r = toku_os_mkdir(envdir, S_IRWXU+S_IRWXG+S_IRWXO);       CKERR(r);

    run_test();

    return 0;
}
