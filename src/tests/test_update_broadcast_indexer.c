// test that an update calls back into the update function

#include "test.h"

const int envflags = DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN|DB_PRIVATE;

DB_ENV *env;


static int update_fun(DB *UU(db),
                      const DBT *UU(key),
                      const DBT *UU(old_val), const DBT *extra,
                      void (*set_val)(const DBT *new_val,
                                         void *set_extra),
                      void *set_extra) {
    set_val(extra, set_extra);
    return 0;
}


static int generate_row_for_del(
    DB *UU(dest_db), 
    DB *UU(src_db),
    DBT *dest_key,
    const DBT *UU(src_key), 
    const DBT *UU(src_val)
    )
{
    dest_key->size=0;
    return 0;
}

static int generate_row_for_put(
    DB *UU(dest_db), 
    DB *UU(src_db),
    DBT *dest_key, 
    DBT *dest_val,
    const DBT *UU(src_key), 
    const DBT *UU(src_val)
    ) 
{
    u_int8_t src_val_data;
    assert(src_val->size == 1);
    src_val_data = *(u_int8_t *)src_val->data;
    assert(src_val_data == 100);
    dest_key->size=0;
    dest_val->size=0;
    return 0;
}

static void setup (void) {
    CHK(system("rm -rf " ENVDIR));
    CHK(toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO));
    CHK(db_env_create(&env, 0));
    env->set_errfile(env, stderr);
    CHK(env->set_generate_row_callback_for_put(env,generate_row_for_put));
    CHK(env->set_generate_row_callback_for_del(env,generate_row_for_del));
    env->set_update(env, update_fun);
    CHK(env->open(env, ENVDIR, envflags, S_IRWXU+S_IRWXG+S_IRWXO));
}

static void cleanup (void) {
    CHK(env->close(env, 0));
}

static void run_test(void) {
    DB* db = NULL;
    DB* hot_index_db = NULL;
    DB_INDEXER* indexer = NULL;
    DBT key, val;
    u_int32_t mult_db_flags = 0;
    u_int8_t key_data = 0;
    u_int8_t val_data = 0;
    DB_TXN* txn_read1 = NULL;
    DB_TXN* txn_read2 = NULL;
    DB_TXN* txn_read3 = NULL;
    

    IN_TXN_COMMIT(env, NULL, txn_create, 0, {
            CHK(db_create(&db, env, 0));
            CHK(db->open(db, txn_create, "foo.db", NULL, DB_BTREE, DB_CREATE, 0666));
        });


    dbt_init(&key,&key_data,sizeof(u_int8_t));
    dbt_init(&val,&val_data,sizeof(u_int8_t));

    val_data = 1;
    IN_TXN_COMMIT(env, NULL, txn_put1, 0, {
            CHK(db->put(db, txn_put1, &key, &val, 0));
        });
    CHK(env->txn_begin(env, NULL, &txn_read1, DB_TXN_SNAPSHOT));

    val_data = 2;
    IN_TXN_COMMIT(env, NULL, txn_put2, 0, {
            CHK(db->put(db, txn_put2, &key, &val, 0));
        });
    CHK(env->txn_begin(env, NULL, &txn_read2, DB_TXN_SNAPSHOT));

    val_data = 3;
    IN_TXN_COMMIT(env, NULL, txn_put3, 0, {
            CHK(db->put(db, txn_put3, &key, &val, 0));
        });
    CHK(env->txn_begin(env, NULL, &txn_read3, DB_TXN_SNAPSHOT));

    //
    // at this point, we should have a leafentry with 3 committed values.
    // 


    //
    // now do an update broadcast that will set the val to something bigger
    //
    val_data = 100;
    IN_TXN_COMMIT(env, NULL, txn_broadcast, 0, {
            CHK(db->update_broadcast(db, txn_broadcast, &val, DB_IS_RESETTING_OP));
        });

    //
    // now create an indexer
    //
    IN_TXN_COMMIT(env, NULL, txn_indexer, 0, {
        // create DB
        CHK(db_create(&hot_index_db, env, 0));
        CHK(hot_index_db->open(hot_index_db, txn_indexer, "bar.db", NULL, DB_BTREE, DB_CREATE|DB_IS_HOT_INDEX, 0666));
        CHK(env->create_indexer(
            env,
            txn_indexer,
            &indexer,
            db,
            1,
            &hot_index_db,
            &mult_db_flags,
            0
            ));
        CHK(indexer->build(indexer));
        CHK(indexer->close(indexer));
        });

    //verify that txn_read1,2,3 cannot open a cursor on db
    DBC* cursor = NULL;
    CHK2(db->cursor(db, txn_read1, &cursor, 0), TOKUDB_MVCC_DICTIONARY_TOO_NEW);
    CHK2(db->cursor(db, txn_read2, &cursor, 0), TOKUDB_MVCC_DICTIONARY_TOO_NEW);
    CHK2(db->cursor(db, txn_read3, &cursor, 0), TOKUDB_MVCC_DICTIONARY_TOO_NEW);
    IN_TXN_COMMIT(env, NULL, txn_read_succ, 0, {
        CHK(db->cursor(db, txn_read_succ, &cursor, 0));
        CHK(cursor->c_close(cursor));
        cursor = NULL;
      });    
    CHK(db->close(db, 0));
    CHK(db_create(&db, env, 0));
    CHK(db->open(db, NULL, "foo.db", NULL, DB_BTREE, 0, 0666));
    CHK2(db->cursor(db, txn_read1, &cursor, 0), TOKUDB_MVCC_DICTIONARY_TOO_NEW);
    CHK2(db->cursor(db, txn_read2, &cursor, 0), TOKUDB_MVCC_DICTIONARY_TOO_NEW);
    CHK2(db->cursor(db, txn_read3, &cursor, 0), TOKUDB_MVCC_DICTIONARY_TOO_NEW);
    IN_TXN_COMMIT(env, NULL, txn_read_succ, 0, {
        CHK(db->cursor(db, txn_read_succ, &cursor, 0));
        CHK(cursor->c_close(cursor));
        cursor = NULL;
      });    

    // commit the read transactions
    CHK(txn_read1->commit(txn_read1, 0)); 
    CHK(txn_read2->commit(txn_read2, 0)); 
    CHK(txn_read3->commit(txn_read3, 0)); 

    CHK(db->close(db, 0));
    CHK(hot_index_db->close(hot_index_db, 0));

}

int test_main (int argc, char * const argv[]) {
    parse_args(argc, argv);
    setup();
    run_test();
    cleanup();
    return 0;
}
