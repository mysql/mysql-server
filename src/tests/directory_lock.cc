/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
// Test that isolation works right for subtransactions.
// In particular, check to see what happens if a subtransaction has different isolation level from its parent.

#include "test.h"

const int envflags = DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN|DB_PRIVATE;


static int
put_multiple_callback(DB *dest_db __attribute__((unused)), DB *src_db __attribute__((unused)), DBT *dest_key __attribute__((unused)), DBT *dest_val __attribute__((unused)), const DBT *src_key __attribute__((unused)), const DBT *src_val __attribute__((unused))) {
    return 0;
}

static int
del_multiple_callback(DB *dest_db __attribute__((unused)), DB *src_db __attribute__((unused)), DBT *dest_key __attribute__((unused)), const DBT *src_key __attribute__((unused)), const DBT *src_val __attribute__((unused))) {
    return 0;
}

static int update_fun(DB *UU(db),
                      const DBT *UU(key),
                      const DBT *UU(old_val), const DBT *UU(extra),
                      void UU((*set_val)(const DBT *new_val,
                                      void *set_extra)),
                      void *UU(set_extra)) {
    return 0;
}

static void verify_shared_ops_fail(DB_ENV* env, DB* db) {
    int r;
    DB_TXN* txn = NULL;
    uint32_t flags = 0;
    DBT key,val;
    DBT in_key,in_val;
    uint32_t in_key_data, in_val_data = 0;
    memset(&in_key, 0, sizeof(in_key));
    memset(&in_val, 0, sizeof(in_val));
    in_key.size = sizeof(in_key_data);
    in_val.size = sizeof(in_val_data);
    in_key.data = &in_key_data;
    in_val.data = &in_val_data;
    in_key.flags = DB_DBT_USERMEM;
    in_val.flags = DB_DBT_USERMEM;
    in_key.ulen = sizeof(in_key_data);
    in_val.ulen = sizeof(in_val_data);
    DBT in_keys[2];
    memset(&in_keys, 0, sizeof(in_keys));
    dbt_init(&key, "a", 2);
    dbt_init(&val, "a", 2);

    r = env->txn_begin(env, NULL, &txn, 0); CKERR(r);
    r = db->put(
        db, 
        txn, 
        &key, 
        &val, 
        0
        ); 
    CKERR2(r, DB_LOCK_NOTGRANTED);
    r = txn->commit(txn,0); CKERR(r);

    r = env->txn_begin(env, NULL, &txn, 0); CKERR(r);
    r = db->del(
        db, 
        txn, 
        &key,  
        DB_DELETE_ANY
        );
    CKERR2(r, DB_LOCK_NOTGRANTED);    
    r = txn->commit(txn,0); CKERR(r);

    r = env->txn_begin(env, NULL, &txn, 0); CKERR(r);
    r = env->put_multiple(
        env, db, txn,
        &key, &val,
        1, &db, &in_key, &in_val, &flags);
    CKERR2(r, DB_LOCK_NOTGRANTED);
    r = txn->commit(txn,0); CKERR(r);

    r = env->txn_begin(env, NULL, &txn, 0); CKERR(r);
    r = env->put_multiple(
        env, NULL, txn,
        &key, &val,
        1, &db, &in_key, &in_val, &flags);
    CKERR2(r, DB_LOCK_NOTGRANTED);
    r = txn->commit(txn,0); CKERR(r);

    flags = DB_DELETE_ANY;

    r = env->txn_begin(env, NULL, &txn, 0); CKERR(r);
    r = env->del_multiple(
        env, db, txn,
        &key, &val,
        1, &db, &in_key, &flags);
    CKERR2(r, DB_LOCK_NOTGRANTED);
    r = txn->commit(txn,0); CKERR(r);

    r = env->txn_begin(env, NULL, &txn, 0); CKERR(r);
    r = env->del_multiple(
        env, NULL, txn,
        &key, &val,
        1, &db, &in_key, &flags);
    CKERR2(r, DB_LOCK_NOTGRANTED);
    r = txn->commit(txn,0); CKERR(r);

    flags = 0;

    r = env->txn_begin(env, NULL, &txn, 0); CKERR(r);
    r = env->update_multiple(
        env, NULL, txn,
        &key, &val,
        &key, &val,
        1, &db, &flags,
        2, in_keys,
        1, &in_val);
    CKERR2(r, DB_LOCK_NOTGRANTED);
    r = txn->commit(txn,0); CKERR(r);

    r = env->txn_begin(env, NULL, &txn, 0); CKERR(r);
    r = env->update_multiple(
        env, db, txn,
        &key, &val,
        &key, &val,
        1, &db, &flags,
        2, in_keys,
        1, &in_val);
    CKERR2(r, DB_LOCK_NOTGRANTED);
    r = txn->commit(txn,0); CKERR(r);

    
    DBT extra_up;
    dbt_init(&extra_up, NULL, 0);

    r = env->txn_begin(env, NULL, &txn, 0); CKERR(r);
    r = db->update(
        db, 
        txn, 
        &key, 
        &extra_up, 
        0
        ); 
    CKERR2(r, DB_LOCK_NOTGRANTED);
    r = txn->commit(txn,0); CKERR(r);

    r = env->txn_begin(env, NULL, &txn, 0); CKERR(r);
    r = db->update_broadcast(db, txn, &extra_up, 0);
    CKERR2(r, DB_LOCK_NOTGRANTED);
    r = txn->commit(txn,0); CKERR(r);

    r = env->txn_begin(env, NULL, &txn, 0); CKERR(r);
    r = db->update_broadcast(db, txn, &extra_up, DB_IS_RESETTING_OP);
    CKERR2(r, DB_LOCK_NOTGRANTED);
    r = txn->commit(txn,0); CKERR(r);

}

static void verify_excl_ops_fail(DB_ENV* env, const char* name) {
    DB_TXN* txn = NULL;
    int r; 
    DBT extra_up;
    dbt_init(&extra_up, NULL, 0);

    r = env->txn_begin(env, NULL, &txn, 0); CKERR(r);
    r = env->dbrename(env, txn, name, NULL, "asdf.db", 0);
    CKERR2(r, EINVAL);
    r = txn->commit(txn,0); CKERR(r);

}


int test_main (int argc, char * const argv[]) {
    parse_args(argc, argv);
    int r;
    DBT in_key,in_val;
    uint32_t in_key_data = 123456;
    uint32_t in_val_data = 654321;
    memset(&in_key, 0, sizeof(in_key));
    memset(&in_val, 0, sizeof(in_val));
    in_key.size = sizeof(in_key_data);
    in_val.size = sizeof(in_val_data);
    in_key.data = &in_key_data;
    in_val.data = &in_val_data;
    in_key.flags = DB_DBT_USERMEM;
    in_val.flags = DB_DBT_USERMEM;
    in_key.ulen = sizeof(in_key_data);
    in_val.ulen = sizeof(in_val_data);
    r = system("rm -rf " ENVDIR);
    CKERR(r);
    toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);
    DB_ENV *env;
    DB_LOADER* loader = NULL;
    uint32_t put_flags = 0;
    uint32_t dbt_flags = 0;
    r = db_env_create(&env, 0);                                                         CKERR(r);
    env->set_errfile(env, stderr);
    r = env->set_generate_row_callback_for_put(env, put_multiple_callback);
    CKERR(r);
    r = env->set_generate_row_callback_for_del(env, del_multiple_callback);
    CKERR(r);
    env->set_update(env, update_fun);
    r = env->open(env, ENVDIR, envflags, S_IRWXU+S_IRWXG+S_IRWXO);                      CKERR(r);
    
    DB* db;
    DB* db2;

    DB_TXN* txna = NULL;
    DB_TXN* txnb = NULL;


    //
    // transactionally create dictionary
    //
    r = env->txn_begin(env, NULL, &txna, 0); CKERR(r);
    r = db_create(&db2, env, 0); CKERR(r);
    r = db2->open(db2, txna, "foo2.db", NULL, DB_BTREE, DB_CREATE|DB_IS_HOT_INDEX, 0666); CKERR(r);
    verify_excl_ops_fail(env, "foo2.db");
    r = txna->commit(txna, 0); CKERR(r);


    //
    // transactionally create dictionary
    //
    r = env->txn_begin(env, NULL, &txna, 0); CKERR(r);
    r = db_create(&db, env, 0); CKERR(r);
    r = db->open(db, txna, "foo.db", NULL, DB_BTREE, DB_CREATE, 0666); CKERR(r);
    r = txna->commit(txna, 0); CKERR(r);

    //
    // create loader
    //
    r = env->txn_begin(env, NULL, &txna, 0); CKERR(r);
    r = env->create_loader(env, txna, &loader, NULL, 1, &db, &put_flags, &dbt_flags, 0); CKERR(r);
    verify_shared_ops_fail(env,db);
    r = loader->abort(loader); CKERR(r);
    loader=NULL;
    r = txna->commit(txna, 0); CKERR(r);

    r = env->txn_begin(env, NULL, &txna, 0); CKERR(r);
    r = env->txn_begin(env, NULL, &txnb, 0); CKERR(r);
    DBT key,val;
    dbt_init(&key, "a", 2);
    dbt_init(&val, "a", 2);
    r = db->put(db, txna, &key, &val, 0);       CKERR(r);
    dbt_init(&key, "b", 2);
    dbt_init(&val, "b", 2);
    r = db->put(db, txnb, &key, &val, 0);       CKERR(r);
    verify_excl_ops_fail(env,"foo.db");
    r = txna->abort(txna); CKERR(r);
    r = txnb->abort(txnb); CKERR(r);

    r = env->txn_begin(env, NULL, &txna, 0); CKERR(r);
    r = env->txn_begin(env, NULL, &txnb, 0); CKERR(r);
    dbt_init(&key, "a", 2);
    r = db->del(db, txna, &key, DB_DELETE_ANY); CKERR(r);
    dbt_init(&key, "b", 2);
    r = db->del(db, txnb, &key, DB_DELETE_ANY); CKERR(r);
    verify_excl_ops_fail(env,"foo.db");
    r = txna->abort(txna); CKERR(r);
    r = txnb->abort(txnb); CKERR(r);


    r = env->txn_begin(env, NULL, &txna, 0); CKERR(r);
    r = env->txn_begin(env, NULL, &txnb, 0); CKERR(r);
    dbt_init(&key, "a", 2);
    r = db->update(db, txna, &key, &val, 0); CKERR(r);
    dbt_init(&key, "b", 2);
    r = db->update(db, txnb, &key, &val, 0); CKERR(r);
    verify_excl_ops_fail(env,"foo.db");
    r = txna->abort(txna); CKERR(r);
    r = txnb->abort(txnb); CKERR(r);

    r = env->txn_begin(env, NULL, &txna, 0); CKERR(r);
    r = db->update_broadcast(db, txna, &val, 0); CKERR(r);
    verify_excl_ops_fail(env,"foo.db");
    r = txna->abort(txna); CKERR(r);

    uint32_t flags = 0;

    r = env->txn_begin(env, NULL, &txna, 0); CKERR(r);
    r = env->txn_begin(env, NULL, &txnb, 0); CKERR(r);
    dbt_init(&key, "a", 2);
    dbt_init(&val, "a", 2);
    env->put_multiple(
        env, NULL, txna,
        &key, &val,
        1, &db, &in_key, &in_val, &flags);
    CKERR(r);
    dbt_init(&key, "b", 2);
    dbt_init(&val, "b", 2);
    env->put_multiple(
        env, NULL, txnb,
        &key, &val,
        1, &db, &in_key, &in_val, &flags);
    CKERR(r);
    verify_excl_ops_fail(env,"foo.db");
    r = txna->abort(txna); CKERR(r);
    r = txnb->abort(txnb); CKERR(r);

    flags = DB_DELETE_ANY;
    r = env->txn_begin(env, NULL, &txna, 0); CKERR(r);
    r = env->txn_begin(env, NULL, &txnb, 0); CKERR(r);
    dbt_init(&key, "a", 2);
    dbt_init(&val, "a", 2);
    env->del_multiple(
        env, NULL, txna,
        &key, &val,
        1, &db, &in_key, &flags);
    CKERR(r);
    dbt_init(&key, "b", 2);
    dbt_init(&val, "b", 2);
    env->del_multiple(
        env, db, txnb,
        &key, &val,
        1, &db, &in_key, &flags);
    CKERR(r);
    verify_excl_ops_fail(env,"foo.db");
    r = txna->abort(txna); CKERR(r);
    r = txnb->abort(txnb); CKERR(r);

    flags = 0;
    DBT in_keys[2];
    memset(&in_keys, 0, sizeof(in_keys));
    r = env->txn_begin(env, NULL, &txna, 0); CKERR(r);
    r = env->txn_begin(env, NULL, &txnb, 0); CKERR(r);
    dbt_init(&key, "a", 2);
    dbt_init(&val, "a", 2);
    env->update_multiple(
        env, NULL, txna,
        &key, &val,
        &key, &val,
        1, &db, &flags,
        2, in_keys,
        1, &in_val);
    CKERR(r);
    dbt_init(&key, "b", 2);
    dbt_init(&val, "b", 2);
    env->update_multiple(
        env, db, txnb,
        &key, &val,
        &key, &val,
        1, &db, &flags,
        2, in_keys,
        1, &in_val);
    CKERR(r);
    verify_excl_ops_fail(env,"foo.db");
    r = txna->abort(txna); CKERR(r);
    r = txnb->abort(txnb); CKERR(r);

    r = db->close(db, 0); CKERR(r);
    r = db2->close(db2, 0); CKERR(r);
    r = env->close(env, 0); CKERR(r);
    
    return 0;
}
