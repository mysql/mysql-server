/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

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

#ident "Copyright (c) 2009-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

/* Purpose of this test is to verify correct behavior of
 * zombie dbs.  
 *
 * A db is destroyed when it is closed by the user and there are no txns using it.
 * If a transaction creates a db and then closes, that leaves an open db with 
 * no transaction associated with it.  If another transaction then uses the db,
 * and then closes it, then that leaves a zombie db.  The db is closed, but 
 * cannot be destroyed because there is still a transaction associated with it
 * (not the transaction that created it).
 *
 * Outline of this test:
 *
 * begin txn_a
 * create db for new dictionary "foo"
 * commit txn_a
 *  => leaves open db with no txn
 *     (releases range lock on "foo" dname in directory)
 * 
 * begin txn_b
 * insert into db
 * close db
 *   => leaves zombie db, held open by txn_b
 * 
 * 
 * create txn_c
 * 
 * test1:
 * try to delete dictionary (env->dbremove(foo)) 
 *   should return DB_LOCK_NOT_GRANTED because txnB is holding range lock on some part of 
 *   the dictionary ("foo") referred to by db
 * 
 * test2:
 * try to rename dictionary (env->dbrename(foo->bar))
 *   should return DB_LOCK_NOT_GRANTED because txnB is holding range lock on some part of 
 *   the dictionary ("foo") referred to by db
 * 
 */

#include "test.h"
#include <db.h>

static DB_ENV *env;
static DB * db;

static void
setup (void) {
    int r;
    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    r=toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO);       CKERR(r);

    r=db_env_create(&env, 0); CKERR(r);
    env->set_errfile(env, stderr);
    r=env->open(env, TOKU_TEST_FILENAME, DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_MPOOL|DB_INIT_TXN|DB_CREATE|DB_PRIVATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
}



static void
test_shutdown(void) {
    int r;
    r=env->close(env, 0); CKERR(r);
}

static void
test_zombie_db(void) {
    int r;
    DBT key, val;
    DB_TXN * txn_b;

    r=env->txn_begin(env, 0, &txn_b, 0); CKERR(r);

    {
	DB_TXN * txn_a;
	dbt_init(&key, "key1", 4);
	dbt_init(&val, "val1", 4);

	r=env->txn_begin(env, 0, &txn_a, 0); CKERR(r);
	r=db_create(&db, env, 0); CKERR(r);
	r=db->open(db, txn_a, "foo.db", 0, DB_BTREE, DB_CREATE, S_IRWXU|S_IRWXG|S_IRWXO); CKERR(r);
	r=db->put(db, txn_a, &key, &val, 0); CKERR(r);
	r=txn_a->commit(txn_a, 0); CKERR(r);
    }

    // db is now open with no associated txn

    {
	dbt_init(&key, "key2", 4);
	dbt_init(&val, "val2", 4);

	r = db->put(db, txn_b, &key, &val, 0); CKERR(r);
	r=db->close(db, 0); CKERR(r);
    }
    
    // db is now closed, but cannot be destroyed until txn_b closes
    // db is now a zombie

    {
	DB_TXN * txn_c;

	r=env->txn_begin(env, 0, &txn_c, 0); CKERR(r);
	r = env->dbremove(env, txn_c, "foo.db", NULL, 0);  
	CKERR2(r, DB_LOCK_NOTGRANTED);
	r = env->dbrename(env, txn_c, "foo.db", NULL, "bar.db", 0); 
	CKERR2(r, DB_LOCK_NOTGRANTED);
	r=txn_c->commit(txn_c, 0); CKERR(r);
    }
    
    r=txn_b->commit(txn_b, 0); CKERR(r);
    
    // db should now be destroyed
}

int
test_main (int argc, char *const argv[]) {
    parse_args(argc, argv);
    setup();
    test_zombie_db();
    test_shutdown();
    return 0;
}
