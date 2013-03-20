/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
// test that an update calls back into the update function

#include "test.h"

const int envflags = DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN|DB_PRIVATE;

DB_ENV *env;


static void setup (void) {
    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    { int chk_r = toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(chk_r); }
    { int chk_r = db_env_create(&env, 0); CKERR(chk_r); }
    env->set_errfile(env, stderr);
    { int chk_r = env->open(env, TOKU_TEST_FILENAME, envflags, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(chk_r); }
}

static void cleanup (void) {
    { int chk_r = env->close(env, 0); CKERR(chk_r); }
}

uint32_t four_byte_desc = 101;
uint64_t eight_byte_desc = 10101;


static void assert_desc_four (DB* db) {
    assert(db->descriptor->dbt.size == sizeof(four_byte_desc));
    assert(*(uint32_t *)(db->descriptor->dbt.data) == four_byte_desc);
}
static void assert_desc_eight (DB* db) {
    assert(db->descriptor->dbt.size == sizeof(eight_byte_desc));
    assert(*(uint32_t *)(db->descriptor->dbt.data) == eight_byte_desc);
}

static void run_test(void) {
    DB* db = NULL;
    DB* db2 = NULL;
    
    DBT orig_desc;
    memset(&orig_desc, 0, sizeof(orig_desc));
    orig_desc.size = sizeof(four_byte_desc);
    orig_desc.data = &four_byte_desc;
    // verify we can only set a descriptor with version 1
    IN_TXN_COMMIT(env, NULL, txn_create, 0, {
            { int chk_r = db_create(&db, env, 0); CKERR(chk_r); }
            assert(db->descriptor == NULL);
            { int chk_r = db->open(db, txn_create, "foo.db", NULL, DB_BTREE, DB_CREATE, 0666); CKERR(chk_r); }
            { int chk_r = db->change_descriptor(db, txn_create, &orig_desc, 0); CKERR(chk_r); }
            assert_desc_four(db);
        });

    { int chk_r = db_create(&db2, env, 0); CKERR(chk_r); }
    { int chk_r = db2->open(db2, NULL, "foo.db", NULL, DB_BTREE, 0, 0666); CKERR(chk_r); }
    assert_desc_four(db2);
    { int chk_r = db2->close(db2, 0); CKERR(chk_r); }
    db2 = NULL;

    { int chk_r = db->close(db, 0); CKERR(chk_r); }
    db = NULL;

    // verify that after closing and reopening db gets the same descriptor
    { int chk_r = db_create(&db, env, 0); CKERR(chk_r); }
    { int chk_r = db->open(db, NULL, "foo.db", NULL, DB_BTREE, 0, 0666); CKERR(chk_r); }
    assert_desc_four(db);

    /********************************************************************/
    
    // now lets test change_descriptor
    DBT change_descriptor;
    memset(&change_descriptor, 0, sizeof(change_descriptor));
    change_descriptor.size = sizeof(eight_byte_desc);
    change_descriptor.data = &eight_byte_desc;

    // test that simple abort works
    IN_TXN_ABORT(env, NULL, txn_change, 0, {
            { int chk_r = db->change_descriptor(db, txn_change, &change_descriptor, 0); CKERR(chk_r); }
        assert_desc_eight(db);
        });
    assert_desc_four(db);
    
    // test that close/reopen gets the right descriptor
    { int chk_r = db->close(db, 0); CKERR(chk_r); }
    db = NULL;
    { int chk_r = db_create(&db, env, 0); CKERR(chk_r); }
    { int chk_r = db->open(db, NULL, "foo.db", NULL, DB_BTREE, 0, 0666); CKERR(chk_r); }
    assert_desc_four(db);
    { int chk_r = db_create(&db2, env, 0); CKERR(chk_r); }
    { int chk_r = db2->open(db2, NULL, "foo.db", NULL, DB_BTREE, 0, 0666); CKERR(chk_r); }
    assert_desc_four(db2);
    { int chk_r = db2->close(db2, 0); CKERR(chk_r); }
    db2 = NULL;

    // test that simple commit works
    IN_TXN_COMMIT(env, NULL, txn_change, 0, {
            { int chk_r = db->change_descriptor(db, txn_change, &change_descriptor, 0); CKERR(chk_r); }
        assert_desc_eight(db);
        });
    assert_desc_eight(db);
    
    // test that close/reopen gets the right descriptor
    { int chk_r = db->close(db, 0); CKERR(chk_r); }
    db = NULL;
    { int chk_r = db_create(&db, env, 0); CKERR(chk_r); }
    { int chk_r = db->open(db, NULL, "foo.db", NULL, DB_BTREE, 0, 0666); CKERR(chk_r); }
    assert_desc_eight(db);
    { int chk_r = db_create(&db2, env, 0); CKERR(chk_r); }
    { int chk_r = db2->open(db2, NULL, "foo.db", NULL, DB_BTREE, 0, 0666); CKERR(chk_r); }
    assert_desc_eight(db2);
    { int chk_r = db2->close(db2, 0); CKERR(chk_r); }
    db2 = NULL;


    change_descriptor.size = sizeof(four_byte_desc);
    change_descriptor.data = &four_byte_desc;
    // test that close then abort works
    IN_TXN_ABORT(env, NULL, txn_change, 0, {
            { int chk_r = db->change_descriptor(db, txn_change, &change_descriptor, 0); CKERR(chk_r); }
            { int chk_r = db->close(db, 0); CKERR(chk_r); }
        db = NULL;
        { int chk_r = db_create(&db, env, 0); CKERR(chk_r); }
        { int chk_r = db->open(db, txn_change, "foo.db", NULL, DB_BTREE, 0, 0666); CKERR(chk_r); }
        assert_desc_four(db);
        { int chk_r = db->close(db, 0); CKERR(chk_r); }
        db = NULL;
        });
    { int chk_r = db_create(&db, env, 0); CKERR(chk_r); }
    { int chk_r = db->open(db, NULL, "foo.db", NULL, DB_BTREE, 0, 0666); CKERR(chk_r); }
    assert_desc_eight(db);
    { int chk_r = db_create(&db2, env, 0); CKERR(chk_r); }
    { int chk_r = db2->open(db2, NULL, "foo.db", NULL, DB_BTREE, 0, 0666); CKERR(chk_r); }
    assert_desc_eight(db2);
    { int chk_r = db2->close(db2, 0); CKERR(chk_r); }
    db2 = NULL;

    // test that close then commit works
    IN_TXN_COMMIT(env, NULL, txn_change, 0, {
            { int chk_r = db->change_descriptor(db, txn_change, &change_descriptor, 0); CKERR(chk_r); }
            { int chk_r = db->close(db, 0); CKERR(chk_r); }
        db = NULL;
        { int chk_r = db_create(&db, env, 0); CKERR(chk_r); }
        { int chk_r = db->open(db, txn_change, "foo.db", NULL, DB_BTREE, 0, 0666); CKERR(chk_r); }
        assert_desc_four(db);
        { int chk_r = db->close(db, 0); CKERR(chk_r); }
        db = NULL;
        });
    { int chk_r = db_create(&db, env, 0); CKERR(chk_r); }
    { int chk_r = db->open(db, NULL, "foo.db", NULL, DB_BTREE, 0, 0666); CKERR(chk_r); }
    assert_desc_four(db);
    { int chk_r = db_create(&db2, env, 0); CKERR(chk_r); }
    { int chk_r = db2->open(db2, NULL, "foo.db", NULL, DB_BTREE, 0, 0666); CKERR(chk_r); }
    assert_desc_four(db2);
    { int chk_r = db2->close(db2, 0); CKERR(chk_r); }
    db2 = NULL;
    { int chk_r = db->close(db, 0); CKERR(chk_r); }
    db = NULL;

    IN_TXN_ABORT(env, NULL, txn_create, 0, {
            { int chk_r = db_create(&db, env, 0); CKERR(chk_r); }
            assert(db->descriptor == NULL);
            { int chk_r = db->open(db, txn_create, "bar.db", NULL, DB_BTREE, DB_CREATE, 0666); CKERR(chk_r); }
            { int chk_r = db->change_descriptor(db, txn_create, &change_descriptor, 0); CKERR(chk_r); }
            // test some error cases
            IN_TXN_COMMIT(env, txn_create, txn_create2, 0, {
                    { int chk_r = db->change_descriptor(db, txn_create, &change_descriptor, 0); CKERR2(chk_r, EINVAL); }
                });
            assert_desc_four(db);
            { int chk_r = db->close(db, 0); CKERR(chk_r); }
            db = NULL;
        });
    IN_TXN_COMMIT(env, NULL, txn_create, 0, {
            { int chk_r = db_create(&db, env, 0); CKERR(chk_r); }
            assert(db->descriptor == NULL);
            { int chk_r = db->open(db, txn_create, "bar.db", NULL, DB_BTREE, DB_CREATE, 0666); CKERR(chk_r); }
            { int chk_r = db->change_descriptor(db, txn_create, &change_descriptor, 0); CKERR(chk_r); }
            assert_desc_four(db);
        });
    assert_desc_four(db);

    { int chk_r = db->close(db, 0); CKERR(chk_r); }
    db = NULL;
}

int test_main (int argc, char * const argv[]) {
    parse_args(argc, argv);
    setup();
    run_test();
    cleanup();
    return 0;
}
