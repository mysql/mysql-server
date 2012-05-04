// verify that txn's can release locks after the lock tree is closed

// A gets W(L)
// close lock tree
// A unlocks

#include "test.h"

int main(int argc, const char *argv[]) {
    int r;

    uint32_t max_locks = 2;
    uint64_t max_lock_memory = 4096;

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
        assert(0);
    }

    // setup
    toku_ltm *ltm = NULL;
    r = toku_ltm_create(&ltm, max_locks, max_lock_memory, dbpanic);
    assert(r == 0 && ltm);

    toku_lock_tree *lt = NULL;
    r = toku_ltm_get_lt(ltm, &lt, (DICTIONARY_ID){1}, NULL, dbcmp);
    assert(r == 0 && lt);

    // add a lock for a transaction
    const TXNID txn_a = 1;
    DBT key_l; dbt_init(&key_l, "L", 1);
    toku_lock_request a_w_l; toku_lock_request_init(&a_w_l, txn_a, &key_l, &key_l, LOCK_REQUEST_WRITE);
    r = toku_lock_request_start(&a_w_l, lt, false); assert(r == 0); 
    assert(a_w_l.state == LOCK_REQUEST_COMPLETE && a_w_l.complete_r == 0);
    toku_lock_request_destroy(&a_w_l);
    
    // add a reference to the lock tree for the transaction
    toku_lt_add_ref(lt);

    // start closing the lock tree
    toku_lt_remove_db_ref(lt);

    // release all locks for the transaction
    r = toku_lt_unlock_txn(lt, txn_a);  assert(r == 0);
    toku_lt_remove_ref(lt);

    // shutdown 
    r = toku_ltm_close(ltm); assert(r == 0);

    return 0;
}
