// truncate a database with open cursors
// verify that the truncate returns EINVAL

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <db.h>
#include "test.h"

// try to truncate with cursors active
int test_truncate_with_cursors(int n) {
    int r;
    
    DB_ENV *env;
    DB *db;
    DBC *cursor;

    r = db_env_create(&env, 0); assert(r == 0);
    r = env->open(env, ENVDIR, DB_INIT_MPOOL + DB_PRIVATE + DB_CREATE, 0777); assert(r == 0);

    int i;

    // populate the tree
    r = db_create(&db, env, 0); assert(r == 0);
    r = db->open(db, 0, "test.db", 0, DB_BTREE, DB_CREATE, 0777); assert(r == 0);

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
    r = db->open(db, 0, "test.db", 0, DB_UNKNOWN, 0, 0777); assert(r == 0);

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
    u_int32_t row_count = 0;
    r = db->truncate(db, 0, &row_count, 0); assert(r == EINVAL);

    r = cursor->c_close(cursor); assert(r == 0);

    // ok, now try it
    r = db->truncate(db, 0, &row_count, 0); assert(r == 0);

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
    r = db->open(db, 0, "test.db", 0, DB_UNKNOWN, 0, 0777); assert(r == 0);

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

int main(int argc, const char *argv[]) {
    parse_args(argc, argv);
    int nodesize = 1024*1024;
    int leafentry = 25;
    int n = (nodesize/leafentry) * 2;
    system("rm -rf " ENVDIR);
    mkdir(ENVDIR, 0777);
    int r = test_truncate_with_cursors(n);
    return r;
}
