/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <memory.h>
#include <sys/stat.h>
#include <db.h>


static void
db_put (DB *db, int k, int v) {
    DB_TXN * const null_txn = 0;
    DBT key, val;
    int r = db->put(db, null_txn, dbt_init(&key, &k, sizeof k), dbt_init(&val, &v, sizeof v), DB_YESOVERWRITE);
    assert(r == 0);
}

static void
expect (DBC *cursor, int k, int v) {
    DBT key, val;
    int r = cursor->c_get(cursor, dbt_init_malloc(&key), dbt_init_malloc(&val), DB_NEXT);
    assert(r == 0);
    assert(key.size == sizeof k);
    int kk;
    memcpy(&kk, key.data, key.size);
    assert(val.size == sizeof v);
    int vv;
    memcpy(&vv, val.data, val.size);
    if (kk != k || vv != v) printf("expect key %u got %u - %u %u\n", (uint32_t)htonl(k), (uint32_t)htonl(kk), (uint32_t)htonl(v), (uint32_t)htonl(vv));
    assert(kk == k);
    assert(vv == v);

    toku_free(key.data);
    toku_free(val.data);
}

/* verify dup keys delete */
static void
test_dup_delete (int n, int dup_mode) {
    if (verbose) printf("test_dup_delete:%d %d\n", n, dup_mode);

    DB_TXN * const null_txn = 0;
    const char * const fname = "test_dup_delete.brt";
    int r;

    r = system("rm -rf " ENVDIR); CKERR(r);
    r = toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);

    /* create the dup database file */
    DB_ENV *env;
    r = db_env_create(&env, 0); assert(r == 0);
    r = env->open(env, ENVDIR, DB_CREATE+DB_PRIVATE+DB_INIT_MPOOL, 0); assert(r == 0);

    DB *db;
    r = db_create(&db, env, 0); assert(r == 0);
    r = db->set_flags(db, dup_mode); assert(r == 0);
    r = db->set_pagesize(db, 4096); assert(r == 0);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, DB_CREATE, 0666); assert(r == 0);

    /* insert n-1 unique keys {0, 1,  n-1} - {n/2} */
    int i;
    for (i=0; i<n; i++) {
        if (i == n/2)
            continue;
        int k = htonl(i);
        int v = htonl(n+i);
        db_put(db, k, v);
    }

    /* reopen the database to force nonleaf buffering */
    r = db->close(db, 0);
    assert(r == 0);
    r = db_create(&db, env, 0);
    assert(r == 0);
    r = db->set_flags(db, dup_mode);
    assert(r == 0);
    r = db->set_pagesize(db, 4096);
    assert(r == 0);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, 0, 0666);
    assert(r == 0);

    /* insert n duplicates */
    for (i=0; i<n; i++) {
        int k = htonl(n/2);
        int v = htonl(n+i);
        db_put(db, k, v);

        DBT key, val;
        r = db->get(db, null_txn, dbt_init(&key, &k, sizeof k), dbt_init_malloc(&val), 0);
        assert(r == 0);
        unsigned int vv;
        assert(val.size == sizeof vv);
        memcpy(&vv, val.data, val.size);
        assert(vv == htonl(n));
        toku_free(val.data);
    } 

    {
	DBT key; int k = htonl(n/2);
	r = db->del(db, null_txn, dbt_init(&key, &k, sizeof k), 0);
	assert(r == 0);
    }

    /* verify lookup fails */
    {
        int k = htonl(n/2);
        DBT key, val;
        r = db->get(db, null_txn, dbt_init(&key, &k, sizeof k), dbt_init_malloc(&val), 0);
        assert(r == DB_NOTFOUND);
    }

    /* verify all dups are removed using a cursor */
    DBC *cursor;
    r = db->cursor(db, null_txn, &cursor, 0);
    assert(r == 0);

    for (i=0; i<n/2; i++) {
        expect(cursor, htonl(i), htonl(n+i));
    }

    for (i=(n/2)+1; i<n; i++) {
        expect(cursor, htonl(i), htonl(n+i));
    }

    r = cursor->c_close(cursor);
    assert(r == 0);

    r = db->close(db, 0); assert(r == 0);
    r = env->close(env, 0), assert(r == 0);
}

static __attribute__((__unused__))
void
test_dup_delete_delete (int n) {
    if (verbose) printf("test_dup_delete_delete:%d\n", n);

    DB_TXN * const null_txn = 0;
    const char * const fname = "test_dup_delete_delete.brt";
    int r;

    /* create the dup database file */
    DB_ENV *env;
    r = db_env_create(&env, 0); assert(r == 0);
    r = env->open(env, ENVDIR, DB_CREATE+DB_PRIVATE+DB_INIT_MPOOL, 0); assert(r == 0);

    DB *db;
    r = db_create(&db, env, 0);
    assert(r == 0);
    r = db->set_flags(db, DB_DUP);
    assert(r == 0);
    r = db->set_pagesize(db, 4096);
    assert(r == 0);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, DB_CREATE, 0666);
    assert(r == 0);

    /* insert n-1 unique keys {0, 1,  n-1} - {n/2} */
    int i;
    for (i=0; i<n; i++) {
        if (i == n/2)
            continue;
        int k = htonl(i);
        int v = i;
        db_put(db, k, v);
    }

    /* reopen the database to force nonleaf buffering */
    r = db->close(db, 0);
    assert(r == 0);
    r = db_create(&db, env, 0);
    assert(r == 0);
    r = db->set_flags(db, DB_DUP);
    assert(r == 0);
    r = db->set_pagesize(db, 4096);
    assert(r == 0);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, 0, 0666);
    assert(r == 0);

    /* insert n duplicates */
    for (i=0; i<n; i++) {
        int k = htonl(n/2);
        int v = i;
        db_put(db, k, v);
    } 

    /* delete the dup key */
    DBT key; int k = htonl(n/2);
    r = db->del(db, null_txn, dbt_init(&key, &k, sizeof k), 0);
    assert(r == 0);
  
    /* delete again */
    r = db->del(db, null_txn, dbt_init(&key, &k, sizeof k), 0);
    assert(r == 0);


    /* verify all dups are remove using a cursor */
    DBC *cursor;
    r = db->cursor(db, null_txn, &cursor, 0);
    assert(r == 0);

    for (i=0; i<n/2; i++) {
        expect(cursor, htonl(i), i);
    }

    for (i=(n/2)+1; i<n; i++) {
        expect(cursor, htonl(i), i);
    }

    r = cursor->c_close(cursor);
    assert(r == 0);

    r = db->close(db, 0);
    assert(r == 0);
    r = env->close(env, 0), assert(r == 0);
}

static void
test_dup_delete_insert (int n, int dup_mode) {
    if (verbose) printf("test_dup_delete_insert:%d %d\n", n, dup_mode);

    DB_TXN * const null_txn = 0;
    const char * const fname = "test_dup_delete_insert.brt";
    int r;

    /* create the dup database file */
    DB_ENV *env;
    r = db_env_create(&env, 0); assert(r == 0);
    r = env->open(env, ENVDIR, DB_CREATE+DB_PRIVATE+DB_INIT_MPOOL, 0); assert(r == 0);

    DB *db;
    r = db_create(&db, env, 0);
    assert(r == 0);
    r = db->set_flags(db, dup_mode);
    assert(r == 0);
    r = db->set_pagesize(db, 4096);
    assert(r == 0);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, DB_CREATE, 0666);
    assert(r == 0);

    /* insert n-1 unique keys {0, 1,  n-1} - {n/2} */
    int i;
    for (i=0; i<n; i++) {
        if (i == n/2)
            continue;
        int k = htonl(i);
        int v = i;
        db_put(db, k, v);
    }

    /* reopen the database to force nonleaf buffering */
    r = db->close(db, 0);
    assert(r == 0);
    r = db_create(&db, env, 0);
    assert(r == 0);
    r = db->set_flags(db, dup_mode);
    assert(r == 0);
    r = db->set_pagesize(db, 4096);
    assert(r == 0);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, 0, 0666);
    assert(r == 0);

    /* insert n duplicates */
    for (i=0; i<n; i++) {
        int k = htonl(n/2);
        int v = htonl(i);
        db_put(db, k, v);

        DBT key, val;
        r = db->get(db, null_txn, dbt_init(&key, &k, sizeof k), dbt_init_malloc(&val), 0);
        assert(r == 0);
        unsigned int vv;
        assert(val.size == sizeof vv);
        memcpy(&vv, val.data, val.size);
        assert(vv == htonl(0));
        toku_free(val.data);
    } 

    {
	int k = htonl(n/2);
	DBT key;
	r = db->del(db, null_txn, dbt_init(&key, &k, sizeof k), 0);
	assert(r == 0);
    }

    /* insert n duplicates */
    for (i=0; i<n; i++) {
        int k = htonl(n/2);
        int v = htonl(i);
        db_put(db, k, v);

        DBT key, val;
        r = db->get(db, null_txn, dbt_init(&key, &k, sizeof k), dbt_init_malloc(&val), 0);
        assert(r == 0);
        unsigned int vv;
        assert(val.size == sizeof vv);
        memcpy(&vv, val.data, val.size);
        assert(vv == htonl(0));
        toku_free(val.data);
    } 

    DBC *cursor;
    r = db->cursor(db, null_txn, &cursor, 0);
    assert(r == 0);

    for (i=0; i<n/2; i++) {
        expect(cursor, htonl(i), i);
    }

    for (i=0; i<n; i++) {
        expect(cursor, htonl(n/2), htonl(i));
    }

    for (i=(n/2)+1; i<n; i++) {
        expect(cursor, htonl(i), i);
    }

    r = cursor->c_close(cursor);
    assert(r == 0);

    r = db->close(db, 0);
    assert(r == 0);
    r = env->close(env, 0), assert(r == 0);
}

static __attribute__((__unused__))
void
test_all_dup_delete_insert (int n) {
    if (verbose) printf("test_all_dup_delete_insert:%d\n", n);

    DB_TXN * const null_txn = 0;
    const char * const fname = "test_all_dup_delete_insert.brt";
    int r;

    /* create the dup database file */
    DB_ENV *env;
    r = db_env_create(&env, 0); assert(r == 0);
    r = env->open(env, ENVDIR, DB_CREATE+DB_PRIVATE+DB_INIT_MPOOL, 0); assert(r == 0);

    DB *db;
    r = db_create(&db, env, 0);
    assert(r == 0);
    r = db->set_flags(db, DB_DUP);
    assert(r == 0);
    r = db->set_pagesize(db, 4096);
    assert(r == 0);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, DB_CREATE, 0666);
    assert(r == 0);

    int i;
    for (i=0; i<n; i++) {
        int k = htonl(n/2);
        int v = i;
        db_put(db, k, v);
    }

    /* reopen the database to force nonleaf buffering */
    r = db->close(db, 0);
    assert(r == 0);
    r = db_create(&db, env, 0);
    assert(r == 0);
    r = db->set_flags(db, DB_DUP);
    assert(r == 0);
    r = db->set_pagesize(db, 4096);
    assert(r == 0);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, 0, 0666);
    assert(r == 0);

    /* insert n duplicates */
    for (i=0; i<n; i++) {
        int k = htonl(n/2);
        int v = n+i;
        db_put(db, k, v);
    } 

    {
	DBT key; int k = htonl(n/2);
	r = db->del(db, null_txn, dbt_init(&key, &k, sizeof k), 0);
	assert(r == 0);
    }

    /* insert n duplicates */
    for (i=0; i<n; i++) {
        int k = htonl(n/2);
        int v = 2*n+i;
        db_put(db, k, v);
    } 

    DBC *cursor;
    r = db->cursor(db, null_txn, &cursor, 0);
    assert(r == 0);

    for (i=0; i<n; i++) {
        expect(cursor, htonl(n/2), 2*n+i);
    }

    r = cursor->c_close(cursor);
    assert(r == 0);

    r = db->close(db, 0);
    assert(r == 0);
    r = env->close(env, 0), assert(r == 0);
}

static void
test_walk_empty (int n, int dup_mode) {
    if (verbose) printf("test_walk_empty:%d %d\n", n, dup_mode);

    DB_TXN * const null_txn = 0;
    const char * const fname = "test_walk_empty.brt";
    int r;

    /* create the dup database file */
    DB_ENV *env;
    r = db_env_create(&env, 0); assert(r == 0);
    r = env->open(env, ENVDIR, DB_CREATE+DB_PRIVATE+DB_INIT_MPOOL, 0); assert(r == 0);

    DB *db;
    r = db_create(&db, env, 0);
    assert(r == 0);
    r = db->set_flags(db, dup_mode);
    assert(r == 0);
    r = db->set_pagesize(db, 4096);
    assert(r == 0);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, DB_CREATE, 0666);
    assert(r == 0);

    /* insert n duplicates */
    int i;
    for (i=0; i<n; i++) {
        int k = htonl(n/2);
        int v = htonl(i);
        db_put(db, k, v);
    } 

    /* reopen the database to force nonleaf buffering */
    r = db->close(db, 0);
    assert(r == 0);
    r = db_create(&db, env, 0);
    assert(r == 0);
    r = db->set_flags(db, dup_mode);
    assert(r == 0);
    r = db->set_pagesize(db, 4096);
    assert(r == 0);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, 0, 0666);
    assert(r == 0);

    /* insert n duplicates */
    for (i=0; i<n; i++) {
        int k = htonl(n/2);
        int v = htonl(n+i);
        db_put(db, k, v);
    } 

    {
	DBT key; int k = htonl(n/2);
	r = db->del(db, null_txn, dbt_init(&key, &k, sizeof k), 0);
	assert(r == 0);
    }

    DBC *cursor;
    r = db->cursor(db, null_txn, &cursor, 0);
    assert(r == 0);

    {
	DBT key, val;
	r = cursor->c_get(cursor, dbt_init_malloc(&key), dbt_init_malloc(&val), DB_NEXT);
	assert(r != 0);
    }

    r = cursor->c_close(cursor);
    assert(r == 0);

    r = db->close(db, 0);
    assert(r == 0);
    r = env->close(env, 0), assert(r == 0);
}

/* insert, close, delete, insert, search */
static __attribute__((__unused__))
void
test_icdi_search (int n, int dup_mode) {
    if (verbose) printf("test_icdi_search:%d %d\n", n, dup_mode);

    DB_TXN * const null_txn = 0;
    const char * const fname = "test_icdi_search.brt";
    int r;

    /* create the dup database file */
    DB_ENV *env;
    r = db_env_create(&env, 0); assert(r == 0);
    r = env->open(env, ENVDIR, DB_CREATE+DB_PRIVATE+DB_INIT_MPOOL, 0); assert(r == 0);

    DB *db;
    r = db_create(&db, env, 0);
    assert(r == 0);
    r = db->set_flags(db, dup_mode);
    assert(r == 0);
    r = db->set_pagesize(db, 4096);
    assert(r == 0);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, DB_CREATE, 0666);
    assert(r == 0);

    /* insert n duplicates */
    int i;
    for (i=0; i<n; i++) {
        int k = htonl(n/2);
        int v = htonl(i);
        db_put(db, k, v);

        DBT key, val;
        r = db->get(db, null_txn, dbt_init(&key, &k, sizeof k), dbt_init_malloc(&val), 0);
        assert(r == 0);
        unsigned int vv;
        assert(val.size == sizeof vv);
        memcpy(&vv, val.data, val.size);
        assert(vv == htonl(0));
        toku_free(val.data);
    } 

    /* reopen the database to force nonleaf buffering */
    r = db->close(db, 0);
    assert(r == 0);
    r = db_create(&db, env, 0);
    assert(r == 0);
    r = db->set_flags(db, dup_mode);
    assert(r == 0);
    r = db->set_pagesize(db, 4096);
    assert(r == 0);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, 0, 0666);
    assert(r == 0);

    {
	int k = htonl(n/2);
	DBT key;
	r = db->del(db, null_txn, dbt_init(&key, &k, sizeof k), 0);
	assert(r == 0);
    }

    /* insert n duplicates */
    for (i=0; i<n; i++) {
        int k = htonl(n/2);
        int v = htonl(n+i);
        db_put(db, k, v);
        
        DBT key, val;
        r = db->get(db, null_txn, dbt_init(&key, &k, sizeof k), dbt_init_malloc(&val), 0);
        assert(r == 0);
        unsigned int vv;
        assert(val.size == sizeof vv);
        memcpy(&vv, val.data, val.size);
        assert(vv == htonl(n));
        toku_free(val.data);
    } 

    DBC *cursor;
    r = db->cursor(db, null_txn, &cursor, 0);
    assert(r == 0);

    for (i=0; i<n; i++) {
        expect(cursor, htonl(n/2), htonl(n+i));
    }

    r = cursor->c_close(cursor);
    assert(r == 0);

    r = db->close(db, 0);
    assert(r == 0);
    r = env->close(env, 0), assert(r == 0);
}

/* insert, close, insert, search */
static __attribute__((__unused__))
void
test_ici_search (int n, int dup_mode) {
    if (verbose) printf("test_ici_search:%d %d\n", n, dup_mode);

    DB_TXN * const null_txn = 0;
    const char * const fname = "test_ici_search.brt";
    int r;

    /* create the dup database file */
    DB_ENV *env;
    r = db_env_create(&env, 0); assert(r == 0);
    r = env->open(env, ENVDIR, DB_CREATE+DB_PRIVATE+DB_INIT_MPOOL, 0); assert(r == 0);

    DB *db;
    r = db_create(&db, env, 0);
    assert(r == 0);
    r = db->set_flags(db, dup_mode);
    assert(r == 0);
    r = db->set_pagesize(db, 4096);
    assert(r == 0);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, DB_CREATE, 0666);
    assert(r == 0);

    /* insert n duplicates */
    int i;
    for (i=0; i<n; i++) {
        int k = htonl(n/2);
        int v = htonl(i);
        db_put(db, k, v);

        DBT key, val;
        r = db->get(db, null_txn, dbt_init(&key, &k, sizeof k), dbt_init_malloc(&val), 0);
        assert(r == 0);
        unsigned int vv;
        assert(val.size == sizeof vv);
        memcpy(&vv, val.data, val.size);
        assert(vv == htonl(0));
        toku_free(val.data);
    } 

    /* reopen the database to force nonleaf buffering */
    r = db->close(db, 0);
    assert(r == 0);
    r = db_create(&db, env, 0);
    assert(r == 0);
    r = db->set_flags(db, dup_mode);
    assert(r == 0);
    r = db->set_pagesize(db, 4096);
    assert(r == 0);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, 0, 0666);
    assert(r == 0);

    /* insert n duplicates */
    for (i=0; i<n; i++) {
        int k = htonl(n/2);
        int v = htonl(n+i);
        db_put(db,k, v);

        DBT key, val;
        r = db->get(db, null_txn, dbt_init(&key, &k, sizeof k), dbt_init_malloc(&val), 0);
        assert(r == 0);
        unsigned int vv;
        assert(val.size == sizeof vv);
        memcpy(&vv, val.data, val.size);
        assert(vv == htonl(0));
        toku_free(val.data);
    } 

    DBC *cursor;
    r = db->cursor(db, null_txn, &cursor, 0);
    assert(r == 0);

    for (i=0; i<2*n; i++) {
        expect(cursor, htonl(n/2), htonl(i));
    }

    r = cursor->c_close(cursor);
    assert(r == 0);

    r = db->close(db, 0);
    assert(r == 0);
    r = env->close(env, 0), assert(r == 0);
}

static void
expect_db_lookup (DB *db, int k, int v) {
    DB_TXN * const null_txn = 0;
    DBT key, val;
    int r = db->get(db, null_txn, dbt_init(&key, &k, sizeof k), dbt_init_malloc(&val), 0);
    assert(r == 0);
    int vv;
    assert(val.size == sizeof vv);
    memcpy(&vv, val.data, val.size);
    assert(vv == v);
    toku_free(val.data);
}

/* insert 0, insert 1, close, insert 0, search 0 */
static __attribute__((__unused__))
void
test_i0i1ci0_search (int n, int dup_mode) {
    if (verbose) printf("test_i0i1ci0_search:%d %d\n", n, dup_mode);

    DB_TXN * const null_txn = 0;
    const char * const fname = "test_i0i1ci0.brt";
    int r;

    /* create the dup database file */
    DB_ENV *env;
    r = db_env_create(&env, 0); assert(r == 0);
    r = env->open(env, ENVDIR, DB_CREATE+DB_PRIVATE+DB_INIT_MPOOL, 0); assert(r == 0);

    DB *db;
    r = db_create(&db, env, 0);
    assert(r == 0);
    r = db->set_flags(db, dup_mode);
    assert(r == 0);
    r = db->set_pagesize(db, 4096);
    assert(r == 0);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, DB_CREATE, 0666);
    assert(r == 0);
    
    /* insert <0,0> */
    db_put(db, 0, 0);

    /* insert n duplicates */
    int i;
    for (i=0; i<n; i++) {
        int k = htonl(1);
        int v = htonl(i);
        db_put(db, k, v);
        expect_db_lookup(db, k, htonl(0));
    } 

    /* reopen the database to force nonleaf buffering */
    r = db->close(db, 0);
    assert(r == 0);
    r = db_create(&db, env, 0);
    assert(r == 0);
    r = db->set_flags(db, dup_mode);
    assert(r == 0);
    r = db->set_pagesize(db, 4096);
    assert(r == 0);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, 0, 0666);
    assert(r == 0);

    /* insert <0,1> */
    db_put(db, 0, 1);

    /* verify dup search digs deep into the tree */
    expect_db_lookup(db, 0, 0);

    r = db->close(db, 0);
    assert(r == 0);
    r = env->close(env, 0), assert(r == 0);
}

int
test_main(int argc, char *argv[]) {
    int i;
    int r;

    parse_args(argc, argv);
  
#ifdef USE_BDB
    /* dup tests */
    for (i = 1; i <= (1<<16); i *= 2) {
        r = system("rm -rf " ENVDIR); CKERR(r);
        r = toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
        test_dup_delete(i, DB_DUP);
        test_dup_delete_insert(i, DB_DUP);
        test_all_dup_delete_insert(i);
        test_walk_empty(i, DB_DUP);
    }
#endif

    /* dupsort tests */
    for (i = 1; i <= (1<<16); i *= 2) {
        r = system("rm -rf " ENVDIR); CKERR(r);
        r = toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
        test_dup_delete(i, DB_DUP + DB_DUPSORT);
        test_dup_delete_insert(i, DB_DUP + DB_DUPSORT);
        test_walk_empty(i, DB_DUP + DB_DUPSORT);
    }

    return 0;
}
