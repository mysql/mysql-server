/* -*- mode: C; c-basic-offset: 4 -*- */
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <sys/resource.h>
#include <db.h>
#include "test.h"

// the exit value of this program is nonzero when the test fails
int testresult = 0;
int numexperiments = 40;

DBT *dbt_init_static(DBT *dbt) {
    memset(dbt, 0, sizeof *dbt);
    return dbt;
}

long long get_vtime() {
#if 0
    // this is useless as the user time only counts milliseconds
    struct rusage rusage;
    int r = getrusage(RUSAGE_SELF, &rusage);
    assert(r == 0);
    return rusage.ru_utime.tv_sec * 1000000LL + rusage.ru_utime.tv_usec;
#else
    // this may be affected by other processes
    struct timeval tv;
    int r = gettimeofday(&tv, 0);
    assert(r == 0);
    return tv.tv_sec * 1000000LL + tv.tv_usec;
#endif
}

void print_times(long long times[], int n) {
    int i;
    for (i=0; i<n; i++)
        printf("%lld ", times[i]);
    printf("\n");
}

void do_times(long long times[], int n) {
    long long xtimes[n];
    int i;
    long long s = 0;
    for (i=0; i<n; i++) {
        xtimes[i] = times[i];
        s += times[i];
        if (verbose) printf("%llu ", times[i]);
    }
    int cmp(const void *a, const void *b) {
        return *(long long *)a - *(long long *)b;
    }
    qsort(xtimes, n, sizeof (long long), cmp);
    printf(": medium %llu mean %llu\n", xtimes[n/2], s/n);

    // verify that the times are within a factor of 10 of the medium time
    // skip the first startup time
    for (i=1; i<n; i++) {
        long long t = times[i] - xtimes[n/2];
        if (t < 0) t = -t;
        if (t > 10*xtimes[n/2]) {
            printf("%s:%d:warning %llu %llu\n", __FILE__, __LINE__, t, xtimes[n/2]);
            if (!verbose)
                print_times(times, n);
            testresult = 1;
        }
    }

    printf("\n");
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
    {
        DB_TXN *txn = 0;
        DBC *cursor;
        r = db->cursor(db, txn, &cursor, 0); assert(r == 0);
        long long t[numexperiments];
        int i;
        for (i=0; i<numexperiments; i++) {
            long long tstart = get_vtime();
            DBT key, val;
            r = cursor->c_get(cursor, dbt_init_malloc(&key), dbt_init_malloc(&val), DB_FIRST);
            assert(r == DB_NOTFOUND);
            long long tnow = get_vtime();
            t[i] = tnow - tstart;
        }
        do_times(t, numexperiments);
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
        long long t[numexperiments];
        int i;
        for (i=0; i<numexperiments; i++) {
            long long tstart = get_vtime();
            DBT key, val;
            r = cursor->c_get(cursor, dbt_init_malloc(&key), dbt_init_malloc(&val), DB_FIRST);
            assert(r == DB_NOTFOUND);
            long long tnow = get_vtime();
            t[i] = tnow - tstart;
        }
        do_times(t, numexperiments);
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
        long long t[numexperiments];
        int i;
        for (i=0; i<numexperiments; i++) {
            long long tstart = get_vtime();
            DBT key, val;
            r = cursor->c_get(cursor, dbt_init_malloc(&key), dbt_init_malloc(&val), DB_FIRST);
            assert(r == DB_NOTFOUND);
            long long tnow = get_vtime();
            t[i] = tnow - tstart;
        }
        do_times(t, numexperiments);
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
        long long t[numexperiments];
        int i;
        for (i=0; i<numexperiments; i++) {
            long long tstart = get_vtime();
            DBT key, val;
            r = cursor->c_get(cursor, dbt_init_malloc(&key), dbt_init_malloc(&val), DB_FIRST);
            assert(r == DB_NOTFOUND);
            long long tnow = get_vtime();
            t[i] = tnow - tstart;
        }
        do_times(t, numexperiments);
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
        long long t[numexperiments];
        int i;
        for (i=0; i<numexperiments; i++) {
            long long tstart = get_vtime();
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
            long long tnow = get_vtime();
            t[i] = tnow - tstart;
        }
        do_times(t, numexperiments);
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
        long long t[numexperiments];
        int i;
        for (i=0; i<numexperiments; i++) {
            long long tstart = get_vtime();
            DBT key, val;
            r = cursor->c_get(cursor, dbt_init_malloc(&key), dbt_init_malloc(&val), DB_FIRST);
            assert(r == DB_NOTFOUND);
            long long tnow = get_vtime();
            t[i] = tnow - tstart;
        }
        do_times(t, numexperiments);
        r = cursor->c_close(cursor); assert(r == 0);
        r = txn->commit(txn, 0); assert(r == 0);

        // close db
        r = db->close(db, 0); assert(r == 0);
    }

    // close env
    r = env->close(env, 0); assert(r == expectr);
}

int main(int argc, const char *argv[]) {
    int i;
    for (i=1; i<argc; i++) {
        const char *arg = argv[i];
        if (strcmp(arg, "-v") == 0) {
            verbose++;
        }
        if (strcmp(arg, "-numexperiments") == 0) {
            if (i+1 >= argc)
                return 1;
            numexperiments = atoi(argv[++i]);
        }
    }

    int n;
    for (n=100000; n<=100000; n *= 10) {
        test_838(n);
        test_838_txn(n);
        test_838_defer_delete_commit(n);
    }
    return testresult;
}
