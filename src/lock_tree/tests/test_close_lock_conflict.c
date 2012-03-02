// verify that the lock tree is maintained after closes if txn's still own locks

// A gets W(L)
// B gets W(M)
// close lock tree
// A unlocks
// reopen lock tree
// B gets W(L)
// B unlocks

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

    DB *fake_db = (DB *) 1;

    toku_lock_tree *lt = NULL;
    r = toku_ltm_get_lt(ltm, &lt, (DICTIONARY_ID){1}, fake_db, dbcmp);
    assert(r == 0 && lt);

    const TXNID txn_a = 1;
    const TXNID txn_b = 2;

    DBT key_l = { .data = "L", .size = 1 };
    DBT key_m = { .data = "M", .size = 1 };

    // txn_a gets W(L)
    toku_lock_request a_w_l; toku_lock_request_init(&a_w_l, fake_db, txn_a, &key_l, &key_l, LOCK_REQUEST_WRITE);
    r = toku_lock_request_start(&a_w_l, lt, false); assert(r == 0); 
    assert(a_w_l.state == LOCK_REQUEST_COMPLETE && a_w_l.complete_r == 0);
    toku_lock_request_destroy(&a_w_l);
    toku_lt_add_ref(lt);

    // txn_b gets W(M)
    toku_lock_request b_w_m; toku_lock_request_init(&b_w_m, fake_db, txn_b, &key_m, &key_m, LOCK_REQUEST_WRITE);
    r = toku_lock_request_start(&b_w_m, lt, false); assert(r == 0); 
    assert(b_w_m.state == LOCK_REQUEST_COMPLETE && b_w_m.complete_r == 0);
    toku_lock_request_destroy(&b_w_m);
    toku_lt_add_ref(lt);

    // start closing the lock tree
    toku_lt_remove_db_ref(lt, fake_db);

    // txn_a unlocks
    r = toku_lt_unlock_txn(lt, txn_a);
    toku_lt_remove_ref(lt);

    // reopen the lock tree
    r = toku_ltm_get_lt(ltm, &lt, (DICTIONARY_ID){1}, fake_db, dbcmp);
    assert(r == 0 && lt);

    // txn_b gets W(L)
    toku_lock_request b_w_l; toku_lock_request_init(&b_w_l, fake_db, txn_b, &key_l, &key_l, LOCK_REQUEST_WRITE);
    r = toku_lock_request_start(&b_w_l, lt, false); assert(r == 0);
    assert(b_w_l.state == LOCK_REQUEST_COMPLETE && b_w_l.complete_r == 0);
    toku_lock_request_destroy(&b_w_l);
    toku_lt_add_ref(lt);

    // release all locks for the transaction
    r = toku_lt_unlock_txn(lt, txn_b);  assert(r == 0);
    toku_lt_remove_ref(lt);

    // shutdown 
    r = toku_ltm_close(ltm); assert(r == 0);

    return 0;
}
