// verify that blocking lock waits eventually time out if the lock owner never releases the lock.

// A begin txn
// A write locks 0
// A sleeps
// B begin txn
// B tries to write lock 0, blocks
// B's write lock times out, B aborts its txn
// A wakes up and commits its txn

#include "test.h"
#include "toku_pthread.h"

struct test_seq {
    int state;
    toku_pthread_mutex_t lock;
    toku_pthread_cond_t cv;
};

static void test_seq_init(struct test_seq *seq) {
    seq->state = 0;
    int r;
    r = toku_pthread_mutex_init(&seq->lock, NULL); assert(r == 0);
    r = toku_pthread_cond_init(&seq->cv, NULL); assert(r == 0);
}

static void test_seq_destroy(struct test_seq *seq) {
    int r;
    r = toku_pthread_mutex_destroy(&seq->lock); assert(r == 0);
    r = toku_pthread_cond_destroy(&seq->cv); assert(r == 0);
}

static void test_seq_sleep(struct test_seq *seq, int new_state) {
    int r;
    r = toku_pthread_mutex_lock(&seq->lock); assert(r == 0);
    while (seq->state != new_state) {
        r = toku_pthread_cond_wait(&seq->cv, &seq->lock); assert(r == 0);
    }
    r = toku_pthread_mutex_unlock(&seq->lock); assert(r == 0);
}

static void test_seq_next_state(struct test_seq *seq) {
    int r;
    r = toku_pthread_mutex_lock(&seq->lock);
    seq->state++;
    r = toku_pthread_cond_broadcast(&seq->cv); assert(r == 0);
    r = toku_pthread_mutex_unlock(&seq->lock); assert(r == 0);
}

static void t_a(DB_ENV *db_env, DB *db, struct test_seq *seq) {
    int r;
    test_seq_sleep(seq, 0);
    int k = 0;
    DB_TXN *txn_a = NULL;
    r = db_env->txn_begin(db_env, NULL, &txn_a, 0); assert(r == 0);
    DBT key = { .data = &k, .size = sizeof k };
    DBT val = { .data = &k, .size = sizeof k };
    r = db->put(db, txn_a, &key, &val, 0); assert(r == 0);
    test_seq_next_state(seq);
    sleep(10);
    r = txn_a->commit(txn_a, 0); assert(r == 0);
}

static void t_b(DB_ENV *db_env, DB *db, struct test_seq *seq) {
    int r;
    test_seq_sleep(seq, 1);
    int k = 0;
    DB_TXN *txn_b = NULL;
    r = db_env->txn_begin(db_env, NULL, &txn_b, 0); assert(r == 0);
    DBT key = { .data = &k, .size = sizeof k };
    DBT val = { .data = &k, .size = sizeof k };
    r = db->put(db, txn_b, &key, &val, 0); 
#if USE_BDB
    assert(r == DB_LOCK_DEADLOCK);
#else
    assert(r == DB_LOCK_NOTGRANTED);
#endif
    r = txn_b->abort(txn_b); assert(r == 0);
}

struct t_a_args {
    DB_ENV *env;
    DB *db;
    struct test_seq *seq;
};

static void *t_a_thread(void *arg) {
    struct t_a_args *a = (struct t_a_args *) arg;
    t_a(a->env, a->db, a->seq);
    return arg;
}

int test_main(int argc, char * const argv[]) {
    uint64_t cachesize = 0;
    uint32_t pagesize = 0;
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
#if USE_BDB
    db_timeout_t t;
    r = db_env->get_timeout(db_env, &t, DB_SET_LOCK_TIMEOUT); assert(r == 0);
    if (verbose) printf("lock %d\n", t);
    r = db_env->get_timeout(db_env, &t, DB_SET_TXN_TIMEOUT); assert(r == 0);
    if (verbose) printf("txn %d\n", t);

    r = db_env->set_timeout(db_env, 5, DB_SET_LOCK_TIMEOUT); assert(r == 0);
    r = db_env->set_timeout(db_env, 5, DB_SET_TXN_TIMEOUT); assert(r == 0);

    r = db_env->get_timeout(db_env, &t, DB_SET_LOCK_TIMEOUT); assert(r == 0);
    if (verbose) printf("lock %d\n", t);
    r = db_env->get_timeout(db_env, &t, DB_SET_TXN_TIMEOUT); assert(r == 0);
    if (verbose) printf("txn %d\n", t);
#endif
#if USE_TDB
    uint64_t lock_timeout_msec;
    r = db_env->get_lock_timeout(db_env, &lock_timeout_msec); assert(r == 0);
    if (verbose) printf("lock timeout: %" PRIu64 "\n", lock_timeout_msec);
    r = db_env->set_lock_timeout(db_env, 5000); assert(r == 0);
    r = db_env->get_lock_timeout(db_env, &lock_timeout_msec); assert(r == 0);
    if (verbose) printf("lock timeout: %" PRIu64 "\n", lock_timeout_msec);
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

    // run test
    struct test_seq seq; test_seq_init(&seq);
    toku_pthread_t t_a_id;
    struct t_a_args t_a_args = { db_env, db, &seq };
    r = toku_pthread_create(&t_a_id, NULL, t_a_thread, &t_a_args); assert(r == 0);
    t_b(db_env, db, &seq);
    void *ret;
    r = toku_pthread_join(t_a_id, &ret); assert(r == 0);
    test_seq_destroy(&seq);

    // close env
    r = db->close(db, 0); assert(r == 0); db = NULL;
    r = db_env->close(db_env, 0); assert(r == 0); db_env = NULL;

    return 0;
}
