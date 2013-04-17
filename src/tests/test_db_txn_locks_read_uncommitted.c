/* -*- mode: C; c-basic-offset: 4 -*- */
#include <toku_portability.h>
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#include <memory.h>
#include <toku_portability.h>
#include <db.h>
#include <assert.h>
#include <errno.h>
#include <sys/stat.h>

#include "test.h"

// ENVDIR is defined in the Makefile

static DB *db;
static DB_TXN* txns[(int)256];
static DB_ENV* dbenv;
static DBC*    cursors[(int)256];

static void
put(BOOL success, char txn, int _key, int _data) {
    assert(txns[(int)txn]);

    int r;
    DBT key;
    DBT data;
    
    r = db->put(db, txns[(int)txn],
                    dbt_init(&key, &_key, sizeof(int)),
                    dbt_init(&data, &_data, sizeof(int)),
                    DB_YESOVERWRITE);

    if (success)    CKERR(r);
    else            CKERR2s(r, DB_LOCK_DEADLOCK, DB_LOCK_NOTGRANTED);
}

static void
init_txn (char name, u_int32_t flags) {
    int r;
    assert(!txns[(int)name]);
    r = dbenv->txn_begin(dbenv, NULL, &txns[(int)name], DB_TXN_NOWAIT | flags);
        CKERR(r);
    assert(txns[(int)name]);
}

static void
init_dbc (char name) {
    int r;

    assert(!cursors[(int)name] && txns[(int)name]);
    r = db->cursor(db, txns[(int)name], &cursors[(int)name], 0);
        CKERR(r);
    assert(cursors[(int)name]);
}

static void
commit_txn (char name) {
    int r;
    assert(txns[(int)name] && !cursors[(int)name]);

    r = txns[(int)name]->commit(txns[(int)name], 0);
        CKERR(r);
    txns[(int)name] = NULL;
}


static void
close_dbc (char name) {
    int r;

    assert(cursors[(int)name]);
    r = cursors[(int)name]->c_close(cursors[(int)name]);
        CKERR(r);
    cursors[(int)name] = NULL;
}

static void
early_commit (char name) {
    assert(cursors[(int)name] && txns[(int)name]);
    close_dbc(name);
    commit_txn(name);
}

static void
setup_dbs (u_int32_t dup_flags) {
    int r;

    system("rm -rf " ENVDIR);
    toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);
    dbenv   = NULL;
    db      = NULL;
    /* Open/create primary */
    r = db_env_create(&dbenv, 0);
        CKERR(r);
    u_int32_t env_txn_flags  = DB_INIT_TXN | DB_INIT_LOCK;
    u_int32_t env_open_flags = DB_CREATE | DB_PRIVATE | DB_INIT_MPOOL;
	r = dbenv->open(dbenv, ENVDIR, env_open_flags | env_txn_flags, 0600);
        CKERR(r);
    
    r = db_create(&db, dbenv, 0);
        CKERR(r);
    if (dup_flags) {
        r = db->set_flags(db, dup_flags);
            CKERR(r);
    }
    r = db->set_bt_compare( db, int_dbt_cmp);
    CKERR(r);
    r = db->set_dup_compare(db, int_dbt_cmp);
    CKERR(r);

    char a;
    for (a = 'a'; a <= 'z'; a++) init_txn(a, 0);
    for (a = '0'; a <= '9'; a++) init_txn(a, DB_READ_UNCOMMITTED);
    init_txn('\0', 0);
    r = db->open(db, txns[(int)'\0'], "foobar.db", NULL, DB_BTREE, DB_CREATE | DB_READ_UNCOMMITTED, 0600);
        CKERR(r);
    commit_txn('\0');
    for (a = 'a'; a <= 'z'; a++) init_dbc(a);
    for (a = '0'; a <= '9'; a++) init_dbc(a);
}

static void
close_dbs(void) {
    char a;
    for (a = 'a'; a <= 'z'; a++) {
        if (cursors[(int)a]) close_dbc(a);
        if (txns[(int)a])    commit_txn(a);
    }
    for (a = '0'; a <= '9'; a++) {
        if (cursors[(int)a]) close_dbc(a);
        if (txns[(int)a])    commit_txn(a);
    }

    int r;
    r = db->close(db, 0);
        CKERR(r);
    db      = NULL;
    r = dbenv->close(dbenv, 0);
        CKERR(r);
    dbenv   = NULL;
}


static void
table_scan(char txn, BOOL success) {
    int r;
    DBT key;
    DBT data;

    assert(txns[(int)txn] && cursors[(int)txn]);
    r = cursors[(int)txn]->c_get(cursors[(int)txn],
                                 dbt_init(&key,  0, 0),
                                 dbt_init(&data, 0, 0),
                                 DB_FIRST);
    while (r==0) {
        r = cursors[(int)txn]->c_get(cursors[(int)txn],
                                     dbt_init(&key,  0, 0),
                                     dbt_init(&data, 0, 0),
                                     DB_NEXT);
    }
    if (success) CKERR2(r, DB_NOTFOUND);
    else         CKERR2s(r, DB_LOCK_NOTGRANTED, DB_LOCK_DEADLOCK);
}

static void
table_prelock(char txn, BOOL success) {
    int r;
#if defined USE_TDB && USE_TDB
    r = db->pre_acquire_table_lock(db,  txns[(int)txn]);
    if (success) CKERR(r);
    else         CKERR2s(r, DB_LOCK_NOTGRANTED, DB_LOCK_DEADLOCK);
#else
    DBT key;
    DBT data;

    assert(txns[(int)txn] && cursors[(int)txn]);
    r = cursors[(int)txn]->c_get(cursors[(int)txn],
                                 dbt_init(&key,  0, 0),
                                 dbt_init(&data, 0, 0),
                                 DB_FIRST | DB_RMW);
    while (r==0) {
        r = cursors[(int)txn]->c_get(cursors[(int)txn],
                                     dbt_init(&key,  0, 0),
                                     dbt_init(&data, 0, 0),
                                     DB_NEXT | DB_RMW);
    }
    if (success) CKERR2(r, DB_NOTFOUND);
    else         CKERR2s(r, DB_LOCK_NOTGRANTED, DB_LOCK_DEADLOCK);
#endif
}

static void
test (u_int32_t dup_flags) {
    char txn;
    /* ********************************************************************** */
    setup_dbs(dup_flags);
    close_dbs();
    /* ********************************************************************** */
    setup_dbs(dup_flags);
    table_scan('0', TRUE);
    table_prelock('a', TRUE);
    put(TRUE, 'a', 0, 0);
    for (txn = 'b'; txn<'z'; txn++) {
        table_scan(txn, FALSE);
    }
    for (txn = '0'; txn<'9'; txn++) {
        table_scan(txn, TRUE);
    }
    early_commit('a');
    for (txn = 'b'; txn<'z'; txn++) {
        table_scan(txn, TRUE);
    }
    for (txn = '0'; txn<'9'; txn++) {
        table_scan(txn, TRUE);
    }
    close_dbs();
    /* ********************************************************************** */
}


int
test_main(int argc, const char* argv[]) {
    parse_args(argc, argv);
    test(0);
    test(DB_DUP | DB_DUPSORT);
    return 0;
}
