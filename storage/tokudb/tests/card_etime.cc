/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

// Verify that analyze can be terminated when its executing time limit is reached

#ident "Copyright (c) 2013 Tokutek Inc.  All rights reserved."
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <memory.h>
#include <assert.h>
#include <errno.h>
#include <db.h>
#if __linux__
#include <endian.h>
#endif
#include <sys/stat.h>
#include <sys/types.h>
typedef unsigned long long ulonglong;
#include "tokudb_status.h"
#include "tokudb_buffer.h"
// Provide some mimimal MySQL classes just to compile the tokudb cardinality functions
class KEY_INFO {
public:
    uint flags;
    uint64_t *rec_per_key;
};
#define HA_NOSAME 1
class TABLE_SHARE {
public:
    uint primary_key;
    uint keys;
};
class TABLE {
public:
    TABLE_SHARE *s;
    KEY_INFO *key_info;
};
uint get_key_parts(KEY_INFO *key_info) {
    assert(key_info);
    return 0;
}
#if __APPLE__
typedef unsigned long ulong;
#endif
#include "tokudb_card.h"

static int verbose = 0;

static uint32_t hton32(uint32_t n) {
#if BYTE_ORDER == LITTLE_ENDIAN
    return __builtin_bswap32(n);
#else
    return n;
#endif
}

struct key {
    uint32_t k0;
}; //  __attribute__((packed));

struct val {
    uint32_t v0;
}; //  __attribute__((packed));

// load nrows into the db
static void load_db(DB_ENV *env, DB *db, uint32_t nrows) {
    DB_TXN *txn = NULL;
    int r = env->txn_begin(env, NULL, &txn, 0);
    assert(r == 0);

    DB_LOADER *loader = NULL;
    uint32_t db_flags[1] = { 0 };
    uint32_t dbt_flags[1] = { 0 };
    uint32_t loader_flags = 0;
    r = env->create_loader(env, txn, &loader, db, 1, &db, db_flags, dbt_flags, loader_flags);
    assert(r == 0);

    for (uint32_t seq = 0; seq < nrows ; seq++) {
        struct key k = { hton32(seq) };
        struct val v = { seq };
        DBT key = { .data = &k, .size = sizeof k };
        DBT val = { .data = &v, .size = sizeof v };
        r = loader->put(loader, &key, &val);
        assert(r == 0);
    }

    r = loader->close(loader);
    assert(r == 0);

    r = txn->commit(txn, 0);
    assert(r == 0);
}

static int analyze_key_compare(DB *db __attribute__((unused)), const DBT *a, const DBT *b, uint level) {
    assert(level == 1);
    assert(a->size == b->size);
    return memcmp(a->data, b->data, a->size);
}

struct analyze_extra {
    uint64_t now;
    uint64_t limit;
};

static int analyze_progress(void *extra, uint64_t rows) {
    assert(rows > 0);
    struct analyze_extra *e = (struct analyze_extra *) extra;
    e->now++;
    int r;
    if (e->limit > 0 && e->now >= e->limit)
        r = ETIME;
    else 
        r = 0;
    if (verbose)
        printf("%s %"PRIu64" %"PRIu64" r=%d\n", __FUNCTION__, e->now, e->limit, r);
    return r;
}

static void test_card(DB_ENV *env, DB *db, uint64_t expect_card, uint64_t limit) {
    int r;
    
    DB_TXN *txn = NULL;
    r = env->txn_begin(env, NULL, &txn, 0);
    assert(r == 0);

    uint64_t num_key_parts = 1;
    uint64_t rec_per_key[num_key_parts];
    for (uint64_t i = 0; i < num_key_parts; i++)
        rec_per_key[i] = 0;

    struct analyze_extra analyze_extra = { 0, limit };
    r = tokudb::analyze_card(db, txn, false, num_key_parts, rec_per_key, analyze_key_compare, analyze_progress, &analyze_extra);
    if (limit == 0) {
        assert(r == 0);
    } else {
        assert(r == ETIME);
        assert(analyze_extra.now == analyze_extra.limit);
    }

    assert(rec_per_key[0] == expect_card);

    r = txn->commit(txn, 0);
    assert(r == 0);
}

int main(int argc, char * const argv[]) {
    uint32_t nrows = 1000000;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--verbose") == 0 || strcmp(argv[i], "-v") == 0) {
            verbose++;
            continue;
        }
        if (strcmp(argv[i], "--nrows") == 0 && i+1 < argc) {
            nrows = atoi(argv[++i]);
            continue;
        }
    }

    int r;
    r = system("rm -rf " __FILE__ ".testdir");
    assert(r == 0);
    r = mkdir(__FILE__ ".testdir", S_IRWXU+S_IRWXG+S_IRWXO);
    assert(r == 0);

    DB_ENV *env = NULL;
    r = db_env_create(&env, 0);
    assert(r == 0);

    r = env->open(env, __FILE__ ".testdir", DB_INIT_MPOOL + DB_INIT_LOG + DB_INIT_LOCK + DB_INIT_TXN + DB_PRIVATE + DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); 
    assert(r == 0);

    // create the db
    DB *db = NULL;
    r = db_create(&db, env, 0);
    assert(r == 0);

    r = db->open(db, NULL, "test.db", 0, DB_BTREE, DB_CREATE + DB_AUTO_COMMIT, S_IRWXU+S_IRWXG+S_IRWXO);
    assert(r == 0);

    // load the db
    load_db(env, db, nrows);

    // test cardinality
    test_card(env, db, 1, 0);
    test_card(env, db, 1, 1);
    test_card(env, db, 1, 10);
    test_card(env, db, 1, 100);

    r = db->close(db, 0);
    assert(r == 0);

    r = env->close(env, 0);
    assert(r == 0);

    return 0;
}
