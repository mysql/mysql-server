/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
// T(A) gets R(TABLE)
// T(B) gets R(L)
// T(C) trys W(L) blocked
// T(C) gets conflicts { A, B }
// T(A) releases locks
// T(C) gets conflicts { B }
// T(B) releases locks
// T(C) gets W(L)

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

    DBT key_l; dbt_init(&key_l, "L", 1);

    const TXNID txn_a = 1;
    toku_lock_request a_r_t; toku_lock_request_init(&a_r_t, txn_a, toku_lt_neg_infinity, toku_lt_infinity, LOCK_REQUEST_READ);

    do_request_and_succeed(lt, &a_r_t);

    const TXNID txn_b = 2;
    READ_REQUEST(b, l);
    do_request_and_succeed(lt, &b_r_l);

    const TXNID txn_c = 3;
    WRITE_REQUEST(c, l);
    do_request_that_blocks(lt, &c_w_l, 2, (TXNID[]){ txn_a, txn_b });

    r = toku_lt_unlock_txn(lt, txn_a); assert(r == 0);
    request_still_blocked(lt, &c_w_l, 1, (TXNID[]){ txn_b });

    r = toku_lt_unlock_txn(lt, txn_b); assert(r == 0);

    verify_and_clean_finished_request(lt, &c_w_l);

    r = toku_lt_unlock_txn(lt, txn_c); assert(r == 0);

    // shutdown 
    toku_lt_remove_db_ref(lt);
    r = toku_ltm_close(ltm); assert(r == 0);

    return 0;
}
