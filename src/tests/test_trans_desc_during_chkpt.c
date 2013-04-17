// test that an update calls back into the update function

#include "test.h"

const int envflags = DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN|DB_PRIVATE;

DB_ENV *env;

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

static void checkpoint_callback_1(void * extra) {
    assert(extra == NULL);
    DB* db = NULL;

    DBT change_descriptor;
    memset(&change_descriptor, 0, sizeof(change_descriptor));
    change_descriptor.size = sizeof(eight_byte_desc);
    change_descriptor.data = &eight_byte_desc;

    CHK(db_create(&db, env, 0));
    CHK(db->open(db, NULL, "foo.db", NULL, DB_BTREE, 0, 0666));
    assert_desc_four(db);
    IN_TXN_COMMIT(env, NULL, txn_change, 0, {
            CHK(db->change_descriptor(db, txn_change, &change_descriptor, 0));
            assert_desc_eight(db);
        });
    assert_desc_eight(db);
    CHK(db->close(db,0));
}

static void setup (void) {
    CHK(system("rm -rf " ENVDIR));
    CHK(toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO));
    CHK(db_env_create(&env, 0));
    db_env_set_checkpoint_callback(checkpoint_callback_1, NULL);
    env->set_errfile(env, stderr);
    CHK(env->open(env, ENVDIR, envflags, S_IRWXU+S_IRWXG+S_IRWXO));
}

static void cleanup (void) {
    CHK(env->close(env, 0));
}

static void run_test(void) {
    DB* db = NULL;
    
    DBT orig_desc;
    memset(&orig_desc, 0, sizeof(orig_desc));
    orig_desc.size = sizeof(four_byte_desc);
    orig_desc.data = &four_byte_desc;
    // verify we can only set a descriptor with version 1
    IN_TXN_COMMIT(env, NULL, txn_create, 0, {
            CHK(db_create(&db, env, 0));
            assert(db->descriptor == NULL);
            CHK(db->open(db, txn_create, "foo.db", NULL, DB_BTREE, DB_CREATE, 0666));
            CHK(db->change_descriptor(db, txn_create, &orig_desc, 0));
            assert_desc_four(db);
        });
    assert_desc_four(db);
    CHK(db->close(db,0));

    CHK(db_create(&db, env, 0));
    CHK(db->open(db, NULL, "foo.db", NULL, DB_BTREE, 0, 0666));    
    assert_desc_four(db);
    CHK(db->close(db,0));

    CHK(env->txn_checkpoint(env, 0, 0, 0));

    CHK(db_create(&db, env, 0));
    CHK(db->open(db, NULL, "foo.db", NULL, DB_BTREE, 0, 0666));    
    assert_desc_eight(db);
    CHK(db->close(db,0));

    db = NULL;
}

int test_main (int argc, char * const argv[]) {
    parse_args(argc, argv);
    setup();
    run_test();
    cleanup();
    return 0;
}
