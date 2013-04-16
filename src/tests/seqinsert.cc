/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "test.h"
#include <db.h>
#include <toku_portability.h>
#include <toku_os.h>
#include <memory.h>
#include <stdint.h>
#include <stdlib.h>

DB_TXN * const null_txn = nullptr;

const size_t nodesize = 128 << 10;
const size_t keysize = 8;
const size_t valsize = 92;
const size_t rowsize = keysize + valsize;
const int max_degree = 16;
const size_t numleaves = max_degree * 3; // want height 2, this should be good enough
const size_t numrows = (numleaves * nodesize + rowsize) / rowsize;

static void test_seqinsert(bool asc) {
    int r;
    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    r = toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO);
    CKERR(r);

    DB_ENV *env;
    r = db_env_create(&env, 0);
    CKERR(r);
    r = env->open(env, TOKU_TEST_FILENAME, DB_INIT_LOCK|DB_INIT_MPOOL|DB_INIT_TXN|DB_CREATE|DB_PRIVATE, S_IRWXU+S_IRWXG+S_IRWXO);
    CKERR(r);

    DB *db;
    r = db_create(&db, env, 0);
    CKERR(r);
    r = db->set_pagesize(db, nodesize);
    CKERR(r);
    r = db->open(db, null_txn, "seqinsert", NULL, DB_BTREE, DB_CREATE, 0666);
    CKERR(r);

    {
        DB_TXN *txn;
        r = env->txn_begin(env, 0, &txn, 0);
        CKERR(r);

        char v[valsize];
        ZERO_ARRAY(v);
        uint64_t k;
        DBT key, val;
        dbt_init(&key, &k, sizeof k);
        dbt_init(&val, v, valsize);
        for (size_t i = 0; i < numrows; ++i) {
            k = toku_htod64(numrows + (asc ? i : -i));
            r = db->put(db, txn, &key, &val, 0);
            CKERR(r);
        }

        r = txn->commit(txn, 0);
        CKERR(r);
    }

    r = db->close(db, 0);
    CKERR(r);

    r = env->close(env, 0);
    CKERR(r);
}

int test_main(int argc, char * const argv[]) {
    default_parse_args(argc, argv);

    test_seqinsert(true);
    test_seqinsert(false);

    return 0;
}
