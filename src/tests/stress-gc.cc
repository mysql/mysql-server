/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
// Test that isolation works right for subtransactions.
// In particular, check to see what happens if a subtransaction has different isolation level from its parent.

#include "test.h"
const int envflags = DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN|DB_PRIVATE;

int test_main (int argc, char * const argv[]) {
    parse_args(argc, argv);
    int useseed;

    {
      struct timeval tv;
      gettimeofday(&tv, 0);
      useseed = tv.tv_sec+tv.tv_usec*997;  // magic:  997 is a prime, and a million (microseconds/second) times 997 is still 32 bits.
    }
    
    int r;
    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO);
    DB_ENV *env;
    r = db_env_create(&env, 0);
    CKERR(r);
    env->set_errfile(env, stderr);
    r = env->open(env, TOKU_TEST_FILENAME, envflags, S_IRWXU+S_IRWXG+S_IRWXO);
    CKERR(r);
    db_env_set_mvcc_garbage_collection_verification(1);
    int max_txns = 400;
    int num_runs = 2000;
    DB_TXN* txns[max_txns];
    memset(txns, 0, sizeof(txns));
    int num_txns = 0;
    int i;

    if (verbose) printf("seed=%d\n", useseed);
    srandom(useseed);

    for (i = 0; i < num_runs; i++) {
        int rand_num = random()%max_txns;
        /*
            if (i%50 == 0) {
                printf("rand_num %d\n", rand_num);
                printf("num_txns %d\n", num_txns);
                printf("iteration %d\n", i);
            }
            */
        if (rand_num >= num_txns) {
            // add a txn
            assert(txns[num_txns] == NULL);
            // 7 out of 8 times, it is snapshot, otherwise, serializable
            int is_snapshot = (random() % 8 != 0);
            r = env->txn_begin(env, NULL, &txns[num_txns], is_snapshot ? DB_TXN_SNAPSHOT : 0); 
            CKERR(r);
            num_txns++;
        }
        else {
            // commit the txn
            r = txns[rand_num]->commit(txns[rand_num], 0);
            CKERR(r);
            int j;
            for (j = rand_num; j < num_txns-1; j++) {
                txns[j] = txns[j+1];
            }
            txns[num_txns-1] = NULL;
            num_txns--;
        }
    }

    for (i = 0; i < num_txns; i++) {
        r = txns[i]->commit(txns[i], 0);
        CKERR(r);
    }

    r = env->close(env, 0);
    CKERR(r);
    
    return 0;
}
