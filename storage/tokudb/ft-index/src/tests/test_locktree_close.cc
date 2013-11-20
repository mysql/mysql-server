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
#include <errno.h>
#include <sys/stat.h>
#include <db.h>


static void
test_cursor (void) {
    if (verbose) printf("test_cursor\n");

    DB_ENV * env;
    DB *db;
    DB_TXN * const null_txn = 0;
    const char * const fname = "test.cursor.brt";
    int r;

    /* create the dup database file */
    r = db_env_create(&env, 0);        assert(r == 0);
    env->set_errfile(env, stderr);
    r = env->open(env, TOKU_TEST_FILENAME, DB_INIT_TXN | DB_INIT_LOCK |DB_CREATE|DB_INIT_MPOOL|DB_THREAD|DB_PRIVATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
    r = db_create(&db, env, 0); CKERR(r);
    db->set_errfile(db,stderr); // Turn off those annoying errors
    r = db->open(db, null_txn, fname, "main", DB_BTREE, DB_CREATE, 0666); assert(r == 0);

    DBC* cursor;
    DBT k0; memset(&k0, 0, sizeof k0);
    DBT v0; memset(&v0, 0, sizeof v0);
    DB_TXN* txn = NULL;
    r = env->txn_begin(env, NULL, &txn, DB_SERIALIZABLE); CKERR(r);
    r = db->cursor(db, txn, &cursor, 0); CKERR(r);
    r = cursor->c_set_bounds(
        cursor, 
        db->dbt_neg_infty(), 
        db->dbt_pos_infty(),
        true,
        0
        );
    r = cursor->c_close(cursor); CKERR(r);
    r = db->close(db, 0); CKERR(r);

    r = db_create(&db, env, 0); CKERR(r);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, DB_THREAD, 0666); assert(r == 0);
    DB_TXN* txn2 = NULL;
    env->txn_begin(env, NULL, &txn2, DB_SERIALIZABLE);
    int k = htonl(1);
    int v = htonl(1);
    DBT key, val;
    // #4838 will improperly allow this put to succeed, whereas we should
    // be returning DB_LOCK_NOTGRANTED
    r = db->put(db, txn2, dbt_init(&key, &k, sizeof k), dbt_init(&val, &v, sizeof v), 0);
    CKERR2(r, DB_LOCK_NOTGRANTED); 

    r = txn->commit(txn, 0);
    r = txn2->commit(txn2, 0);

    r = db->close(db, 0); CKERR(r);
    r = env->close(env, 0); assert(r == 0);
}

int
test_main(int argc, char *const argv[]) {

    parse_args(argc, argv);
  
    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO);
    
    test_cursor();

    return 0;
}
