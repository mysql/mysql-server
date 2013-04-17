/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2011-2013 Tokutek Inc.  All rights reserved."

#include "test.h"

static DB_ENV *env = NULL;
static DB *db = NULL;
static const char *envdir = TOKU_TEST_FILENAME;

static void setup_env (void) {
    const int len = strlen(envdir)+100;
    char cmd[len];
    snprintf(cmd, len, "rm -rf %s", envdir);
    {int r = system(cmd);                                                                                                               CKERR(r); }
    {int r = toku_os_mkdir(envdir, S_IRWXU+S_IRWXG+S_IRWXO);                                                                            CKERR(r); }
    {int r = db_env_create(&env, 0);                                                                                                    CKERR(r); }
    //env->set_errfile(env, stderr);
#ifdef TOKUDB
    CKERR(env->set_redzone(env, 0));
#endif
    { int r = env->open(env, envdir, DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_MPOOL|DB_INIT_TXN|DB_CREATE|DB_PRIVATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r); }
    { int r = db_create(&db, env, 0);                                                                                                   CKERR(r); }
    { int r = db->open(db, NULL, "foo.db", 0, DB_BTREE, DB_CREATE | DB_AUTO_COMMIT, S_IRWXU+S_IRWXG+S_IRWXO);                           CKERR(r); }
}

static void shutdown_env (void) {
    { int r = db->close(db, 0);   CKERR(r); }
    { int r = env->close(env, 0); CKERR(r); }
}

static void put (const char *keystring, int size, bool should_work) {
    DBT k, v;
    dbt_init(&k, keystring, 1+strlen(keystring));
    dbt_init(&v, toku_xcalloc(size, 1), size);
#ifdef USE_BDB
#define DB_YES_OVERWRITE 0
#endif
    static DB_TXN *txn = NULL;
    { int r = env->txn_begin(env, 0, &txn, 0); CKERR(r); }
    {
	int r = db->put(db, NULL, &k, &v, 0);
	if (!IS_TDB || should_work) {
	    CKERR(r);
	} else {
	    assert(r!=0);
	}
    }
    { int r = txn->commit(txn, 0); CKERR(r); }
    toku_free(v.data);
}

int test_main (int argc, char *const argv[]) {
    if (0) parse_args(argc, argv);
    setup_env();
    if (0) put("foo", 32, true);
    put("foo", 32*1024*1024, true);
    put("bar", 32*1024*1024+1, false);
    shutdown_env();
    
    return 0;
}
