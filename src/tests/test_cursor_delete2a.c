/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."
#include "test.h"

#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>
#include <memory.h>
#include <errno.h>
#include <sys/stat.h>
#include <db.h>


static DB_ENV *dbenv;
static DB *db;
static DB_TXN * txn;
static DBC *cursor;

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
    r = db->put(db, txn, dbt_init(&key, "a", 2), dbt_init(&val, "c", 2), DB_YESOVERWRITE);   CKERR(r);

    cursor=cursor;

    r = db->cursor(db, txn, &cursor, 0);                                                     CKERR(r);
    r = cursor->c_get(cursor, dbt_init_malloc(&key), dbt_init_malloc(&val), DB_FIRST);       CKERR(r);
    assert(strcmp(key.data, "a")==0);  toku_free(key.data);
    assert(strcmp(val.data, "c")==0);  toku_free(val.data);
    r = cursor->c_del(cursor, 0);                                                            CKERR(r);
    r = cursor->c_del(cursor, 0);                                                            assert(r==DB_KEYEMPTY);
    r = cursor->c_get(cursor, dbt_init_malloc(&key), dbt_init_malloc(&val), DB_NEXT);        assert(r==DB_NOTFOUND);

    r = cursor->c_close(cursor);                                                             CKERR(r);
    r = txn->commit(txn, 0);                                                                 CKERR(r);



    r = db->close(db, 0);                                                                    CKERR(r);
    r = dbenv->close(dbenv, 0);                                                              CKERR(r);
}

int
test_main(int argc, char *const argv[]) {

    parse_args(argc, argv);
  
    int r;
    r = system("rm -rf " ENVDIR);
    CKERR(r);
    toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);
    
    test_cursor_delete2();

    return 0;
}
