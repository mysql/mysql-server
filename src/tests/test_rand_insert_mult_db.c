/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."
#include "test.h"

#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>
#include <memory.h>
#include <sys/stat.h>
#include <db.h>


static void
test_mult_insert (int num_dbs, int num_elements) {

    DB_TXN * const null_txn = 0;
    int r;

    r = system("rm -rf " ENVDIR);
    CKERR(r);
    r=toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO); assert(r==0);

    /* create the dup database file */
    DB_ENV *env;
    r = db_env_create(&env, 0); assert(r == 0);
    r = env->set_cachesize(env, 0, 4096*4, 1); assert(r==0);
    r = env->open(env, ENVDIR, DB_CREATE+DB_PRIVATE+DB_INIT_MPOOL, 0); assert(r == 0);

    DB* dbs[num_dbs];
    for (int i = 0; i < num_dbs; i++) {
        r = db_create(&dbs[i], env, 0);
        assert(r == 0);
        r = dbs[i]->set_flags(dbs[i], 0);
        assert(r == 0);
        r = dbs[i]->set_pagesize(dbs[i], 4096);
        assert(r == 0);
        char curr_name[15];
        sprintf(curr_name, "main_%d", i);
        r = dbs[i]->open(dbs[i], null_txn, curr_name, 0, DB_BTREE, DB_CREATE, 0666);
        assert(r == 0);
    }


    /* insert n/2 <random(), i> pairs */
    for (int i=0; i<num_elements; i++) {
        for (int j = 0; j < num_dbs; j++) {
            DBT key, val;
            int rand_key = random();
            r = dbs[j]->put(dbs[j], null_txn, dbt_init(&key, &rand_key, sizeof rand_key), dbt_init(&val, &i, sizeof i), 0);
            assert(r == 0);
        }
    } 

    /* reopen the database to force nonleaf buffering */
    for (int i = 0; i < num_dbs; i++) {
        r = dbs[i]->close(dbs[i], 0);
        assert(r == 0);
    }

    r = env->close(env, 0); assert(r == 0);
}

int
test_main(int argc, char *const argv[]) {
    parse_args(argc, argv);
    test_mult_insert(50, 1000);

    return 0;
}
