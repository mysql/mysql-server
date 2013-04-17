/* -*- mode: C; c-basic-offset: 4 -*- */

#ident "Copyright (c) 2009 Tokutek Inc.  All rights reserved."
#ident "$Id$"

/* Purpose of this test is to verify correct behavior of transactional file
 * operations.  File operations to be tested (and expected results) are:
 *  - open
 *  - create  (dictionary is created only if transaction is committed)
 *  - rename  (dictionary is renamed only if transaction is committed)
 *  - delete  (dictionary is deleted only if transaction is committed)
 *
 *
 * TODO:  
 *   - verify correct behavior with "subdb" names (e.g. foo/bar)
 *   - Yoni Notes
 *    - Directory based stuff (lock types?)
 *    - Done:
 *     - Create commit
 *      - verify before operation, after operation, and after commit
 *     - Create abort
 *      - verify before operation, after operation, and after abort
 *     - Remove commit
 *      - verify before operation, after operation, and after commit
 *     - Remove abort
 *      - verify before operation, after operation, and after abort
 *     - Rename commit
 *      - verify before operation, after operation, and after commit
 *     - Rename abort
 *      - verify before operation, after operation, and after abort
 *     - Open commit
 *      - verify before operation, after operation, and after commit
 *     - Open abort
 *      - verify before operation, after operation, and after abort
 *    - Maybe have tests for:
 *     - Create EXCL
 *     - Create failed lock
 *      - Due to another txn 'dbremove' but didn't commit yet
 *     - Open failed lock
 *      - Due to other txn 'creating' but didn't yet commit
 *     - Remove (oldname) failed lock
 *      - Some other txn renamed (oldname->newname)
 *     - Rename (oldname->newname) failed lock
 *      - Some other txn renamed (foo->oldname)
 *     - Rename (oldname->newname) failed lock
 *      - Some other txn renamed (newname->foo)
 *
 *     - Remove fail on open db                          EINVAL
 *     - Rename fail on open (oldname) db                EINVAL
 *     - Rename fail on open (newname) db                EINVAL
 *
 *     - Remove fail on non-existant                     ENOENT
 *     - Rename fail on non-existant oldname db          ENOENT
 *
 *     - Rename fail on not open but existant newname db EEXIST
 *
 *     - In one txn:
 *      - Rename a->b
 *      - create a
 *     - In one txn: (Truncate table foo)
 *      - 'x' was in a from prev transaction
 *      - Remove a
 *      - Create a
 *      - insert 'y' into a.
 *      - commit: a has 'y' but not 'x'
 *      - abort: a has 'x' but not 'y'
 *     - In one txn:
 *      - Create a
 *      - remove a
 *      - create a
 *      - remove a...
 *
 *
 * overview:
 *
 * begin txn
 *  create a, b, c
 * commit txn
 *
 * verify_abcd()     // verify a,b,c exist, c2 and x do not exist
 *
 * begin txn
 *  perform_ops()   // delete b, rename c to c2, create x
 *  verify_ac2dx()   // verify a,c2,x exist, b and c do not
 * abort txn
 * verify_abcd()     // verify a,b,c exist, c2 and x do not exist
 * begin_txn
 *  perform_ops()
 * commit
 * verify_ac2dx()    // verify a,c2,x exist, b and c do not
 *
 * 
 * perform_ops() {
 *   delete b
 *   rename c to c2
 *   create x
 * }
 *
 *
 * verify_abcd() {
 *    a,b,c,d exist, x, c2 do not exist
 * }
 *
 *
 * verify_ac2dx() {
 *   a  exists
 *   c2 exists
 *   d  exists
 *   x  exists
 *   b  does not exist
 *   c  does not exist
 * }
 *
 *
 * Future work (possible enhancements to this test, if desired):
 *  - beyond verifying that a dictionary exists, open it and read one entry, verify that the entry is correct
 *    (especially useful for renamed dictionary)
 *  - perform repeatedly in multiple threads
 *
 */


#include "test.h"
#include <db.h>

static DB_ENV *env;


static void
setup (void) {
    int r;
    system("rm -rf " ENVDIR);
    r=toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);       CKERR(r);

    r=db_env_create(&env, 0); CKERR(r);
    env->set_errfile(env, stderr);
    r=env->open(env, ENVDIR, DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_MPOOL|DB_INIT_TXN|DB_CREATE|DB_PRIVATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
}



static void
test_shutdown(void) {
    int r;
    r=env->close(env, 0); CKERR(r);
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
verify_locked_open(char * name) {
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
verify_locked_remove(char * name) {
    int r;
    DB_TXN * txn;

    r=env->txn_begin(env, 0, &txn, 0); CKERR(r);
    r = env->dbremove(env, txn, name, NULL, 0);
    CKERR2(r, DB_LOCK_NOTGRANTED);
    r=txn->abort(txn); CKERR(r);
}

static void
verify_locked_rename(char * oldname, char * newname) {
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


int
test_main (int argc, char *argv[]) {
    parse_args(argc, argv);
    setup();
    print_engine_status(env);
    test_fileops_1();
    print_engine_status(env);
    test_fileops_2();
    print_engine_status(env);
    test_shutdown();
    return 0;
}
