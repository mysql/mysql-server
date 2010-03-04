/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."
#include "test.h"
#include <stdio.h>
#include <stdlib.h>

#include <sys/stat.h>
#include <time.h>
#include <db.h>

static void
seqinsert (int n, float p) {
    if (verbose) printf("%s %d %f\n", __FUNCTION__, n, p);

    system("rm -rf " ENVDIR);
    toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);

    int r;
    DB_ENV *env;
    r = db_env_create(&env, 0); assert(r == 0);

    r = env->open(env, ENVDIR, DB_INIT_MPOOL + DB_PRIVATE + DB_CREATE, 077); assert(r == 0);

    DB *db;
    r = db_create(&db, env, 0); assert(r == 0);

    r = db->open(db, 0, "test.db", 0, DB_BTREE, DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); assert(r == 0);

    int i;
    for (i = 2; i <= 2*n; i += 2) {
        int k = htonl(i);
        int v = i;
        DBT key, val;
        r = db->put(db, 0, dbt_init(&key, &k, sizeof k), dbt_init(&val, &v, sizeof v), 0); assert(r == 0);
        if (random() <= RAND_MAX * p) {
            k = htonl(i-1);
            v = i-1;
            r = db->put(db, 0, dbt_init(&key, &k, sizeof k), dbt_init(&val, &v, sizeof v), 0); assert(r == 0);
        }
    }

    r = db->close(db, 0); assert(r == 0);

    r = env->close(env, 0); assert(r == 0);
}

int
test_main(int argc, char *const argv[]) {
    srandom(time(0));
    int i;
    for (i=1; i<argc; i++) {
        const char *arg = argv[i];
        if (strcmp(arg, "-v") == 0) {
            verbose++;
            continue;
        }
        if (strcmp(arg, "-seed") == 0) {
            if (i+1 >= argc) return 1;
            srandom(atoi(argv[++i]));
            continue;
        }
    }

    int nodesize = 1024*1024;
    int entrysize = 25;
    int d = nodesize/entrysize;
    int n = d + d/4;

    float ps[] = { 0.0, 0.0001, 0.001, 0.01, 0.1, 0.25, 0.5, 1 };
    for (i=0; i<(int)(sizeof ps / sizeof (float)); i++) {
        seqinsert(n, ps[i]);
    }
    return 0;
}
