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
// test txn commit after db close

#include "test.h"
#include <sys/stat.h>

DB_ENV *null_env = NULL;
DB *null_db = NULL;
DB_TXN *null_txn = NULL;
DBC *null_cursor = NULL;

static void create_non_empty(int n) {
    DB_ENV *env = null_env;
    int r;
    r = db_env_create(&env, 0); assert(r == 0); assert(env != NULL);
    r = env->open(env, 
		  TOKU_TEST_FILENAME,
                  DB_INIT_MPOOL+DB_INIT_LOG+DB_INIT_LOCK+DB_INIT_TXN+DB_PRIVATE+DB_CREATE, 
                  S_IRWXU+S_IRWXG+S_IRWXO); 
    assert(r == 0);

    DB_TXN *txn = null_txn;
    r = env->txn_begin(env, null_txn, &txn, 0); assert(r == 0); assert(txn != NULL);

    DB *db = null_db;
    r = db_create(&db, env, 0); assert(r == 0); assert(db != NULL);

    r = db->open(db, txn, "test.db", 0, DB_BTREE, DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); 
    assert(r == 0);

    int i;
    for (i=n; i<2*n; i++) {
        DBT key, val;
        int k = toku_htonl(i);
        r = db->put(db, txn, dbt_init(&key, &k, sizeof k), dbt_init(&val, &i, sizeof i), 0);
        assert(r == 0);
    }

    r = db->close(db, 0); assert(r == 0); db = null_db;

    r = txn->commit(txn, 0); assert(r == 0); txn = null_txn;

    r = env->close(env, 0); assert(r == 0); env = null_env;
}

static void root_fifo_verify(DB_ENV *env, int n) {
    if (verbose) printf("%s:%d %d\n", __FUNCTION__, __LINE__, n);

    int r;

    DB_TXN *txn = null_txn;
    r = env->txn_begin(env, null_txn, &txn, 0); assert(r == 0); assert(txn != NULL);
    DB *db = null_db;
    r = db_create(&db, env, 0); assert(r == 0); assert(db != NULL);
    r = db->open(db, txn, "test.db", 0, DB_BTREE, DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); 
    assert(r == 0);

    DBC *cursor = null_cursor;
    r = db->cursor(db, txn, &cursor, 0); assert(r == 0);
    int i;
    for (i = 0; ; i++) {
        DBT key, val;
        memset(&key, 0, sizeof key); memset(&val, 0, sizeof val);
        r = cursor->c_get(cursor, &key, &val, DB_NEXT);
        if (r != 0) break;
        int k;
        assert(key.size == sizeof k);
        memcpy(&k, key.data, key.size);
        assert((int)toku_ntohl(k) == i);
    }
    assert(i == 2*n);

    r = cursor->c_close(cursor); assert(r == 0); cursor = null_cursor;
    
    r = txn->commit(txn, 0); assert(r == 0); txn = null_txn;

    r = db->close(db, 0); assert(r == 0); db = null_db;
}

static void root_fifo_31(int n) {
    if (verbose) printf("%s:%d %d\n", __FUNCTION__, __LINE__, n);
    int r;

    // create the env
    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO);

    // populate
    create_non_empty(n);

    DB_ENV *env = null_env;
    r = db_env_create(&env, 0); assert(r == 0); assert(env != NULL);
    r = env->open(env, 
                  TOKU_TEST_FILENAME, 
                  DB_INIT_MPOOL+DB_INIT_LOG+DB_INIT_LOCK+DB_INIT_TXN+DB_PRIVATE+DB_CREATE, 
                  S_IRWXU+S_IRWXG+S_IRWXO); 
    assert(r == 0);

    DB_TXN *txn = null_txn;
    r = env->txn_begin(env, null_txn, &txn, 0); assert(r == 0); assert(txn != NULL);

    int i;
    for (i=0; i<n; i++) {
        DB *db = null_db;
        r = db_create(&db, env, 0); assert(r == 0); assert(db != NULL);

        r = db->open(db, txn, "test.db", 0, DB_BTREE, DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); 
        assert(r == 0);

        DBT key, val;
        int k = toku_htonl(i);
        r = db->put(db, txn, dbt_init(&key, &k, sizeof k), dbt_init(&val, &i, sizeof i), 0);
        assert(r == 0);

        r = db->close(db, 0); assert(r == 0); db = null_db;
    }

    r = txn->commit(txn, 0); assert(r == 0); txn = null_txn;

    // verify the db
    root_fifo_verify(env, n);

    // cleanup
    r = env->close(env, 0); assert(r == 0); env = null_env;
}

int test_main(int argc, char *const argv[]) {
    int i;
    int n = -1;

    // parse_args(argc, argv);
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0) {
            verbose = 1;
            continue;
        }
        if (strcmp(argv[i], "-n") == 0) {
            if (i+1 < argc)
                n = atoi(argv[++i]);
            continue;
        }
    }
              
    if (n >= 0)
        root_fifo_31(n);
    else 
        for (i=0; i<100; i++)
            root_fifo_31(i);
    return 0;
}

