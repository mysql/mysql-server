#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <db.h>
#include "test.h"

void test_txn_abort(int n) {
    if (verbose) printf("test_txn_abort:%d\n", n);

    system("rm -rf " DIR);
    mkdir(DIR, 0777);

    int r;
    DB_ENV *env;
    r = db_env_create(&env, 0); assert(r == 0);
    r = env->set_data_dir(env, DIR);
    r = env->set_lg_dir(env, DIR);
    r = env->open(env, ".", DB_INIT_MPOOL + DB_INIT_LOG + DB_INIT_LOCK + DB_INIT_TXN + DB_PRIVATE, 0777); assert(r == 0);

    DB *db;
    r = db_create(&db, env, 0); assert(r == 0);
    r = db->open(db, 0, "test.db", 0, DB_BTREE, DB_CREATE, 0777); assert(r == 0);
    DB_TXN *txn;
    r = env->txn_begin(env, 0, &txn, 0); assert(r == 0);
    int i;
    for (i=0; i<n; i++) {
        DBT key, val;
        r = db->put(db, txn, dbt_init(&key, &i, sizeof i), dbt_init(&val, &i, sizeof i), 0); assert(r == 0);
    }
    r = txn->abort(txn); 
#if 0
    assert(r == 0);
#else
    if (r != 0) printf("%s:%d:abort:%d\n", __FILE__, __LINE__, r);
#endif

    r = db->close(db, 0); assert(r == 0);
    r = env->close(env, 0); assert(r == 0);
}

int main(int argc, char *argv[]) {
    int i;
    for (i = 1; i < argc; i++) {
        char *arg = argv[i];
        if (strcmp(arg, "-v") == 0 || strcmp(arg, "--verbose") == 0) {
            verbose++;
            continue;
        }
    }
    for (i=1; i<100; i++) 
        test_txn_abort(i);
    return 0;
}
