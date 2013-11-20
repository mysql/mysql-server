/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id: test_get_max_row_size.cc 45903 2012-07-19 13:06:39Z leifwalsh $"
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
#include "test.h"

int test_main(int argc, char * const argv[])
{
    int r;
    DB * db;
    DB_ENV * env;
    (void) argc;
    (void) argv;

    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    r = toku_os_mkdir(TOKU_TEST_FILENAME, 0755); { int chk_r = r; CKERR(chk_r); }

    // set things up
    r = db_env_create(&env, 0); 
    CKERR(r);
    r = env->open(env, TOKU_TEST_FILENAME, DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN|DB_PRIVATE, 0755); 
    CKERR(r);
    r = db_create(&db, env, 0); 
    CKERR(r);
    r = db->open(db, NULL, "foo.db", NULL, DB_BTREE, DB_CREATE, 0644); 
    CKERR(r);


    DB_TXN* txn = NULL;
    r = env->txn_begin(env, 0, &txn, DB_TXN_SNAPSHOT);
    CKERR(r);

    int k = 1;
    int v = 10;
    DBT key, val;
    r = db->put(
        db, 
        txn, 
        dbt_init(&key, &k, sizeof k), 
        dbt_init(&val, &v, sizeof v), 
        0
        );
    CKERR(r);
    k = 2;
    v = 20;
    r = db->put(
        db, 
        txn, 
        dbt_init(&key, &k, sizeof k), 
        dbt_init(&val, &v, sizeof v), 
        0
        );
    CKERR(r);
    r = txn->commit(txn, 0);
    CKERR(r);
    
    r = env->txn_begin(env, 0, &txn, DB_TXN_SNAPSHOT | DB_TXN_READ_ONLY);
    CKERR(r);
    DBC* cursor = NULL;
    r = db->cursor(db, txn, &cursor, 0);
    CKERR(r);
    DBT key1, val1;
    memset(&key1, 0, sizeof key1);
    memset(&val1, 0, sizeof val1);
    r = cursor->c_get(cursor, &key1, &val1, DB_FIRST);
    CKERR(r);
    invariant(key1.size == sizeof(int));
    invariant(*(int *)key1.data == 1);
    invariant(val1.size == sizeof(int));
    invariant(*(int *)val1.data == 10);

    r = cursor->c_get(cursor, &key1, &val1, DB_NEXT);
    CKERR(r);
    invariant(key1.size == sizeof(int));
    invariant(*(int *)key1.data == 2);
    invariant(val1.size == sizeof(int));
    invariant(*(int *)val1.data == 20);

    r = cursor->c_close(cursor);
    CKERR(r);
    r = txn->commit(txn, 0);
    CKERR(r);    

    // clean things up
    r = db->close(db, 0); 
    CKERR(r);
    r = env->close(env, 0); 
    CKERR(r);

    return 0;
}
