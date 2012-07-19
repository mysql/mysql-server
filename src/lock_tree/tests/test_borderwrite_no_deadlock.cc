/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
// See #4844
//
// T(A) gets R(1)
// T(B) gets W(3)
// T(B) gets W(7)
// T(C) gets R(5)
// T(A) trys W(5) blocked
// T(A) gets conflicts { C }
// T(B) trys W(1) blocked
// T(B) gets conflicts { A }
// T(C) releases locks
// T(A) gets W(5)
// T(A) releases locks
// T(B) gets W(1)

#include "test.h"



int main(int argc, const char *argv[]) {
    int r;

    uint32_t max_locks = 4;
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
    r = toku_ltm_get_lt(ltm, &lt, (DICTIONARY_ID){1}, NULL, dbcmp, NULL, NULL, NULL);
    assert(r == 0 && lt);

    const TXNID txn_a = 1;
    const TXNID txn_b = 2;
    const TXNID txn_c = 3;
    DBT key_1; dbt_init(&key_1, "1", 1);
    DBT key_3; dbt_init(&key_3, "3", 1);
    DBT key_5; dbt_init(&key_5, "5", 1);
    DBT key_7; dbt_init(&key_7, "7", 1);


    READ_REQUEST(a, 1);
    WRITE_REQUEST(b, 3);
    WRITE_REQUEST(b, 7);
    READ_REQUEST(c, 5);
    WRITE_REQUEST(a, 5);
    WRITE_REQUEST(b, 1);

    do_request_and_succeed(lt, &a_r_1);
    do_request_and_succeed(lt, &b_w_3);
    do_request_and_succeed(lt, &b_w_7);
    do_request_and_succeed(lt, &c_r_5);

    TXNID ta1[] = { txn_c };
    TXNID ta2[] = { txn_a };
    do_request_that_blocks(lt, &a_w_5, 1, ta1);
    do_request_that_blocks(lt, &b_w_1, 1, ta2);

    r = toku_lt_unlock_txn(lt, txn_c);
    CKERR(r);

    verify_and_clean_finished_request(lt, &a_w_5);

    r = toku_lt_unlock_txn(lt, txn_a);
    CKERR(r);

    verify_and_clean_finished_request(lt, &b_w_1);

    r = toku_lt_unlock_txn(lt, txn_b);
    CKERR(r);

    // shutdown 
    toku_lt_remove_db_ref(lt);
    r = toku_ltm_close(ltm); assert(r == 0);

    return 0;
}
