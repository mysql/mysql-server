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
// test that an update calls back into the update function

#include "test.h"

const int envflags = DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN|DB_PRIVATE;

DB_ENV *env;

bool cmp_desc_is_four;
uint32_t four_byte_desc = 0xffffffff;
uint64_t eight_byte_desc = 0x12345678ffffffff;


static int generate_row_for_put(
    DB *UU(dest_db), 
    DB *UU(src_db), 
    DBT_ARRAY *dest_key_arrays, 
    DBT_ARRAY *dest_val_arrays, 
    const DBT *src_key, 
    const DBT *src_val
    ) 
{    
    toku_dbt_array_resize(dest_key_arrays, 1);
    toku_dbt_array_resize(dest_val_arrays, 1);
    DBT *dest_key = &dest_key_arrays->dbts[0];
    DBT *dest_val = &dest_val_arrays->dbts[0];
    dest_key->data = src_key->data;
    dest_key->size = src_key->size;
    dest_key->flags = 0;
    dest_val->data = src_val->data;
    dest_val->size = src_val->size;
    dest_val->flags = 0;
    return 0;
}
static void assert_cmp_desc_valid (DB* db) {
    if (cmp_desc_is_four) {
        assert(db->cmp_descriptor->dbt.size == sizeof(four_byte_desc));
    }
    else {
        assert(db->cmp_descriptor->dbt.size == sizeof(eight_byte_desc));
    }
    unsigned char* CAST_FROM_VOIDP(cmp_desc_data, db->cmp_descriptor->dbt.data);
    assert(cmp_desc_data[0] == 0xff);
    assert(cmp_desc_data[1] == 0xff);
    assert(cmp_desc_data[2] == 0xff);
    assert(cmp_desc_data[3] == 0xff);
}

static void assert_desc_four (DB* db) {
    assert(db->descriptor->dbt.size == sizeof(four_byte_desc));
    assert(*(uint32_t *)(db->descriptor->dbt.data) == four_byte_desc);
}
static void assert_desc_eight (DB* db) {
    assert(db->descriptor->dbt.size == sizeof(eight_byte_desc));
    assert(*(uint64_t *)(db->descriptor->dbt.data) == eight_byte_desc);
}

static int
desc_int64_dbt_cmp (DB *db, const DBT *a, const DBT *b) {
    assert_cmp_desc_valid(db);
    assert(a);
    assert(b);

    assert(a->size == sizeof(int64_t));
    assert(b->size == sizeof(int64_t));

    int64_t x = *(int64_t *) a->data;
    int64_t y = *(int64_t *) b->data;

    if (x<y) return -1;
    if (x>y) return 1;
    return 0;
}


static void open_env(void) {
    { int chk_r = db_env_create(&env, 0); CKERR(chk_r); }
    env->set_errfile(env, stderr);
    int r = env->set_default_bt_compare(env, desc_int64_dbt_cmp); CKERR(r);
    //r = env->set_cachesize(env, 0, 500000, 1); CKERR(r);
    r = env->set_generate_row_callback_for_put(env, generate_row_for_put); CKERR(r);
    { int chk_r = env->open(env, TOKU_TEST_FILENAME, envflags, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(chk_r); }
}

static void setup (void) {
    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    { int chk_r = toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(chk_r); }
    open_env();
}

static void cleanup (void) {
    int chk_r = env->close(env, 0);
    CKERR(chk_r);
    env = NULL;
}

static void do_inserts_and_queries(DB* db) {
    int r = 0;
    DB_TXN* write_txn = NULL;
    r = env->txn_begin(env, NULL, &write_txn, 0);
    CKERR(r);
    for (int i = 0; i < 2000; i++) {
        uint64_t key_data = random();
        uint64_t val_data = random();
        DBT key, val;
        dbt_init(&key, &key_data, sizeof(key_data));
        dbt_init(&val, &val_data, sizeof(val_data));
        { int chk_r = db->put(db, write_txn, &key, &val, 0); CKERR(chk_r); }
    }
    r = write_txn->commit(write_txn, 0);
    CKERR(r);
    for (int i = 0; i < 2; i++) {
        DB_TXN* read_txn = NULL;
        r = env->txn_begin(env, NULL, &read_txn, 0);
        CKERR(r);
        DBC* cursor = NULL;
        r = db->cursor(db, read_txn, &cursor, 0);
        CKERR(r);
        if (i == 0) { 
            r = cursor->c_set_bounds(
                cursor,
                db->dbt_neg_infty(),
                db->dbt_pos_infty(),
                true,
                0
                );
            CKERR(r);
        }
        while(r != DB_NOTFOUND) {
            DBT key, val;
            memset(&key, 0, sizeof(key));
            memset(&val, 0, sizeof(val));
            r = cursor->c_get(cursor, &key, &val, DB_NEXT);
            assert(r == 0 || r == DB_NOTFOUND);
        }
        r = cursor->c_close(cursor);
        CKERR(r);
        r = read_txn->commit(read_txn, 0);
        CKERR(r);
    }
}

static void run_test(void) {
    DB* db = NULL;
    int r;
    cmp_desc_is_four = true;

    DBT orig_desc;
    memset(&orig_desc, 0, sizeof(orig_desc));
    orig_desc.size = sizeof(four_byte_desc);
    orig_desc.data = &four_byte_desc;

    DBT other_desc;
    memset(&other_desc, 0, sizeof(other_desc));
    other_desc.size = sizeof(eight_byte_desc);
    other_desc.data = &eight_byte_desc;

    DB_LOADER *loader = NULL;    
    DBT key, val;
    uint64_t k = 0;
    uint64_t v = 0;
    IN_TXN_COMMIT(env, NULL, txn_create, 0, {
            { int chk_r = db_create(&db, env, 0); CKERR(chk_r); }
            assert(db->descriptor == NULL);
            r = db->set_pagesize(db, 2048);
            CKERR(r);
            r = db->set_readpagesize(db, 1024);
            CKERR(r);
            { int chk_r = db->open(db, txn_create, "foo.db", NULL, DB_BTREE, DB_CREATE, 0666); CKERR(chk_r); }
            assert(db->descriptor->dbt.size == 0);
            assert(db->cmp_descriptor->dbt.size == 0);
            { int chk_r = db->change_descriptor(db, txn_create, &orig_desc, DB_UPDATE_CMP_DESCRIPTOR); CKERR(chk_r); }
            assert_desc_four(db);
            assert_cmp_desc_valid(db);
            r = env->create_loader(env, txn_create, &loader, db, 1, &db, NULL, NULL, 0); 
            CKERR(r);
            dbt_init(&key, &k, sizeof k);
            dbt_init(&val, &v, sizeof v);
            r = loader->put(loader, &key, &val); 
            CKERR(r);
            r = loader->close(loader);
            CKERR(r);
            assert_cmp_desc_valid(db);    
        });
    assert_cmp_desc_valid(db);    
    CKERR(r);
    do_inserts_and_queries(db);
    IN_TXN_COMMIT(env, NULL, txn_1, 0, {
            { int chk_r = db->change_descriptor(db, txn_1, &other_desc, 0); CKERR(chk_r); }
        assert_desc_eight(db);
        assert_cmp_desc_valid(db);
    });
    assert_desc_eight(db);
    assert_cmp_desc_valid(db);
    do_inserts_and_queries(db);

    IN_TXN_ABORT(env, NULL, txn_1, 0, {
            { int chk_r = db->change_descriptor(db, txn_1, &orig_desc, 0); CKERR(chk_r); }
        assert_desc_four(db);
        assert_cmp_desc_valid(db);
    });
    assert_desc_eight(db);
    assert_cmp_desc_valid(db);
    do_inserts_and_queries(db);
    
    { 
        int chk_r = db->close(db, 0); CKERR(chk_r); 
        cleanup();
        open_env();
    }

    // verify that after close and reopen, cmp_descriptor is now
    // latest descriptor
    cmp_desc_is_four = false;
    { int chk_r = db_create(&db, env, 0); CKERR(chk_r); }
    { int chk_r = db->open(db, NULL, "foo.db", NULL, DB_BTREE, DB_AUTO_COMMIT, 0666); CKERR(chk_r); }
    assert_desc_eight(db);
    assert_cmp_desc_valid(db);
    do_inserts_and_queries(db);
    { int chk_r = db->close(db, 0); CKERR(chk_r); }

    cmp_desc_is_four = true;
    { int chk_r = db_create(&db, env, 0); CKERR(chk_r); }
    { int chk_r = db->open(db, NULL, "foo.db", NULL, DB_BTREE, DB_AUTO_COMMIT, 0666); CKERR(chk_r); }
    IN_TXN_COMMIT(env, NULL, txn_1, 0, {
            { int chk_r = db->change_descriptor(db, txn_1, &orig_desc, DB_UPDATE_CMP_DESCRIPTOR); CKERR(chk_r); }
        assert_desc_four(db);
        assert_cmp_desc_valid(db);
    });
    assert_desc_four(db);
    assert_cmp_desc_valid(db);
    do_inserts_and_queries(db);
    { int chk_r = db->close(db, 0); CKERR(chk_r); }
    
}

int test_main (int argc, char * const argv[]) {
    parse_args(argc, argv);
    setup();
    run_test();
    cleanup();
    return 0;
}
