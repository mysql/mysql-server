/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#include "test.h"
#include <db.h>
#include <sys/stat.h>

static DB_ENV *env;
static DB *db;
DB_TXN *txn;

const int num_insert = 25000;

static void
setup (void) {
    int r;
    r=toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO);
    if (r != 0) {
        CKERR2(errno, EEXIST);
    }

    r=db_env_create(&env, 0); CKERR(r);
#ifdef TOKUDB
    r=env->set_redzone(env, 0); CKERR(r);
    r=env->set_default_bt_compare(env, int_dbt_cmp); CKERR(r);
#endif
    env->set_errfile(env, stderr);
#ifdef USE_BDB
    r=env->set_lk_max_objects(env, 2*num_insert); CKERR(r);
#endif
    
    r=env->open(env, TOKU_TEST_FILENAME, DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_MPOOL|DB_INIT_TXN|DB_CREATE|DB_PRIVATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
    r=db_create(&db, env, 0); CKERR(r);

    r=env->txn_begin(env, 0, &txn, 0); assert(r==0);
#ifdef USE_BDB
    r=db->set_bt_compare(db, int_dbt_cmp); CKERR(r);
#endif
    r=db->open(db, txn, "foo.db", 0, DB_BTREE, DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
    r=txn->commit(txn, 0);    assert(r==0);
}

static void
test_shutdown (void) {
    int r;
    r= db->close(db, 0); CKERR(r);
    r= env->close(env, 0); CKERR(r);
}

static void
doit(void) {
    int r;

    DBC *dbc;
    r=env->txn_begin(env, 0, &txn, 0); CKERR(r);
    r = env->get_cursor_for_persistent_environment(env, txn, &dbc); CKERR(r);
    DBT key;
    DBT val;
    dbt_init_realloc(&key);
    dbt_init_realloc(&val);

    while ((r = dbc->c_get(dbc, &key, &val, DB_NEXT)) == 0) {
        if (verbose) {
            printf("ENTRY\n\tKEY [%.*s]",
                    key.size,
                    (char*)key.data);
            if (val.size == sizeof(uint32_t)) {
                //assume integer
                printf("\n\tVAL [%" PRIu32"]\n",
                        toku_dtoh32(*(uint32_t*)val.data));
            } else if (val.size == sizeof(uint64_t)) {
                //assume 64 bit integer
                printf("\n\tVAL [%" PRIu64"]\n",
                        toku_dtoh64(*(uint64_t*)val.data));
            } else {
                printf("\n\tVAL [%.*s]\n",
                        val.size,
                        (char*)val.data);
            }
        }
    }
    CKERR2(r, DB_NOTFOUND);
    r = dbc->c_close(dbc);
    CKERR(r);
    r = txn->commit(txn, 0);
    CKERR(r);

    toku_free(key.data);
    toku_free(val.data);
}

int
test_main (int argc, char * const argv[]) {
    parse_args(argc, argv);

    setup();
    doit();
    test_shutdown();

    return 0;
}

