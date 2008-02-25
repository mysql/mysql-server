#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <db.h>
#include "test.h"

#define N_TXNS 1

void test_txn_abort(int n, int which_guys_to_abort) {
    if (verbose) printf("test_txn_abort:%d\n", n);

    system("rm -rf " DIR);
    mkdir(DIR, 0777);

    int r;
    DB_ENV *env;
    r = db_env_create(&env, 0); assert(r == 0);
    r = env->set_data_dir(env, DIR);
    r = env->set_lg_dir(env, DIR);
    r = env->open(env, 0, DB_INIT_MPOOL + DB_INIT_LOG + DB_INIT_LOCK + DB_INIT_TXN + DB_PRIVATE + DB_CREATE, 0777); 
    if (r != 0) printf("%s:%d:%d:%s\n", __FILE__, __LINE__, r, db_strerror(r));
    assert(r == 0);

    DB *db;
    {
	DB_TXN *txn;
	r = env->txn_begin(env, 0, &txn, 0); assert(r == 0);
	
	r = db_create(&db, env, 0); assert(r == 0);
	r = db->open(db, txn, "test.db", 0, DB_BTREE, DB_CREATE, 0777); assert(r == 0);
	r = txn->commit(txn, 0); assert(r == 0);
    }
    {
	DB_TXN *txns[N_TXNS];
	{
	    int j;
	    for (j=0; j<N_TXNS; j++) {
		r = env->txn_begin(env, 0, &txns[j], 0); assert(r == 0);
	    }
	}
	
	{
	    int i;
	    for (i=0; i<n; i++) {
		int j;
		for (j=N_TXNS; j>0; j--) {
		    if (i%j==0) { // This is guaranteed to be true when j==1, so someone will do it.
			DBT key, val;
			r = db->put(db, txns[j], dbt_init(&key, &i, sizeof i), dbt_init(&val, &i, sizeof i), 0); 
			if (r != 0) printf("%s:%d:%d:%s\n", __FILE__, __LINE__, r, db_strerror(r));
			assert(r == 0);
			break;
		    }
		}
	    }
	}
	{
	    int j;
	    for (j=0; j<N_TXNS; j++) {
		if (which_guys_to_abort&(1<<j)) {
		    r = txns[j]->abort(txns[j]);
		} else {
		    r = txns[j]->commit(txns[j], 0);
		}
	    }
	}
    }
#if 0
    assert(r == 0);
#else
    if (r != 0) printf("%s:%d:abort:%d\n", __FILE__, __LINE__, r);
#endif

    r = db->close(db, 0); assert(r == 0);
    r = env->close(env, 0); assert(r == 0);
}

int main(int argc, char *argv[]) {
    int i,j;
    for (i = 1; i < argc; i++) {
        char *arg = argv[i];
        if (strcmp(arg, "-v") == 0 || strcmp(arg, "--verbose") == 0) {
            verbose++;
            continue;
        }
    }
    for (j=0; j<(1<<N_TXNS); j++)
	for (i=1; i<100; i*=2) 
	    test_txn_abort(i, j);
    return 0;
}
