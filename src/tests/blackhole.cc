/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

// Test that a db ignores insert messages in blackhole mode

#include "test.h"
#include <ft/ybt.h>

static DB *db;
static DB *blackhole_db;
static DB_ENV *env;

static int num_inserts = 10000;

static void fill_dbt(DBT *dbt, void *data, size_t size) {
    dbt->data = data;
    dbt->size = dbt->ulen = size;
    dbt->flags = DB_DBT_USERMEM;
}

static void setup (bool use_txns) {
    int r;
    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    r = toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
    r = db_env_create(&env, 0); CKERR(r);
    r = env->open(env, TOKU_TEST_FILENAME, 0, 0);
    int txnflags = use_txns ? (DB_INIT_LOCK | DB_INIT_LOG | DB_INIT_TXN) : 0;
    r = env->open(env, TOKU_TEST_FILENAME, DB_CREATE|DB_PRIVATE|txnflags, 0777);

    // create a regular db and a blackhole db
    r = db_create(&db, env, 0); CKERR(r);
    r = db_create(&blackhole_db, env, 0); CKERR(r);
    r = db->open(db, NULL, "test.db", 0, DB_BTREE,
            DB_CREATE,
            S_IRWXU+S_IRWXG+S_IRWXO);
    CKERR(r);
    r = blackhole_db->open(blackhole_db, NULL, "blackhole.db", 0, DB_BTREE, 
            DB_CREATE | DB_BLACKHOLE,
            S_IRWXU+S_IRWXG+S_IRWXO);
    CKERR(r);
}

static void cleanup (void) {
    int r;
    r = db->close(db, 0); CKERR(r);
    r = blackhole_db->close(blackhole_db, 0); CKERR(r);
    r = env->close(env, 0); CKERR(r);
}

static void test_blackhole(void) {
    int r = 0;

    for (int i = 0; i < num_inserts; i++) {
        int k = random();
        int v = k + 100;
        DBT key, value;
        fill_dbt(&key, &k, sizeof k); 
        fill_dbt(&value, &v, sizeof v); 

        // put a random key into the regular db.
        r = db->put(db, NULL, &key, &value, 0);
        assert(r == 0);

        // put that key into the blackhole db.
        r = blackhole_db->put(blackhole_db, NULL, &key, &value, 0);
        assert(r == 0);

        // we should be able to find this key in the regular db
        int get_v;
        DBT get_value;
        fill_dbt(&get_value, &get_v, sizeof get_v);
        r = db->get(db, NULL, &key, &get_value, 0);
        assert(r == 0);
        assert(*(int *)get_value.data == v);
        assert(get_value.size == sizeof v);

        // we shouldn't be able to get it back from the blackhole
        r = blackhole_db->get(blackhole_db, NULL, &key, &get_value, 0);
        assert(r == DB_NOTFOUND);
    }
}

int test_main (int argc __attribute__((__unused__)), char *const argv[] __attribute__((__unused__))) {
    // without txns
    setup(false);
    test_blackhole();
    cleanup();

    // with txns
    setup(true);
    test_blackhole();
    cleanup();
    return 0;
}
