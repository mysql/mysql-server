/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007-2011 Tokutek Inc.  All rights reserved."
#include "test.h"

// verify stats after a new row inserted into the root
// verify stats after a row overwrite in the root
// verify stats after a row deletion in the root
// verify stats after an update callback inserts row
// verify stats after an update callback overwrites row
// verify ststs after an update callback deletes row

#include <db.h>
#include <unistd.h>
#include <sys/stat.h>

static int 
my_update_callback(DB *db UU(), const DBT *key UU(), const DBT *old_val, const DBT *extra, void (*set_val)(const DBT *new_val, void *set_extra), void *set_extra) {
    if (old_val != NULL && old_val->size == 42) // special code for delete
        set_val(NULL, set_extra);
    else
        set_val(extra, set_extra);
    return 0;
}

static void 
run_test (void) {
    int r;

    r = system("rm -rf " ENVDIR); CKERR(r);
    r = toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);       CKERR(r);

    DB_ENV *env = NULL;
    r = db_env_create(&env, 0); CKERR(r);
    env->set_errfile(env, stderr);
    r = env->set_redzone(env, 0); CKERR(r);
    env->set_update(env, my_update_callback);
    r = env->open(env, ENVDIR, DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_MPOOL|DB_INIT_TXN|DB_CREATE|DB_PRIVATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);

    DB *db = NULL;
    r = db_create(&db, env, 0); CKERR(r);
    DB_TXN *txn = NULL;
    r = env->txn_begin(env, 0, &txn, 0); CKERR(r);
    r = db->open(db, txn, "foo.db", 0, DB_BTREE, DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
    r = txn->commit(txn, 0);    CKERR(r);

    // verify that stats include a new row inserted into the root
    {
        r = env->txn_begin(env, 0, &txn, 0); CKERR(r);
        int key = 1; char val = 1;
        DBT k,v;
        r = db->put(db, txn, dbt_init(&k, &key, sizeof key), dbt_init(&v, &val, sizeof val), 0); CKERR(r);
        r = txn->commit(txn, 0);    CKERR(r);

        DB_BTREE_STAT64 s;
        r = db->stat64(db, NULL, &s); CKERR(r);
        assert(s.bt_nkeys == 1 && s.bt_dsize == sizeof key + sizeof val);

        r = db->close(db, 0);     CKERR(r);

        r = db_create(&db, env, 0); CKERR(r);
        r = env->txn_begin(env, 0, &txn, 0); CKERR(r);
        r = db->open(db, txn, "foo.db", 0, DB_BTREE, DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
        r = txn->commit(txn, 0);    CKERR(r);

        r = db->stat64(db, NULL, &s); CKERR(r);
        assert(s.bt_nkeys == 1 && s.bt_dsize == sizeof key + sizeof val);
    }

    // verify that stats are updated by row overwrite in the root
    {
        r = env->txn_begin(env, 0, &txn, 0); CKERR(r);
        int key = 1; int val = 2;
        DBT k,v;
        r = db->put(db, txn, dbt_init(&k, &key, sizeof key), dbt_init(&v, &val, sizeof val), 0); CKERR(r);
        r = txn->commit(txn, 0);    CKERR(r);

        DB_BTREE_STAT64 s;
        r = db->stat64(db, NULL, &s); CKERR(r);
        assert(s.bt_nkeys == 1 && s.bt_dsize == sizeof key + sizeof val);

        r = db->close(db, 0);     CKERR(r);

        r = db_create(&db, env, 0); CKERR(r);
        r = env->txn_begin(env, 0, &txn, 0); CKERR(r);
        r = db->open(db, txn, "foo.db", 0, DB_BTREE, DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
        r = txn->commit(txn, 0);    CKERR(r);

        r = db->stat64(db, NULL, &s); CKERR(r);
        assert(s.bt_nkeys == 1 && s.bt_dsize == sizeof key + sizeof val);
    }

    // verify that stats are updated by row deletion in the root
    {
        r = env->txn_begin(env, 0, &txn, 0); CKERR(r);
        int key = 1;
        DBT k;
        r = db->del(db, txn, dbt_init(&k, &key, sizeof key), 0); CKERR(r);
        r = txn->commit(txn, 0);    CKERR(r);

        DB_BTREE_STAT64 s;
        r = db->stat64(db, NULL, &s); CKERR(r);
        assert(s.bt_nkeys == 0 && s.bt_dsize == 0);

        r = db->close(db, 0);     CKERR(r);

        r = db_create(&db, env, 0); CKERR(r);
        r = env->txn_begin(env, 0, &txn, 0); CKERR(r);
        r = db->open(db, txn, "foo.db", 0, DB_BTREE, DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
        r = txn->commit(txn, 0);    CKERR(r);

        r = db->stat64(db, NULL, &s); CKERR(r);
        assert(s.bt_nkeys == 0 && s.bt_dsize == 0);
    }

    // verify update of non-existing key inserts a row
    {
        r = env->txn_begin(env, 0, &txn, 0); CKERR(r);
        int key = 1; char val = 1;
        DBT k = { .data = &key, .size = sizeof key };
        DBT e = { .data = &val, .size = sizeof val };
        r = db->update(db, txn, &k, &e, 0); CKERR(r);
        r = txn->commit(txn, 0);    CKERR(r);

        DB_BTREE_STAT64 s;
        r = db->stat64(db, NULL, &s); CKERR(r);
        assert(s.bt_nkeys == 1 && s.bt_dsize == sizeof key + sizeof val);

        r = db->close(db, 0);     CKERR(r);

        r = db_create(&db, env, 0); CKERR(r);
        r = env->txn_begin(env, 0, &txn, 0); CKERR(r);
        r = db->open(db, txn, "foo.db", 0, DB_BTREE, DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
        r = txn->commit(txn, 0);    CKERR(r);

        r = db->stat64(db, NULL, &s); CKERR(r);
        assert(s.bt_nkeys == 1 && s.bt_dsize == sizeof key + sizeof val);
    }

    // verify update callback overwrites the row
    {
        r = env->txn_begin(env, 0, &txn, 0); CKERR(r);
        int key = 1; int val = 2;
        DBT k = { .data = &key, .size = sizeof key };
        DBT e = { .data = &val, .size = sizeof val };
        r = db->update(db, txn, &k, &e, 0); CKERR(r);
        r = txn->commit(txn, 0);    CKERR(r);

        DB_BTREE_STAT64 s;
        r = db->stat64(db, NULL, &s); CKERR(r);
        assert(s.bt_nkeys == 1 && s.bt_dsize == sizeof key + sizeof val);

        r = db->close(db, 0);     CKERR(r);

        r = db_create(&db, env, 0); CKERR(r);
        r = env->txn_begin(env, 0, &txn, 0); CKERR(r);
        r = db->open(db, txn, "foo.db", 0, DB_BTREE, DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
        r = txn->commit(txn, 0);    CKERR(r);

        r = db->stat64(db, NULL, &s); CKERR(r);
        assert(s.bt_nkeys == 1 && s.bt_dsize == sizeof key + sizeof val);
    }

    // verify update callback deletes the row
    {
        // insert a new row
        r = env->txn_begin(env, 0, &txn, 0); CKERR(r);
        int key = 1; char val[42]; memset(val, 0, sizeof val);
        DBT k = { .data = &key, .size = sizeof key };
        DBT e = { .data = &val, .size = sizeof val };
        r = db->update(db, txn, &k, &e, 0); CKERR(r);
        r = txn->commit(txn, 0);    CKERR(r);

        DB_BTREE_STAT64 s;
        r = db->stat64(db, NULL, &s); CKERR(r);
        assert(s.bt_nkeys == 1 && s.bt_dsize == sizeof key + sizeof val);

        // update it again, this should delete the row
        r = env->txn_begin(env, 0, &txn, 0); CKERR(r);
        r = db->update(db, txn, &k, &e, 0); CKERR(r);
        r = txn->commit(txn, 0);    CKERR(r);

        r = db->stat64(db, NULL, &s); CKERR(r);
        assert(s.bt_nkeys == 0 && s.bt_dsize == 0);

        r = db->close(db, 0);     CKERR(r);

        r = db_create(&db, env, 0); CKERR(r);
        r = env->txn_begin(env, 0, &txn, 0); CKERR(r);
        r = db->open(db, txn, "foo.db", 0, DB_BTREE, DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
        r = txn->commit(txn, 0);    CKERR(r);

        r = db->stat64(db, NULL, &s); CKERR(r);
        assert(s.bt_nkeys == 0 && s.bt_dsize == 0);
    }

    r = db->close(db, 0);     CKERR(r);

    r = env->close(env, 0);   CKERR(r);
}

int
test_main (int argc , char * const argv[]) {
    parse_args(argc, argv);
    run_test();
    return 0;
}
