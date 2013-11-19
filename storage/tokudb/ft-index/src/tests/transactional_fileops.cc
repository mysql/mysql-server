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

/* Purpose of this test is to verify correct behavior of transactional file
 * operations.  File operations to be tested (and expected results) are:
 *  - open
 *  - create  (dictionary is created only if transaction is committed)
 *  - rename  (dictionary is renamed only if transaction is committed)
 *  - delete  (dictionary is deleted only if transaction is committed)
 *
 * The following subtests are here:
 *
 *  test_fileops_1:
 *    Verify that operations appear effective within a transaction,
 *    but are truly effective only if the transaction is committed.
 *
 *  test_fileops_2:
 *    Verify that attempting to open, remove or rename a dictionary that
 *    is marked for removal or renaming by another transaction in
 *    progress results in a DB_LOCK_NOTGRANTED error code.
 *
 *  test_fileops_3:
 *    Verify that the correct error codes are returned when attempting
 *    miscellaneous operations that should fail.
 *
 *
 * Future work (possible enhancements to this test, if desired):
 *  - verify correct behavior with "subdb" names (e.g. foo/bar)
 *  - beyond verifying that a dictionary exists, open it and read one entry, verify that the entry is correct
 *    (especially useful for renamed dictionary)
 *  - perform repeatedly in multiple threads
 *
 */


#include "test.h"
#include <db.h>

static DB_ENV *env;
static FILE *error_file = NULL;

static void
setup (void) {
    int r;
    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    r=toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO);       CKERR(r);

    if (verbose==0) {
        char errfname[TOKU_PATH_MAX+1];
	error_file = fopen(toku_path_join(errfname, 2, TOKU_TEST_FILENAME, "stderr"), "w");                             assert(error_file);
    }
    else error_file = stderr;

    r=db_env_create(&env, 0); CKERR(r);
    env->set_errfile(env, error_file ? error_file : stderr);
    r=env->open(env, TOKU_TEST_FILENAME, DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_MPOOL|DB_INIT_TXN|DB_CREATE|DB_PRIVATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
}



static void
test_shutdown(void) {
    int r;
    r=env->close(env, 0); CKERR(r);
    if (verbose==0) {
        fclose(error_file);
        error_file = NULL;
    }
}


// create dictionaries a.db, b.db, c.db
static void
create_abcd(void) {
    int r;
    DB_TXN * txn;
    DB * db_a;
    DB * db_b;
    DB * db_c;
    DB * db_d;
    
    r=env->txn_begin(env, 0, &txn, 0); CKERR(r);
    r=db_create(&db_a, env, 0); CKERR(r);
    r=db_create(&db_b, env, 0); CKERR(r);
    r=db_create(&db_c, env, 0); CKERR(r);
    r=db_create(&db_d, env, 0); CKERR(r);
    
    r=db_a->open(db_a, txn, "a.db", 0, DB_BTREE, DB_CREATE, S_IRWXU|S_IRWXG|S_IRWXO); CKERR(r);
    r=db_b->open(db_b, txn, "b.db", 0, DB_BTREE, DB_CREATE, S_IRWXU|S_IRWXG|S_IRWXO); CKERR(r);
    r=db_c->open(db_c, txn, "c.db", 0, DB_BTREE, DB_CREATE, S_IRWXU|S_IRWXG|S_IRWXO); CKERR(r);
    r=db_d->open(db_d, txn, "d.db", 0, DB_BTREE, DB_CREATE, S_IRWXU|S_IRWXG|S_IRWXO); CKERR(r);
    
    r=db_a->close(db_a, 0); CKERR(r);
    r=db_b->close(db_b, 0); CKERR(r);
    r=db_c->close(db_c, 0); CKERR(r);
    
    r=txn->commit(txn, 0); CKERR(r);

    r=db_d->close(db_d, 0); CKERR(r); //Should work whether close is before or after commit.  Do one after.
}



//   delete b
//   rename c to c2
//   create x
static void
perform_ops(DB_TXN * txn) {
    int r;
    DB * db_x;

    r = env->dbremove(env, txn, "b.db", NULL, 0);  CKERR(r);

    r = env->dbrename(env, txn, "c.db", NULL, "c2.db", 0); CKERR(r);

    r=db_create(&db_x, env, 0); CKERR(r);
    r=db_x->open(db_x, txn, "x.db", 0, DB_BTREE, DB_CREATE, S_IRWXU|S_IRWXG|S_IRWXO); CKERR(r);
    r=db_x->close(db_x, 0); CKERR(r);  // abort requires db be closed first
}


// verify that:
//   dictionaries a.db, b.db, c.db, d.db exist
//   dictionaries x.db and c2.db do not exist
static void
verify_abcd(void) {
    int r;
    DB_TXN * txn;
    DB * db_a;
    DB * db_b;
    DB * db_c;
    DB * db_d;
    DB * db_x;
    DB * db_c2;
    
    r=env->txn_begin(env, NULL, &txn, 0); CKERR(r);
    r=db_create(&db_a, env, 0); CKERR(r);
    r=db_create(&db_b, env, 0); CKERR(r);
    r=db_create(&db_c, env, 0); CKERR(r);
    r=db_create(&db_d, env, 0); CKERR(r);
    r=db_create(&db_x, env, 0); CKERR(r);
    r=db_create(&db_c2, env, 0); CKERR(r);
    
    // should exist:
    r=db_a->open(db_a, txn, "a.db", 0, DB_BTREE, 0, S_IRWXU|S_IRWXG|S_IRWXO); CKERR(r);
    r=db_b->open(db_b, txn, "b.db", 0, DB_BTREE, 0, S_IRWXU|S_IRWXG|S_IRWXO); CKERR(r);
    r=db_c->open(db_c, txn, "c.db", 0, DB_BTREE, 0, S_IRWXU|S_IRWXG|S_IRWXO); CKERR(r);
    r=db_d->open(db_d, txn, "d.db", 0, DB_BTREE, 0, S_IRWXU|S_IRWXG|S_IRWXO); CKERR(r);

    // should not exist:
    r=db_x->open(db_x, txn, "x.db", 0, DB_BTREE, 0, S_IRWXU|S_IRWXG|S_IRWXO); CKERR2(r, ENOENT);
    r=db_c2->open(db_c2, txn, "c2.db", 0, DB_BTREE, 0, S_IRWXU|S_IRWXG|S_IRWXO); CKERR2(r, ENOENT);

    r=db_a->close(db_a, 0); CKERR(r);
    r=db_b->close(db_b, 0); CKERR(r);
    r=db_c->close(db_c, 0); CKERR(r);
    r=db_d->close(db_d, 0); CKERR(r);
    r=db_x->close(db_x, 0); CKERR(r);
    r=db_c2->close(db_c2, 0); CKERR(r);
    
    r=txn->commit(txn, 0); CKERR(r);
}


// verify that:
//   dictionary a.db exists
//   dictionaries b.db, c.db do not exist
//   dictionary c2.db exists
//   dictionary d.db exists
//   dictionary x.db exists
static void
verify_ac2dx(DB_TXN * parent_txn) {
    int r;
    DB_TXN * txn;
    DB * db_a;
    DB * db_b;
    DB * db_c;
    DB * db_d;
    DB * db_x;
    DB * db_c2;
    
    r=env->txn_begin(env, parent_txn, &txn, 0); CKERR(r);
    r=db_create(&db_a, env, 0);  CKERR(r);
    r=db_create(&db_b, env, 0);  CKERR(r);
    r=db_create(&db_c, env, 0);  CKERR(r);
    r=db_create(&db_d, env, 0);  CKERR(r);
    r=db_create(&db_x, env, 0);  CKERR(r);
    r=db_create(&db_c2, env, 0); CKERR(r);
    
    // should exist:
    r=db_a->open(db_a, txn, "a.db", 0, DB_BTREE, 0, S_IRWXU|S_IRWXG|S_IRWXO);    CKERR(r);
    r=db_c2->open(db_c2, txn, "c2.db", 0, DB_BTREE, 0, S_IRWXU|S_IRWXG|S_IRWXO); CKERR(r);
    r=db_d->open(db_d, txn, "d.db", 0, DB_BTREE, 0, S_IRWXU|S_IRWXG|S_IRWXO);    CKERR(r);
    r=db_x->open(db_x, txn, "x.db", 0, DB_BTREE, 0, S_IRWXU|S_IRWXG|S_IRWXO);    CKERR(r);

    // should not exist:
    r=db_b->open(db_b, txn, "b.db", 0, DB_BTREE, 0, S_IRWXU|S_IRWXG|S_IRWXO);    CKERR2(r, ENOENT);
    r=db_c->open(db_c, txn, "c.db", 0, DB_BTREE, 0, S_IRWXU|S_IRWXG|S_IRWXO);    CKERR2(r, ENOENT);

    r=db_a->close(db_a, 0);   CKERR(r);
    r=db_b->close(db_b, 0);   CKERR(r);
    r=db_c->close(db_c, 0);   CKERR(r);
    r=db_d->close(db_d, 0);   CKERR(r);
    r=db_x->close(db_x, 0);   CKERR(r);
    r=db_c2->close(db_c2, 0); CKERR(r);
    
    r=txn->commit(txn, 0); CKERR(r);
}


static void
test_fileops_1(void) {
    int r;
    DB_TXN *txn;

    create_abcd();
    verify_abcd();

    r=env->txn_begin(env, 0, &txn, 0); CKERR(r);
    perform_ops(txn);
    verify_ac2dx(txn);  // verify that operations appear effective within this txn
    r=txn->abort(txn); CKERR(r);

    // verify that aborted transaction changed nothing
    verify_abcd(); 

    r=env->txn_begin(env, 0, &txn, 0); CKERR(r);
    perform_ops(txn);
    verify_ac2dx(txn);  // verify that operations appear effective within this txn
    r=txn->commit(txn, 0); CKERR(r);

    // verify that committed transaction actually changed db
    verify_ac2dx(NULL);
}



static void
verify_locked_open(const char * name) {
    int r;
    DB_TXN * txn;
    DB * db;

    r=env->txn_begin(env, 0, &txn, 0); CKERR(r);
    r=db_create(&db, env, 0); CKERR(r);
    r=db->open(db, txn, name, 0, DB_BTREE, DB_CREATE, S_IRWXU|S_IRWXG|S_IRWXO); 
    CKERR2(r, DB_LOCK_NOTGRANTED);
    r=db->close(db, 0); CKERR(r);  // always safe to close
    r=txn->abort(txn); CKERR(r);
}

static void
verify_locked_remove(const char * name) {
    int r;
    DB_TXN * txn;

    r=env->txn_begin(env, 0, &txn, 0); CKERR(r);
    r = env->dbremove(env, txn, name, NULL, 0);
    CKERR2(r, DB_LOCK_NOTGRANTED);
    r=txn->abort(txn); CKERR(r);
}

static void
verify_locked_rename(const char * oldname, const char * newname) {
    int r;
    DB_TXN * txn;

    r=env->txn_begin(env, 0, &txn, 0); CKERR(r);
    r = env->dbrename(env, txn, oldname, NULL, newname, 0); 
    CKERR2(r, DB_LOCK_NOTGRANTED);
    r=txn->abort(txn); CKERR(r);
}


// Purpose of test_fileops_2() is to verify correct operation of 
// directory range locks.  It should not be possible to open or
// rename or remove a dictionary that is marked for removal or
// rename by another open transaction.
static void
test_fileops_2(void) {
    int r;
    DB_TXN * txn_a;

    verify_ac2dx(NULL);  // should still exist

    // begin txn_a
    //  remove a
    //  create e
    //  rename x->x2
    //  rename c2->c3
    //  open x2, c3, should succeed
    //  close x2, c3
    {
	DB * db_e;
	DB * db_c3;
	DB * db_x2;

 	r=env->txn_begin(env, 0, &txn_a, 0); CKERR(r);
	r=db_create(&db_e, env, 0); CKERR(r);
	r=db_create(&db_x2, env, 0); CKERR(r);
	r=db_create(&db_c3, env, 0); CKERR(r);

	r = env->dbremove(env, txn_a, "a.db", NULL, 0);  CKERR(r);
	r=db_e->open(db_e, txn_a, "e.db", 0, DB_BTREE, DB_CREATE, S_IRWXU|S_IRWXG|S_IRWXO); CKERR(r);
	r = env->dbrename(env, txn_a, "x.db", NULL, "x2.db", 0); CKERR(r);
	r = env->dbrename(env, txn_a, "c2.db", NULL, "c3.db", 0); CKERR(r);

	r=db_x2->open(db_x2, txn_a, "x2.db", 0, DB_BTREE, 0, S_IRWXU|S_IRWXG|S_IRWXO); CKERR(r);
	r=db_c3->open(db_c3, txn_a, "c3.db", 0, DB_BTREE, 0, S_IRWXU|S_IRWXG|S_IRWXO); CKERR(r);

	r=db_e->close(db_e, 0);   CKERR(r);  // abort requires db be closed first
	r=db_x2->close(db_x2, 0); CKERR(r);  // abort requires db be closed first
	r=db_c3->close(db_c3, 0); CKERR(r);  // abort requires db be closed first
	
    }

    // within another transaction:
    //   open a, should fail   DB_LOCK_NOTGRANTED
    //   open e, should fail   DB_LOCK_NOTGRANTED
    //   open x, should fail   DB_LOCK_NOTGRANTED
    //   open x2, should fail  DB_LOCK_NOTGRANTED
    //   open c2, should fail  DB_LOCK_NOTGRANTED
    //   open c3, should fail  DB_LOCK_NOTGRANTED
    //   remove a, e, x, x2, c2, c3  DB_LOCK_NOTGRANTED
    //   rename a, e, x, x2, c2, c3  DB_LOCK_NOTGRANTED

    verify_locked_open("a.db");
    verify_locked_open("e.db");
    verify_locked_open("x.db");
    verify_locked_open("x2.db");
    verify_locked_open("c2.db");
    verify_locked_open("c3.db");

    verify_locked_remove("a.db");
    verify_locked_remove("e.db");
    verify_locked_remove("x.db");
    verify_locked_remove("x2.db");
    verify_locked_remove("c2.db");
    verify_locked_remove("c3.db");

    verify_locked_rename("a.db", "z.db");
    verify_locked_rename("e.db", "z.db");
    verify_locked_rename("x.db", "z.db");
    verify_locked_rename("x2.db", "z.db");
    verify_locked_rename("c2.db", "z.db");
    verify_locked_rename("c3.db", "z.db");

    verify_locked_rename("d.db", "a.db");
    verify_locked_rename("d.db", "e.db");
    verify_locked_rename("d.db", "x.db");
    verify_locked_rename("d.db", "x2.db");
    verify_locked_rename("d.db", "c2.db");
    verify_locked_rename("d.db", "c3.db");


    r=txn_a->abort(txn_a); CKERR(r);
    
}


static void
test_fileops_3(void) {
    // verify cannot remove an open db

    int r;
    DB_TXN * txn_a;
    DB_TXN * txn_b;
    DB * db_d;

    r=env->txn_begin(env, 0, &txn_a, 0); CKERR(r);
    r=db_create(&db_d, env, 0); CKERR(r);
    r=db_d->open(db_d, txn_a, "d.db", 0, DB_BTREE, 0, S_IRWXU|S_IRWXG|S_IRWXO); CKERR(r);

    // Verify correct error return codes when trying to
    // remove or rename an open dictionary
    r=env->txn_begin(env, 0, &txn_b, 0); CKERR(r);
    r = env->dbremove(env, txn_b, "d.db", NULL, 0);  
    CKERR2(r, EINVAL);
    r = env->dbrename(env, txn_b, "d.db", NULL, "z.db", 0);  
    CKERR2(r, EINVAL);
    r = env->dbrename(env, txn_b, "a.db", NULL, "d.db", 0);  
    CKERR2(r, EINVAL);
    r=db_d->close(db_d, 0); CKERR(r);
    r=txn_b->abort(txn_b); CKERR(r);


    // verify correct error return codes when trying to 
    // remove or rename a non-existent dictionary
    r = env->dbremove(env, txn_a, "nonexistent.db", NULL, 0);  
    CKERR2(r, ENOENT);
    r = env->dbrename(env, txn_a, "nonexistent.db", NULL, "z.db", 0);  
    CKERR2(r, ENOENT);

    // verify correct error return code when trying to
    // rename a dictionary to a name that already exists
    r = env->dbrename(env, txn_a, "a.db", NULL, "d.db", 0);  
    CKERR2(r, EEXIST);
    
    r=txn_a->abort(txn_a); CKERR(r);
}


int
test_main (int argc, char *const argv[]) {
    parse_args(argc, argv);
    setup();
    if (verbose >= 2) {
	printf("Immediately after setup:\n");
	print_engine_status(env);
    }
    test_fileops_1();
    if (verbose >= 2) {
	printf("After test_1:\n");
	print_engine_status(env);
    }
    test_fileops_2();
    test_fileops_3();
    if (verbose >= 2) {
	printf("After test_2 and test_3:\n");
	print_engine_status(env);
    }
    test_shutdown();
    return 0;
}
