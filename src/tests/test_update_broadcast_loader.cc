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


static int update_fun(DB *UU(db),
                      const DBT *UU(key),
                      const DBT *UU(old_val), const DBT *extra,
                      void (*set_val)(const DBT *new_val,
                                         void *set_extra),
                      void *set_extra) {
    set_val(extra, set_extra);
    return 0;
}


static int generate_row_for_del(
    DB *UU(dest_db), 
    DB *UU(src_db),
    DBT_ARRAY *dest_key_arrays,
    const DBT *UU(src_key), 
    const DBT *UU(src_val)
    )
{
    toku_dbt_array_resize(dest_key_arrays, 1);
    DBT *dest_key = &dest_key_arrays->dbts[0];
    dest_key->flags = 0;
    dest_key->size=0;
    return 0;
}

static int generate_row_for_put(
    DB *UU(dest_db), 
    DB *UU(src_db),
    DBT_ARRAY *dest_key_arrays, 
    DBT_ARRAY *dest_val_arrays,
    const DBT *UU(src_key), 
    const DBT *UU(src_val)
    ) 
{
    toku_dbt_array_resize(dest_key_arrays, 1);
    toku_dbt_array_resize(dest_val_arrays, 1);
    DBT *dest_key = &dest_key_arrays->dbts[0];
    DBT *dest_val = &dest_val_arrays->dbts[0];
    dest_key->flags = 0;
    dest_val->flags = 0;

    uint8_t src_val_data;
    assert(src_val->size == 1);
    src_val_data = *(uint8_t *)src_val->data;
    assert(src_val_data == 100);
    dest_key->size=0;
    dest_val->size=0;
    return 0;
}

static void setup (void) {
    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    { int chk_r = toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(chk_r); }
    { int chk_r = db_env_create(&env, 0); CKERR(chk_r); }
    env->set_errfile(env, stderr);
    { int chk_r = env->set_generate_row_callback_for_put(env,generate_row_for_put); CKERR(chk_r); }
    { int chk_r = env->set_generate_row_callback_for_del(env,generate_row_for_del); CKERR(chk_r); }
    env->set_update(env, update_fun);
    { int chk_r = env->open(env, TOKU_TEST_FILENAME, envflags, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(chk_r); }
}

static void cleanup (void) {
    { int chk_r = env->close(env, 0); CKERR(chk_r); }
}

static void run_test(void) {
    DB* db = NULL;
    DB_LOADER* loader = NULL;
    DBT key, val;
    uint32_t mult_db_flags = 0;
    uint32_t mult_dbt_flags = DB_DBT_REALLOC;
    uint8_t key_data = 0;
    uint8_t val_data = 0;
    

    IN_TXN_COMMIT(env, NULL, txn_create, 0, {
            { int chk_r = db_create(&db, env, 0); CKERR(chk_r); }
            { int chk_r = db->open(db, txn_create, "foo.db", NULL, DB_BTREE, DB_CREATE, 0666); CKERR(chk_r); }
        });


    dbt_init(&key,&key_data,sizeof(uint8_t));
    dbt_init(&val,&val_data,sizeof(uint8_t));

    val_data = 1;


    //
    // now do an update broadcast that will set the val to something bigger
    //
    val_data = 100;
    IN_TXN_COMMIT(env, NULL, txn_broadcast, 0, {
            { int chk_r = db->update_broadcast(db, txn_broadcast, &val, DB_IS_RESETTING_OP); CKERR(chk_r); }
        });

    //
    // now create a loader
    //
    IN_TXN_COMMIT(env, NULL, txn_loader, 0, {
        // create DB
            { int chk_r = env->create_loader(
            env,
            txn_loader,
            &loader,
            db,
            1,
            &db,
            &mult_db_flags,
            &mult_dbt_flags,
            0
                    ); CKERR(chk_r); }
            { int chk_r = loader->put(loader, &key, &val); CKERR(chk_r); }
            { int chk_r = loader->close(loader); CKERR(chk_r); }
        });

    IN_TXN_COMMIT(env, NULL, txn_update, 0, {
            { int chk_r = db->update(db, txn_update, &key, &val, 0); CKERR(chk_r); }
        });

    { int chk_r = db->close(db, 0); CKERR(chk_r); }

}

int test_main (int argc, char * const argv[]) {
    parse_args(argc, argv);
    setup();
    run_test();
    cleanup();
    return 0;
}
