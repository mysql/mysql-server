/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
#include "test.h"

int test_main(int argc, char * const argv[])
{
    int r;
    DB * db;
    DB_ENV * db_env;
    (void) argc;
    (void) argv;

    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    r = toku_os_mkdir(TOKU_TEST_FILENAME, 0755); { int chk_r = r; CKERR(chk_r); }

    // set things up
    r = db_env_create(&db_env, 0); { int chk_r = r; CKERR(chk_r); }
    r = db_env->open(db_env, TOKU_TEST_FILENAME, DB_CREATE|DB_INIT_MPOOL|DB_PRIVATE, 0755); { int chk_r = r; CKERR(chk_r); }
    r = db_create(&db, db_env, 0); { int chk_r = r; CKERR(chk_r); }
    r = db->open(db, NULL, "db", NULL, DB_BTREE, DB_CREATE, 0644); { int chk_r = r; CKERR(chk_r); }

    // - does not test low bounds, so a 0 byte key is "okay"
    // - assuming 32k keys and 32mb values are the max
    uint32_t max_key, max_val;
    db->get_max_row_size(db, &max_key, &max_val);
    // assume it is a red flag for the key to be outside the 16-32kb range
    assert(max_key >= 16*1024);
    assert(max_key <= 32*1024);
    // assume it is a red flag for the value to be outside the 16-32mb range
    assert(max_val >= 16*1024*1024);
    assert(max_val <= 32*1024*1024);

    // clean things up
    r = db->close(db, 0); { int chk_r = r; CKERR(chk_r); }
    r = db_env->close(db_env, 0); { int chk_r = r; CKERR(chk_r); }

    return 0;
}
