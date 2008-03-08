/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#include <string.h>
#include <db.h>
#include <assert.h>
#include <errno.h>
#include <sys/stat.h>

#include "test.h"

// ENVDIR is defined in the Makefile

int dbtcmp(DBT *dbt1, DBT *dbt2) {
    int r;
    
    r = dbt1->size - dbt2->size;  if (r) return r;
    return memcmp(dbt1->data, dbt2->data, dbt1->size);
}

DB *db;
DB_ENV* dbenv;
DBC*    cursors[(int)256];
DB_TXN* null_txn = NULL;

void put(char txn, int _key, int _data) {
    int r;
    DBT key;
    DBT data;
    dbt_init(&key,  &_key,  sizeof(int));
    dbt_init(&data, &_data, sizeof(int));
    if (_key == -1) {
        key.data = NULL;
        key.size = 0;
    }
    if (_data == -1) {
        data.data = NULL;
        data.size = 0;
    }
    
    r = db->put(db, null_txn, &key, &data, DB_YESOVERWRITE);
    CKERR(r);
}

void cget(u_int32_t flag, BOOL find, char txn, int _key, int _data) {
    assert(cursors[(int)txn]);

    int r;
    DBT key;
    DBT data;
    if (flag == DB_CURRENT) {
        _key++;
        _data++;
        dbt_init(&key,  &_key,  sizeof(int));
        dbt_init(&data, &_data, sizeof(int));
        _key--;
        _data--;
    }
    else if (flag == DB_SET) {
        dbt_init(&key,  &_key,  sizeof(int));
        if (_key == -1) {
            key.data = NULL;
            key.size = 0;
        }
        _data++;
        dbt_init(&data, &_data, sizeof(int));
        _data--;
    }
    else assert(FALSE);
    r = cursors[(int)txn]->c_get(cursors[(int)txn], &key, &data, flag);
    if (find) {
        CKERR(r);
        if (_key == -1) {
            assert(key.data == NULL);
            assert(key.size == 0);
        }
        else {
            assert(key.size == sizeof(int));
            assert(*(int*)key.data == _key);
        }
        if (_data == -1) {
            assert(data.data == NULL);
            assert(data.size == 0);
        }
        else {
            assert(data.size == sizeof(int));
            assert(*(int*)data.data == _data);
        }
    }
    else        CKERR2(r, DB_NOTFOUND);
}

void init_dbc(char name) {
    int r;

    assert(!cursors[(int)name]);
    r = db->cursor(db, null_txn, &cursors[(int)name], 0);
        CKERR(r);
    assert(cursors[(int)name]);
}

void close_dbc(char name) {
    int r;

    assert(cursors[(int)name]);
    r = cursors[(int)name]->c_close(cursors[(int)name]);
        CKERR(r);
    cursors[(int)name] = NULL;
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
    u_int32_t env_txn_flags  = 0;
    u_int32_t env_open_flags = DB_CREATE | DB_PRIVATE | DB_INIT_MPOOL;
	r = dbenv->open(dbenv, ENVDIR, env_open_flags | env_txn_flags, 0600);
        CKERR(r);
    
    r = db_create(&db, dbenv, 0);
        CKERR(r);
    if (dup_flags) {
        r = db->set_flags(db, dup_flags);
            CKERR(r);
    }

    char a;
    r = db->open(db, null_txn, "foobar.db", NULL, DB_BTREE, DB_CREATE, 0600);
        CKERR(r);
    for (a = 'a'; a <= 'z'; a++) init_dbc(a);
}

void close_dbs(void) {
    char a;
    for (a = 'a'; a <= 'z'; a++) {
        if (cursors[(int)a]) close_dbc(a);
    }

    int r;
    r = db->close(db, 0);
        CKERR(r);
    db      = NULL;
    r = dbenv->close(dbenv, 0);
        CKERR(r);
    dbenv   = NULL;
}

void test(u_int32_t dup_flags) {
    /* ********************************************************************** */
    int key;
    int data;
    int i;
    for (i = 0; i < 4; i++) {
        if (i & 0x1) key  = -1;
        else         key  = 1;
        if (i & 0x2) data = -1;
        else         data = 1;
        setup_dbs(dup_flags);
        put('a', key, data);
        cget(DB_SET,     TRUE, 'a', key, data);
        cget(DB_CURRENT, TRUE, 'a', key, data);
        close_dbs();
    }
    /* ********************************************************************** */
}

int main() {
    test(0);
    test(DB_DUP | DB_DUPSORT);
    return 0;
}
