/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."
#include "test.h"
/* Test to see if DB->get works on a zeroed DBT. */

#include <db.h>
#include <memory.h>
#include <stdlib.h>

#include <sys/stat.h>



static void
test_get (int dup_mode) {
    DB_TXN * const null_txn = 0;
    DBT key,data;
    int fnamelen = sizeof(ENVDIR) + 30;
    char fname[fnamelen];
    int r;
    snprintf(fname, fnamelen, "test%d.db", dup_mode);
    DB_ENV *env;
    r = db_env_create(&env, 0); assert(r == 0);
    r = env->open(env, ENVDIR, DB_CREATE+DB_PRIVATE+DB_INIT_MPOOL, 0); assert(r == 0);

    DB *db;
    r = db_create (&db, env, 0);                                        assert(r == 0);
    r = db->set_flags(db, dup_mode);                                         assert(r == 0);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, DB_CREATE, 0666);    assert(r == 0);
    dbt_init(&key, "a", 2);
    r = db->put(db, null_txn, &key, dbt_init(&data, "b", 2), DB_YESOVERWRITE); assert(r==0);
    memset(&data, 0, sizeof(data));
    r = db->get(db, null_txn, &key, &data, 0);                               assert(r == 0);
    assert(strcmp(data.data, "b")==0);
    r = db->close(db, 0); assert(r == 0);
    r = env->close(env, 0); assert(r == 0);
}

int
test_main (int argc, char *const argv[]) {
    parse_args(argc, argv);
    system("rm -rf " ENVDIR);
    toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);

    test_get(0);
    test_get(DB_DUP + DB_DUPSORT);
    return 0;
}
