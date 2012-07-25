/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
#include <stdio.h>
#include <string.h>
#include <db.h>
#include "tokudb_common_funcs.h"
#include <assert.h>

static int verbose = 0;
static int env_open_flags_yesx = DB_CREATE|DB_PRIVATE|DB_INIT_MPOOL|DB_INIT_TXN|DB_INIT_LOG|DB_INIT_LOCK;
static int env_open_flags_nox = DB_CREATE|DB_PRIVATE|DB_INIT_MPOOL;

int test_main(int argc, char * const argv[]) {
    int r;
    const char *envdir = "bench.tokudb";
    const char *dbfilename = "bench.db";
    bool do_txns = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose++;
            continue;
        }
        if (strcmp(argv[i], "-q") == 0) {
            if (verbose > 0)
                verbose--;
            continue;
        }
        if (strcmp(argv[i], "-x") == 0) {
            do_txns = true;
            continue;
        }
    }   

    DB_ENV *env = NULL;
    r = db_env_create(&env, 0); 
    assert(r == 0);

    r = env->open(env, envdir, do_txns ? env_open_flags_yesx : env_open_flags_nox, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
    assert(r == 0);

    DB *db = NULL;
    r = db_create(&db, env, 0); 
    assert(r == 0);

    r = db->open(db, NULL, dbfilename, NULL, DB_BTREE, DB_AUTO_COMMIT, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
    assert(r == 0);

    if (verbose) {
        DB_BTREE_STAT64 s;
        r = db->stat64(db, NULL, &s); assert(r == 0);
        printf("nkeys=%" PRIu64 " dsize=%" PRIu64 "\n", s.bt_nkeys, s.bt_dsize);
    }

    r = db->verify_with_progress(db, NULL, NULL, verbose > 0, false);
    assert(r == 0);

    r = db->close(db, 0);
    assert(r == 0);

    r = env->close(env, 0); 
    assert(r == 0);
    return 0;
}
