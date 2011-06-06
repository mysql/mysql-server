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
test_cursor_delete_2119 (u_int32_t c_del_flags, u_int32_t txn_isolation_flags) {
    int r;
    r = system("rm -rf " ENVDIR);
        CKERR(r);
    r = toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);
        CKERR(r);
    DBT key,val;

    r = db_env_create(&dbenv, 0);                                                            CKERR(r);
    r = dbenv->open(dbenv, ENVDIR, DB_PRIVATE|DB_INIT_MPOOL|DB_CREATE|DB_INIT_TXN|DB_INIT_LOCK, 0);       CKERR(r);

    r = db_create(&db, dbenv, 0);                                                            CKERR(r);
    r = dbenv->txn_begin(dbenv, 0, &txn, txn_isolation_flags);                               CKERR(r);
    r = db->open(db, txn, "primary.db", NULL, DB_BTREE, DB_CREATE, 0600);                    CKERR(r);
    r = txn->commit(txn, 0);                                                                 CKERR(r);

    r = dbenv->txn_begin(dbenv, 0, &txn, txn_isolation_flags);                               CKERR(r);
    r = db->put(db, txn, dbt_init(&key, "a", 2), dbt_init(&val, "b", 2), 0);   CKERR(r);
    r = txn->commit(txn, 0);                                                                 CKERR(r);

    r = dbenv->txn_begin(dbenv, 0, &txn, txn_isolation_flags);                               CKERR(r);
    r = db->del(db, txn, dbt_init(&key, "a", 2), 0);                                         CKERR(r);
    r = txn->commit(txn, 0);                                                                 CKERR(r);

    r = dbenv->txn_begin(dbenv, 0, &txn, txn_isolation_flags);                               CKERR(r);
    r = db->put(db, txn, dbt_init(&key, "a", 2), dbt_init(&val, "c", 2), 0);   CKERR(r);

    cursor=cursor;

    r = db->cursor(db, txn, &cursor, 0);                                                     CKERR(r);
    r = cursor->c_get(cursor, dbt_init_malloc(&key), dbt_init_malloc(&val), DB_FIRST);       CKERR(r);
    assert(strcmp(key.data, "a")==0);  toku_free(key.data);
    assert(strcmp(val.data, "c")==0);  toku_free(val.data);
    r = cursor->c_del(cursor, c_del_flags);                                                  CKERR(r);
    r = cursor->c_del(cursor, c_del_flags);                                                  assert(r==DB_KEYEMPTY);
    r = cursor->c_get(cursor, dbt_init_malloc(&key), dbt_init_malloc(&val), DB_NEXT);        assert(r==DB_NOTFOUND);

    r = cursor->c_close(cursor);                                                             CKERR(r);
    r = txn->commit(txn, 0);                                                                 CKERR(r);



    r = db->close(db, 0);                                                                    CKERR(r);
    r = dbenv->close(dbenv, 0);                                                              CKERR(r);
}

int
test_main(int argc, char *const argv[]) {

    parse_args(argc, argv);

    int isolation;
    int read_prelocked;
    int write_prelocked;
    for (isolation = 0; isolation < 2; isolation++) {
        u_int32_t isolation_flag = isolation ? DB_READ_UNCOMMITTED : 0;
        for (read_prelocked = 0; read_prelocked < 2; read_prelocked++) {
            u_int32_t read_prelocked_flag = read_prelocked ? DB_PRELOCKED : 0;
            for (write_prelocked = 0; write_prelocked < 2; write_prelocked++) {
                u_int32_t write_prelocked_flag = write_prelocked ? DB_PRELOCKED_WRITE : 0;
                test_cursor_delete_2119(read_prelocked_flag | write_prelocked_flag,
                                        isolation_flag);
            }
        }
    }
  
    
    return 0;
}
