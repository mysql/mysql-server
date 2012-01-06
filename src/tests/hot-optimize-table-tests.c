/* -*- mode: C; c-basic-offset: 4 -*- */
// hot-optimize-table-tests.c

#include "test.h"
#include "includes.h"
#include <brt-cachetable-wrappers.h>
#include "db.h"
#include "ydb.h"

const int envflags = DB_INIT_MPOOL |
                     DB_CREATE |
                     DB_THREAD |
                     DB_INIT_LOCK |
                     DB_INIT_LOG |
                     DB_INIT_TXN |
                     DB_PRIVATE;

DB_ENV* env;
unsigned int leaf_hits;

// Custom Update Function for our test BRT.
static int
update_func(DB* UU(db),
            const DBT* key,
            const DBT* old_val,
            const DBT* extra,
            void (*set_val)(const DBT* new_val, void* set_extra) __attribute__((unused)),
            void* UU(set_extra))
{
    unsigned int *x_results;
    assert(extra->size == sizeof x_results);
    x_results = *(unsigned int **) extra->data;
    assert(x_results);
    assert(old_val->size > 0);
    unsigned int* indexptr;
    assert(key->size == (sizeof *indexptr));
    indexptr = (unsigned int*)key->data;
    ++leaf_hits;

    if (verbose && x_results[*indexptr] != 0) {
        printf("x_results = %p, indexptr = %p, *indexptr = %u, x_results[*indexptr] = %u\n", x_results, indexptr, *indexptr, x_results[*indexptr]);
    }

    assert(x_results[*indexptr] == 0);
    x_results[*indexptr]++;
    // ++(x_results[*indexptr]);
    // memset(&new_val, 0, sizeof(new_val));
    // set_val(&new_val, set_extra);
    unsigned int i = *indexptr;
    if (verbose && ((i + 1) % 50000 == 0)) {
        printf("applying update to %u\n", i);
        //printf("x_results[] = %u\n", x_results[*indexptr]);
    }

    return 0;
}

///
static void
hot_test_setup(void)
{
    int r = 0;
    // Remove any previous environment.
    CHK(system("rm -rf " ENVDIR));

    // Set up a new TokuDB.
    CHK(toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO));
    CHK(db_env_create(&env, 0));
    env->set_errfile(env, stderr);
    r = env->set_default_bt_compare(env, uint_dbt_cmp);CKERR(r);
    env->set_update(env, update_func);
    CHK(env->open(env, ENVDIR, envflags, S_IRWXU+S_IRWXG+S_IRWXO));
}

///
static void
hot_insert_keys(DB* db, unsigned int key_count)
{
    int r = 0;
    DB_TXN * xact;
    unsigned int limit = 1;
    if (key_count > 10) {
        limit = 100000;
    }

    // Dummy data.
    const unsigned int DUMMY_SIZE = 100;
    size_t size = DUMMY_SIZE;
    char* dummy = NULL;
    dummy = (char*)toku_xmalloc(size);
    memset(dummy, 0, size);

    // Start the transaction for insertions.
    //
    r = env->txn_begin(env, 0, &xact, 0); CKERR(r);

    unsigned int key;

    DBT key_thing;
    DBT *keyptr = dbt_init(&key_thing, &key, sizeof(key));
    DBT value_thing;
    DBT *valueptr = dbt_init(&value_thing, dummy, size);
    for (key = 0; key < key_count; ++key)
    {
        CHK(db->put(db, xact, keyptr, valueptr, 0));

        // DEBUG OUTPUT
        //
        if (verbose && (key + 1) % limit == 0) {
            printf("%d Elements inserted.\n", key + 1);
        }
    }

    // Commit the insert transaction.
    //
    r = xact->commit(xact, 0); CKERR(r);

    toku_free(dummy);
}

///
static void
hot_create_db(DB** db, const char* c)
{
    int r = 0;
    DB_TXN* xact;
    verbose ? printf("Creating DB.\n") : 0;
    r = env->txn_begin(env, 0, &xact, 0); CKERR(r);
    CHK(db_create(db, env, 0));
    CHK((*db)->open((*db), xact, c, NULL, DB_BTREE, DB_CREATE, 0666));
    r = xact->commit(xact, 0); CKERR(r);
    verbose ? printf("DB Created.\n") : 0;
}

///
static void
hot_test(DB* db, unsigned int size)
{
    int r = 0;
    leaf_hits = 0;
    verbose ? printf("Insert some data.\n") : 0;

    // Insert our keys to assemble the tree.
    hot_insert_keys(db, size);

    // Insert Broadcast Message.
    verbose ? printf("Insert Broadcast Message.\n") : 0;
    unsigned int *XMALLOC_N(size, x_results);
    memset(x_results, 0, (sizeof x_results[0]) * size);
    DBT extra;
    DBT *extrap = dbt_init(&extra, &x_results, sizeof x_results);
    DB_TXN * xact;
    r = env->txn_begin(env, 0, &xact, 0); CKERR(r);
    r = CHK(db->update_broadcast(db, xact, extrap, 0));
    r = xact->commit(xact, 0); CKERR(r);

    // Flatten the tree.
    verbose ? printf("Calling hot optimize...\n") : 0;
    r = db->hot_optimize(db, NULL, NULL);
    assert(r == 0);
    verbose ? printf("HOT Finished!\n") : 0;
    for (unsigned int i = 0; i < size; ++i) {
        assert(x_results[i] == 1);
    }
    verbose ? printf("Leaves hit = %u\n", leaf_hits) :0;
    toku_free(x_results);
}

///
int 
test_main(int argc, char * const argv[])
{
    int r = 0;
    default_parse_args(argc, argv);
    hot_test_setup();

    // Create and Open the Database/BRT
    DB *db = NULL;
    const unsigned int BIG = 4000000;
    const unsigned int SMALL = 10;
    const unsigned int NONE = 0;

    hot_create_db(&db, "none.db");
    hot_test(db, NONE);
    hot_create_db(&db, "small.db");
    hot_test(db, SMALL);
    hot_create_db(&db, "big.db");
    hot_test(db, BIG);

    verbose ? printf("Exiting Test.\n") : 0;
    return r;
}
