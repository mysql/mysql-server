/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."
#include "test.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <sys/stat.h>
#include <db.h>
#include <memory.h>

const char *dbfile = ENVDIR "/" "test.db";
const char *dbname = 0;

static int
db_put (DB *db, int k, int v) {
    DBT key, val;
    int r = db->put(db, 0, dbt_init(&key, &k, sizeof k), dbt_init(&val, &v, sizeof v), 0);
    return r;
}

static int
db_get (DB *db, int k, int expectv, int val_flags) {
    int v;
    DBT key, val;
    memset(&val, 0, sizeof val); val.flags = val_flags;
    if (val.flags == DB_DBT_USERMEM) {
        val.ulen = sizeof v; val.data = &v;
    }
    int r = db->get(db, 0, dbt_init(&key, &k, sizeof k), &val, 0);
    if (r == 0) {
        assert(val.size == sizeof v); 
        if ((val.flags & DB_DBT_USERMEM) == 0) memcpy(&v, val.data, val.size); 
        assert(v == expectv);
    } else {
        if (verbose) printf("%s:%d:%d:%s\n", __FILE__, __LINE__, r, db_strerror(r));
    }
    if (val.flags & (DB_DBT_MALLOC|DB_DBT_REALLOC))
        toku_free(val.data);
    return r;
}

static void
test_db_create (void) {
    int r;
    DB *db;

    unlink(dbfile);
    r = db_create(&db, 0, 0); assert(r == 0);
    db->set_errfile(db,0); // Turn off those annoying errors
    r = db->open(db, 0, dbfile, dbname, DB_BTREE, DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); assert(r == 0);
    r = db_put(db, htonl(1), 1); assert(r == 0);
    r = db_get(db, htonl(1), 1, 0); assert(r == 0);
    r = db_get(db, htonl(1), 1, DB_DBT_USERMEM); assert(r == 0);
    r = db->close(db, 0); assert(r == 0);
}

static void
test_db_thread (void) {
    int r;
    DB *db;

    unlink(dbfile);
    r = db_create(&db, 0, 0); assert(r == 0);
    db->set_errfile(db,0); // Turn off those annoying errors
    r = db->open(db, 0, dbfile, dbname, DB_BTREE, DB_CREATE + DB_THREAD, S_IRWXU+S_IRWXG+S_IRWXO); assert(r == 0);
    r = db_put(db, htonl(1), 1); assert(r == 0);
    r = db_get(db, htonl(1), 1, 0); assert(r == EINVAL);
    r = db_get(db, htonl(1), 1, DB_DBT_MALLOC); assert(r == 0);
    r = db_get(db, htonl(1), 1, DB_DBT_REALLOC); assert(r == 0);
    r = db_get(db, htonl(1), 1, DB_DBT_USERMEM); assert(r == 0);
    r = db->close(db, 0); assert(r == 0);
}

int
test_main(int argc, char *argv[]) {
    parse_args(argc, argv);
    system("rm -rf " ENVDIR);
    toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);
    test_db_create();
    test_db_thread();
    return 0;
}
