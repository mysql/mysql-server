// test that update broadcast does nothing if the table is empty

#include "test.h"

const int envflags = DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN|DB_PRIVATE;

DB_ENV *env;

static int update_fun(DB *UU(db),
                      const DBT *UU(key),
                      const DBT *UU(old_val), const DBT *UU(extra),
                      void UU((*set_val)(const DBT *new_val,
                                         void *set_extra)),
                      void *UU(set_extra)) {
    assert(0);
}

static void setup (void) {
    CHK(system("rm -rf " ENVDIR));
    CHK(toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO));
    CHK(db_env_create(&env, 0));
    env->set_errfile(env, stderr);
    env->set_update(env, update_fun);
    CHK(env->open(env, ENVDIR, envflags, S_IRWXU+S_IRWXG+S_IRWXO));
}

static void cleanup (void) {
    CHK(env->close(env, 0));
}

static int do_updates(DB_TXN *txn, DB *db, u_int32_t flags) {
    DBT extra;
    DBT *extrap = dbt_init(&extra, NULL, 0);
    int r = CHK(db->update_broadcast(db, txn, extrap, flags));
    return r;
}

static void run_test(BOOL is_resetting, BOOL prelock) {
    DB *db;
    u_int32_t update_flags = is_resetting ? DB_IS_RESETTING_OP : 0;

    IN_TXN_COMMIT(env, NULL, txn_1, 0, {
            CHK(db_create(&db, env, 0));
            CHK(db->open(db, txn_1, "foo.db", NULL, DB_BTREE, DB_CREATE, 0666));
        });
    if (prelock) {
        IN_TXN_COMMIT(env, NULL, txn_2, 0, {
            CHK(db->pre_acquire_table_lock(db, txn_2));
        });
    }

    IN_TXN_COMMIT(env, NULL, txn_2, 0, {
            CHK(do_updates(txn_2, db, update_flags));
        });

    CHK(db->close(db, 0));
}

int test_main(int argc, char * const argv[]) {
    parse_args(argc, argv);
    setup();
    run_test(TRUE,TRUE);
    run_test(FALSE,TRUE);
    run_test(TRUE,FALSE);
    run_test(FALSE,FALSE);
    cleanup();

    return 0;
}
