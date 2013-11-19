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
#include "test.h"
#include <db.h>
#include <sys/stat.h>

static DB_ENV *env;
static DB *db;
DB_TXN *txn;

const int num_insert = 25000;

static void
setup (void) {
    int r;
    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    r=toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO);       CKERR(r);

    r=db_env_create(&env, 0); CKERR(r);
#ifdef TOKUDB
    r=env->set_redzone(env, 0); CKERR(r);
    r=env->set_default_bt_compare(env, int_dbt_cmp); CKERR(r);
#endif
    env->set_errfile(env, stderr);
#ifdef USE_BDB
    r=env->set_lk_max_objects(env, 2*num_insert); CKERR(r);
#endif
    
    r=env->open(env, TOKU_TEST_FILENAME, DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_MPOOL|DB_INIT_TXN|DB_CREATE|DB_PRIVATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
    r=db_create(&db, env, 0); CKERR(r);

    r=env->txn_begin(env, 0, &txn, 0); assert(r==0);
#ifdef USE_BDB
    r=db->set_bt_compare(db, int_dbt_cmp); CKERR(r);
#endif
    r=db->open(db, txn, "foo.db", 0, DB_BTREE, DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
    r=txn->commit(txn, 0);    assert(r==0);
}

static void
test_shutdown (void) {
    int r;
    r= db->close(db, 0); CKERR(r);
    r= env->close(env, 0); CKERR(r);
}

static void
doit (bool committed_provdels) {
    DBT key,data;
    DBC *dbc;
    int r;
    int i;
    int j;

    r=env->txn_begin(env, 0, &txn, 0); CKERR(r);
    for (i = 0; i < num_insert; i++) {
        j = (i<<1) + 37;
        r=db->put(db, txn, dbt_init(&key, &i, sizeof(i)), dbt_init(&data, &j, sizeof(j)), 0);
    }
    r=txn->commit(txn, 0);    CKERR(r);
    r=env->txn_begin(env, 0, &txn, 0); CKERR(r);
    r = db->cursor(db, txn, &dbc, 0);                           CKERR(r);
    for (i = 0; i < num_insert; i++) {
        j = (i<<1) + 37;
        r = dbc->c_get(dbc, &key, &data, DB_NEXT); CKERR(r);
        assert(*(int*)key.data == i);
        assert(*(int*)data.data == j);
        r = db->del(db, txn, &key, DB_DELETE_ANY); CKERR(r);
    }
    r = dbc->c_get(dbc, &key, &data, DB_NEXT); CKERR2(r, DB_NOTFOUND);
    r = dbc->c_get(dbc, &key, &data, DB_FIRST); CKERR2(r, DB_NOTFOUND);
    if (committed_provdels) {
        r = dbc->c_close(dbc);                                      CKERR(r);
        r=txn->commit(txn, 0);    CKERR(r);
        r=env->txn_begin(env, 0, &txn, 0); CKERR(r);
        r = db->cursor(db, txn, &dbc, 0);                           CKERR(r);
    }
    int ifirst, ilast, jfirst, jlast;
    ilast=2*num_insert;
    jlast=(ilast<<1)+37;
    ifirst=-1*num_insert;
    jfirst=(ifirst<<1)+37;
    r=db->put(db, txn, dbt_init(&key, &ifirst, sizeof(ifirst)), dbt_init(&data, &jfirst, sizeof(jfirst)), 0);
    CKERR(r);
    r=db->put(db, txn, dbt_init(&key, &ilast, sizeof(ilast)), dbt_init(&data, &jlast, sizeof(jlast)), 0);
    CKERR(r);

    r = dbc->c_get(dbc, dbt_init(&key, NULL, 0), dbt_init(&data, NULL, 0), DB_FIRST); CKERR(r);
    assert(*(int*)key.data == ifirst);
    assert(*(int*)data.data == jfirst);
    r = dbc->c_get(dbc, dbt_init(&key, NULL, 0), dbt_init(&data, NULL, 0), DB_NEXT); CKERR(r);
    assert(*(int*)key.data == ilast);
    assert(*(int*)data.data == jlast);
    r = dbc->c_get(dbc, dbt_init(&key, NULL, 0), dbt_init(&data, NULL, 0), DB_LAST); CKERR(r);
    assert(*(int*)key.data == ilast);
    assert(*(int*)data.data == jlast);
    r = dbc->c_get(dbc, dbt_init(&key, NULL, 0), dbt_init(&data, NULL, 0), DB_PREV); CKERR(r);
    assert(*(int*)key.data == ifirst);
    assert(*(int*)data.data == jfirst);
    r = dbc->c_close(dbc);                                      CKERR(r);
    r=txn->commit(txn, 0);    CKERR(r);
}

int
test_main (int argc, char * const argv[]) {
    parse_args(argc, argv);

    setup();
    doit(true);
    test_shutdown();
    setup();
    doit(false);
    test_shutdown();

    return 0;
}

