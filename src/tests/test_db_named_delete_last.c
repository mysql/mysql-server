/* -*- mode: C; c-basic-offset: 4 -*- */
#include <toku_portability.h>
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#include <memory.h>
#include <toku_portability.h>
#include <db.h>

#include <errno.h>
#include <sys/stat.h>

#include "test.h"

// ENVDIR is defined in the Makefile
#define FNAME       "foo.tokudb"
char *name = NULL;

#define NUM         8
#define MAX_LENGTH  (1<<16)

DB_ENV *env;

DB *db;
DB_TXN *null_txn;

static void
open_db(void) {
    int r = db_create(&db, env, 0);
    CKERR(r);
    r = db->open(db, null_txn, FNAME, name, DB_BTREE, DB_CREATE, 0666);
    CKERR(r);
}

static void
delete_db(void) {
    int r = db_create(&db, env, 0);
    CKERR(r);
    r = db->remove(db, FNAME, name, 0);
}

static void
close_db(void) {
    int r;
    r = db->close(db, 0);
    CKERR(r);
}

static void
setup_data(void) {
    int r = db_env_create(&env, 0);                                           CKERR(r);
    const int envflags = DB_CREATE|DB_INIT_MPOOL|DB_INIT_TXN|DB_INIT_LOCK |DB_THREAD |DB_PRIVATE;
    r = env->open(env, ENVDIR, envflags, S_IRWXU+S_IRWXG+S_IRWXO);        CKERR(r);
}

static void
runtest(void) {
    system("rm -rf " ENVDIR);
    int r=toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO); assert(r==0);
    setup_data();

    name = "foo";
    open_db();
    close_db();
    delete_db();

    name = "foo1";
    open_db();
    close_db();
    name = "foo2";
    open_db();
    close_db();
    name = "foo1";
    delete_db();
    name = "foo2";
    delete_db();

    name = "foo1";
    open_db();
    close_db();
    name = "foo2";
    open_db();
    close_db();
    name = "foo2";
    delete_db();
    name = "foo1";
    delete_db();

    env->close(env, 0);
}


int
test_main(int argc, char *argv[]) {
    parse_args(argc, argv);

    runtest();
    return 0;
}                                        

