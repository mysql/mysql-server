/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
#include "test.h"

static int
put_callback(DB *dest_db, DB *src_db, DBT *dest_key, DBT *dest_data, const DBT *src_key, const DBT *src_data) {
    (void) dest_db; (void) src_db; (void) dest_key; (void) dest_data; (void) src_key; (void) src_data;
    lazy_assert(src_db != NULL && dest_db != NULL);

    dest_key->data = toku_xmemdup(src_data->data, src_data->size);
    dest_key->size = src_data->size;
    dest_data->size = 0;
    
    return 0;
}

static void
run_test(void) {
    int r;
    DB_ENV *env = NULL;
    r = db_env_create(&env, 0); assert_zero(r);

    r = env->set_generate_row_callback_for_put(env, put_callback); assert_zero(r);

    r = env->open(env, TOKU_TEST_FILENAME, DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN|DB_PRIVATE, S_IRWXU+S_IRWXG+S_IRWXO); assert_zero(r);

    DB *src_db = NULL;
    r = db_create(&src_db, env, 0); assert_zero(r);
    r = src_db->open(src_db, NULL, "0.tdb", NULL, DB_BTREE, DB_AUTO_COMMIT+DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); assert_zero(r);

    DB *dest_db = NULL;
    r = db_create(&dest_db, env, 0); assert_zero(r);
    r = dest_db->open(dest_db, NULL, "1.tdb", NULL, DB_BTREE, DB_AUTO_COMMIT+DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); assert_zero(r);

    DB_TXN* index_txn = NULL;
    r = env->txn_begin(env, NULL, &index_txn , 0); assert_zero(r);
    DB_TXN* put_txn = NULL;
    r = env->txn_begin(env, NULL, &put_txn , 0); assert_zero(r);

    DBT key,data;
    r = src_db->put(
        src_db, 
        put_txn,
        dbt_init(&key,  "hello", 6),
        dbt_init(&data, "there", 6),
        0
        );

    DB_INDEXER *indexer = NULL;
    r = env->create_indexer(env, index_txn, &indexer, src_db, 1, &dest_db, NULL, 0); assert_zero(r);
    r = indexer->build(indexer); assert_zero(r);        
    r = indexer->close(indexer); assert_zero(r);
    r = index_txn->abort(index_txn); assert_zero(r);

    r = put_txn->abort(put_txn); assert_zero(r);


    r = src_db->close(src_db, 0); assert_zero(r);
    r = dest_db->close(dest_db, 0); assert_zero(r);

    r = env->close(env, 0); assert_zero(r);
}

int
test_main(int argc, char * const argv[]) {
    int r;

    // parse_args(argc, argv);
    for (int i = 1; i < argc; i++) {
        char * const arg = argv[i];
        if (strcmp(arg, "-v") == 0) {
            verbose++;
            continue;
        }
        if (strcmp(arg, "-q") == 0) {
            verbose = 0;
            continue;
        }
    }

    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    r = toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO); assert_zero(r);

    run_test();

    return 0;
}

