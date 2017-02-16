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


static void
test_txn_recover3 (int nrows) {
    if (verbose) printf("test_txn_recover1:%d\n", nrows);

    int r;
    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO);
    char dirname[TOKU_PATH_MAX+1];
    toku_os_mkdir(toku_path_join(dirname, 2, TOKU_TEST_FILENAME, "t.tokudb"), S_IRWXU+S_IRWXG+S_IRWXO);

    DB_ENV *env;
    DB *mdb, *sdb;
    DB_TXN * const null_txn = 0;
    const char * const fname = "t.tokudb/main.ft_handle";
    const char * const sname = "t.tokudb/status.ft_handle";

    r = db_env_create(&env, 0);        assert(r == 0);
    env->set_errfile(env, stderr);
    r = env->open(env, TOKU_TEST_FILENAME, DB_CREATE|DB_INIT_MPOOL|DB_INIT_TXN|DB_INIT_LOCK|DB_INIT_LOG |DB_THREAD |DB_PRIVATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
    r = env->close(env, 0); CKERR(r);

    r = db_env_create(&env, 0);        assert(r == 0);
    env->set_errfile(env, stderr);
    r = env->open(env, TOKU_TEST_FILENAME, DB_CREATE|DB_INIT_MPOOL|DB_INIT_TXN|DB_INIT_LOCK|DB_INIT_LOG |DB_THREAD |DB_PRIVATE | DB_RECOVER, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);

    r = db_create(&mdb, env, 0); assert(r == 0);
    mdb->set_errfile(mdb,stderr); // Turn off those annoying errors
    r = mdb->open(mdb, null_txn, fname, NULL, DB_BTREE, DB_CREATE+DB_THREAD+DB_AUTO_COMMIT, 0666); assert(r == 0);
    r = mdb->close(mdb, 0); assert(r == 0);

    r = db_create(&sdb, env, 0); assert(r == 0);
    sdb->set_errfile(sdb,stderr); // Turn off those annoying errors
    r = sdb->open(sdb, null_txn, sname, NULL, DB_BTREE, DB_CREATE+DB_THREAD+DB_AUTO_COMMIT, 0666); assert(r == 0);
    r = sdb->close(sdb, 0); assert(r == 0);

    r = db_create(&mdb, env, 0); assert(r == 0);
    mdb->set_errfile(mdb,stderr); // Turn off those annoying errors
    r = mdb->open(mdb, null_txn, fname, NULL, DB_BTREE, DB_CREATE+DB_THREAD+DB_AUTO_COMMIT, 0666); assert(r == 0);

    r = db_create(&sdb, env, 0); assert(r == 0);
    sdb->set_errfile(sdb,stderr); // Turn off those annoying errors
    r = sdb->open(sdb, null_txn, sname, NULL, DB_BTREE, DB_CREATE+DB_THREAD+DB_AUTO_COMMIT, 0666); assert(r == 0);


    DB_TXN *txn;
    r = env->txn_begin(env, null_txn, &txn, 0); assert(r == 0);

    int i;
    for (i=0; i<nrows; i++) {
        int k = htonl(i);
        int v = htonl(i);
        DBT key, val;
        r = mdb->put(mdb, txn, dbt_init(&key, &k, sizeof k), dbt_init(&val, &v, sizeof v), 0);
        assert(r == 0); 
        r = sdb->put(sdb, txn, dbt_init(&key, &k, sizeof k), dbt_init(&val, &v, sizeof v), 0);
        assert(r == 0); 
    }
   
    r = txn->commit(txn, 0); assert(r == 0);

    r = mdb->close(mdb, 0); assert(r == 0);
    r = sdb->close(sdb, 0); assert(r == 0);

    r = env->txn_checkpoint(env, 0, 0, 0); assert(r == 0);

    char **names;
    r = env->log_archive(env, &names, 0); assert(r == 0);
    if (names) {
        for (i=0; names[i]; i++)
            printf("%d:%s\n", i, names[i]);
        toku_free(names);
    }

    r = env->close(env, 0); assert(r == 0);

    r = db_env_create(&env, 0);        assert(r == 0);
    env->set_errfile(env, stderr);
    r = env->open(env, TOKU_TEST_FILENAME, DB_CREATE|DB_INIT_MPOOL|DB_INIT_TXN|DB_INIT_LOCK|DB_INIT_LOG |DB_THREAD |DB_PRIVATE | DB_RECOVER, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
    r = env->close(env, 0); assert(r == 0);
}

int
test_main(int argc, char *const argv[]) {

    parse_args(argc, argv);
  
    test_txn_recover3(1);

    return 0;
}
