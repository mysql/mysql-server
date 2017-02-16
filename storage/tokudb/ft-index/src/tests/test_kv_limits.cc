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

#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>
#include <memory.h>
#include <sys/stat.h>
#include <db.h>


static uint64_t lorange = 0;
static uint64_t hirange = 1<<24;
static uint32_t pagesize = 0;

static void test_key_size_limit (void) {
    if (verbose > 1) printf("%s\n", __FUNCTION__);

    DB_TXN * const null_txn = 0;
    const char * const fname = "test.rand.insert.ft_handle";
    int r;

    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    r=toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO); assert(r==0);

    DB_ENV *env;
    r = db_env_create(&env, 0); assert(r == 0);
    r = env->open(env, TOKU_TEST_FILENAME, DB_CREATE+DB_PRIVATE+DB_INIT_MPOOL, 0); assert(r == 0);

    DB *db;
    r = db_create(&db, env, 0);
    assert(r == 0);
    if (pagesize) {
        r = db->set_pagesize(db, pagesize); assert(r == 0);
    }
    r = db->open(db, null_txn, fname, "main", DB_BTREE, DB_CREATE, 0666);
    assert(r == 0);

    void *k = 0;
    void *v = 0;
    uint32_t lo = lorange, mi = 0, hi = hirange;
    uint32_t bigest = 0;
    while (lo <= hi) {
        mi = lo + (hi - lo) / 2;
        assert(lo <= mi && mi <= hi);
        uint32_t ks = mi;
        if (verbose > 1) printf("trying %u %u %u ks=%u\n", lo, mi, hi, ks);
        k = toku_realloc(k, ks); assert(k);
        memset(k, 0, ks);
        memcpy(k, &ks, sizeof ks);
        uint32_t vs = sizeof (uint32_t);
        v = toku_realloc(v, vs); assert(v);
        memset(v, 0, vs);
        memcpy(v, &vs, sizeof vs);
        DBT key, val;
        r = db->put(db, null_txn, dbt_init(&key, k, ks), dbt_init(&val, v, vs), 0);
        if (r == 0) {
            bigest = mi;
            lo = mi+1;
        } else {
            if (verbose > 1) printf("%u too big\n", ks);
            hi = mi-1;
        }
    }
    toku_free(k);
    toku_free(v);
    assert(bigest > 0);
    if (verbose) printf("%s bigest %u\n", __FUNCTION__, bigest);

    r = db->close(db, 0); assert(r == 0);
    r = env->close(env, 0); assert(r == 0);
}

static void test_data_size_limit (void) {
    if (verbose > 1) printf("%s\n", __FUNCTION__);

    DB_TXN * const null_txn = 0;
    const char * const fname = "test.rand.insert.ft_handle";
    int r;

    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    r=toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO); assert(r==0);

    DB_ENV *env;
    r = db_env_create(&env, 0); assert(r == 0);
    r = env->open(env, TOKU_TEST_FILENAME, DB_CREATE+DB_PRIVATE+DB_INIT_MPOOL, 0); assert(r == 0);

    DB *db;
    r = db_create(&db, env, 0);
    assert(r == 0);
    if (pagesize) {
        r = db->set_pagesize(db, pagesize); assert(r == 0);
    }
    r = db->open(db, null_txn, fname, "main", DB_BTREE, DB_CREATE, 0666);
    assert(r == 0);

    void *k = 0;
    void *v = 0;
    uint32_t lo = lorange, mi = 0, hi = hirange;
    uint32_t bigest = 0;
    while (lo <= hi) {
        mi = lo + (hi - lo) / 2;
        assert(lo <= mi && mi <= hi);
        uint32_t ks = sizeof (uint32_t);
        if (verbose > 1) printf("trying %u %u %u ks=%u\n", lo, mi, hi, ks);
        k = toku_realloc(k, ks); assert(k);
        memset(k, 0, ks);
        memcpy(k, &ks, sizeof ks);
        uint32_t vs = mi;
        v = toku_realloc(v, vs); assert(v);
        memset(v, 0, vs);
        memcpy(v, &vs, sizeof vs);
        DBT key, val;
        r = db->put(db, null_txn, dbt_init(&key, k, ks), dbt_init(&val, v, vs), 0);
        if (r == 0) {
            bigest = mi;
            lo = mi+1;
        } else {
            if (verbose > 1) printf("%u too big\n", vs);
            hi = mi-1;
        }
    }
    toku_free(k);
    toku_free(v);
    if (verbose && bigest > 0) printf("%s bigest %u\n", __FUNCTION__, bigest);

    r = db->close(db, 0); assert(r == 0);
    r = env->close(env, 0); assert(r == 0);
}

int
test_main(int argc, char *const argv[]) {
    int do_key = 1;
    int do_data = 1;
    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (strcmp(arg, "-v") == 0) {
            verbose++;
            continue;
        }
        if (strcmp(arg, "-lorange") == 0 && i+1 < argc) {
            lorange = strtoull(argv[++i], 0, 10);
            if (lorange > ULLONG_MAX)
                return 2;
            continue;
        }
        if (strcmp(arg, "-hirange") == 0 && i+1 < argc) {
            hirange = strtoull(argv[++i], 0, 10);
            if (hirange > ULLONG_MAX)
                return 2;
            continue;
        }
        if (strcmp(arg, "-pagesize") == 0 && i+1 < argc) {
            pagesize = atoi(argv[++i]);
            continue;
        }
        if (strcmp(arg, "-nokey") == 0) {
            do_key = 0;
            continue;
        }
        if (strcmp(arg, "-nodata") == 0) {
            do_data = 0;
            continue;
        }
    }

    if (do_key)
        test_key_size_limit();
    if (do_data)
        test_data_size_limit();

    return 0;
}
