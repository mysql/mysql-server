/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."
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
    r = cursor->c_pre_acquire_range_lock(
        cursor, 
        db->dbt_neg_infty(), 
        db->dbt_pos_infty()
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
