// Test that isolation works right for subtransactions.
// In particular, check to see what happens if a subtransaction has different isolation level from its parent.

#include "test.h"

const int envflags = DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN|DB_PRIVATE;


static int
put_multiple_callback(DB *dest_db __attribute__((unused)), DB *src_db __attribute__((unused)), DBT *dest_key __attribute__((unused)), DBT *dest_val __attribute__((unused)), const DBT *src_key __attribute__((unused)), const DBT *src_val __attribute__((unused)), void *extra __attribute__((unused))) {
    return 0;
}

static int
del_multiple_callback(DB *dest_db __attribute__((unused)), DB *src_db __attribute__((unused)), DBT *dest_key __attribute__((unused)), const DBT *src_key __attribute__((unused)), const DBT *src_val __attribute__((unused)), void *extra __attribute__((unused))) {
    return 0;
}

static void verify_shared_ops_fail(DB_ENV* env, DB* db) {
    int r;
    DB_TXN* txn = NULL;
    u_int32_t flags = DB_YESOVERWRITE;
    DBC* c = NULL;
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
    dbt_init(&key, "a", 4);
    dbt_init(&val, "a", 4);

    r = env->txn_begin(env, NULL, &txn, 0); CKERR(r);
    r = db->cursor(db, txn, &c, 0); CKERR2(r,DB_LOCK_NOTGRANTED);
    r = txn->commit(txn,0); CKERR(r);

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
        1, &db, &in_key, &in_val, &flags,
        NULL);
    CKERR2(r, DB_LOCK_NOTGRANTED);
    r = txn->commit(txn,0); CKERR(r);

    r = env->txn_begin(env, NULL, &txn, 0); CKERR(r);
    r = env->put_multiple(
        env, NULL, txn,
        &key, &val,
        1, &db, &in_key, &in_val, &flags,
        NULL);
    CKERR2(r, DB_LOCK_NOTGRANTED);
    r = txn->commit(txn,0); CKERR(r);

    flags = DB_DELETE_ANY;

    r = env->txn_begin(env, NULL, &txn, 0); CKERR(r);
    r = env->del_multiple(
        env, db, txn,
        &key, &val,
        1, &db, &in_key, &flags,
        NULL);
    CKERR2(r, DB_LOCK_NOTGRANTED);
    r = txn->commit(txn,0); CKERR(r);

    r = env->txn_begin(env, NULL, &txn, 0); CKERR(r);
    r = env->del_multiple(
        env, NULL, txn,
        &key, &val,
        1, &db, &in_key, &flags,
        NULL);
    CKERR2(r, DB_LOCK_NOTGRANTED);
    r = txn->commit(txn,0); CKERR(r);

    flags = DB_YESOVERWRITE;

    r = env->txn_begin(env, NULL, &txn, 0); CKERR(r);
    r = env->update_multiple(
        env, NULL, txn,
        &key, &val,
        &key, &val,
        1, &db, &flags,
        2, in_keys,
        1, &in_val,
        NULL
        );
    CKERR2(r, DB_LOCK_NOTGRANTED);
    r = txn->commit(txn,0); CKERR(r);

    r = env->txn_begin(env, NULL, &txn, 0); CKERR(r);
    r = env->update_multiple(
        env, db, txn,
        &key, &val,
        &key, &val,
        1, &db, &flags,
        2, in_keys,
        1, &in_val,
        NULL
        );
    CKERR2(r, DB_LOCK_NOTGRANTED);
    r = txn->commit(txn,0); CKERR(r);

}

static void verify_excl_ops_fail(DB_ENV* env, DB* db) {
    DB_TXN* txn = NULL;
    int r; 
    DB_LOADER* loader = NULL;
    uint32_t put_flags = 0;
    uint32_t dbt_flags = 0;

    r = env->txn_begin(env, NULL, &txn, 0); CKERR(r);
    r = db->pre_acquire_fileops_lock(db, txn);
    CKERR2(r, DB_LOCK_NOTGRANTED);
    r = txn->commit(txn,0); CKERR(r);

    r = env->txn_begin(env, NULL, &txn, 0); CKERR(r);
    r = env->create_loader(env, txn, &loader, NULL, 1, &db, &put_flags, &dbt_flags, 0);
    //CKERR2(r, -1);
    r = txn->commit(txn,0); CKERR(r);

}


int test_main (int argc, char * const argv[]) {
    parse_args(argc, argv);
    int r;
    DBC* c1 = NULL;
    DBC* c2 = NULL;
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
    r = env->open(env, ENVDIR, envflags, S_IRWXU+S_IRWXG+S_IRWXO);                      CKERR(r);
    
    DB *db;

    DB_TXN* txna = NULL;
    DB_TXN* txnb = NULL;

    //
    // transactionally create dictionary
    //
    r = env->txn_begin(env, NULL, &txna, 0); CKERR(r);
    r = db_create(&db, env, 0); CKERR(r);
    r = db->open(db, txna, "foo.db", NULL, DB_BTREE, DB_CREATE, 0666); CKERR(r);
    verify_shared_ops_fail(env, db);
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

    //
    // preacquire fileops lock
    //
    r = env->txn_begin(env, NULL, &txna, 0); CKERR(r);
    r = db->pre_acquire_fileops_lock(db,txna); CKERR(r);
    verify_shared_ops_fail(env,db);
    r = txna->commit(txna, 0); CKERR(r);

    r = env->txn_begin(env, NULL, &txna, 0); CKERR(r);
    r = db->pre_acquire_fileops_lock(db,txna); CKERR(r);
    verify_shared_ops_fail(env,db);
    r = txna->commit(txna, 0); CKERR(r);


    r = env->txn_begin(env, NULL, &txna, 0); CKERR(r);
    r = env->txn_begin(env, NULL, &txnb, 0); CKERR(r);
    r = db->cursor(db, txna, &c1, 0); CKERR(r);
    r = db->cursor(db, txnb, &c2, 0); CKERR(r);
    verify_excl_ops_fail(env,db);
    c1->c_close(c1);
    c2->c_close(c2);
    r = txna->abort(txna); CKERR(r);
    r = txnb->abort(txnb); CKERR(r);

    r = env->txn_begin(env, NULL, &txna, 0); CKERR(r);
    r = env->txn_begin(env, NULL, &txnb, 0); CKERR(r);
    DBT key,val;
    dbt_init(&key, "a", 4);
    dbt_init(&val, "a", 4);
    r = db->put(db, txna, &key, &val, 0);       CKERR(r);
    dbt_init(&key, "b", 4);
    dbt_init(&val, "b", 4);
    r = db->put(db, txna, &key, &val, 0);       CKERR(r);
    verify_excl_ops_fail(env,db);
    r = txna->abort(txna); CKERR(r);
    r = txnb->abort(txnb); CKERR(r);

    r = env->txn_begin(env, NULL, &txna, 0); CKERR(r);
    r = env->txn_begin(env, NULL, &txnb, 0); CKERR(r);
    dbt_init(&key, "a", 4);
    r = db->del(db, txna, &key, DB_DELETE_ANY); CKERR(r);
    dbt_init(&key, "b", 4);
    r = db->del(db, txna, &key, DB_DELETE_ANY); CKERR(r);
    verify_excl_ops_fail(env,db);
    r = txna->abort(txna); CKERR(r);
    r = txnb->abort(txnb); CKERR(r);

    /*
    u_int32_t flags = DB_YESOVERWRITE;


    r = env->txn_begin(env, NULL, &txna, 0); CKERR(r);
    r = env->txn_begin(env, NULL, &txnb, 0); CKERR(r);
    dbt_init(&key, "a", 4);
    dbt_init(&val, "a", 4);
    env->put_multiple(
        env, NULL, txna,
        &key, &val,
        1, &db, &in_key, &in_val, &flags,
        NULL);
    CKERR(r);
    dbt_init(&key, "b", 4);
    dbt_init(&val, "b", 4);
    env->put_multiple(
        env, NULL, txnb,
        &key, &val,
        1, &db, &in_key, &in_val, &flags,
        NULL);
    CKERR(r);
    verify_excl_ops_fail(env,db);
    r = txna->abort(txna); CKERR(r);
    r = txnb->abort(txnb); CKERR(r);

    flags = DB_DELETE_ANY;
    r = env->txn_begin(env, NULL, &txna, 0); CKERR(r);
    r = env->txn_begin(env, NULL, &txnb, 0); CKERR(r);
    dbt_init(&key, "a", 4);
    dbt_init(&val, "a", 4);
    env->del_multiple(
        env, NULL, txna,
        &key, &val,
        1, &db, &in_key, &flags,
        NULL);
    CKERR(r);
    dbt_init(&key, "b", 4);
    dbt_init(&val, "b", 4);
    env->del_multiple(
        env, db, txnb,
        &key, &val,
        1, &db, &in_key, &flags,
        NULL);
    CKERR(r);
    verify_excl_ops_fail(env,db);
    r = txna->abort(txna); CKERR(r);
    r = txnb->abort(txnb); CKERR(r);

    flags = DB_YESOVERWRITE;
    DBT in_keys[2];
    memset(&in_keys, 0, sizeof(in_keys));
    r = env->txn_begin(env, NULL, &txna, 0); CKERR(r);
    r = env->txn_begin(env, NULL, &txnb, 0); CKERR(r);
    dbt_init(&key, "a", 4);
    dbt_init(&val, "a", 4);
    env->update_multiple(
        env, NULL, txna,
        &key, &val,
        &key, &val,
        1, &db, &flags,
        2, in_keys,
        1, &in_val,
        NULL
        );
    CKERR(r);
    dbt_init(&key, "b", 4);
    dbt_init(&val, "b", 4);
    env->update_multiple(
        env, db, txnb,
        &key, &val,
        &key, &val,
        1, &db, &flags,
        2, in_keys,
        1, &in_val,
        NULL
        );
    CKERR(r);
    verify_excl_ops_fail(env,db);
    r = txna->abort(txna); CKERR(r);
    r = txnb->abort(txnb); CKERR(r);
    */
    r = db->close(db, 0); CKERR(r);
    r = env->close(env, 0); CKERR(r);
    
    return 0;
}
