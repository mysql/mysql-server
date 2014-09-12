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
#include <stdio.h>

#include <stdlib.h>
#include <unistd.h>
#include <memory.h>
#include <sys/stat.h>
#include <db.h>

static DB *db;
static DB_ENV *env;

static __attribute__((__unused__)) int
length_int_dbt_cmp (DB *db_compare, const DBT *a, const DBT *b) {
    assert(db_compare && a && b);
    assert(a->size > sizeof(int));
    assert(b->size > sizeof(int));

    int i;
    int extra_len_a = *(uint8_t*)((uint8_t*)a->data    +4);
    assert(a->size == sizeof(int)+extra_len_a);

    for (i = 1; i < extra_len_a; i++) {
        assert(((char*)a->data+4)[i] == ' ');
    }

    int extra_len_b = *(uint8_t*)((uint8_t*)b->data+4);
    assert(b->size == sizeof(int)+extra_len_b);
    for (i = 1; i < extra_len_b; i++) {
        assert(((char*)b->data+4)[i] == ' ');
    }

    int x = *(int *) a->data;
    int y = *(int *) b->data;

    if (x<y) return -1;
    if (x>y) return 1;

    if (extra_len_a<extra_len_b) return -1;
    if (extra_len_a>extra_len_b) return 1;
    return 0;
}

static void
setup_db (uint32_t dup_mode) {
    int r;
    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO);

    r = db_env_create(&env, 0); CKERR(r);
#ifdef TOKUDB
    r = env->set_default_bt_compare(env, int_dbt_cmp); CKERR(r);
#endif
    r = env->open(env, TOKU_TEST_FILENAME, DB_INIT_MPOOL | DB_INIT_LOG | DB_INIT_LOCK | DB_INIT_TXN | DB_PRIVATE | DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); 
    CKERR(r);

    {
        DB_TXN *txn = 0;
        r = env->txn_begin(env, 0, &txn, 0); CKERR(r);

        r = db_create(&db, env, 0); CKERR(r);
#ifndef TOKUDB
        r=db->set_bt_compare(db, int_dbt_cmp); CKERR(r);
#endif
        r = db->set_flags(db, dup_mode); assert(r == 0); CKERR(r);
        r = db->open(db, txn, "test.db", 0, DB_BTREE, DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
        r = txn->commit(txn, 0); CKERR(r);
    }
}

static inline DBT *
dbt_init_length(DBT *dbt, int val, uint8_t extra_len, uint8_t* buf) {
    *(int*)buf = val;
    buf[sizeof(int)] = extra_len;
    int i;
    for (i = 1; i < extra_len; i++) {
        buf[sizeof(int)+i] = ' ';
    }
    return dbt_init(dbt, buf, sizeof(int)+extra_len);
}

static void
test_txn_abort (uint32_t dup_mode) {
    setup_db(dup_mode);
    DBT key, val;
    int r;


    DB_TXN *txn = NULL;
    r = env->txn_begin(env, 0, &txn, 0); CKERR(r);
    uint8_t value1[256];
    uint8_t value2[256];

    int k = 1;
    int v1 = 1;
    int v2 = 1;
    uint8_t extra_1 = 1;
    uint8_t extra_2 = 2;
    r = db->put(db, txn, dbt_init(&key, &k, sizeof k), dbt_init_length(&val, v1, extra_1, value1), 0); 
        CKERR(r);
    r = txn->commit(txn, DB_TXN_NOSYNC); 
    txn = NULL;

    r = env->txn_begin(env, 0, &txn, 0); CKERR(r);
    r = db->put(db, txn, dbt_init(&key, &k, sizeof k), dbt_init_length(&val, v2, extra_2, value2), 0); 
        CKERR(r);
    r = db->del(db, txn, dbt_init(&key, &k, sizeof k), DB_DELETE_ANY);
        CKERR(r);
    //Flush by scanning
    {
        DBC *c;
        r = db->cursor(db, txn, &c, 0);
            CKERR(r);
        DBT ck;
        DBT cv;
        memset(&ck, 0, sizeof(ck));
        memset(&cv, 0, sizeof(cv));
        do {
            r = c->c_get(c, &ck, &cv, DB_NEXT);
        } while (r==0);
        CKERR2(r, DB_NOTFOUND);
        r = c->c_close(c);
            CKERR(r);
    }
    r = txn->commit(txn, 0); 
    r=db->close(db, 0); CKERR(r);
    r=env->close(env, 0); CKERR(r);

}

int
test_main(int argc, char *const argv[]) {
    parse_args(argc, argv);
    test_txn_abort(0);
//    test_txn_abort(DB_DUP|DB_DUPSORT);
    return 0;
}
/*
BNC_NBYTESINBUF
comparison does assert that 'val(value) == length(value)'
insert 1,'1' (commit it)
       1,'2 '
do a db->del (1) we'll crash on the assert
*/
