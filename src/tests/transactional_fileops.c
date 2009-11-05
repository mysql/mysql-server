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
 * verify_abc()     // verify a,b,c exist, c2 and x do not exist
 *
 * begin txn
 *  perform_ops()   // delete b, rename c to c2, create x
 *  verify_ac2x()   // verify a,c2,x exist, b and c do not
 * abort txn
 * verify_abc()     // verify a,b,c exist, c2 and x do not exist
 * begin_txn
 *  perform_ops()
 * commit
 * verify_ac2x()    // verify a,c2,x exist, b and c do not
 *
 * 
 * perform_ops() {
 *   delete b
 *   rename c to c2
 *   create x
 * }
 *
 *
 * verify_abc() {
 *    a,b,c exist, x, c2 do not exist
 * }
 *
 *
 * verify_ac2x() {
 *   a exists
 *   c2 exists
 *   x exists
 *   b does not exist
 *   c does not exist
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
create_abc(void) {
    int r;
    DB_TXN * txn;
    DB * db_a;
    DB * db_b;
    DB * db_c;
    
    r=env->txn_begin(env, 0, &txn, 0); CKERR(r);
    r=db_create(&db_a, env, 0); CKERR(r);
    r=db_create(&db_b, env, 0); CKERR(r);
    r=db_create(&db_c, env, 0); CKERR(r);
    
    r=db_a->open(db_a, txn, "a.db", 0, DB_BTREE, DB_CREATE, S_IRWXU|S_IRWXG|S_IRWXO); CKERR(r);
    r=db_b->open(db_b, txn, "b.db", 0, DB_BTREE, DB_CREATE, S_IRWXU|S_IRWXG|S_IRWXO); CKERR(r);
    r=db_c->open(db_c, txn, "c.db", 0, DB_BTREE, DB_CREATE, S_IRWXU|S_IRWXG|S_IRWXO); CKERR(r);
    
    r=db_a->close(db_a, 0); CKERR(r);
    r=db_b->close(db_b, 0); CKERR(r);
    
    r=txn->commit(txn, 0); CKERR(r);

    r=db_c->close(db_c, 0); CKERR(r); //Should work whether close is before or after commit.  Do one after.
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
//   dictionaries a.db, b.db, c.db exist
//   dictionaries x.db and c2.db do not exist
static void
verify_abc(void) {
    int r;
    DB_TXN * txn;
    DB * db_a;
    DB * db_b;
    DB * db_c;
    DB * db_x;
    DB * db_c2;
    
    r=env->txn_begin(env, NULL, &txn, 0); CKERR(r);
    r=db_create(&db_a, env, 0); CKERR(r);
    r=db_create(&db_b, env, 0); CKERR(r);
    r=db_create(&db_c, env, 0); CKERR(r);
    r=db_create(&db_x, env, 0); CKERR(r);
    r=db_create(&db_c2, env, 0); CKERR(r);
    
    // should exist:
    r=db_a->open(db_a, txn, "a.db", 0, DB_BTREE, 0, S_IRWXU|S_IRWXG|S_IRWXO); CKERR(r);
    r=db_b->open(db_b, txn, "b.db", 0, DB_BTREE, 0, S_IRWXU|S_IRWXG|S_IRWXO); CKERR(r);
    r=db_c->open(db_c, txn, "c.db", 0, DB_BTREE, 0, S_IRWXU|S_IRWXG|S_IRWXO); CKERR(r);

    // should not exist:
    r=db_x->open(db_x, txn, "x.db", 0, DB_BTREE, 0, S_IRWXU|S_IRWXG|S_IRWXO); CKERR2(r, ENOENT);
    r=db_c2->open(db_c2, txn, "c2.db", 0, DB_BTREE, 0, S_IRWXU|S_IRWXG|S_IRWXO); CKERR2(r, ENOENT);

    r=db_a->close(db_a, 0); CKERR(r);
    r=db_b->close(db_b, 0); CKERR(r);
    r=db_c->close(db_c, 0); CKERR(r);
    r=db_x->close(db_x, 0); CKERR(r);
    r=db_c2->close(db_c2, 0); CKERR(r);
    
    r=txn->commit(txn, 0); CKERR(r);
}


// verify that:
//   dictionary a.db exists
//   dictionaries b.db, c.db do not exist
//   dictionary c2.db exists
//   dictionary x.db exists
static void
verify_ac2x(DB_TXN * parent_txn) {
    int r;
    DB_TXN * txn;
    DB * db_a;
    DB * db_b;
    DB * db_c;
    DB * db_x;
    DB * db_c2;
    
    r=env->txn_begin(env, parent_txn, &txn, 0); CKERR(r);
    r=db_create(&db_a, env, 0);  CKERR(r);
    r=db_create(&db_b, env, 0);  CKERR(r);
    r=db_create(&db_c, env, 0);  CKERR(r);
    r=db_create(&db_x, env, 0);  CKERR(r);
    r=db_create(&db_c2, env, 0); CKERR(r);
    
    // should exist:
    r=db_a->open(db_a, txn, "a.db", 0, DB_BTREE, 0, S_IRWXU|S_IRWXG|S_IRWXO);    CKERR(r);
    r=db_c2->open(db_c2, txn, "c2.db", 0, DB_BTREE, 0, S_IRWXU|S_IRWXG|S_IRWXO); CKERR(r);
    r=db_x->open(db_x, txn, "x.db", 0, DB_BTREE, 0, S_IRWXU|S_IRWXG|S_IRWXO);    CKERR(r);

    // should not exist:
    r=db_b->open(db_b, txn, "b.db", 0, DB_BTREE, 0, S_IRWXU|S_IRWXG|S_IRWXO);    CKERR2(r, ENOENT);
    r=db_c->open(db_c, txn, "c.db", 0, DB_BTREE, 0, S_IRWXU|S_IRWXG|S_IRWXO);    CKERR2(r, ENOENT);

    r=db_a->close(db_a, 0);   CKERR(r);
    r=db_b->close(db_b, 0);   CKERR(r);
    r=db_c->close(db_c, 0);   CKERR(r);
    r=db_x->close(db_x, 0);   CKERR(r);
    r=db_c2->close(db_c2, 0); CKERR(r);
    
    r=txn->commit(txn, 0); CKERR(r);
}


static void
test_fileops(void) {
    int r;
    DB_TXN *txn;

    create_abc();
    verify_abc();

    r=env->txn_begin(env, 0, &txn, 0); CKERR(r);
    perform_ops(txn);
    verify_ac2x(txn);  // verify that operations appear effective within this txn
    r=txn->abort(txn); CKERR(r);

    // verify that aborted transaction changed nothing
    verify_abc(); 

    r=env->txn_begin(env, 0, &txn, 0); CKERR(r);
    perform_ops(txn);
    verify_ac2x(txn);  // verify that operations appear effective within this txn
    r=txn->commit(txn, 0); CKERR(r);

    // verify that committed transaction actually changed db
    verify_ac2x(NULL);
}


int
test_main (int argc, char *argv[]) {
    parse_args(argc, argv);
    setup();
    print_engine_status(env);
    test_fileops();
    print_engine_status(env);
    test_shutdown();
    return 0;
}
