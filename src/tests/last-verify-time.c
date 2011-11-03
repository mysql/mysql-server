#include "test.h"

// test for the last verify time

static void
test_verify_time_after_create(DB_ENV *env) {
    int r;

    DB *db = NULL;
    r = db_create(&db, env, 0); assert_zero(r);

    r = db->open(db, NULL, "test.tdb", NULL, DB_BTREE, DB_AUTO_COMMIT+DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); assert_zero(r);

    DB_BTREE_STAT64 stats;
    r = db->stat64(db, NULL, &stats); assert_zero(r);
    assert(stats.bt_verify_time_sec == 0);

    r = db->close(db, 0); assert_zero(r);
}

static void
test_verify_time_after_open(DB_ENV *env) {
    int r;

    DB *db = NULL;
    r = db_create(&db, env, 0); assert_zero(r);

    r = db->open(db, NULL, "test.tdb", NULL, DB_BTREE, DB_AUTO_COMMIT+DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); assert_zero(r);

    DB_BTREE_STAT64 stats;
    r = db->stat64(db, NULL, &stats); assert_zero(r);
    assert(stats.bt_verify_time_sec == 0);

    r = db->close(db, 0); assert_zero(r);
}

static void
test_verify_time_after_check(DB_ENV *env) {
    int r;

    DB *db = NULL;
    r = db_create(&db, env, 0); assert_zero(r);

    r = db->open(db, NULL, "test.tdb", NULL, DB_BTREE, DB_AUTO_COMMIT+DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); assert_zero(r);

    DB_BTREE_STAT64 stats;
    r = db->stat64(db, NULL, &stats); assert_zero(r);
    assert(stats.bt_verify_time_sec == 0);

    r = db->verify_with_progress(db, NULL, NULL, 0, 0); assert_zero(r);

    r = db->stat64(db, NULL, &stats); assert_zero(r);
    assert(stats.bt_verify_time_sec != 0);

    r = db->close(db, 0); assert_zero(r);
}

static void
test_verify_time_after_reopen(DB_ENV *env) {
    int r;

    DB *db = NULL;
    r = db_create(&db, env, 0); assert_zero(r);

    r = db->open(db, NULL, "test.tdb", NULL, DB_BTREE, DB_AUTO_COMMIT+DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); assert_zero(r);

    DB_BTREE_STAT64 stats;
    r = db->stat64(db, NULL, &stats); assert_zero(r);
    assert(stats.bt_verify_time_sec != 0);

    r = db->close(db, 0); assert_zero(r);
}

int
test_main(int argc, char * const argv[]) {
    int r;

    // parse_args(argc, argv);
    for (int i = 1; i < argc; i++) {
        char * const arg = argv[i];
        if (strcmp(arg, "-v") == 0) {
            verbose++;
            continue;
        }
        if (strcmp(arg, "-q") == 0) {
            verbose = 0;
            continue;
        }
        assert(0);
    }

    r = system("rm -rf " ENVDIR); assert_zero(r);
    r = toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO); assert_zero(r);

    DB_ENV *env = NULL;
    r = db_env_create(&env, 0); assert_zero(r);

    r = env->open(env, ENVDIR, DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN|DB_PRIVATE, S_IRWXU+S_IRWXG+S_IRWXO); assert_zero(r);

    test_verify_time_after_create(env);

    test_verify_time_after_open(env);

    test_verify_time_after_check(env);

    test_verify_time_after_reopen(env);

    r = env->close(env, 0); assert_zero(r);

    return 0;
}

