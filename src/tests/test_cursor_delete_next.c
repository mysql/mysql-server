/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#include <string.h>
#include <db.h>
#include <assert.h>
#include <errno.h>
#include <sys/stat.h>

#include "test.h"

// DIR is defined in the Makefile

DBT *dbt_init(DBT *dbt, void *data, u_int32_t size) {
    memset(dbt, 0, sizeof *dbt);
    dbt->data = data;
    dbt->size = size;
    return dbt;
}

DB *db;
DB_ENV *env;
DBT key;
DBT value;
DBC *dbc;
DB_TXN *const null_txn = 0;

void setup_db(char* name) {
    int r;

    system("rm -rf " DIR);
    mkdir(DIR, 0777);

    r = db_env_create(&env, 0);                     CKERR(r);
    r = env->open(env, DIR, DB_CREATE | DB_PRIVATE | DB_INIT_MPOOL, 0666);  CKERR(r);
    r = db_create(&db, env, 0);                     CKERR(r);
    r = db->set_flags(db, DB_DUP | DB_DUPSORT);     CKERR(r);
    r = db->set_pagesize(db, 4096);                 CKERR(r);
    r = db->open(db, null_txn, name, "main", DB_BTREE, DB_CREATE, 0666);    CKERR(r);
}

void close_db() {
    int r;

    r = db->close(db, 0);                           CKERR(r);
    r = env->close(env, 0);                         CKERR(r);
}

void insert() {
    int r;

    dbt_init(&key, "key", sizeof("key"));
    dbt_init(&value, "value1", sizeof("value1"));
    r = db->put(db, null_txn, &key, &value, 0);     CKERR(r);

    dbt_init(&key, "key", sizeof("key"));
    dbt_init(&value, "value2", sizeof("value2"));
    r = db->put(db, null_txn, &key, &value, 0);     CKERR(r);
}

void cursor_range_with_delete(u_int32_t flag) {
    int r;

    r = db->cursor(db, null_txn, &dbc, 0);          CKERR(r);
    r = dbc->c_get(dbc, &key, &value, DB_FIRST);    CKERR(r);
    r = dbc->c_del(dbc, 0);                         CKERR(r);
    r = dbc->c_get(dbc, &key, &value, flag);        CKERR(r);
    r = dbc->c_del(dbc, 0);                         CKERR(r);
    r = dbc->c_close(dbc);                          CKERR(r);
}

int main() {
    setup_db("next.db");
    insert();
    cursor_range_with_delete(DB_NEXT);
    close_db();
    
    setup_db("nextdup.db");
    insert();
    cursor_range_with_delete(DB_NEXT_DUP);
    close_db();
    
    return 0;
}
