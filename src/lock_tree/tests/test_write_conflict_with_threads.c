// T(A) gets W(L)
// T(B) tries W(L), gets DB_LOCK_NOTGRANTED
// T(A) releases locks
// T(B) gets W(L)
// T(B) releases locks

#include "test.h"

static int write_lock(toku_lock_tree *lt, TXNID txnid, char *k) {
    DBT key; dbt_init(&key, k, strlen(k));
    toku_lock_request lr;
    toku_lock_request_init(&lr, (DB*)1, txnid, &key, &key, LOCK_REQUEST_WRITE);
    int r = toku_lt_acquire_lock_request_with_timeout(lt, &lr, NULL);
    toku_lock_request_destroy(&lr);
    return r;
}

struct writer_arg {
    TXNID id;
    toku_lock_tree *lt;
    char *name;
};

static void *writer_thread(void *arg) {
    struct writer_arg *writer_arg = (struct writer_arg *) arg;
    printf("%lu wait\n", writer_arg->id);
    int r = write_lock(writer_arg->lt, writer_arg->id, writer_arg->name); assert(r == 0);
    printf("%lu locked\n", writer_arg->id);
    sleep(1);
    toku_lt_unlock(writer_arg->lt, writer_arg->id);
    printf("%lu unlocked\n", writer_arg->id);
    return arg;
}

int main(int argc, const char *argv[]) {
    int r;

    uint32_t max_locks = 1;
    uint64_t max_lock_memory = 4096;
    int max_threads = 1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose++;
            continue;
        }
        if (strcmp(argv[i], "-q") == 0 || strcmp(argv[i], "--quiet") == 0) {
            if (verbose > 0) verbose--;
            continue;
        }
        if (strcmp(argv[i], "--max_locks") == 0 && i+1 < argc) {
            max_locks = atoi(argv[++i]);
            continue;
        }
        if (strcmp(argv[i], "--max_lock_memory") == 0 && i+1 < argc) {
            max_lock_memory = atoi(argv[++i]);
            continue;
        }        
        if (strcmp(argv[i], "--max_threads") == 0 && i+1 < argc) {
            max_threads = atoi(argv[++i]);
            continue;
        }        
        assert(0);
    }

    // setup
    toku_ltm *ltm = NULL;
    r = toku_ltm_create(&ltm, max_locks, max_lock_memory, dbpanic, get_compare_fun_from_db, toku_malloc, toku_free, toku_realloc);
    assert(r == 0 && ltm);

    toku_lock_tree *lt = NULL;
    r = toku_lt_create(&lt, dbpanic, ltm, get_compare_fun_from_db, toku_malloc, toku_free, toku_realloc);
    assert(r == 0 && lt);

    const TXNID txn_a = 1;

    r = write_lock(lt, txn_a, "L"); assert(r == 0);
    printf("main locked\n");

    toku_pthread_t tids[max_threads];
    for (int i = 0 ; i < max_threads; i++) {
        struct writer_arg *writer_arg = (struct writer_arg *) toku_malloc(sizeof (struct writer_arg));
        *writer_arg = (struct writer_arg) { i+10, lt, "L"};
        r = toku_pthread_create(&tids[i], NULL, writer_thread, writer_arg); assert(r == 0);
    }
    sleep(10);
    r = toku_lt_unlock(lt, txn_a);  assert(r == 0);
    printf("main unlocked\n");

    for (int i = 0; i < max_threads; i++) {
        void *retarg;
        r = toku_pthread_join(tids[i], &retarg); assert(r == 0);
        toku_free(retarg);
    }

    // shutdown 
    r = toku_lt_close(lt); assert(r == 0);
    r = toku_ltm_close(ltm); assert(r == 0);

    return 0;
}
