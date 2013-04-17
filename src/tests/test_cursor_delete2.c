/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007,2008 Tokutek Inc.  All rights reserved."

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <memory.h>
#include <errno.h>
#include <sys/stat.h>
#include <toku_portability.h>
#include <db.h>

#include "test.h"

static DB_ENV *dbenv;
static DB *db;
static DB_TXN * txn;

static void
test_cursor_delete2 (void) {
    int r;
    DBT key,val;

    r = db_env_create(&dbenv, 0);                                                            CKERR(r);
    r = dbenv->open(dbenv, ENVDIR, DB_PRIVATE|DB_INIT_MPOOL|DB_CREATE|DB_INIT_TXN, 0);       CKERR(r);

    r = db_create(&db, dbenv, 0);                                                            CKERR(r);
    r = dbenv->txn_begin(dbenv, 0, &txn, 0);                                                 CKERR(r);
    r = db->open(db, txn, "primary.db", NULL, DB_BTREE, DB_CREATE, 0600);                    CKERR(r);
    r = txn->commit(txn, 0);                                                                 CKERR(r);

    r = dbenv->txn_begin(dbenv, 0, &txn, 0);                                                 CKERR(r);
    r = db->put(db, txn, dbt_init(&key, "a", 2), dbt_init(&val, "b", 2), DB_YESOVERWRITE);   CKERR(r);
    r = txn->commit(txn, 0);                                                                 CKERR(r);

    r = dbenv->txn_begin(dbenv, 0, &txn, 0);                                                 CKERR(r);
    r = db->del(db, txn, dbt_init(&key, "a", 2), 0);                                         CKERR(r);
    r = txn->commit(txn, 0);                                                                 CKERR(r);

    r = dbenv->txn_begin(dbenv, 0, &txn, 0);                                                 CKERR(r);
    r = db->del(db, txn, dbt_init(&key, "a", 2), DB_DELETE_ANY);                             CKERR_depending(r,0,DB_NOTFOUND);
    r = txn->commit(txn, 0);                                                                 CKERR(r);

    r = dbenv->txn_begin(dbenv, 0, &txn, 0);                                                 CKERR(r);
    r = db->put(db, txn, dbt_init(&key, "a", 2), dbt_init(&val, "c", 2), DB_YESOVERWRITE);   CKERR(r);
    r = db->del(db, txn, dbt_init(&key, "a", 2), 0);                                         CKERR(r);
    r = txn->commit(txn, 0);                                                                 CKERR(r);

    r = dbenv->txn_begin(dbenv, 0, &txn, 0);                                                 CKERR(r);
    r = db->put(db, txn, dbt_init(&key, "a", 2), dbt_init(&val, "c", 2), DB_YESOVERWRITE);   CKERR(r);
    r = txn->commit(txn, 0);                                                                 CKERR(r);

    r = dbenv->txn_begin(dbenv, 0, &txn, 0);                                                 CKERR(r);
    r = db->del(db, txn, dbt_init(&key, "a", 2), 0);                                         CKERR(r);
    r = db->del(db, txn, dbt_init(&key, "a", 2), DB_DELETE_ANY);                             CKERR_depending(r,0,DB_NOTFOUND);
    r = txn->commit(txn, 0);                                                                 CKERR(r);

    r = db->close(db, 0);                                                                    CKERR(r);
    r = dbenv->close(dbenv, 0);                                                              CKERR(r);
}

int
test_main(int argc, const char *argv[]) {

    parse_args(argc, argv);
  
    system("rm -rf " ENVDIR);
    toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);
    
    test_cursor_delete2();

    return 0;
}
