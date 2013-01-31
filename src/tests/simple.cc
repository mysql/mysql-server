/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

#ident "Copyright (c) 2009 Tokutek Inc.  All rights reserved."
#ident "$Id$"

/* Purpose of this test is to verify that a failed assert will
 * cause a panic, which should be visible via engine status.
 * This is a manual test, should not be checked in to repository.
 * The panic must be manually induced in the debugger.
 */


#include "test.h"
#include <db.h>

static DB_ENV *env;

#define FLAGS_NOLOG DB_INIT_LOCK|DB_INIT_MPOOL|DB_CREATE|DB_PRIVATE
#define FLAGS_LOG   FLAGS_NOLOG|DB_INIT_TXN|DB_INIT_LOG

static int mode = S_IRWXU+S_IRWXG+S_IRWXO;

static void test_shutdown(void);

static void
test_shutdown(void) {
    int r;
    r=env->close(env, 0); CKERR(r);
    env = NULL;
}

static void
setup (uint32_t flags) {
    int r;
    if (env)
        test_shutdown();
    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    r=toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO);
    CKERR(r);
    r=db_env_create(&env, 0); 
    CKERR(r);
    env->set_errfile(env, stderr);
    r=env->open(env, TOKU_TEST_FILENAME, flags, mode); 
    CKERR(r);
}


int
test_main (int argc, char * const argv[]) {
    parse_args(argc, argv);
    setup(FLAGS_LOG);
    env->txn_checkpoint(env, 0, 0, 0);
#ifdef USE_TDB
    print_engine_status(env);
#endif
    test_shutdown();
    return 0;
}
