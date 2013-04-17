#include "test.h"

// verify recovery of an update log entry which changes values at keys

static const int envflags = DB_INIT_MPOOL|DB_CREATE|DB_THREAD|DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN|DB_PRIVATE;
u_int32_t four_byte_desc = 101;
u_int64_t eight_byte_desc = 10101;

static void assert_desc_four (DB* db) {
    assert(db->descriptor->dbt.size == sizeof(four_byte_desc));
    assert(*(u_int32_t *)(db->descriptor->dbt.data) == four_byte_desc);
}
static void assert_desc_eight (DB* db) {
    assert(db->descriptor->dbt.size == sizeof(eight_byte_desc));
    assert(*(u_int32_t *)(db->descriptor->dbt.data) == eight_byte_desc);
}

static void run_test(void)
{
    DB_ENV *env;
    DB *db;
    DB *db2;
    DB *db3;
    DB_TXN* txn;
    DB_TXN* txn2;
    DB_TXN* txn3;
    DBT desc;
    memset(&desc, 0, sizeof(desc));
    desc.size = sizeof(four_byte_desc);
    desc.data = &four_byte_desc;

    DBT other_desc;
    memset(&other_desc, 0, sizeof(other_desc));
    other_desc.size = sizeof(eight_byte_desc);
    other_desc.data = &eight_byte_desc;

    CHK(system("rm -rf " ENVDIR));
    CHK(toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO));
    CHK(db_env_create(&env, 0));
    env->set_errfile(env, stderr);
    CHK(env->open(env, ENVDIR, envflags, S_IRWXU+S_IRWXG+S_IRWXO));

    IN_TXN_COMMIT(env, NULL, txn_1, 0, {
            CHK(db_create(&db, env, 0));
            CHK(db->open(db, txn_1, "foo.db", NULL, DB_BTREE, DB_CREATE, 0666));
        });
    IN_TXN_COMMIT(env, NULL, txn_2, 0, {
            CHK(db_create(&db2, env, 0));
            CHK(db2->open(db2, txn_2, "foo2.db", NULL, DB_BTREE, DB_CREATE, 0666));
            CHK(db2->change_descriptor(db2, txn_2, &other_desc, 0));
            assert_desc_eight(db2);
        });
    IN_TXN_COMMIT(env, NULL, txn_3, 0, {
            CHK(db_create(&db3, env, 0));
            CHK(db3->open(db3, txn_3, "foo3.db", NULL, DB_BTREE, DB_CREATE, 0666));
            CHK(db3->change_descriptor(db3, txn_3, &other_desc, 0));
            assert_desc_eight(db3);
        });
    
    CHK(env->txn_begin(env, NULL, &txn, 0));
    CHK(db->change_descriptor(db, txn, &desc, 0));

    CHK(env->txn_begin(env, NULL, &txn2, 0));
    CHK(db2->change_descriptor(db2, txn2, &desc, 0));

    CHK(env->txn_begin(env, NULL, &txn3, 0));
    CHK(db3->change_descriptor(db3, txn3, &desc, 0));

    CHK(env->txn_checkpoint(env,0,0,0));
    CHK(txn->commit(txn,0));
    CHK(txn2->abort(txn2));

    toku_hard_crash_on_purpose();
}


static void run_recover(void)
{
    DB_ENV *env;
    DB *db;
    DB *db2;
    DB *db3;

    CHK(db_env_create(&env, 0));
    env->set_errfile(env, stderr);
    CHK(env->open(env, ENVDIR, envflags|DB_RECOVER, S_IRWXU+S_IRWXG+S_IRWXO));

    CHK(db_create(&db, env, 0));
    CHK(db->open(db, NULL, "foo.db", NULL, DB_BTREE, DB_AUTO_COMMIT, 0666));
    assert_desc_four(db);
    CHK(db->close(db, 0));

    CHK(db_create(&db2, env, 0));
    CHK(db2->open(db2, NULL, "foo2.db", NULL, DB_BTREE, DB_AUTO_COMMIT, 0666));
    assert_desc_eight(db2);
    CHK(db2->close(db2, 0));

    CHK(db_create(&db3, env, 0));
    CHK(db3->open(db3, NULL, "foo3.db", NULL, DB_BTREE, DB_AUTO_COMMIT, 0666));
    assert_desc_eight(db3);
    CHK(db3->close(db3, 0));

    CHK(env->close(env, 0));
}

static int usage(void)
{
    return 1;
}

int test_main(int argc, char * const argv[])
{
    BOOL do_test = FALSE;
    BOOL do_recover = FALSE;

    for (int i = 1; i < argc; i++) {
        char * const arg = argv[i];
        if (strcmp(arg, "-v") == 0) {
            verbose++;
            continue;
        }
        if (strcmp(arg, "-q") == 0) {
            verbose--;
            if (verbose < 0)
                verbose = 0;
            continue;
        }
        if (strcmp(arg, "--test") == 0) {
            do_test = TRUE;
            continue;
        }
        if (strcmp(arg, "--recover") == 0) {
            do_recover = TRUE;
            continue;
        }
        if (strcmp(arg, "--help") == 0) {
            return usage();
        }
    }

    if (do_test) {
        run_test();
    }
    if (do_recover) {
        run_recover();
    }

    return 0;
}
