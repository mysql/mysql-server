/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id: test_cursor_2.cc 45903 2012-07-19 13:06:39Z leifwalsh $"
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."
#include "test.h"

#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>
#include <memory.h>
#include <errno.h>
#include <sys/stat.h>
#include <db.h>


//
// This test ensures that we can do many updates to a single key when the dictionary
// is just that key.
//
static void
run_test (void) {

    DB_ENV * env;
    DB *db;
    const char * const fname = "test.updates_single_key.ft_handle";
    int r;

    r = db_env_create(&env, 0);        assert(r == 0);
    env->set_errfile(env, stderr);
    // no need to run with logging, so DB_INIT_LOG not passed in
    r = env->open(env, ENVDIR, DB_CREATE | DB_PRIVATE | DB_INIT_MPOOL | DB_INIT_TXN | DB_INIT_LOCK | DB_THREAD, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
    r = db_create(&db, env, 0); assert(r == 0);
    db->set_errfile(db,stderr); // Turn off those annoying errors
    r = db->open(db, NULL, fname, "main", DB_BTREE, DB_CREATE, 0666); assert(r == 0);

    int i;
    for (i=0; i<1000000; i++) {
        int k = 1;
        int v = i;
        DBT key, val;
        DB_TXN* txn = NULL;
        r = env->txn_begin(env, NULL, &txn, 0);
        CKERR(r);
        // want this test to go as fast as possible, so no need to use the lock tree
        // we just care to see that #5700 is behaving better, that some garbage collection is happening
        r = db->put(db, txn, dbt_init(&key, &k, sizeof k), dbt_init(&val, &v, sizeof v), DB_PRELOCKED_WRITE);
        txn->commit(txn, DB_TXN_NOSYNC);
        CKERR(r);
    }

    r = db->close(db, 0); assert(r == 0);
    r = env->close(env, 0); assert(r == 0);
}

int
test_main(int argc, char *const argv[]) {

    parse_args(argc, argv);
  
    int r;
    r = system("rm -rf " ENVDIR);
    CKERR(r);
    toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);
    
    run_test();

    return 0;
}
