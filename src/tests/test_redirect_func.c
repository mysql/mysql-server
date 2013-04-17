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
    assert(FALSE);
    return 0;
}

static int generate_row_for_put(
    DB *UU(dest_db), 
    DB *UU(src_db),
    DBT *dest_key, 
    DBT *dest_val,
    const DBT *src_key, 
    const DBT *src_val
    ) 
{
    dest_key->size=src_key->size;
    dest_key->data=src_key->data;
    dest_key->flags = 0;
    dest_val->size=src_val->size;
    dest_val->data=src_val->data;
    dest_val->flags = 0;
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
    DB_LOADER* loader = NULL;
    DBT key, val;
    u_int32_t mult_db_flags = 0;
    u_int32_t mult_dbt_flags = DB_DBT_REALLOC;
    u_int8_t key_data = 0;
    u_int8_t val_data = 0;
    

    IN_TXN_COMMIT(env, NULL, txn_create, 0, {
            CHK(db_create(&db, env, 0));
            CHK(db->open(db, txn_create, "foo.db", NULL, DB_BTREE, DB_CREATE, 0666));
        });


    dbt_init(&key,&key_data,sizeof(u_int8_t));
    dbt_init(&val,&val_data,sizeof(u_int8_t));

    val_data = 100;

    //
    // now create a loader
    //
    IN_TXN_COMMIT(env, NULL, txn_loader, 0, {
        // create DB
        CHK(env->create_loader(
            env,
            txn_loader,
            &loader,
            db,
            1,
            &db,
            &mult_db_flags,
            &mult_dbt_flags,
            0
            ));
        CHK(loader->put(loader, &key, &val));
        CHK(loader->close(loader));
        });

    val_data = 101;
    IN_TXN_COMMIT(env, NULL, txn_update, 0, {
            CHK(db->update(db, txn_update, &key, &val, 0));
        });

    key_data = 11;
    val_data = 11;
    IN_TXN_COMMIT(env, NULL, txn_update, 0, {
            CHK(db->update(db, txn_update, &key, &val, 0));
        });

    
    DBC *cursor = NULL;
    IN_TXN_COMMIT(env, NULL, txn_3, 0, {
        CHK(db->cursor(db, txn_3, &cursor, 0));
        CHK(cursor->c_get(cursor, &key, &val, DB_NEXT));
        assert(key.size == sizeof(u_int8_t));
        assert(val.size == sizeof(u_int8_t));
        assert(*(u_int8_t *)(key.data) == 0);
        assert(*(u_int8_t *)(val.data) == 101);
        CHK(cursor->c_get(cursor, &key, &val, DB_NEXT));
        assert(key.size == sizeof(u_int8_t));
        assert(val.size == sizeof(u_int8_t));
        assert(*(u_int8_t *)(key.data) == 11);
        assert(*(u_int8_t *)(val.data) == 11);
        CHK(cursor->c_close(cursor));
    });
    
    CHK(db->close(db, 0));

}

int test_main (int argc, char * const argv[]) {
    parse_args(argc, argv);
    setup();
    run_test();
    cleanup();
    return 0;
}
