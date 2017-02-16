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
#include <errno.h>
#include <sys/stat.h>
#include <db.h>


static int
db_put (DB *db, DB_TXN *txn, int k, int v) {
    DBT key, val;
    return db->put(db, txn, dbt_init(&key, &k, sizeof k), dbt_init(&val, &v, sizeof v), DB_NOOVERWRITE);
}

static const char *db_error(int error) {
    static char errorbuf[32];
    switch (error) {
    case DB_NOTFOUND: return "DB_NOTFOUND";
    case DB_LOCK_DEADLOCK: return "DB_LOCK_DEADLOCK"; 
    case DB_LOCK_NOTGRANTED: return "DB_LOCK_NOTGRANTED";
    case DB_KEYEXIST: return "DB_KEYEXIST";
    default:
        sprintf(errorbuf, "%d", error);
        return errorbuf;
    }
}

static void
test_txn_nested(int do_commit) {
    if (verbose) printf("test_txn_nested:%d\n", do_commit);

    int r;
    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO);

    DB_ENV *env;
    DB *db;
    DB_TXN * const null_txn = 0;
    const char * const fname = "test.txn.nested.abort.ft_handle";

    /* create the dup database file */
    r = db_env_create(&env, 0);        assert(r == 0);
    env->set_errfile(env, stderr);
    r = env->open(env, TOKU_TEST_FILENAME, DB_CREATE|DB_INIT_MPOOL|DB_INIT_TXN|DB_INIT_LOCK|DB_INIT_LOG |DB_THREAD |DB_PRIVATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
    r = db_create(&db, env, 0); assert(r == 0);
    db->set_errfile(db,stderr); // Turn off those annoying errors
    r = db->open(db, null_txn, fname, "main", DB_BTREE, DB_CREATE+DB_AUTO_COMMIT, 0666); assert(r == 0);
   
    DB_TXN *t1;
    r = env->txn_begin(env, null_txn, &t1, 0); assert(r == 0);
    if (verbose) printf("t1:begin\n");

    DB_TXN *t2;
    r = env->txn_begin(env, t1, &t2, 0); assert(r == 0);
    if (verbose) printf("t2:begin\n");

    r = db_put(db, t2, htonl(1), htonl(1));
    if (verbose) printf("t1:put:%s\n", db_error(r));

    if (do_commit) {
        r = t2->commit(t2, 0); 
        if (verbose) printf("t2:commit:%s\n", db_error(r));
    } else {
        r = t2->abort(t2);
        if (verbose) printf("t2:abort:%s\n", db_error(r));
    }

    r = db->close(db, 0); assert(r == 0);

    r = t1->commit(t1, 0); 
    if (verbose) printf("t1:commit:%s\n", db_error(r));

    r = env->close(env, 0); assert(r == 0);
}


int
test_main(int argc, char *const argv[]) {

    parse_args(argc, argv);
  
    test_txn_nested(0);
    test_txn_nested(1);

    return 0;
}
