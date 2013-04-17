// verify that a simle write lock deadlock is detected by the BDB locker
// A write locks L
// B write locks M
// A tries to write lock M, gets DB_LOCK_NOTGRANTED
// B tries to write lock L, gets DB_LOCK_NOTGRANTED

#include "test.h"

static void simple_deadlock(DB_ENV *db_env) {
    int r;

    u_int32_t locker_a;
    r = db_env->lock_id(db_env, &locker_a); assert(r == 0);
    u_int32_t locker_b;
    r = db_env->lock_id(db_env, &locker_b); assert(r == 0);

    DBT object_l = { .data = "L", .size = 1 };
    DBT object_m = { .data = "M", .size = 1 };

    DB_LOCK lock_a_l;
    r = db_env->lock_get(db_env, locker_a, DB_LOCK_NOWAIT, &object_l, DB_LOCK_WRITE, &lock_a_l); assert(r == 0);

    DB_LOCK lock_b_m;
    r = db_env->lock_get(db_env, locker_b, DB_LOCK_NOWAIT, &object_m, DB_LOCK_WRITE, &lock_b_m); assert(r == 0);

    DB_LOCK lock_a_m;
    r = db_env->lock_get(db_env, locker_a, DB_LOCK_NOWAIT, &object_m, DB_LOCK_WRITE, &lock_a_m); assert(r == DB_LOCK_NOTGRANTED);

    DB_LOCK lock_b_l;
    r = db_env->lock_get(db_env, locker_b, DB_LOCK_NOWAIT, &object_l, DB_LOCK_WRITE, &lock_b_l); assert(r == DB_LOCK_NOTGRANTED);

    r = db_env->lock_put(db_env, &lock_a_l); assert(r == 0);
    r = db_env->lock_put(db_env, &lock_b_m); assert(r == 0);

    r = db_env->lock_id_free(db_env, locker_a); assert(r == 0);
    r = db_env->lock_id_free(db_env, locker_b); assert(r == 0);
}

int test_main(int argc, char * const argv[]) {
    uint64_t cachesize = 0;
    int do_txn = 1;
#if defined(USE_TDB)
    char *db_env_dir = "dir." __FILE__ ".tokudb";
#elif defined(USE_BDB)
    char *db_env_dir = "dir." __FILE__ ".bdb";
#else
#error
#endif
    int db_env_open_flags = DB_CREATE | DB_PRIVATE | DB_INIT_MPOOL | DB_INIT_TXN | DB_INIT_LOCK | DB_INIT_LOG | DB_THREAD;

    // parse_args(argc, argv);
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose++;
            continue;
        }
        if (strcmp(argv[i], "-q") == 0 || strcmp(argv[i], "--quiet") == 0) {
            if (verbose > 0)
                verbose--;
            continue;
        }
        assert(0);
    }

    // setup env
    int r;
    char rm_cmd[strlen(db_env_dir) + strlen("rm -rf ") + 1];
    snprintf(rm_cmd, sizeof(rm_cmd), "rm -rf %s", db_env_dir);
    r = system(rm_cmd); assert(r == 0);

    r = toku_os_mkdir(db_env_dir, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH); assert(r == 0);

    DB_ENV *db_env = NULL;
    r = db_env_create(&db_env, 0); assert(r == 0);
    if (cachesize) {
        const u_int64_t gig = 1 << 30;
        r = db_env->set_cachesize(db_env, cachesize / gig, cachesize % gig, 1); assert(r == 0);
    }
    if (!do_txn)
        db_env_open_flags &= ~(DB_INIT_TXN | DB_INIT_LOG);
    r = db_env->open(db_env, db_env_dir, db_env_open_flags, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH); assert(r == 0);
#if 0 && defined(USE_BDB)
    r = db_env->set_lk_detect(db_env, DB_LOCK_YOUNGEST); assert(r == 0);
#endif

    // run test
    simple_deadlock(db_env);

    // close env
    r = db_env->close(db_env, 0); assert(r == 0); db_env = NULL;

    return 0;
}
