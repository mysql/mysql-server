// T(a) put 0
// T(b) put N-1
// T(a) put N-1, should wait on lock W(N-1)
// T(b) put 0, should return deadlock
// T(b) abort
// T(a) gets lock W(N-1)
// T(A) commit

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

static void insert_row(DB *db, DB_TXN *txn, int k, int v, int expect_r) {
    DBT key; dbt_init(&key, &k, sizeof k);
    DBT value; dbt_init(&value, &v, sizeof v);
    int r = db->put(db, txn, &key, &value, 0); assert(r == expect_r);
}

struct run_txn_b_arg {
    struct test_seq *test_seq;
    DB_TXN *txn_b;
    DB *db;
    int n;
};

static void *run_txn_b(void *arg) {
    struct run_txn_b_arg *b_arg = (struct run_txn_b_arg *) arg;
    struct test_seq *test_seq = b_arg->test_seq;
    DB_TXN *txn_b = b_arg->txn_b;
    DB *db = b_arg->db;
    int n = b_arg->n;

    test_seq_sleep(test_seq, 1);
    insert_row(db, txn_b, htonl(n-1), n-1, 0);
    test_seq_next_state(test_seq);

#if defined(USE_TDB)
    test_seq_sleep(test_seq, 3);
    insert_row(db, txn_b, htonl(0), 0, DB_LOCK_NOTGRANTED);
    int r = txn_b->commit(txn_b, 0); assert(r == 0);
#elif defined(USE_BDB)
    test_seq_sleep(test_seq, 2);
    insert_row(db, txn_b, htonl(0), 0, DB_LOCK_DEADLOCK);
    int r = txn_b->abort(txn_b); assert(r == 0);
#else
#error
#endif

    return arg;
}

static void simple_deadlock(DB_ENV *db_env, DB *db, int do_txn, int n) {
    int r;

    DB_TXN *txn_init = NULL;
    if (do_txn) {
        r = db_env->txn_begin(db_env, NULL, &txn_init, 0); assert(r == 0);
    }
    
    for (int k = 0; k < n; k++) {
        insert_row(db, txn_init, htonl(k), k, 0);
    }

    if (do_txn) {
        r = txn_init->commit(txn_init, 0); assert(r == 0);
    }


    DB_TXN *txn_a = NULL;
    if (do_txn) {
        r = db_env->txn_begin(db_env, NULL, &txn_a, 0); assert(r == 0);
    }

    DB_TXN *txn_b = NULL;
    if (do_txn) {
        r = db_env->txn_begin(db_env, NULL, &txn_b, 0); assert(r == 0);
    }

    struct test_seq test_seq; test_seq_init(&test_seq);

    toku_pthread_t tid;
    struct run_txn_b_arg arg = { &test_seq, txn_b, db, n};
    r = toku_pthread_create(&tid, NULL, run_txn_b, &arg);

    test_seq_sleep(&test_seq, 0);
    insert_row(db, txn_a, htonl(0), 0, 0);
    test_seq_next_state(&test_seq);

    test_seq_sleep(&test_seq, 2);
#if defined(USE_TDB)
    insert_row(db, txn_a, htonl(n-1), n-1, DB_LOCK_NOTGRANTED);
#elif defined(USE_BDB)
    insert_row(db, txn_a, htonl(n-1), n-1, 0);
#else
#error
#endif
    test_seq_next_state(&test_seq);

    void *ret = NULL;
    r = toku_pthread_join(tid, &ret); assert(r == 0);

    if (do_txn) {
        r = txn_a->commit(txn_a, 0); assert(r == 0);
    }

    test_seq_destroy(&test_seq);
}

int test_main(int argc, char * const argv[]) {
    uint64_t cachesize = 0;
    uint32_t pagesize = 0;
    int do_txn = 1;
    int nrows = 1000;
#if defined(USE_TDB)
    char *db_env_dir = "dir." __FILE__ ".tokudb";
#elif defined(USE_BDB)
    char *db_env_dir = "dir." __FILE__ ".bdb";
#else
#error
#endif
    char *db_filename = "simple_deadlock";
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
        if (strcmp(argv[i], "-n") == 0 && i+1 < argc) {
            nrows = atoi(argv[++i]);
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
#if defined(USE_BDB)
    r = db_env->set_lk_detect(db_env, DB_LOCK_YOUNGEST); assert(r == 0);
#endif

    // create the db
    DB *db = NULL;
    r = db_create(&db, db_env, 0); assert(r == 0);
    DB_TXN *create_txn = NULL;
    if (do_txn) {
        r = db_env->txn_begin(db_env, NULL, &create_txn, 0); assert(r == 0);
    }
    if (pagesize) {
        r = db->set_pagesize(db, pagesize); assert(r == 0);
    }
    r = db->open(db, create_txn, db_filename, NULL, DB_BTREE, DB_CREATE, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH); assert(r == 0);
    if (do_txn) {
        r = create_txn->commit(create_txn, 0); assert(r == 0);
    }

    // run test
    simple_deadlock(db_env, db, do_txn, nrows);

    // close env
    r = db->close(db, 0); assert(r == 0); db = NULL;
    r = db_env->close(db_env, 0); assert(r == 0); db_env = NULL;

    return 0;
}
