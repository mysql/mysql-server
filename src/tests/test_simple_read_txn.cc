/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id: test_get_max_row_size.cc 45903 2012-07-19 13:06:39Z leifwalsh $"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
#include "test.h"


static void test_read_txn_creation(DB_ENV* env, uint32_t iso_flags) {
    int r;
    DB_TXN* parent_txn = NULL;
    DB_TXN* child_txn = NULL;
    r = env->txn_begin(env, 0, &parent_txn, iso_flags);
    CKERR(r);
    r = env->txn_begin(env, parent_txn, &child_txn, iso_flags | DB_TXN_READ_ONLY);
    CKERR2(r, EINVAL);
    r = env->txn_begin(env, parent_txn, &child_txn, iso_flags);
    CKERR(r);    
    r = child_txn->commit(child_txn, 0);
    CKERR(r);
    r = parent_txn->commit(parent_txn, 0);
    CKERR(r);

    r = env->txn_begin(env, 0, &parent_txn, iso_flags | DB_TXN_READ_ONLY);
    CKERR(r);
    r = env->txn_begin(env, parent_txn, &child_txn, iso_flags | DB_TXN_READ_ONLY);
    CKERR(r);
    r = child_txn->commit(child_txn, 0);
    CKERR(r);
    r = env->txn_begin(env, parent_txn, &child_txn, iso_flags);
    CKERR(r);    
    r = child_txn->commit(child_txn, 0);
    CKERR(r);
    r = parent_txn->commit(parent_txn, 0);
    CKERR(r);

}

int test_main(int argc, char * const argv[])
{
    int r;
    DB_ENV * env;
    (void) argc;
    (void) argv;

    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    r = toku_os_mkdir(TOKU_TEST_FILENAME, 0755); { int chk_r = r; CKERR(chk_r); }

    // set things up
    r = db_env_create(&env, 0); 
    CKERR(r);
    r = env->open(env, TOKU_TEST_FILENAME, DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN|DB_PRIVATE, 0755); 
    CKERR(r);

    test_read_txn_creation(env, 0);
    test_read_txn_creation(env, DB_TXN_SNAPSHOT);
    test_read_txn_creation(env, DB_READ_COMMITTED);
    test_read_txn_creation(env, DB_READ_UNCOMMITTED);

    r = env->close(env, 0); 
    CKERR(r);

    return 0;
}
