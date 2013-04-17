/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."
#include "test.h"
// The helgrind2 test performs a DB->get() in two different concurrent threads.
#include <arpa/inet.h>

#include <db.h>
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>
#include <memory.h>

DB_ENV *env;
DB *db;

static void initialize (void) {
    int r;
    r = system("rm -rf " ENVDIR);
    CKERR(r);
    toku_os_mkdir(ENVDIR, 0777);

    // setup environment
    {
        r = db_env_create(&env, 0); assert(r == 0);
        env->set_errfile(env, stdout);
        r = env->open(env, ENVDIR, DB_INIT_MPOOL + DB_PRIVATE + DB_CREATE, 0777); 
        assert(r == 0);
    }

    // setup DB
    {
        DB_TXN *txn = 0;
        r = db_create(&db, env, 0); assert(r == 0);
        r = db->open(db, txn, "test.db", 0, DB_BTREE, DB_CREATE, 0777); assert(r == 0);
    }

    // Put some stuff in
    {    
        char v[10];
        DB_TXN *txn = 0;
        int i;
	const int n = 10;
	memset(v, 0, sizeof(v));
        for (i=0; i<n; i++) {
            int k = htonl(i);
            DBT key, val;
            r = db->put(db, txn, dbt_init(&key, &k, sizeof k), dbt_init(&val, v, sizeof(v)), 0);
            assert(r == 0);
        }
    }
}

static void finish (void) {
    int r;
    r = db->close(db, 0); assert(r==0);
    r = env->close(env, 0); assert(r==0);
}

static void *starta(void* ignore __attribute__((__unused__))) {
    DB_TXN *txn = 0;
    DBT key, val;
    char data[10];
    val.data = data;
    memset(&key, 0, sizeof(key));
    memset(&val, 0, sizeof(val));
    val.flags |= DB_DBT_MALLOC;
    int k = htonl(99);
    int r = db->put(db, txn, dbt_init(&key, &k, sizeof k), &val, 0);
    assert(r==0);
    //printf("val.data=%p\n", val.data);
    return 0;
}
static void *startb(void* ignore __attribute__((__unused__))) {
    DB_TXN *txn = 0;
    DBT key, val;
    memset(&key, 0, sizeof(key));
    memset(&val, 0, sizeof(val));
    int k = htonl(0);
    val.flags |= DB_DBT_MALLOC;
    int r = db->get(db, txn, dbt_init(&key, &k, sizeof k), &val, 0);
    assert(r==0);
    //printf("val.data=%p\n", val.data);
    int i; for (i=0; i<10; i++) assert(((char*)val.data)[i]==0);
    toku_free(val.data);
    return 0;
}

int
test_main (int argc, char * const argv[]) {
    parse_args(argc, argv);
    pthread_t a,b;
    initialize();
    { int x = pthread_create(&a, NULL, starta, NULL); assert(x==0); }
    { int x = pthread_create(&b, NULL, startb, NULL); assert(x==0); }
    { int x = pthread_join(a, NULL);           assert(x==0); }
    { int x = pthread_join(b, NULL);           assert(x==0); }
    finish();
    return 0;
}
