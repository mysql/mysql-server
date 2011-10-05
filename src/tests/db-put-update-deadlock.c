// for all i: T(i) reads 0, gets a read lock on 0
// for all i: T(i) writes 0, enters a deadlock
// run deadlock detector until forward progress is possible

#include "test.h"
#include "toku_pthread.h"

static void write_row(DB *db, DB_TXN *txn, int k, int v, int expect_r) {
    DBT key; dbt_init(&key, &k, sizeof k);
    DBT value; dbt_init(&value, &v, sizeof v);
    int r = db->put(db, txn, &key, &value, 0); assert(r == expect_r);
}

static void read_row(DB *db, DB_TXN *txn, int k, int expect_r) {
    DBT key; dbt_init(&key, &k, sizeof k);
    DBT value; dbt_init_malloc(&value);
    int r = db->get(db, txn, &key, &value, 0); assert(r == expect_r);
    toku_free(value.data);
}

static volatile int n_txns;

struct write_one_arg {
    DB_TXN *txn;
    DB *db;
    int k;
    int v;
};

static void *write_one_f(void *arg) {
    struct write_one_arg *f_arg = (struct write_one_arg *) arg;
    DB_TXN *txn = f_arg->txn;
    DB *db = f_arg->db;
    int k = f_arg->k;
    int v = f_arg->v;

    DBT key; dbt_init(&key, &k, sizeof k);
    DBT value; dbt_init(&value, &v, sizeof v);
    int r = db->put(db, txn, &key, &value, 0);
    if (verbose)
        printf("%s %p %d\n", __FUNCTION__, arg, r);
    assert(r == 0 || r == DB_LOCK_DEADLOCK);
    if (r == 0) {
        r = txn->commit(txn, 0); assert(r == 0);
    } else {
        r = txn->abort(txn); assert(r == 0);
    }
    (void) __sync_fetch_and_sub(&n_txns, 1);

    return arg;
}

static void update_deadlock(DB_ENV *db_env, DB *db, int do_txn, int nrows, int ntxns, int poll_deadlock UU()) {
    int r;

    // populate the initial tree
    DB_TXN *txn_init = NULL;
    if (do_txn) {
        r = db_env->txn_begin(db_env, NULL, &txn_init, 0); assert(r == 0);
    }
    for (int k = 0; k < nrows; k++) {
        write_row(db, txn_init, htonl(k), k, 0);
    }
    if (do_txn) {
        r = txn_init->commit(txn_init, 0); assert(r == 0);
    }

    // create the transactions
    n_txns = ntxns;
    DB_TXN *txns[ntxns];
    for (int i = 0; i < ntxns; i++) {
        txns[i] = NULL;
        if (do_txn) {
            r = db_env->txn_begin(db_env, NULL, &txns[i], 0); assert(r == 0);
        }
    }

    // get read locks
    for (int i = 0; i < ntxns; i++) {
        read_row(db, txns[i], htonl(0), 0);
    }

    // get write locks
    toku_pthread_t tids[ntxns];
    for (int i = 0 ; i < ntxns; i++) {
        struct write_one_arg *arg = toku_malloc(sizeof (struct write_one_arg));
        *arg = (struct write_one_arg) { txns[i], db, htonl(0), 0};
        r = toku_pthread_create(&tids[i], NULL, write_one_f, arg);
    }

#if defined(USE_BDB)
    // check for deadlocks
    if (poll_deadlock) {
        while (n_txns > 0) {
            sleep(10);
            int rejected = 0;
            r = db_env->lock_detect(db_env, 0, DB_LOCK_YOUNGEST, &rejected); assert(r == 0);
            printf("%s rejected %d\n", __FUNCTION__, rejected);
        }
    }
#endif

    // cleanup
    for (int i = 0; i < ntxns; i++) {
        void *ret = NULL;
        r = toku_pthread_join(tids[i], &ret); assert(r == 0); toku_free(ret);
    }
}

int test_main(int argc, char * const argv[]) {
    uint64_t cachesize = 0;
    uint32_t pagesize = 0;
    int do_txn = 1;
    int nrows = 1000;
    int ntxns = 2;
    int poll_deadlock = 0;
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
        if (strcmp(argv[i], "--nrows") == 0 && i+1 < argc) {
            nrows = atoi(argv[++i]);
            continue;
        }
        if (strcmp(argv[i], "--ntxns") == 0 && i+1 < argc) {
            ntxns = atoi(argv[++i]);
            continue;
        }
        if (strcmp(argv[i], "--poll") == 0) {
            poll_deadlock = 1;
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
#if defined(TOKUDB)
    r = db_env->set_lock_timeout(db_env, 30 * 1000000); assert(r == 0);
#endif
#if defined(USE_BDB)
    if (!poll_deadlock) {
        r = db_env->set_lk_detect(db_env, DB_LOCK_YOUNGEST); assert(r == 0);
    }
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
    update_deadlock(db_env, db, do_txn, nrows, ntxns, poll_deadlock);

    // close env
    r = db->close(db, 0); assert(r == 0); db = NULL;
    r = db_env->close(db_env, 0); assert(r == 0); db_env = NULL;

    return 0;
}
