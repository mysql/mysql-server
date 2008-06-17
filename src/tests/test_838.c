/* -*- mode: C; c-basic-offset: 4 -*- */
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <db.h>
#include "test.h"

// the exit value of this program is nonzero when the test fails
int testresult = 0;
int numexperiments = 20;

// maxt is set to the longest cursor next without transactions
// we then compare this time to the time with transactions and try to be within a factor of 10
unsigned long long maxt;

DBT *dbt_init_static(DBT *dbt) {
    memset(dbt, 0, sizeof *dbt);
    return dbt;
}

void test_838(int n) {
    if (verbose) printf("%s:%d\n", __FUNCTION__, n);
    int r;

    // setup test directory
    system("rm -rf " ENVDIR);
    mkdir(ENVDIR, 0777);

    // setup environment
    DB_ENV *env;
    {
        r = db_env_create(&env, 0); assert(r == 0);
        r = env->set_data_dir(env, ENVDIR);
        r = env->set_lg_dir(env, ENVDIR);
        env->set_errfile(env, stdout);
        r = env->open(env, 0, DB_INIT_MPOOL + DB_PRIVATE + DB_CREATE, 0777); 
        assert(r == 0);
    }

    // setup database
    DB *db;
    {
        DB_TXN *txn = 0;
        r = db_create(&db, env, 0); assert(r == 0);
        r = db->open(db, txn, "test.db", 0, DB_BTREE, DB_CREATE, 0777); assert(r == 0);
    }

    // insert, commit
    {    
        DB_TXN *txn = 0;
        int i;
        for (i=0; i<n; i++) {
            int k = htonl(i);
            int v = 0;
            DBT key, val;
            r = db->put(db, txn, dbt_init(&key, &k, sizeof k), dbt_init(&val, &v, sizeof v), 0);
            assert(r == 0);
        }
    }

    // delete, commit
    {    
        DB_TXN *txn = 0;
        int i;
        for (i=0; i<n; i++) {
            int k = htonl(i);
            DBT key;
            r = db->del(db, txn, dbt_init(&key, &k, sizeof k), 0);
            assert(r == 0);
        }
    }

    // walk
    maxt = 0;
    {
        DB_TXN *txn = 0;
        DBC *cursor;
        r = db->cursor(db, txn, &cursor, 0); assert(r == 0);
        int i;
        for (i=0; i<numexperiments; i++) {
            struct timeval tstart, tnow;
            gettimeofday(&tstart, 0);
            DBT key, val;
            r = cursor->c_get(cursor, dbt_init_malloc(&key), dbt_init_malloc(&val), DB_FIRST);
            assert(r == DB_NOTFOUND);
            gettimeofday(&tnow, 0);
            unsigned long long t = tnow.tv_sec * 1000000ULL + tnow.tv_usec;
            t -= tstart.tv_sec * 1000000ULL + tstart.tv_usec;
            if (verbose) printf("%d %llu\n", i, t);
            if (t > maxt) maxt = t;
        }
        r = cursor->c_close(cursor); assert(r == 0);
    }

    // close db
    r = db->close(db, 0); assert(r == 0);

    // reopen and walk
    {
        DB_TXN *txn = 0;
        r = db_create(&db, env, 0); assert(r == 0);
        r = db->open(db, txn, "test.db", 0, DB_BTREE, DB_CREATE, 0777); assert(r == 0);
    }
    {
        DB_TXN *txn = 0;
        DBC *cursor;
        r = db->cursor(db, txn, &cursor, 0); assert(r == 0);
        int i;
        for (i=0; i<numexperiments; i++) {
            struct timeval tstart, tnow;
            gettimeofday(&tstart, 0);
            DBT key, val;
            r = cursor->c_get(cursor, dbt_init_malloc(&key), dbt_init_malloc(&val), DB_FIRST);
            assert(r == DB_NOTFOUND);
            gettimeofday(&tnow, 0);
            unsigned long long t = tnow.tv_sec * 1000000ULL + tnow.tv_usec;
            t -= tstart.tv_sec * 1000000ULL + tstart.tv_usec;
            if (verbose) printf("%d %llu\n", i, t);
            if (t > maxt) maxt = t;
        }
        r = cursor->c_close(cursor); assert(r == 0);

        // close db
        r = db->close(db, 0); assert(r == 0);
    }

    // close env
    r = env->close(env, 0); assert(r == 0);
}

void test_838_txn(int n) {
    if (verbose) printf("%s:%d\n", __FUNCTION__, n);    
    int r;

    // setup test directory
    system("rm -rf " ENVDIR);
    mkdir(ENVDIR, 0777);

    // setup environment
    DB_ENV *env;
    {
        r = db_env_create(&env, 0); assert(r == 0);
        r = env->set_data_dir(env, ENVDIR);
        r = env->set_lg_dir(env, ENVDIR);
        env->set_errfile(env, stdout);
        r = env->open(env, 0, DB_INIT_MPOOL + DB_INIT_LOG + DB_INIT_LOCK + DB_INIT_TXN + DB_PRIVATE + DB_CREATE, 0777); 
        assert(r == 0);
    }

    // setup database
    DB *db;
    {
        DB_TXN *txn = 0;
        r = env->txn_begin(env, 0, &txn, 0); assert(r == 0);

        r = db_create(&db, env, 0); assert(r == 0);
        r = db->open(db, txn, "test.db", 0, DB_BTREE, DB_CREATE, 0777); assert(r == 0);

        r = txn->commit(txn, 0); assert(r == 0);
    }

    // insert, commit
    {    
        DB_TXN *txn_master;
        r = env->txn_begin(env, 0, &txn_master, 0); assert(r == 0);
        DB_TXN *txn;
        r = env->txn_begin(env, txn_master, &txn, 0); assert(r == 0);
        int i;
        for (i=0; i<n; i++) {
            int k = htonl(i);
            int v = 0;
            DBT key, val;
            r = db->put(db, txn, dbt_init(&key, &k, sizeof k), dbt_init(&val, &v, sizeof v), 0);
            assert(r == 0);
        }
        r = txn->commit(txn, 0); assert(r == 0);
        r = txn_master->commit(txn_master, 0); assert(r == 0);
    }

    // delete, commit
    {    
        DB_TXN *txn_master;
        r = env->txn_begin(env, 0, &txn_master, 0); assert(r == 0);
        DB_TXN *txn;
        r = env->txn_begin(env, txn_master, &txn, 0); assert(r == 0);
        int i;
        for (i=0; i<n; i++) {
            int k = htonl(i);
            DBT key;
            r = db->del(db, txn, dbt_init(&key, &k, sizeof k), 0);
            assert(r == 0);
        }
        r = txn->commit(txn, 0); assert(r == 0);
        r = txn_master->commit(txn_master, 0); assert(r == 0);
    }

    // walk
    {
        DB_TXN *txn;
        r = env->txn_begin(env, 0, &txn, 0); assert(r == 0);
        DBC *cursor;
        r = db->cursor(db, txn, &cursor, 0); assert(r == 0);
        int i;
        for (i=0; i<numexperiments; i++) {
            struct timeval tstart, tnow;
            gettimeofday(&tstart, 0);
            DBT key, val;
            r = cursor->c_get(cursor, dbt_init_malloc(&key), dbt_init_malloc(&val), DB_FIRST);
            assert(r == DB_NOTFOUND);
            gettimeofday(&tnow, 0);
            unsigned long long t = tnow.tv_sec * 1000000ULL + tnow.tv_usec;
            t -= tstart.tv_sec * 1000000ULL + tstart.tv_usec;
            if (verbose) printf("%d %llu %llu\n", i, t, maxt);
            
            // the first cursor op takes a long time as it needs to clean out the provisionally
            // deleted messages
            if (i > 0 && t > 10*maxt)
                testresult = 1;
        }
        r = cursor->c_close(cursor); assert(r == 0);
        r = txn->commit(txn, 0); assert(r == 0);
    }

    // close db
    r = db->close(db, 0); assert(r == 0);

    // reopen and walk
    {
        DB_TXN *txn = 0;
        r = env->txn_begin(env, 0, &txn, 0); assert(r == 0);

        r = db_create(&db, env, 0); assert(r == 0);
        r = db->open(db, txn, "test.db", 0, DB_BTREE, DB_CREATE, 0777); assert(r == 0);

        r = txn->commit(txn, 0); assert(r == 0);
    }
    {
        DB_TXN *txn;
        r = env->txn_begin(env, 0, &txn, 0); assert(r == 0);
        DBC *cursor;
        r = db->cursor(db, txn, &cursor, 0); assert(r == 0);
        int i;
        for (i=0; i<numexperiments; i++) {
            struct timeval tstart, tnow;
            gettimeofday(&tstart, 0);
            DBT key, val;
            r = cursor->c_get(cursor, dbt_init_malloc(&key), dbt_init_malloc(&val), DB_FIRST);
            assert(r == DB_NOTFOUND);
            gettimeofday(&tnow, 0);
            unsigned long long t = tnow.tv_sec * 1000000ULL + tnow.tv_usec;
            t -= tstart.tv_sec * 1000000ULL + tstart.tv_usec;
            if (verbose) printf("%d %llu %llu\n", i, t, maxt);
            if (i > 0 && t > 10*maxt)
                testresult = 1;
        }
        r = cursor->c_close(cursor); assert(r == 0);
        r = txn->commit(txn, 0); assert(r == 0);
        
        // close db
        r = db->close(db, 0); assert(r == 0);
    }

    // close env
    r = env->close(env, 0); assert(r == 0);
}

void test_838_defer_delete_commit(int n) {
    if (verbose) printf("%s:%d\n", __FUNCTION__, n);    
    int r;

    // setup test directory
    system("rm -rf " ENVDIR);
    mkdir(ENVDIR, 0777);

    // setup environment
    DB_ENV *env;
    {
        r = db_env_create(&env, 0); assert(r == 0);
        r = env->set_data_dir(env, ENVDIR);
        r = env->set_lg_dir(env, ENVDIR);
        env->set_errfile(env, stdout);
        r = env->open(env, 0, DB_INIT_MPOOL + DB_INIT_LOG + DB_INIT_LOCK + DB_INIT_TXN + DB_PRIVATE + DB_CREATE, 0777); 
        assert(r == 0);
    }

    // setup database
    DB *db;
    {
        DB_TXN *txn = 0;
        r = env->txn_begin(env, 0, &txn, 0); assert(r == 0);

        r = db_create(&db, env, 0); assert(r == 0);
        r = db->open(db, txn, "test.db", 0, DB_BTREE, DB_CREATE, 0777); assert(r == 0);

        r = txn->commit(txn, 0); assert(r == 0);
    }

    // insert, commit
    {    
        DB_TXN *txn_master;
        r = env->txn_begin(env, 0, &txn_master, 0); assert(r == 0);
        DB_TXN *txn;
        r = env->txn_begin(env, txn_master, &txn, 0); assert(r == 0);
        int i;
        for (i=0; i<n; i++) {
            int k = htonl(i);
            int v = 0;
            DBT key, val;
            r = db->put(db, txn, dbt_init(&key, &k, sizeof k), dbt_init(&val, &v, sizeof v), 0);
            assert(r == 0);
        }
        r = txn->commit(txn, 0); assert(r == 0);
        r = txn_master->commit(txn_master, 0); assert(r == 0);
    }

    // delete
        DB_TXN *txn_master_delete;
        r = env->txn_begin(env, 0, &txn_master_delete, 0); assert(r == 0);
        DB_TXN *txn_delete;
        r = env->txn_begin(env, txn_master_delete, &txn_delete, 0); assert(r == 0);
        int i;
        for (i=0; i<n; i++) {
            int k = htonl(i);
            DBT key;
            r = db->del(db, txn_delete, dbt_init(&key, &k, sizeof k), 0);
            assert(r == 0);
        }

    
    int expectr = 0;

    // walk
    {
        DB_TXN *txn;
        r = env->txn_begin(env, 0, &txn, 0); assert(r == 0);
        DBC *cursor;
        r = db->cursor(db, txn, &cursor, 0); assert(r == 0);
        int i;
        for (i=0; i<numexperiments; i++) {
            struct timeval tstart, tnow;
            gettimeofday(&tstart, 0);
            DBT key, val;
            r = cursor->c_get(cursor, dbt_init_malloc(&key), dbt_init_malloc(&val), DB_FIRST);
#if USE_TDB
            assert(r == DB_LOCK_NOTGRANTED);
#elif USE_BDB
            assert(r == DB_RUNRECOVERY);
            expectr = r;
#else
#error
#endif
            gettimeofday(&tnow, 0);
            unsigned long long t = tnow.tv_sec * 1000000ULL + tnow.tv_usec;
            t -= tstart.tv_sec * 1000000ULL + tstart.tv_usec;
            if (verbose) printf("%d %llu %llu\n", i, t, maxt);
            
            // the first cursor op takes a long time as it needs to clean out the provisionally
            // deleted messages
            if (i > 0 && t > 10*maxt)
                testresult = 1;
        }
        r = cursor->c_close(cursor);
#if USE_BDB
        if (r != expectr) printf("%s:%d:WARNING r=%d expectr=%d\n", __FILE__, __LINE__, r, expectr);
#else
        assert(r == expectr);
#endif
        r = txn->commit(txn, 0); assert(r == expectr);
    }

    // delete commit
    r = txn_delete->commit(txn_delete, 0); assert(r == expectr);
    r = txn_master_delete->commit(txn_master_delete, 0); assert(r == expectr);

    // close db
    r = db->close(db, 0); assert(r == expectr);

    // reopen and walk
    if (expectr == 0) {
        DB_TXN *txn = 0;
        r = env->txn_begin(env, 0, &txn, 0); assert(r == 0);

        r = db_create(&db, env, 0); assert(r == 0);
        r = db->open(db, txn, "test.db", 0, DB_BTREE, DB_CREATE, 0777); assert(r == 0);

        r = txn->commit(txn, 0); assert(r == 0);
    }
    if (expectr == 0) {
        DB_TXN *txn;
        r = env->txn_begin(env, 0, &txn, 0); assert(r == 0);
        DBC *cursor;
        r = db->cursor(db, txn, &cursor, 0); assert(r == 0);
        int i;
        for (i=0; i<numexperiments; i++) {
            struct timeval tstart, tnow;
            gettimeofday(&tstart, 0);
            DBT key, val;
            r = cursor->c_get(cursor, dbt_init_malloc(&key), dbt_init_malloc(&val), DB_FIRST);
            assert(r == DB_NOTFOUND);
            gettimeofday(&tnow, 0);
            unsigned long long t = tnow.tv_sec * 1000000ULL + tnow.tv_usec;
            t -= tstart.tv_sec * 1000000ULL + tstart.tv_usec;
            if (verbose) printf("%d %llu %llu\n", i, t, maxt);
            if (i > 0 && t > 10*maxt)
                testresult = 1;
        }
        r = cursor->c_close(cursor); assert(r == 0);
        r = txn->commit(txn, 0); assert(r == 0);

        // close db
        r = db->close(db, 0); assert(r == 0);
    }

    // close env
    r = env->close(env, 0); assert(r == expectr);
}

int main(int argc, const char *argv[]) {
    parse_args(argc, argv);

    int n;
    for (n=100000; n<=100000; n *= 10) {
        test_838(n);
        test_838_txn(n);
        test_838_defer_delete_commit(n);
    }
    return testresult;
}
