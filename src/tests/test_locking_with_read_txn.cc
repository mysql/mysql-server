/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id: test_get_max_row_size.cc 45903 2012-07-19 13:06:39Z leifwalsh $"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
#include "test.h"

int test_main(int argc, char * const argv[])
{
    int r;
    DB * db;
    DB_ENV * env;
    (void) argc;
    (void) argv;

    const char *db_env_dir = TOKU_TEST_FILENAME;
    char rm_cmd[strlen(db_env_dir) + strlen("rm -rf ") + 1];
    snprintf(rm_cmd, sizeof(rm_cmd), "rm -rf %s", db_env_dir);

    r = system(rm_cmd); { int chk_r = r; CKERR(chk_r); }
    r = toku_os_mkdir(db_env_dir, 0755); { int chk_r = r; CKERR(chk_r); }

    // set things up
    r = db_env_create(&env, 0); 
    CKERR(r);
    r = env->open(env, db_env_dir, DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN|DB_PRIVATE, 0755); 
    CKERR(r);
    r = db_create(&db, env, 0); 
    CKERR(r);
    r = db->open(db, NULL, "foo.db", NULL, DB_BTREE, DB_CREATE, 0644); 
    CKERR(r);


    DB_TXN* txn1 = NULL;
    DB_TXN* txn2 = NULL;
    r = env->txn_begin(env, 0, &txn1, DB_TXN_READ_ONLY);
    CKERR(r);
    r = env->txn_begin(env, 0, &txn2, DB_TXN_READ_ONLY);
    CKERR(r);

    
    r=db->pre_acquire_table_lock(db, txn1); CKERR(r);
    r=db->pre_acquire_table_lock(db, txn2); CKERR2(r, DB_LOCK_NOTGRANTED);

    r = txn1->commit(txn1, 0);
    CKERR(r);
    r = txn2->commit(txn2, 0);
    CKERR(r);    

    // clean things up
    r = db->close(db, 0); 
    CKERR(r);
    r = env->close(env, 0); 
    CKERR(r);

    return 0;
}
