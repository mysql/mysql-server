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
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
// Test that isolation works right for subtransactions.
// In particular, check to see what happens if a subtransaction has different isolation level from its parent.

#include "test.h"

const int envflags = DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN|DB_PRIVATE;

int test_main (int argc, char * const argv[]) {
    parse_args(argc, argv);
    int r;
    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO);
    DB_ENV *env;
    r = db_env_create(&env, 0);                                                         CKERR(r);
    env->set_errfile(env, stderr);
    r = env->open(env, TOKU_TEST_FILENAME, envflags, S_IRWXU+S_IRWXG+S_IRWXO);                      CKERR(r);
    
    DB *db;
    {
        DB_TXN *txna;
        r = env->txn_begin(env, NULL, &txna, 0);                                        CKERR(r);

        r = db_create(&db, env, 0);                                                     CKERR(r);
        r = db->open(db, txna, "foo.db", NULL, DB_BTREE, DB_CREATE, 0666);              CKERR(r);

        DBT key,val;
        r = db->put(db, txna, dbt_init(&key, "a", 2), dbt_init(&val, "a", 2), 0);       CKERR(r);

        r = txna->commit(txna, 0);                                                      CKERR(r);
    }
    DB_TXN *txn_put, *txn_committed, *txn_uncommitted;
    r = env->txn_begin(env, NULL, &txn_put, DB_READ_COMMITTED);                          CKERR(r);
    r = env->txn_begin(env, NULL, &txn_committed, DB_READ_COMMITTED);                          CKERR(r);
    r = env->txn_begin(env, NULL, &txn_uncommitted, DB_READ_UNCOMMITTED);                          CKERR(r);

    //
    // test a simple get
    //
    {
        DBT key,val;
        r = db->put(db, txn_put, dbt_init(&key, "x", 2), dbt_init(&val, "x", 2), 0);   CKERR(r);
        dbt_init_malloc(&val);
        r = db->get(db, txn_put, dbt_init(&key, "x", 2), &val, 0);  CKERR(r);
        toku_free(val.data);

        dbt_init_malloc(&val);
        r = db->get(db, txn_committed, dbt_init(&key, "x", 2), &val, 0);    CKERR2(r, DB_NOTFOUND);
        toku_free(val.data);

        dbt_init_malloc(&val);
        r = db->get(db, txn_uncommitted, dbt_init(&key, "x", 2), &val, 0);  CKERR(r);
        toku_free(val.data);
        
        r = db->del(db, txn_put, dbt_init(&key, "a", 2), 0);  CKERR(r);

        dbt_init_malloc(&val);
        r = db->get(db, txn_put, dbt_init(&key, "a", 2), &val, 0);  CKERR2(r, DB_NOTFOUND);
        toku_free(val.data);

        dbt_init_malloc(&val);
        r = db->get(db, txn_committed, dbt_init(&key, "a", 2), &val, 0);    CKERR(r);
        toku_free(val.data);

        dbt_init_malloc(&val);
        r = db->get(db, txn_uncommitted, dbt_init(&key, "a", 2), &val, 0);  CKERR2(r, DB_NOTFOUND);
        toku_free(val.data);

        val.data = NULL;
    }

    
    r = txn_put->commit(txn_put, 0);                                                          CKERR(r);
    r = txn_committed->commit(txn_committed, 0);                                             CKERR(r);
    r = txn_uncommitted->commit(txn_uncommitted, 0);                                             CKERR(r);

    r = env->txn_begin(env, NULL, &txn_put, DB_READ_COMMITTED);                          CKERR(r);
    r = env->txn_begin(env, NULL, &txn_committed, DB_READ_COMMITTED);                          CKERR(r);
    r = env->txn_begin(env, NULL, &txn_uncommitted, DB_READ_UNCOMMITTED);                          CKERR(r);

    //
    // test a simple get
    //
    {
        DBT key,val;
        DBT curr_key, curr_val;
        DBC* cursor_committed = NULL;
        DBC* cursor_uncommitted = NULL;
        memset(&curr_key, 0, sizeof(curr_key));
        memset(&curr_val, 0, sizeof(curr_val));

        r = db->cursor(db, txn_committed, &cursor_committed, 0); assert(r == 0);
        r = db->cursor(db, txn_uncommitted, &cursor_uncommitted, 0); assert(r == 0);

        r = db->put(db, txn_put, dbt_init(&key, "y", 2), dbt_init(&val, "y", 2), 0);   CKERR(r);

        r = cursor_uncommitted->c_get(cursor_uncommitted, &curr_key, &curr_val, DB_NEXT); CKERR(r);
        assert(((char *)(curr_key.data))[0] == 'x');
        assert(((char *)(curr_val.data))[0] == 'x');

        r = cursor_committed->c_get(cursor_committed, &curr_key, &curr_val, DB_NEXT); CKERR(r);
        assert(((char *)(curr_key.data))[0] == 'x');
        assert(((char *)(curr_val.data))[0] == 'x');
    


        r = cursor_committed->c_get(cursor_committed, &curr_key, &curr_val, DB_NEXT); CKERR2(r, DB_NOTFOUND);
        r = cursor_uncommitted->c_get(cursor_uncommitted, &curr_key, &curr_val, DB_NEXT); CKERR(r);
        assert(((char *)(curr_key.data))[0] == 'y');
        assert(((char *)(curr_val.data))[0] == 'y');

        cursor_committed->c_close(cursor_committed);
        cursor_uncommitted->c_close(cursor_uncommitted);
    }
    r = txn_put->commit(txn_put, 0);                                                          CKERR(r);
    r = txn_committed->commit(txn_committed, 0);                                             CKERR(r);
    r = txn_uncommitted->commit(txn_uncommitted, 0);                                             CKERR(r);


    r = db->close(db, 0);                                                               CKERR(r);
    r = env->close(env, 0);                                                             CKERR(r);
    
    return 0;
}
