/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#include "test.h"
/* Test to see if DB->get works on a zeroed DBT. */

#include <db.h>
#include <memory.h>
#include <stdlib.h>

#include <sys/stat.h>



static void
test_get (void) {
    DB_TXN * const null_txn = 0;
    DBT key,data;
    char fname[] = "test.db";
    int r;
    DB_ENV *env;
    r = db_env_create(&env, 0); assert(r == 0);
    r = env->open(env, TOKU_TEST_FILENAME, DB_CREATE+DB_PRIVATE+DB_INIT_MPOOL, 0); assert(r == 0);

    DB *db;
    r = db_create (&db, env, 0);                                        assert(r == 0);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, DB_CREATE, 0666);    assert(r == 0);
    dbt_init(&key, "a", 2);
    r = db->put(db, null_txn, &key, dbt_init(&data, "b", 2), 0); assert(r==0);
    memset(&data, 0, sizeof(data));
    r = db->get(db, null_txn, &key, &data, 0);                               assert(r == 0);
    assert(strcmp((char*)data.data, "b")==0);
    r = db->close(db, 0); assert(r == 0);
    r = env->close(env, 0); assert(r == 0);
}

int
test_main (int argc, char *const argv[]) {
    parse_args(argc, argv);
    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO);

    test_get();
    return 0;
}
