/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."
#include "test.h"
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <memory.h>
#include <sys/stat.h>
#include <db.h>
#include <memory.h>

static void
test_789(void) {
    int r;

    /* setup test directory */
    system("rm -rf " ENVDIR);
    toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);

    /* setup environment */
    DB_ENV *env;
    {
        r = db_env_create(&env, 0); assert(r == 0);
        env->set_errfile(env, stdout);
        r = env->open(env, ENVDIR, DB_INIT_MPOOL + DB_INIT_LOG + DB_INIT_LOCK + DB_INIT_TXN + DB_PRIVATE + DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); 
        assert(r == 0);
    }

    /* setup database */
    DB *db;
    {
        DB_TXN *txn = 0;
        r = env->txn_begin(env, 0, &txn, 0); assert(r == 0);

        r = db_create(&db, env, 0); assert(r == 0);
        r = db->open(db, txn, "test.db", 0, DB_BTREE, DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); assert(r == 0);

        r = txn->commit(txn, 0); assert(r == 0);
    }

    /* insert, commit */
    {    
        DB_TXN *txn_master;
        r = env->txn_begin(env, 0, &txn_master, 0); assert(r == 0);
        DB_TXN *txn;
        r = env->txn_begin(env, txn_master, &txn, 0); assert(r == 0);
        int i;
        for (i=0; i<3; i++) {
            int k = htonl(i);
            int v = 0;
            DBT key, val;
            r = db->put(db, txn, dbt_init(&key, &k, sizeof k), dbt_init(&val, &v, sizeof v), 0);
            assert(r == 0);
        }
        r = txn->commit(txn, 0); assert(r == 0);
        r = txn_master->commit(txn_master, 0); assert(r == 0);
    }

    /* update, rollback */
    {    
        DB_TXN *txn_master;
        r = env->txn_begin(env, 0, &txn_master, 0); assert(r == 0);
        DB_TXN *txn;
        r = env->txn_begin(env, txn_master, &txn, 0); assert(r == 0);
        DBC *cursor;
        r = db->cursor(db, txn, &cursor, 0); assert(r == 0);
        DBT key, val;
        r = cursor->c_get(cursor, dbt_init_malloc(&key), dbt_init_malloc(&val), DB_NEXT); assert(r == 0);
        *(char*)val.data = 1;
        r = db->put(db, txn, &key, &val, 0); assert(r == 0);
        r = cursor->c_close(cursor); assert(r == 0);
        toku_free(key.data); toku_free(val.data);
        r = txn->commit(txn, 0); assert(r == 0);
        r = txn_master->abort(txn_master); assert(r == 0);
    }

    /* delete, rollback */
    {    
        DB_TXN *txn_master;
        r = env->txn_begin(env, 0, &txn_master, 0); assert(r == 0);
        DB_TXN *txn;
        r = env->txn_begin(env, txn_master, &txn, 0); assert(r == 0);
        DBC *cursor;
        r = db->cursor(db, txn, &cursor, 0); assert(r == 0);
        DBT key, val;
        r = cursor->c_get(cursor, dbt_init_malloc(&key), dbt_init_malloc(&val), DB_NEXT); assert(r == 0);
        r = cursor->c_del(cursor, 0); assert(r == 0);
        r = cursor->c_close(cursor); assert(r == 0);
        toku_free(key.data); toku_free(val.data);
        r = txn->commit(txn, 0); assert(r == 0);
        r = txn_master->abort(txn_master); assert(r == 0);
    }

    /* update, commit */
    {    
        DB_TXN *txn_master;
        r = env->txn_begin(env, 0, &txn_master, 0); assert(r == 0);
        DB_TXN *txn;
        r = env->txn_begin(env, txn_master, &txn, 0); assert(r == 0);
        DBC *cursor;
        r = db->cursor(db, txn, &cursor, 0); assert(r == 0);
        DBT key, val;
        r = cursor->c_get(cursor, dbt_init_malloc(&key), dbt_init_malloc(&val), DB_NEXT); assert(r == 0);
        *(char*)val.data = 2;
        r = db->put(db, txn, &key, &val, 0); assert(r == 0);
        r = cursor->c_close(cursor); assert(r == 0);
        toku_free(key.data); toku_free(val.data);
        r = txn->commit(txn, 0); assert(r == 0);
        r = txn_master->commit(txn_master, 0); assert(r == 0);
    }

    /* delete, commit */
    {    
        DB_TXN *txn_master;
        r = env->txn_begin(env, 0, &txn_master, 0); assert(r == 0);
        DB_TXN *txn;
        r = env->txn_begin(env, txn_master, &txn, 0); assert(r == 0);
        DBC *cursor;
        r = db->cursor(db, txn, &cursor, 0); assert(r == 0);
        DBT key, val;
        r = cursor->c_get(cursor, dbt_init_malloc(&key), dbt_init_malloc(&val), DB_NEXT); assert(r == 0);
        r = cursor->c_del(cursor, 0); assert(r == 0);
        r = cursor->c_close(cursor); assert(r == 0);
        toku_free(key.data); toku_free(val.data);
        r = txn->commit(txn, 0); assert(r == 0);
        r = txn_master->commit(txn_master, 0); assert(r == 0);
    }

    /* close db */
    r = db->close(db, 0); assert(r == 0);

    /* close env */
    r = env->close(env, 0); assert(r == 0);
}

int
test_main(int UU(argc), char UU(*argv[])) {
    test_789();
    return 0;
}
