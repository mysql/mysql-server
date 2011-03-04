// test that an update calls back into the update function

#include "test.h"

const int envflags = DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN|DB_PRIVATE;

DB_ENV *env;

int updates_called[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

// the commands are: byte 1 is "nop" "add" or "del".  Byte 2 is the amount to add.
enum cmd { CNOP, CADD, CDEL };

static int update_fun(DB *UU(db),
                      const DBT *key,
                      const DBT *UU(old_val), const DBT *UU(extra),
                      void UU((*set_val)(const DBT *new_val,
                                         void *set_extra)),
                      void *UU(set_extra)) {
    unsigned int *k;
    assert(key->size == sizeof(*k));
    k = key->data;
    assert(updates_called[*k] == 0);
    updates_called[*k] = 1;
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

static void run_test(BOOL is_resetting) {
    DB *db;
    u_int32_t update_flags = is_resetting ? DB_IS_RESETTING_OP : 0;
    for (unsigned int i = 0;
         i < (sizeof(updates_called) / sizeof(updates_called[0])); ++i) {
        updates_called[i] = 0;
    }

    {
        DB_TXN* txna = NULL;
        CHK(env->txn_begin(env, NULL, &txna, 0));

        CHK(db_create(&db, env, 0));
        CHK(db->open(db, txna, "foo.db", NULL, DB_BTREE, DB_CREATE, 0666));

        {
            DBT key, val;
            unsigned int i;
            DBT *keyp = dbt_init(&key, &i, sizeof(i));
            DBT *valp = dbt_init(&val, "a", 2);
            for (i = 0; i < (sizeof(updates_called) / sizeof(updates_called[0])); ++i) {
                CHK(db->put(db, txna, keyp, valp, 0));
            }
        }

        CHK(txna->commit(txna, 0));
    }

    {
        DB_TXN *txnb = NULL;
        CHK(env->txn_begin(env, NULL, &txnb, 0));

        {
            DBT nullextra;
            DBT *nullextrap = dbt_init(&nullextra, NULL, 0);
            CHK(db->update_broadcast(db, txnb, nullextrap, update_flags));
        }

        CHK(txnb->commit(txnb, 0));
    }

    CHK(db->close(db, 0));

    for (unsigned int i = 0;
         i < (sizeof(updates_called) / sizeof(updates_called[0])); ++i) {
        assert(updates_called[i]);
    }

}

int test_main (int argc, char * const argv[]) {
    parse_args(argc, argv);
    setup();
    run_test(TRUE);
    run_test(FALSE);
    cleanup();
    return 0;
}
