// verify that the BDB locker can detect deadlocks on the fly and allow
// the deadlock to be unwound by the deadlocked threads.  the main thread
// polls for deadlocks with the lock_detect function.
//
// A write locks L
// B write locks M
// A tries to write lock M, gets blocked
// B tries to write lock L, gets DEADLOCK error
// B releases its lock on M
// A resumes

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

struct locker_args {
    DB_ENV *db_env;
    struct test_seq *test_seq;
};

static void *run_locker_a(void *arg) {
    struct locker_args *locker_args = (struct locker_args *) arg;
    DB_ENV *db_env = locker_args->db_env;
    struct test_seq *test_seq = locker_args->test_seq;
    int r;

    u_int32_t locker_a;
    r = db_env->lock_id(db_env, &locker_a); assert(r == 0);

    DBT object_l = { .data = "L", .size = 1 };
    DBT object_m = { .data = "M", .size = 1 };

    test_seq_sleep(test_seq, 0);
    DB_LOCK lock_a_l;
    r = db_env->lock_get(db_env, locker_a, DB_LOCK_NOWAIT, &object_l, DB_LOCK_WRITE, &lock_a_l); assert(r == 0);
    test_seq_next_state(test_seq);

    test_seq_sleep(test_seq, 2);
    DB_LOCK lock_a_m;
    r = db_env->lock_get(db_env, locker_a, 0, &object_m, DB_LOCK_WRITE, &lock_a_m); assert(r == 0);

    r = db_env->lock_put(db_env, &lock_a_l); assert(r == 0);

    r = db_env->lock_put(db_env, &lock_a_m); assert(r == 0);

    r = db_env->lock_id_free(db_env, locker_a); assert(r == 0);

    return arg;
}

static void *run_locker_b(void *arg) {
    struct locker_args *locker_args = (struct locker_args *) arg;
    DB_ENV *db_env = locker_args->db_env;
    struct test_seq *test_seq = locker_args->test_seq;
    int r;

    u_int32_t locker_b;
    r = db_env->lock_id(db_env, &locker_b); assert(r == 0);

    DBT object_l = { .data = "L", .size = 1 };
    DBT object_m = { .data = "M", .size = 1 };

    test_seq_sleep(test_seq, 1);
    DB_LOCK lock_b_m;
    r = db_env->lock_get(db_env, locker_b, DB_LOCK_NOWAIT, &object_m, DB_LOCK_WRITE, &lock_b_m); assert(r == 0);
    test_seq_next_state(test_seq);

    test_seq_sleep(test_seq, 2);
    DB_LOCK lock_b_l;
    r = db_env->lock_get(db_env, locker_b, 0, &object_l, DB_LOCK_WRITE, &lock_b_l); assert(r == DB_LOCK_DEADLOCK);

    r = db_env->lock_put(db_env, &lock_b_m); assert(r == 0);

    r = db_env->lock_id_free(db_env, locker_b); assert(r == 0);

    return arg;
}

static void simple_deadlock(DB_ENV *db_env) {
    int r;

    struct test_seq test_seq; test_seq_init(&test_seq);

    toku_pthread_t tid_a;
    struct locker_args args_a = { db_env, &test_seq };
    r = toku_pthread_create(&tid_a, NULL, run_locker_a, &args_a); assert(r == 0);

    toku_pthread_t tid_b;
    struct locker_args args_b = { db_env, &test_seq };
    r = toku_pthread_create(&tid_b, NULL, run_locker_b, &args_b); assert(r == 0);

    while (1) {
        sleep(10);
        int rejected = 0;
        r = db_env->lock_detect(db_env, 0, DB_LOCK_YOUNGEST, &rejected); assert(r == 0);
        if (verbose)
            printf("%s %d\n", __FUNCTION__, rejected);
        if (rejected == 0)
            break;
    }

    void *ret = NULL;
    r = toku_pthread_join(tid_a, &ret); assert(r == 0);
    r = toku_pthread_join(tid_b, &ret); assert(r == 0);

    test_seq_destroy(&test_seq);
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

    // run test
    simple_deadlock(db_env);

    // close env
    r = db_env->close(db_env, 0); assert(r == 0); db_env = NULL;

    return 0;
}
