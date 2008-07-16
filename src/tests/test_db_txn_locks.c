/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#include <string.h>
#include <db.h>
#include <assert.h>
#include <errno.h>
#include <sys/stat.h>

#include "test.h"

struct heavi_extra {
    DBT key;
    DBT val;
    DB* db;
};

int heavi_after(const DBT *key, const DBT *val, void *extra) {
    //Assumes cmp is int_dbt_cmp
    struct heavi_extra *info = extra;
    int cmp = int_dbt_cmp(info->db, key, &info->key);
    if (cmp!=0) return cmp;
    if (!val) return -1;
    cmp = int_dbt_cmp(info->db, val, &info->val);
    return cmp<=0 ? -1 : 0;
    //Returns <0 for too small/equal
    //Returns 0 for greater, but with the same key
    //Returns >0 for greater with different key
}

int heavi_before(const DBT *key, const DBT *val, void *extra) {
    struct heavi_extra *info = extra;
    int cmp = int_dbt_cmp(info->db, key, &info->key);
    if (cmp!=0) return cmp;
    if (!val) return +1;
    cmp = int_dbt_cmp(info->db, val, &info->val);
    return cmp>=0 ? 1 : 0;
    //Returns >0 for too large/equal
    //Returns 0 for smaller with same key
    //returns -1 for smaller with different key
}

// ENVDIR is defined in the Makefile

int dbtcmp(DBT *dbt1, DBT *dbt2) {
    int r;
    
    r = dbt1->size - dbt2->size;  if (r) return r;
    return memcmp(dbt1->data, dbt2->data, dbt1->size);
}

DB *db;
DB_TXN* txns[(int)256];
DB_ENV* dbenv;
DBC*    cursors[(int)256];

void put(BOOL success, char txn, int _key, int _data) {
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

void cget(BOOL success, BOOL find, char txn, int _key, int _data, 
          int _key_expect, int _data_expect, u_int32_t flags) {
    assert(txns[(int)txn] && cursors[(int)txn]);

    int r;
    DBT key;
    DBT data;
    
    r = cursors[(int)txn]->c_get(cursors[(int)txn],
                                 dbt_init(&key,  &_key,  sizeof(int)),
                                 dbt_init(&data, &_data, sizeof(int)),
                                 flags);
    if (success) {
        if (find) {
            CKERR(r);
            assert(*(int *)key.data  == _key_expect);
            assert(*(int *)data.data == _data_expect);
        }
        else        CKERR2s(r,  DB_NOTFOUND, DB_KEYEMPTY);
    }
    else            CKERR2s(r, DB_LOCK_DEADLOCK, DB_LOCK_NOTGRANTED);
}

void cdel(BOOL success, BOOL find, char txn) {
    int r;

    r = cursors[(int)txn]->c_del(cursors[(int)txn], 0);
    if (success) {
        if (find) CKERR(r);
        else      CKERR2(r, DB_KEYEMPTY);
    }
    else            CKERR2s(r, DB_LOCK_DEADLOCK, DB_LOCK_NOTGRANTED);
}

void dbdel(BOOL success, BOOL find, char txn, int _key) {
    int r;
    DBT key;

    /* If DB_DELETE_ANY changes to 0, then find is meaningful and 
       has to be fixed in test_dbdel*/
    r = db->del(db, txns[(int)txn], dbt_init(&key,&_key, sizeof(int)), 
                DB_DELETE_ANY);
    if (success) {
        if (find) CKERR(r);
        else      CKERR2( r, DB_NOTFOUND);
    }
    else          CKERR2s(r, DB_LOCK_DEADLOCK, DB_LOCK_NOTGRANTED);
}

void init_txn(char name) {
    int r;
    assert(!txns[(int)name]);
    r = dbenv->txn_begin(dbenv, NULL, &txns[(int)name], DB_TXN_NOWAIT);
        CKERR(r);
    assert(txns[(int)name]);
}

void init_dbc(char name) {
    int r;

    assert(!cursors[(int)name] && txns[(int)name]);
    r = db->cursor(db, txns[(int)name], &cursors[(int)name], 0);
        CKERR(r);
    assert(cursors[(int)name]);
}

void commit_txn(char name) {
    int r;
    assert(txns[(int)name] && !cursors[(int)name]);

    r = txns[(int)name]->commit(txns[(int)name], 0);
        CKERR(r);
    txns[(int)name] = NULL;
}

void abort_txn(char name) {
    int r;
    assert(txns[(int)name] && !cursors[(int)name]);

    r = txns[(int)name]->abort(txns[(int)name]);
        CKERR(r);
    txns[(int)name] = NULL;
}

void close_dbc(char name) {
    int r;

    assert(cursors[(int)name]);
    r = cursors[(int)name]->c_close(cursors[(int)name]);
        CKERR(r);
    cursors[(int)name] = NULL;
}

void early_commit(char name) {
    assert(cursors[(int)name] && txns[(int)name]);
    close_dbc(name);
    commit_txn(name);
}

void early_abort(char name) {
    assert(cursors[(int)name] && txns[(int)name]);
    close_dbc(name);
    abort_txn(name);
}

void setup_dbs(u_int32_t dup_flags) {
    int r;

    system("rm -rf " ENVDIR);
    mkdir(ENVDIR, 0777);
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
    for (a = 'a'; a <= 'z'; a++) init_txn(a);
    init_txn('\0');
    r = db->open(db, txns[(int)'\0'], "foobar.db", NULL, DB_BTREE, DB_CREATE, 0600);
        CKERR(r);
    commit_txn('\0');
    for (a = 'a'; a <= 'z'; a++) init_dbc(a);
}

void close_dbs(void) {
    char a;
    for (a = 'a'; a <= 'z'; a++) {
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


void test_abort(u_int32_t dup_flags) {
    /* ********************************************************************** */
    setup_dbs(dup_flags);
    put(TRUE, 'a', 1, 1);
    early_abort('a');
    cget(TRUE, FALSE, 'b', 1, 1, 0, 0, DB_SET);
    close_dbs();
    /* ********************************************************************** */
    setup_dbs(dup_flags);
    cget(TRUE, FALSE, 'a', 1, 1, 0, 0, DB_SET);
    cget(TRUE, FALSE, 'b', 1, 1, 0, 0, DB_SET);
    put(FALSE, 'a', 1, 1);
    early_commit('b');
    put(TRUE, 'a', 1, 1);
    cget(TRUE, TRUE, 'a', 1, 1, 1, 1, DB_SET);
    cget(TRUE, FALSE, 'a', 2, 1, 1, 1, DB_SET);
    cget(FALSE, TRUE, 'c', 1, 1, 0, 0, DB_SET);
    early_abort('a');
    cget(TRUE, FALSE, 'c', 1, 1, 0, 0, DB_SET);
    close_dbs();
    /* ********************************************************************** */
}

void test_both(u_int32_t dup_flags, u_int32_t db_flags) {
    /* ********************************************************************** */
    setup_dbs(dup_flags);
    cget(TRUE, FALSE, 'a', 1, 1, 0, 0, db_flags);
    close_dbs();
    /* ********************************************************************** */
    setup_dbs(dup_flags);
    cget(TRUE, FALSE, 'a', 1, 1, 0, 0, db_flags);
    cget(TRUE, FALSE, 'a', 2, 1, 0, 0, db_flags);
    close_dbs();
    /* ********************************************************************** */
    setup_dbs(dup_flags);
    cget(TRUE, FALSE, 'a', 1, 1, 0, 0, db_flags);
    cget(TRUE, FALSE, 'a', 1, 1, 0, 0, db_flags);
    close_dbs();
    /* ********************************************************************** */
    setup_dbs(dup_flags);
    cget(TRUE, FALSE, 'a', 1, 1, 0, 0, db_flags);
    cget(TRUE, FALSE, 'b', 2, 1, 0, 0, db_flags);
    close_dbs();
    /* ********************************************************************** */
    setup_dbs(dup_flags);
    cget(TRUE, FALSE, 'a', 1, 1, 0, 0, db_flags);
    cget(TRUE, FALSE, 'b', 1, 1, 0, 0, db_flags);
    close_dbs();
    /* ********************************************************************** */
    setup_dbs(dup_flags);
    cget(TRUE, FALSE, 'a', 1, 1, 0, 0, db_flags);
    cget(TRUE, FALSE, 'b', 1, 1, 0, 0, db_flags);
    put(FALSE, 'a', 1, 1);
    early_commit('b');
    put(TRUE, 'a', 1, 1);
    cget(TRUE, TRUE, 'a', 1, 1, 1, 1, db_flags);
    cget(TRUE, FALSE, 'a', 2, 1, 0, 0, db_flags);
    cget(FALSE, TRUE, 'c', 1, 1, 0, 0, db_flags);
    early_commit('a');
    cget(TRUE, TRUE, 'c', 1, 1, 1, 1, db_flags);
    close_dbs();
}


void test_last(u_int32_t dup_flags) {
    /* ********************************************************************** */
    setup_dbs(dup_flags);
    cget(TRUE, FALSE, 'a', 0, 0, 0, 0, DB_LAST);
    put(FALSE, 'b', 2, 1);
    put(TRUE, 'a', 2, 1);
    cget(TRUE, TRUE, 'a', 0, 0, 2, 1, DB_LAST);
    early_commit('a');
    put(TRUE, 'b', 2, 1);
    close_dbs();
    /* ****************************************** */
    setup_dbs(dup_flags);
    put(TRUE, 'a', 1, 1);
    cget(TRUE, TRUE, 'a', 0, 0, 1, 1, DB_LAST);
    put(FALSE, 'b', 2, 1);
    put(TRUE, 'b', -1, 1);
    cget(TRUE, TRUE, 'a', 0, 0, 1, 1, DB_LAST);
    close_dbs();
    /* ****************************************** */
    setup_dbs(dup_flags);
    put(TRUE, 'a', 1, 1);
    put(TRUE, 'a', 3, 1);
    put(TRUE, 'a', 6, 1);
    cget(TRUE, TRUE, 'a', 0, 0, 6, 1, DB_LAST);
    put(TRUE, 'b', 2, 1);
    put(TRUE, 'b', 4, 1);
    put(FALSE, 'b', 7, 1);
    put(TRUE, 'b', -1, 1);
    close_dbs();
    /* ****************************************** */
    setup_dbs(dup_flags);
    put(TRUE, 'a', 1, 1);
    cget(TRUE, TRUE, 'a', 0, 0, 1, 1, DB_LAST);
    put(dup_flags != 0, 'b', 1, 0);
    close_dbs();
}

void test_first(u_int32_t dup_flags) {
    /* ********************************************************************** */
    setup_dbs(dup_flags);
    cget(TRUE, FALSE, 'a', 0, 0, 0, 0, DB_FIRST);
    put(FALSE, 'b', 2, 1);
    put(TRUE, 'a', 2, 1);
    cget(TRUE, TRUE, 'a', 0, 0, 2, 1, DB_FIRST);
    early_commit('a');
    put(TRUE, 'b', 2, 1);
    close_dbs();
    /* ****************************************** */
    setup_dbs(dup_flags);
    put(TRUE, 'a', 1, 1);
    cget(TRUE, TRUE, 'a', 0, 0, 1, 1, DB_FIRST);
    put(TRUE, 'b', 2, 1);
    put(FALSE, 'b', -1, 1);
    cget(TRUE, TRUE, 'a', 0, 0, 1, 1, DB_FIRST);
    close_dbs();
    /* ****************************************** */
    setup_dbs(dup_flags);
    put(TRUE, 'a', 1, 1);
    put(TRUE, 'a', 3, 1);
    put(TRUE, 'a', 6, 1);
    cget(TRUE, TRUE, 'a', 0, 0, 1, 1, DB_FIRST);
    put(TRUE, 'b', 2, 1);
    put(TRUE, 'b', 4, 1);
    put(TRUE, 'b', 7, 1);
    put(FALSE, 'b', -1, 1);
    close_dbs();
    /* ****************************************** */
    setup_dbs(dup_flags);
    put(TRUE, 'a', 1, 1);
    cget(TRUE, TRUE, 'a', 0, 0, 1, 1, DB_FIRST);
    put(dup_flags != 0, 'b', 1, 2);
    close_dbs();
}

void test_set_range(u_int32_t dup_flags) {
    /* ********************************************************************** */
    setup_dbs(dup_flags);
    cget(TRUE, FALSE, 'a', 1, 1, 0, 0, DB_SET_RANGE);
    close_dbs();
    /* ********************************************************************** */
    setup_dbs(dup_flags);
    cget(TRUE, FALSE, 'a', 1, 1, 0, 0, DB_SET_RANGE);
    cget(TRUE, FALSE, 'a', 2, 1, 0, 0, DB_SET_RANGE);
    close_dbs();
    /* ********************************************************************** */
    setup_dbs(dup_flags);
    cget(TRUE, FALSE, 'a', 1, 1, 0, 0, DB_SET_RANGE);
    cget(TRUE, FALSE, 'a', 1, 1, 0, 0, DB_SET_RANGE);
    close_dbs();
    /* ********************************************************************** */
    setup_dbs(dup_flags);
    cget(TRUE, FALSE, 'a', 1, 1, 0, 0, DB_SET_RANGE);
    cget(TRUE, FALSE, 'b', 2, 1, 0, 0, DB_SET_RANGE);
    close_dbs();
    /* ********************************************************************** */
    setup_dbs(dup_flags);
    cget(TRUE, FALSE, 'a', 1, 1, 0, 0, DB_SET_RANGE);
    cget(TRUE, FALSE, 'b', 1, 1, 0, 0, DB_SET_RANGE);
    close_dbs();
    /* ********************************************************************** */
    setup_dbs(dup_flags);
    cget(TRUE, FALSE, 'a', 1, 1, 0, 0, DB_SET_RANGE);
    cget(TRUE, FALSE, 'b', 5, 5, 0, 0, DB_SET_RANGE);
    put(FALSE, 'a', 7, 6);
    put(FALSE, 'a', 5, 5);
    put(TRUE,  'a', 4, 4);
    put(TRUE,  'b', -1, 4);
    put(FALSE,  'b', 2, 4);
    put(FALSE, 'a', 5, 4);
    early_commit('b');
    put(TRUE, 'a', 7, 6);
    put(TRUE, 'a', 5, 5);
    put(TRUE,  'a', 4, 4);
    put(TRUE, 'a', 5, 4);
    cget(TRUE, TRUE, 'a', 1, 1, 4, 4, DB_SET_RANGE);
    cget(TRUE, TRUE, 'a', 2, 1, 4, 4, DB_SET_RANGE);
    cget(FALSE, TRUE, 'c', 6, 6, 7, 6, DB_SET_RANGE);
    early_commit('a');
    cget(TRUE, TRUE, 'c', 6, 6, 7, 6, DB_SET_RANGE);
    close_dbs();
}

void test_both_range(u_int32_t dup_flags) {
    if (dup_flags == 0) {
      test_both(dup_flags, DB_GET_BOTH_RANGE);
      return;
    }
    /* ********************************************************************** */
    setup_dbs(dup_flags);
    cget(TRUE, FALSE, 'a', 1, 1, 0, 0, DB_GET_BOTH_RANGE);
    close_dbs();
    /* ********************************************************************** */
    setup_dbs(dup_flags);
    cget(TRUE, FALSE, 'a', 1, 1, 0, 0, DB_GET_BOTH_RANGE);
    cget(TRUE, FALSE, 'a', 2, 1, 0, 0, DB_GET_BOTH_RANGE);
    close_dbs();
    /* ********************************************************************** */
    setup_dbs(dup_flags);
    cget(TRUE, FALSE, 'a', 1, 1, 0, 0, DB_GET_BOTH_RANGE);
    cget(TRUE, FALSE, 'a', 1, 1, 0, 0, DB_GET_BOTH_RANGE);
    close_dbs();
    /* ********************************************************************** */
    setup_dbs(dup_flags);
    cget(TRUE, FALSE, 'a', 1, 1, 0, 0, DB_GET_BOTH_RANGE);
    cget(TRUE, FALSE, 'b', 2, 1, 0, 0, DB_GET_BOTH_RANGE);
    close_dbs();
    /* ********************************************************************** */
    setup_dbs(dup_flags);
    cget(TRUE, FALSE, 'a', 1, 1, 0, 0, DB_GET_BOTH_RANGE);
    cget(TRUE, FALSE, 'b', 1, 1, 0, 0, DB_GET_BOTH_RANGE);
    close_dbs();
    /* ********************************************************************** */
    setup_dbs(dup_flags);
    cget(TRUE, FALSE, 'a', 1, 1, 0, 0, DB_GET_BOTH_RANGE);
    cget(TRUE, FALSE, 'b', 5, 5, 0, 0, DB_GET_BOTH_RANGE);
    put(TRUE, 'a', 5, 0);
    put(FALSE, 'a', 5, 5);
    put(FALSE, 'a', 5, 6);
    put(TRUE,  'a', 6, 0);
    put(TRUE,  'b', 1, 0);
    early_commit('b');
    put(TRUE, 'a', 5, 0);
    put(TRUE, 'a', 5, 5);
    put(TRUE, 'a', 5, 6);
    put(TRUE,  'a', 6, 0);
    cget(TRUE, FALSE, 'a', 1, 1, 4, 4, DB_GET_BOTH_RANGE);
    cget(TRUE,  TRUE, 'a', 1, 0, 1, 0, DB_GET_BOTH_RANGE);
    cget(FALSE, TRUE, 'c', 5, 5, 5, 5, DB_GET_BOTH_RANGE);
    early_commit('a');
    cget(TRUE, TRUE, 'c', 5, 5, 5, 5, DB_GET_BOTH_RANGE);
    close_dbs();
}

void test_next(u_int32_t dup_flags, u_int32_t next_type) {
    /* ********************************************************************** */
    setup_dbs(dup_flags);
    put(TRUE,  'a', 2, 1);
    put(TRUE,  'a', 5, 1);
    cget(TRUE, TRUE, 'a', 0, 0, 2, 1, next_type);
    put(FALSE, 'b', 2, 1);
    put(TRUE,  'b', 4, 1);
    put(FALSE, 'b', -1, 1);
    cget(FALSE, TRUE, 'a', 0, 0, 4, 1, next_type);
    early_commit('b');
    cget(TRUE,  TRUE, 'a', 2, 1, 2, 1, DB_GET_BOTH);
    cget(TRUE,  TRUE, 'a', 0, 0, 4, 1, next_type);
    cget(TRUE,  TRUE, 'a', 0, 0, 5, 1, next_type);
    close_dbs();
    /* ****************************************** */
    setup_dbs(dup_flags);
    put(TRUE, 'a', 1, 1);
    put(TRUE, 'a', 3, 1);
    put(TRUE, 'a', 6, 1);
    cget(TRUE, TRUE, 'a', 0, 0, 1, 1, next_type);
    cget(TRUE, TRUE, 'a', 0, 0, 3, 1, next_type);
    put(FALSE, 'b', 2, 1);
    put(TRUE,  'b', 4, 1);
    put(TRUE,  'b', 7, 1);
    put(FALSE, 'b', -1, 1);
    close_dbs();
}

void test_prev(u_int32_t dup_flags, u_int32_t next_type) {
    /* ********************************************************************** */
    setup_dbs(dup_flags);
    put(TRUE,  'a', -2, -1);
    put(TRUE,  'a', -5, -1);
    cget(TRUE, TRUE, 'a', 0, 0, -2, -1, next_type);
    put(FALSE, 'b', -2, -1);
    put(TRUE,  'b', -4, -1);
    put(FALSE, 'b', 1, -1);
    cget(FALSE, TRUE, 'a', 0, 0, -4, -1, next_type);
    early_commit('b');
    cget(TRUE,  TRUE, 'a', -2, -1, -2, -1, DB_GET_BOTH);
    cget(TRUE,  TRUE, 'a', 0, 0, -4, -1, next_type);
    cget(TRUE,  TRUE, 'a', 0, 0, -5, -1, next_type);
    close_dbs();
    /* ****************************************** */
    setup_dbs(dup_flags);
    put(TRUE, 'a', -1, -1);
    put(TRUE, 'a', -3, -1);
    put(TRUE, 'a', -6, -1);
    cget(TRUE, TRUE, 'a', 0, 0, -1, -1, next_type);
    cget(TRUE, TRUE, 'a', 0, 0, -3, -1, next_type);
    put(FALSE, 'b', -2, -1);
    put(TRUE,  'b', -4, -1);
    put(TRUE,  'b', -7, -1);
    put(FALSE, 'b', 1, -1);
    close_dbs();
}

void test_nextdup(u_int32_t dup_flags, u_int32_t next_type, int i) {
    /* ****************************************** */
    if (dup_flags == 0) return;
    setup_dbs(dup_flags);
    put(TRUE, 'c', i*1, i*1);
    early_commit('c');
    cget(TRUE, TRUE,  'a', i*1, i*1, i*1, i*1, DB_GET_BOTH);
    cget(TRUE, FALSE, 'a', 0, 0, i*1, i*1, next_type);
    put(TRUE,  'b', i*2, i*1);
    put(FALSE, 'b', i*1, i*1);
    put(FALSE, 'b', i*1, i*2);
    put(TRUE,  'b', i*1, 0);
    close_dbs();
    /* ****************************************** */
    setup_dbs(dup_flags);
    put(TRUE, 'c', i*1, i*1);
    put(TRUE, 'c', i*1, i*3);
    early_commit('c');
    cget(TRUE, TRUE, 'a', i*1, i*1, i*1, i*1, DB_GET_BOTH);
    cget(TRUE, TRUE, 'a', 0, 0, i*1, i*3, next_type);
    put(TRUE,  'b', i*2, i*1);
    put(TRUE,  'b', i*1, i*4);
    put(FALSE, 'b', i*1, i*1);
    put(FALSE, 'b', i*1, i*2);
    put(FALSE, 'b', i*1, i*3);
    put(TRUE,  'b', i*1, 0);
    close_dbs();
}

void test_cdel(u_int32_t dup_flags) {
    /* ********************************************************************** */
    setup_dbs(dup_flags);
    put(TRUE, 'c', 1, 1);
    early_commit('c');
    cget(TRUE,  TRUE, 'a', 1, 1, 1, 1, DB_GET_BOTH);
    cdel(TRUE, TRUE, 'a');
    cget(FALSE, TRUE, 'b', 1, 1, 1, 1, DB_GET_BOTH);
    cget(dup_flags != 0, FALSE, 'b', 1, 2, 1, 2, DB_GET_BOTH);
    cget(dup_flags != 0, FALSE, 'b', 1, 0, 1, 0, DB_GET_BOTH);
    cget(TRUE, FALSE, 'b', 0, 0, 0, 0, DB_GET_BOTH);
    cget(TRUE, FALSE, 'b', 2, 10, 2, 10, DB_GET_BOTH);
    close_dbs();
    /* ********************************************************************** */
    setup_dbs(dup_flags);
    put(TRUE, 'c', 1, 1);
    early_commit('c');
    cget(TRUE,  TRUE, 'a', 1, 1, 1, 1, DB_GET_BOTH);
    cget(TRUE,  TRUE, 'b', 1, 1, 1, 1, DB_GET_BOTH);
    cdel(FALSE, TRUE, 'a');
    close_dbs();
}

void test_dbdel(u_int32_t dup_flags) {
    if (dup_flags != 0) {
        if (verbose) printf("Pinhead! Can't dbdel now with duplicates!\n");
        return;
    }
    /* If DB_DELETE_ANY changes to 0, then find is meaningful and 
       has to be fixed in test_dbdel*/
    /* ********************************************************************** */
    setup_dbs(dup_flags);
    put(TRUE, 'c', 1, 1);
    early_commit('c');
    dbdel(TRUE, TRUE, 'a', 1);
    cget(FALSE, TRUE, 'b', 1, 1, 1, 1, DB_GET_BOTH);
    cget(FALSE, TRUE, 'b', 1, 4, 1, 4, DB_GET_BOTH);
    cget(FALSE, TRUE, 'b', 1, 0, 1, 4, DB_GET_BOTH);
    cget(TRUE, FALSE, 'b', 0, 0, 0, 0, DB_GET_BOTH);
    cget(TRUE, FALSE, 'b', 2, 10, 2, 10, DB_GET_BOTH);
    close_dbs();
    /* ********************************************************************** */
    setup_dbs(dup_flags);
    dbdel(TRUE, TRUE, 'a', 1);
    cget(FALSE, TRUE, 'b', 1, 1, 1, 1, DB_GET_BOTH);
    cget(FALSE, TRUE, 'b', 1, 4, 1, 4, DB_GET_BOTH);
    cget(FALSE, TRUE, 'b', 1, 0, 1, 4, DB_GET_BOTH);
    cget(TRUE, FALSE, 'b', 0, 0, 0, 0, DB_GET_BOTH);
    cget(TRUE, FALSE, 'b', 2, 10, 2, 10, DB_GET_BOTH);
    close_dbs();
    /* ********************************************************************** */
    setup_dbs(dup_flags);
    put(TRUE, 'c', 1, 1);
    early_commit('c');
    cget(TRUE,  TRUE, 'b', 1, 1, 1, 1, DB_GET_BOTH);
    dbdel(FALSE, TRUE, 'a', 1);
    dbdel(TRUE, TRUE, 'a', 2);
    dbdel(TRUE, TRUE, 'a', 0);
    close_dbs();
}

void test_current(u_int32_t dup_flags) {
    /* ********************************************************************** */
    setup_dbs(dup_flags);
    put(TRUE, 'a', 1, 1);
    early_commit('a');
    cget(TRUE,  TRUE, 'b', 1, 1, 1, 1, DB_GET_BOTH);
    cget(TRUE,  TRUE, 'b', 1, 1, 1, 1, DB_CURRENT);
    cdel(TRUE, TRUE, 'b');
    cget(TRUE, FALSE, 'b', 1, 1, 1, 1, DB_CURRENT);
    close_dbs();
}

struct dbt_pair {
    DBT key;
    DBT val;
};

struct int_pair {
    int key;
    int val;
};

int got_r_h;

void f_heavi(DBT const *key, DBT const *val, void *extra_f, int r_h) {
    struct int_pair *info = extra_f;

    if (r_h==0) got_r_h = 0;
    assert(key->size == 4);
    assert(val->size == 4);
    
    info->key = *(int*)key->data;
    info->val = *(int*)val->data;
}

void cget_heavi(BOOL success, BOOL find, char txn, int _key, int _val, 
          int _key_expect, int _val_expect, int direction,
          int r_h_expect,
          int (*h)(const DBT*,const DBT*,void*)) {
#if defined(USE_BDB)
    return;
#else
    assert(txns[(int)txn] && cursors[(int)txn]);

    int r;
    struct heavi_extra input;
    struct int_pair output;
    dbt_init(&input.key, &_key, sizeof(int));
    dbt_init(&input.val, &_val, sizeof(int));
    input.db = db;
    output.key = 0;
    output.val = 0;
    
    got_r_h = direction;

    r = cursors[(int)txn]->c_getf_heavi(cursors[(int)txn], 0, //No prelocking
               f_heavi, &output,
               h, &input, direction);
    if (!success) {
        CKERR2s(r, DB_LOCK_DEADLOCK, DB_LOCK_NOTGRANTED);
        return;
    }
    if (!find) {
        CKERR2s(r,  DB_NOTFOUND, DB_KEYEMPTY);
        return;
    }
    CKERR(r);
    assert(got_r_h == r_h_expect);
    assert(output.key == _key_expect);
    assert(output.val == _val_expect);
#endif
}


void test_heavi(u_int32_t dup_flags) {
    /* ********************************************************************** */
    setup_dbs(dup_flags);
    cget_heavi(TRUE, FALSE, 'a', 0, 0, 0, 0,  1, 0, heavi_after); 
    cget_heavi(TRUE, FALSE, 'a', 0, 0, 0, 0, -1, 0, heavi_before); 
    close_dbs();
    /* ********************************************************************** */
    //Not found locks left to right (with empty db == entire db)
    setup_dbs(dup_flags);
    cget_heavi(TRUE, FALSE, 'a', 0, 0, 0, 0,  1, 0, heavi_after); 
    put(FALSE, 'b', 7, 6);
    put(FALSE, 'b', -1, -1);
    put(TRUE,  'a', 4, 4);
    early_commit('a');
    put(TRUE, 'b', 7, 6);
    put(TRUE, 'b', -1, -1);
    close_dbs();
    /* ********************************************************************** */
    //Not found locks left to right (with empty db == entire db)
    setup_dbs(dup_flags);
    cget_heavi(TRUE, FALSE, 'a', 0, 0, 0, 0, -1, 0, heavi_before); 
    put(FALSE, 'b', 7, 6);
    put(FALSE, 'b', -1, -1);
    put(TRUE,  'a', 4, 4);
    early_commit('a');
    put(TRUE, 'b', 7, 6);
    put(TRUE, 'b', -1, -1);
    close_dbs();
    /* ********************************************************************** */
    //Duplicate mode behaves differently.
    setup_dbs(dup_flags);
    int k,v;
    for (k = 10; k <= 100; k+= 10) {
        v = k+5;
        put(TRUE, 'a', k, v);
    }
    if (dup_flags) {
        cget_heavi(TRUE, TRUE, 'a', 100, 0, 100, 105,  1, 0, heavi_after); 
    }
    else {
        cget_heavi(TRUE, FALSE, 'a', 100, 0, 0, 0,  1, 0, heavi_after); 
    }
    close_dbs();
    /* ********************************************************************** */
    //Locks stop at actual elements in the DB.
    setup_dbs(dup_flags);
    //int k,v;
    for (k = 10; k <= 100; k+= 10) {
        v = k+5;
        put(TRUE, 'a', k, v);
    }
    cget_heavi(TRUE, FALSE, 'a', 105, 1, 0, 0,  1, 0, heavi_after); 
    put(FALSE, 'b', 104, 1);
    put(FALSE, 'b', 105, 0);
    put(FALSE, 'b', 105, 1);
    put(FALSE, 'b', 105, 2);
    put(FALSE, 'b', 106, 0);
    put(TRUE,  'b', 99,  0);
    put(dup_flags!=0, 'b', 100, 104);
    close_dbs();
    /* ********************************************************************** */
    // Test behavior of heavi_after
    setup_dbs(dup_flags);
    //int k,v;
    for (k = 10; k <= 100; k+= 10) {
        v = k+5;
        put(TRUE, 'a', k, v);
    }
    for (k = 5; k <= 95; k+= 10) {
        v = k+5;
        cget_heavi(TRUE, TRUE, 'a', k, v, k+5, v+5,  1, 1, heavi_after); 
    }
    put(FALSE, 'b', -1, -2);
    put(TRUE, 'b', 200, 201);
    cget_heavi(FALSE, FALSE, 'a', 105, 105, 0, 0, 1, 0, heavi_after);
    close_dbs();
    /* ********************************************************************** */
    // Test behavior of heavi_before
    setup_dbs(dup_flags);
    //int k,v;
    for (k = 10; k <= 100; k+= 10) {
        v = k+5;
        put(TRUE, 'a', k, v);
    }
    for (k = 105; k >= 15; k-= 10) {
        v = k+5;
        cget_heavi(TRUE, TRUE, 'a', k, v, k-5, v-5,  -1, -1, heavi_before); 
    }
    put(FALSE, 'b', 200, 201);
    put(TRUE,  'b', -1, -2);
    cget_heavi(FALSE, FALSE, 'a', -5, -5, 0, 0, -1, 0, heavi_after);
    close_dbs();
}

void test(u_int32_t dup_flags) {
    /* ********************************************************************** */
    setup_dbs(dup_flags);
    close_dbs();
    /* ********************************************************************** */
    setup_dbs(dup_flags);
    early_abort('a');
    close_dbs();
    /* ********************************************************************** */
    setup_dbs(dup_flags);
    early_commit('a');
    close_dbs();
    /* ********************************************************************** */
    setup_dbs(dup_flags);
    put(TRUE, 'a', 1, 1);
    close_dbs();
    /* ********************************************************************** */
    test_both(dup_flags, DB_SET);
    test_both(dup_flags, DB_GET_BOTH);
    /* ********************************************************************** */
    test_first(dup_flags);
    /* ********************************************************************** */
    test_last(dup_flags);
    /* ********************************************************************** */
    test_set_range(dup_flags);
    /* ********************************************************************** */
    test_both_range(dup_flags);
    /* ********************************************************************** */
    test_next(dup_flags, DB_NEXT);
    test_next(dup_flags, DB_NEXT_NODUP);
    /* ********************************************************************** */
    test_prev(dup_flags, DB_PREV);
    test_prev(dup_flags, DB_PREV_NODUP);
    /* ********************************************************************** */
    test_nextdup(dup_flags, DB_NEXT_DUP, 1);
    #ifdef DB_PREV_DUP
        test_nextdup(dup_flags, DB_PREV_DUP, -1);
    #endif
    /* ********************************************************************** */
    test_cdel(dup_flags);
    /* ********************************************************************** */
    test_dbdel(dup_flags);
    /* ********************************************************************** */
    test_current(dup_flags);
    /* ********************************************************************** */
    test_heavi(dup_flags);
    /* ********************************************************************** */
}


int main(int argc, const char* argv[]) {
    parse_args(argc, argv);
#if defined(USE_BDB)
    if (verbose) {
	printf("Warning: " __FILE__" does not work in BDB.\n");
    }
    return 0;
#endif
    test(0);
    test(DB_DUP | DB_DUPSORT);
    /*
    test_abort(0);
    test_abort(DB_DUP | DB_DUPSORT);
    */
    return 0;
}
