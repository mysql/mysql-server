#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <db.h>
#include <byteswap.h>

static int verbose = 0;
static uint64_t maxk = 100000;

static int usage(const char *prog) {
    fprintf(stderr, "%s: run single row insertions with prelocking\n", prog);
    fprintf(stderr, "[--n %"PRIu64"]\n", maxk);
    return 1;
}

static int inserter(DB_ENV *env, DB *db, uint64_t _maxk, int expectr) {
    if (verbose) printf("%p %p\n", env, db);
    int r;
    for (uint64_t k = 0; k < _maxk; k++) {
        
        if (verbose) printf("%"PRIu64"\n", k);

        DB_TXN *txn;
        r = env->txn_begin(env, NULL, &txn, 0);
        assert(r == 0);

        r = db->pre_acquire_table_lock(db, txn);
        assert(r == 0);

        uint64_t kk = bswap_64(k);
        DBT key = { .data = &kk, .size = sizeof kk };
        DBT val = { .data = &k, .size = sizeof k };
        r = db->put(db, txn, &key, &val, DB_NOOVERWRITE);
        assert(r == expectr);

        r = txn->commit(txn, DB_TXN_NOSYNC);
        assert(r == 0);
    }

    return 0;
}

static int env_init(DB_ENV **envptr, const char *envdir) {
    int r;
    DB_ENV *env;

    r = db_env_create(&env, 0);
    if (r == 0) {
        // env setup

        // env open
        r = env->open(env, envdir, DB_CREATE+DB_PRIVATE+DB_INIT_LOCK+DB_INIT_LOG+DB_INIT_MPOOL+DB_INIT_TXN, 0777);
    }
    if (r == 0)
        *envptr = env;
    return r;
}

static int db_init(DB_ENV *env, const char *dbname, DB **dbptr) {
    int r;
    DB *db;

    r = db_create(&db, env, 0);
    if (r == 0) {
        // db create
        r = db->open(db, NULL, dbname, NULL, DB_BTREE, DB_CREATE, 0777);
        if (r != 0) {
            r = db->close(db, 0);
            assert(r == 0);
        }
    }
    if (r == 0)
        *dbptr = db;
    return r;
}   

int main(int argc, char *argv[]) {
    int r;

    for (int i = 1; i < argc; i++) {
        char *arg = argv[i];
        if (strcmp(arg, "--n") == 0 && i+1 < argc) {
            maxk = atoi(argv[++i]);
            continue;
        }
        if (strcmp(arg, "--verbose") == 0 || strcmp(arg, "-v") == 0) {
            verbose++;
            continue;
        }
        if (strcmp(arg, "-q") == 0) {
            verbose = 0;
            continue;
        }

        return usage(argv[0]);
    }

    const char *envdir = "dir." __FILE__ ".dir";
    r = system("rm -rf dir." __FILE__ ".dir");
    assert(r == 0);
    r = mkdir(envdir, 0777);
    assert(r == 0);

    DB_ENV *env;
    r = env_init(&env, envdir);
    assert(r == 0);

    DB *db;
    r = db_init(env, "db0", &db);
    assert(r == 0);

    r = inserter(env, db, maxk, 0);
    assert(r == 0);

    r = inserter(env, db, maxk, DB_KEYEXIST);
    assert(r == 0);

    r = db->close(db, 0);
    assert(r == 0);

    r = env->close(env, 0);
    assert(r == 0);

    return 0;
}
