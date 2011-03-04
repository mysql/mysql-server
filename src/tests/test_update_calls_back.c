// test that an update calls back into the update function

#include "test.h"

const int envflags = DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN|DB_PRIVATE;

DB_ENV *env;

const int to_update[] =      { 0, 1, 1, 1, 0, 0, 1, 0, 1, 0  };
      int updates_called[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0  };

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
    assert(to_update[*k] == 1);
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

int test_main (int argc, char * const argv[]) {
    parse_args(argc, argv);
    setup();

    DB *db;

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
            for (i = 0; i < (sizeof(to_update) / sizeof(to_update[0])); ++i) {
                CHK(db->put(db, txna, keyp, valp, 0));
            }
        }

        CHK(txna->commit(txna, 0));
    }

    {
        DB_TXN *txnb = NULL;
        CHK(env->txn_begin(env, NULL, &txnb, 0));

        {
            DBT key, nullextra;
            unsigned int i;
            DBT *keyp = dbt_init(&key, &i, sizeof(i));
            DBT *nullextrap = dbt_init(&nullextra, NULL, 0);
            for (i = 0; i < (sizeof(to_update) / sizeof(to_update[0])); ++i) {
                if (to_update[i] == 1) {
                    CHK(db->update(db, txnb, keyp, nullextrap, 0));
                }
            }
        }

        CHK(txnb->commit(txnb, 0));
    }

    CHK(db->close(db, 0));

    cleanup();

    for (unsigned int i = 0;
         i < (sizeof(to_update) / sizeof(to_update[0])); ++i) {
        assert(to_update[i] == updates_called[i]);
    }

    return 0;
}
