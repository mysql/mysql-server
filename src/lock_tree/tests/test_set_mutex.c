// verify that a user supplied mutex works
// T(A) gets W(L)
// T(B) tries W(L), gets lock request blocked
// T(B) lock request W(L) times out
// T(A) releases locks
// T(B) releases locks

#include "test.h"

int 
main(int argc, const char *argv[]) {
    int r;

    uint32_t max_locks = 2;
    uint64_t max_lock_memory = 4096;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            if (verbose > 0) verbose++;
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
        assert(0);
    }

    // setup
    toku_ltm *ltm = NULL;
    r = toku_ltm_create(&ltm, max_locks, max_lock_memory, dbpanic, get_compare_fun_from_db, toku_malloc, toku_free, toku_realloc);
    assert(r == 0 && ltm);

    toku_ltm_set_lock_wait_time(ltm, 5000000);
    
    toku_pthread_mutex_t my_mutex = TOKU_PTHREAD_MUTEX_INITIALIZER;
    toku_ltm_set_mutex(ltm, &my_mutex);

    toku_lock_tree *lt = NULL;
    r = toku_lt_create(&lt, dbpanic, ltm, get_compare_fun_from_db, toku_malloc, toku_free, toku_realloc);
    assert(r == 0 && lt);

    const TXNID txn_a = 1;
    const TXNID txn_b = 2;

    DBT key_l; dbt_init(&key_l, "L", 1);
    toku_lock_request a_w_l; toku_lock_request_init(&a_w_l, (DB *)1, txn_a, &key_l, &key_l, LOCK_REQUEST_WRITE);
    toku_ltm_lock_mutex(ltm);
    r = toku_lock_request_start_locked(&a_w_l, lt, false); assert(r == 0); 
    toku_ltm_unlock_mutex(ltm);
    assert(a_w_l.state == LOCK_REQUEST_COMPLETE && a_w_l.complete_r == 0);

    toku_lock_request b_w_l; toku_lock_request_init(&b_w_l, (DB *)1, txn_b, &key_l, &key_l, LOCK_REQUEST_WRITE);
    toku_ltm_lock_mutex(ltm);
    r = toku_lock_request_start_locked(&b_w_l, lt, false); assert(r != 0); 
    toku_ltm_unlock_mutex(ltm);
    assert(b_w_l.state == LOCK_REQUEST_PENDING);

    toku_ltm_lock_mutex(ltm);
    r = toku_lock_request_wait_with_default_timeout(&b_w_l, lt);
    toku_ltm_unlock_mutex(ltm);
    assert(r == DB_LOCK_NOTGRANTED);
    assert(b_w_l.state == LOCK_REQUEST_COMPLETE);

    toku_lock_request_destroy(&a_w_l);
    toku_lock_request_destroy(&b_w_l);

    r = toku_lt_unlock(lt, txn_a); assert(r == 0);
    r = toku_lt_unlock(lt, txn_b);  assert(r == 0);

    // shutdown 
    r = toku_lt_close(lt); assert(r == 0);
    r = toku_ltm_close(ltm); assert(r == 0);

    return 0;
}
