// test that update broadcast does nothing if the table is empty

#include "test.h"

const int envflags = DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN|DB_PRIVATE;

DB_ENV *env;

static int update_fun(DB *UU(db),
                      const DBT *UU(key),
                      const DBT *UU(old_val), const DBT *UU(extra),
                      void (*set_val)(const DBT *new_val,
                                         void *set_extra),
                      void *set_extra) {
  set_val(extra,set_extra);
  return 0;
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
  DBT key, val;
  u_int32_t k = 101;
  u_int32_t v = 10101;
  dbt_init(&key, &k, sizeof(k));
  dbt_init(&val, &v, sizeof(v));

  int r = CHK(db->update(db, txn, &key, &val, flags));
    return r;
}

static void run_test(BOOL prelock, BOOL commit) {
    DB *db;
    u_int32_t update_flags = 0;
    setup();

    IN_TXN_COMMIT(env, NULL, txn_1, 0, {
            CHK(db_create(&db, env, 0));
            CHK(db->open(db, txn_1, "foo.db", NULL, DB_BTREE, DB_CREATE, 0666));
        });
    if (prelock) {
        IN_TXN_COMMIT(env, NULL, txn_2, 0, {
            CHK(db->pre_acquire_table_lock(db, txn_2));
        });
    }

    if (commit) {
        IN_TXN_COMMIT(env, NULL, txn_2, 0, {
            CHK(do_updates(txn_2, db, update_flags));
        });
        DBC *cursor = NULL;
        DBT key, val;
        memset(&key, 0, sizeof(key));
        memset(&val, 0, sizeof(val));

        IN_TXN_COMMIT(env, NULL, txn_3, 0, {
            CHK(db->cursor(db, txn_3, &cursor, 0));
            CHK(cursor->c_get(cursor, &key, &val, DB_NEXT));
            assert(key.size == sizeof(u_int32_t));
            assert(val.size == sizeof(u_int32_t));
            assert(*(u_int32_t *)(key.data) == 101);
            assert(*(u_int32_t *)(val.data) == 10101);
            CHK(cursor->c_close(cursor));
        });
    }
    else {
        IN_TXN_ABORT(env, NULL, txn_2, 0, {
            CHK(do_updates(txn_2, db, update_flags));
        });
        DBC *cursor = NULL;
        DBT key, val;
        memset(&key, 0, sizeof(key));
        memset(&val, 0, sizeof(val));

        IN_TXN_COMMIT(env, NULL, txn_3, 0, {
            CHK(db->cursor(db, txn_3, &cursor, 0));
            CHK2(cursor->c_get(cursor, &key, &val, DB_NEXT), DB_NOTFOUND);
            CHK(cursor->c_close(cursor));
        });
    }
    CHK(db->close(db, 0));
    cleanup();
}

int test_main(int argc, char * const argv[]) {
    parse_args(argc, argv);
    run_test(TRUE,TRUE);
    run_test(FALSE,TRUE);
    run_test(TRUE,FALSE);
    run_test(FALSE,FALSE);

    return 0;
}
