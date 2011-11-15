#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include <db.h>
#include "tokudb_common_funcs.h"

static int verbose = 0;
static int env_open_flags_yesx = DB_CREATE|DB_PRIVATE|DB_INIT_MPOOL|DB_INIT_TXN|DB_INIT_LOG|DB_INIT_LOCK;
static int env_open_flags_nox = DB_CREATE|DB_PRIVATE|DB_INIT_MPOOL;

int test_main(int argc, char * const argv[]) {
    int r;
    char *envdir = "bench.tokudb";
    char *dbfilename = "bench.db";
    bool do_txns = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose++;
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

    r = db->verify_with_progress(db, NULL, NULL, verbose > 0, false);
    assert(r == 0);

    r = db->close(db, 0);
    assert(r == 0);

    r = env->close(env, 0); 
    assert(r == 0);
    return 0;
}
