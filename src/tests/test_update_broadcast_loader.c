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

    val_data = 1;


    //
    // now do an update broadcast that will set the val to something bigger
    //
    val_data = 100;
    IN_TXN_COMMIT(env, NULL, txn_broadcast, 0, {
            CHK(db->update_broadcast(db, txn_broadcast, &val, DB_IS_RESETTING_OP));
        });

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

    IN_TXN_COMMIT(env, NULL, txn_update, 0, {
            CHK(db->update(db, txn_update, &key, &val, 0));
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
