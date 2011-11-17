// verify that point write locks are exclusive for multiple threads. 
//
// run multiple transactionss with conflicting locks and verify that any lock that is granted is
// not owned by some other transaction

#include "test.h"

// my lock tree is used to verify the state write locks granted by the lock tree
struct my_locktree {
    TXNID txn;
    int count;
} my_locktree[1000];
pthread_mutex_t my_locktree_mutex = PTHREAD_MUTEX_INITIALIZER;

static void get_lock(int k, TXNID txn) {
    pthread_mutex_lock(&my_locktree_mutex);
    if (my_locktree[k].txn == 0)
        assert(my_locktree[k].count == 0);
    else
        assert(my_locktree[k].txn == txn && my_locktree[k].count > 0);
    my_locktree[k].txn = txn;
    my_locktree[k].count++;
    pthread_mutex_unlock(&my_locktree_mutex);
}

static void release_lock(int k, TXNID txn) {
    assert(my_locktree[k].txn == txn && my_locktree[k].count > 0);
    my_locktree[k].count--;
    if (my_locktree[k].count == 0)
        my_locktree[k].txn = 0;
}

static void release_locks(uint64_t keys[], int n, TXNID txn) {
    pthread_mutex_lock(&my_locktree_mutex);
    for (int i = 0; i < n; i++)
        release_lock(keys[i], txn);
    pthread_mutex_unlock(&my_locktree_mutex);
}

struct test_arg {
    DB *db;
    TXNID txn;
    toku_ltm *ltm;
    toku_lock_tree *lt;
    uint64_t locks_per_txn;
    uint64_t nrows;
    uint64_t iterations;
};

static void runtest(DB *db, TXNID txn, toku_ltm *ltm, toku_lock_tree *lt, uint64_t locks_per_txn, uint64_t nrows, uint64_t iterations) {
    int r;

    uint64_t notgranted = 0, deadlocked = 0;
    for (uint64_t iter = 0; iter < iterations; iter++) {
        uint64_t keys[locks_per_txn];
        for (uint64_t i = 0; i < locks_per_txn; i++)
            keys[i] = random() % nrows;
        uint64_t i;
        for (i = 0; i < locks_per_txn; i++) {
            DBT key = { .data = &keys[i], .size = sizeof keys[i] };
            toku_lock_request lr;
            toku_lock_request_init(&lr, db, txn, &key, &key, LOCK_REQUEST_WRITE);
            toku_ltm_lock_mutex(ltm);
            r = toku_lt_acquire_lock_request_with_default_timeout_locked(lt, &lr);
            toku_ltm_unlock_mutex(ltm);
            if (r == 0) {
                get_lock(keys[i], txn);
                continue;
            }
            else if (r == DB_LOCK_NOTGRANTED) {
                notgranted++;
                break;
            } else if (r == DB_LOCK_DEADLOCK) {
                deadlocked++;
                break;
            } else
                assert(0);
        }

        // usleep(random() % 1000);
        release_locks(keys, i, txn);

        toku_ltm_lock_mutex(ltm);
        r = toku_lt_unlock(lt, txn);  assert(r == 0);
        toku_ltm_unlock_mutex(ltm);

        if ((iter % 10000) == 0)
            printf("%lu %lu %lu\n", (long unsigned) iter, (long unsigned) notgranted, (long unsigned) deadlocked);
    }
}

static void *runtest_wrapper(void *arg_wrapper) {
    struct test_arg *arg = (struct test_arg *) arg_wrapper;
    runtest(arg->db, arg->txn, arg->ltm, arg->lt, arg->locks_per_txn, arg->nrows, arg->iterations);
    return arg;
}

int main(int argc, const char *argv[]) {
    int r;

    uint64_t nthreads = 2;
    uint64_t locks_per_txn = 10;
    uint64_t nrows = 1000;
    uint32_t max_locks = nthreads * locks_per_txn;
    uint64_t max_lock_memory = 4096;
    uint64_t iterations = 1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose++;
            continue;
        }
        if (strcmp(argv[i], "-q") == 0 || strcmp(argv[i], "--quiet") == 0) {
            if (verbose > 0) verbose--;
            continue;
        }
        if (strcmp(argv[i], "--nthreads") == 0 && i+1 < argc) {
            nthreads = atoll(argv[++i]);
            continue;
        }
        if (strcmp(argv[i], "--locks_per_txn") == 0 && i+1 < argc) {
            locks_per_txn = atoll(argv[++i]);
            continue;
        }
        if (strcmp(argv[i], "--nrows") == 0 && i+1 < argc) {
            nrows = atoll(argv[++i]);
            continue;
        }
        if (strcmp(argv[i], "--iterations") == 0 && i+1 < argc) {
            iterations = atoll(argv[++i]);
            continue;
        }
        if (strcmp(argv[i], "--max_locks") == 0 && i+1 < argc) {
            max_locks = atoi(argv[++i]);
            continue;
        }
        if (strcmp(argv[i], "--max_lock_memory") == 0 && i+1 < argc) {
            max_lock_memory = atoll(argv[++i]);
            continue;
        }        
        assert(0);
    }

    // setup
    toku_ltm *ltm = NULL;
    r = toku_ltm_create(&ltm, max_locks, max_lock_memory, dbpanic, get_compare_fun_from_db);
    assert(r == 0 && ltm);

    toku_lock_tree *lt = NULL;
    r = toku_lt_create(&lt, dbpanic, ltm, get_compare_fun_from_db);
    assert(r == 0 && lt);

    DB *fake_db = (DB *) 1;

    toku_pthread_t tids[nthreads];
    struct test_arg args[nthreads];
    for (uint64_t i = 1; i < nthreads; i++) {
        args[i] = (struct test_arg) { fake_db, (TXNID) i, ltm, lt, locks_per_txn, nrows, iterations };
        toku_pthread_create(&tids[i], NULL, runtest_wrapper, &args[i]);
    }

    runtest(fake_db, (TXNID)nthreads, ltm, lt, locks_per_txn, nrows, iterations);
    
    for (uint64_t i = 1; i < nthreads; i++) {
        void *retptr;
        toku_pthread_join(tids[i], &retptr);
    }

    // shutdown 
    r = toku_lt_close(lt); assert(r == 0);
    r = toku_ltm_close(ltm); assert(r == 0);

    return 0;
}
