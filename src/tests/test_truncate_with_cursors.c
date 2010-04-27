/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."
#include "test.h"
// truncate a database with open cursors
// verify that the truncate returns EINVAL
// BDB returns 0 but calls the error callback

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <db.h>

#ifdef USE_BDB
int test_errors;

static void
test_errcall (const DB_ENV *env __attribute__((__unused__)), const char *errpfx, const char *msg) {
    if (verbose) fprintf(stderr, "%s %s\n", errpfx, msg);
    test_errors++;
}

#define DB_TRUNCATE_WITHCURSORS 0
#endif

// try to truncate with cursors active
static int
test_truncate_with_cursors (int n, u_int32_t trunc_flag) {
#ifdef USE_BDB
    test_errors = 0;
#endif
    int r;
    
    DB_ENV *env;
    DB *db;
    DBC *cursor;

    r = db_env_create(&env, 0); assert(r == 0);
    r = env->open(env, ENVDIR, DB_INIT_MPOOL + DB_PRIVATE + DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); assert(r == 0);

    int i;

    // populate the tree
    r = db_create(&db, env, 0); assert(r == 0);
    r = db->open(db, 0, "test.db", 0, DB_BTREE, DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); assert(r == 0);

    for (i=0; i<n; i++) {
        int k = htonl(i); int v = i;
        DBT key, val;
        r = db->put(db, 0, dbt_init(&key, &k, sizeof k), dbt_init(&val, &v, sizeof v), 0); assert(r == 0);
    }

    r = db->close(db, 0); assert(r == 0);

    // test1:
    // walk the tree - expect n
    // truncate
    // walk the tree - expect 0
    r = db_create(&db, env, 0); assert(r == 0);
    r = db->open(db, 0, "test.db", 0, DB_UNKNOWN, 0, S_IRWXU+S_IRWXG+S_IRWXO); assert(r == 0);

    i = 0;
    r = db->cursor(db, 0, &cursor, 0); assert(r == 0);
    while (1) {
        DBT key, val;
        r = cursor->c_get(cursor, dbt_init(&key, 0, 0), dbt_init(&val, 0, 0), DB_NEXT);
        if (r == DB_NOTFOUND) break;
        i++;
    }
    assert(i == n);

    // try to truncate with an active cursor
#ifdef USE_BDB
    db->set_errcall(db, test_errcall);
    assert(test_errors == 0);
#endif
    u_int32_t row_count = 0;
    r = db->truncate(db, 0, &row_count, trunc_flag); 

    BOOL truncated = FALSE;
#ifdef USE_BDB
    // It looks like for 4.6 there's no error code, even though the documentation says "it is an error to truncate with open cursors".
    // For 4.3 and 4.7 the error code is EINVAL
    // I don't know where the boundary really is:  Is it an error in 4.5 or 4.4?
    if (DB_VERSION_MAJOR==4 && DB_VERSION_MINOR>=4 && DB_VERSION_MINOR < 7) {
	assert(r == 0 && test_errors);
    } else { 
	assert(r == EINVAL && test_errors);
    }
#else
    if (trunc_flag == 0)
        assert(r == EINVAL);
    else {
        assert(trunc_flag == DB_TRUNCATE_WITHCURSORS);
        assert(r == 0);
        truncated = TRUE;
    }
#endif

    r = cursor->c_close(cursor); assert(r == 0);
    // ok, now try it
    if (!truncated) {
        r = db->truncate(db, 0, &row_count, 0); assert(r == 0);
    }

    i = 0;
    r = db->cursor(db, 0, &cursor, 0); assert(r == 0);
    while (1) {
        DBT key, val;
        r = cursor->c_get(cursor, dbt_init(&key, 0, 0), dbt_init(&val, 0, 0), DB_NEXT);
        if (r == DB_NOTFOUND) break;
        i++;
    }
    r = cursor->c_close(cursor); assert(r == 0);
    assert(i == 0);

    r = db->close(db, 0); assert(r == 0);

    // test 2: walk the tree - expect 0
    r = db_create(&db, env, 0); assert(r == 0);
    r = db->open(db, 0, "test.db", 0, DB_UNKNOWN, 0, S_IRWXU+S_IRWXG+S_IRWXO); assert(r == 0);

    i = 0;
    r = db->cursor(db, 0, &cursor, 0); assert(r == 0);
    while (1) {
        DBT key, val;
        r = cursor->c_get(cursor, dbt_init(&key, 0, 0), dbt_init(&val, 0, 0), DB_NEXT);
        if (r == DB_NOTFOUND) break;
        i++;
    }
    r = cursor->c_close(cursor); assert(r == 0);
    assert(i == 0);

    r = db->close(db, 0); assert(r == 0);

    r = env->close(env, 0); assert(r == 0);
    return 0;
}

int
test_main(int argc, char *const argv[]) {
    parse_args(argc, argv);
    int nodesize = 1024*1024;
    int leafentry = 25;
    int n = (nodesize/leafentry) * 2;
    int r;
    r = system("rm -rf " ENVDIR);
    CKERR(r);
    toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);
    r = test_truncate_with_cursors(n, 0);
    CKERR(r);
    r = system("rm -rf " ENVDIR);
    CKERR(r);
    toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);
    r = test_truncate_with_cursors(n, DB_TRUNCATE_WITHCURSORS);
    CKERR(r);
    return 0;
}
