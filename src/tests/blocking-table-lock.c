// verify that table locks used by multiple transactions suspend the conflicting thread rather than just return DB_LOCK_NOTGRANTED.

#include "test.h"
#include "toku_pthread.h"

static void blocking_table_lock(DB_ENV *db_env, DB *db, uint64_t nrows, long sleeptime) {
    int r;

    for (uint64_t i = 0; i < nrows; i++) {
        DB_TXN *txn = NULL;
        r = db_env->txn_begin(db_env, NULL, &txn, 0); assert(r == 0);

        r = db->pre_acquire_table_lock(db, txn); assert(r == 0);

        usleep(sleeptime);

        r = txn->commit(txn, 0); assert(r == 0);
        if (verbose)
            printf("%lu %lu\n", toku_pthread_self(), i);
    }
}

struct blocking_table_lock_args {
    DB_ENV *db_env;
    DB *db;
    uint64_t nrows;
    long sleeptime;
};

static void *blocking_table_lock_thread(void *arg) {
    struct blocking_table_lock_args *a = (struct blocking_table_lock_args *) arg;
    blocking_table_lock(a->db_env, a->db, a->nrows, a->sleeptime);
    return arg;
}

int test_main(int argc, char * const argv[]) {
    uint64_t cachesize = 0;
    uint32_t pagesize = 0;
    uint64_t nrows = 100;
    int nthreads = 2;
    long sleeptime = 100000;
#if defined(USE_TDB)
    char *db_env_dir = "dir." __FILE__ ".tokudb";
#elif defined(USE_BDB)
    char *db_env_dir = "dir." __FILE__ ".bdb";
#else
#error
#endif
    char *db_filename = "test.db";
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
        if (strcmp(argv[i], "--nrows") == 0 && i+1 < argc) {
            nrows = atoll(argv[++i]);
            continue;
        }
        if (strcmp(argv[i], "--nthreads") == 0 && i+1 < argc) {
            nthreads = atoi(argv[++i]);
            continue;
        }
        if (strcmp(argv[i], "--sleeptime") == 0 && i+1 < argc) {
            sleeptime = atol(argv[++i]);
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
    r = db_env->open(db_env, db_env_dir, db_env_open_flags, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH); assert(r == 0);
#if TOKUDB
    r = db_env->set_lock_timeout(db_env, 30 * 1000000); assert(r == 0);
#endif

    // create the db
    DB *db = NULL;
    r = db_create(&db, db_env, 0); assert(r == 0);
    DB_TXN *create_txn = NULL;
    r = db_env->txn_begin(db_env, NULL, &create_txn, 0); assert(r == 0);
    if (pagesize) {
        r = db->set_pagesize(db, pagesize); assert(r == 0);
    }
    r = db->open(db, create_txn, db_filename, NULL, DB_BTREE, DB_CREATE, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH); assert(r == 0);
    r = create_txn->commit(create_txn, 0); assert(r == 0);

    toku_pthread_t tids[nthreads];
    for (int i = 0; i < nthreads-1; i++) {
        struct blocking_table_lock_args a = { db_env, db, nrows, sleeptime };
        r = toku_pthread_create(&tids[i], NULL, blocking_table_lock_thread, &a); assert(r == 0);
    }
    blocking_table_lock(db_env, db, nrows, sleeptime);
    for (int i = 0; i < nthreads-1; i++) {
        void *ret;
        r = toku_pthread_join(tids[i], &ret); assert(r == 0);
    }

    // close env
    r = db->close(db, 0); assert(r == 0); db = NULL;
    r = db_env->close(db_env, 0); assert(r == 0); db_env = NULL;

    return 0;
}
