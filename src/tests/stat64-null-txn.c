/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

// test the stat64 function on flat databases

#include "test.h"

#include <db.h>
#include <sys/stat.h>

static void
test_stat64 (unsigned int N) {
    if (verbose) printf("%s:%d\n", __FUNCTION__, __LINE__);

    int r;
    r = system("rm -rf " ENVDIR);
    CKERR(r);
    toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);
    
    DB_ENV *env;
    DB *db;
    DB_TXN *txn;
    r = db_env_create(&env, 0);                                           CKERR(r);
    r = env->set_redzone(env, 0);                                         CKERR(r);

    r = env->set_cachesize(env, 0, 20*1000000, 1);
    /* Open the environment without transactions. */
    r = env->open(env, ENVDIR, DB_INIT_LOCK|DB_INIT_MPOOL|DB_CREATE|DB_PRIVATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
    r = db_create(&db, env, 0);                                           CKERR(r);

    {
    /* Don't begin a transaction, just set it to null.
	r=env->txn_begin(env, 0, &txn, 0); assert(r==0);
    */
    txn = NULL;
	r=db->open(db, txn, "foo.db", 0, DB_BTREE, DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
	/* r=txn->commit(txn, 0);    assert(r==0); */
    }

    /* No transactions.
    r=env->txn_begin(env, 0, &txn, 0);  CKERR(r);
    */

    // insert sequential keys into the databases

    unsigned int i;
    u_int64_t dsize=0;
    for (i=0; i<N; i++) {
        if (verbose>1 && i % (1<<14) == 0) {
            printf("%s(total=%u) inserted %u so far\n", __FILE__, N, i);
            fflush(stdout);
        }
	char hello[30], there[30];
	snprintf(hello, sizeof(hello), "hello%8d", i);
	snprintf(there, sizeof(there), "there%d", i);
	DBT key, val;
	r=db->put(db, txn,
		      dbt_init(&key, hello, strlen(hello)+1),
		      dbt_init(&val, there, strlen(there)+1),
		      0);
	dsize += strlen(hello)+1 + strlen(there)+1;
	CKERR(r);
    }
    /* r=txn->commit(txn, 0); CKERR(r); */

    // get and verify stats, should be treated as estimates
    /* r=env->txn_begin(env, 0, &txn, 0);  CKERR(r); */
    {
        DB_BTREE_STAT64 s;
        r=db->stat64(db, txn, &s); CKERR(r);
        if (verbose) {
            r = system("ls -l " ENVDIR);
            CKERR(r);
            printf("nkeys=%" PRIu64 "\nndata=%" PRIu64 "\ndsize=%" PRIu64 "\n",
                   s.bt_nkeys, s.bt_ndata, s.bt_dsize);
            printf("fsize=%" PRIu64 "\n", s.bt_fsize);
            printf("expected dsize=%" PRIu64 "\n", dsize); 
        }
        assert(0 < s.bt_nkeys && s.bt_nkeys <= N);
        assert(s.bt_ndata == s.bt_nkeys);
        assert(0 < s.bt_dsize && s.bt_dsize <= dsize);
        assert(s.bt_fsize > N);
    }
    /* r=txn->commit(txn, 0); CKERR(r); */

    // get the last row, this forces the root estimates to be updated
    {
        /* r = env->txn_begin(env, NULL, &txn, 0); CKERR(r); */
        DBC *c = NULL;
        r = db->cursor(db, txn, &c, 0); CKERR(r);
        DBT key = {.size = 0};
        DBT val = {.size = 0};
        r = c->c_get(c, &key, &val, DB_LAST);
        CKERR(r);
        r = c->c_close(c); CKERR(r);
        /* r = txn->commit(txn, 0); CKERR(r); */
    }

    // get and verify stats
    /* r=env->txn_begin(env, 0, &txn, 0);  CKERR(r); */
    {
        DB_BTREE_STAT64 s;
        r=db->stat64(db, txn, &s); CKERR(r);
        if (verbose) {
            r = system("ls -l " ENVDIR);
            CKERR(r);
            printf("nkeys=%" PRIu64 "\nndata=%" PRIu64 "\ndsize=%" PRIu64 "\n",
                   s.bt_nkeys, s.bt_ndata, s.bt_dsize);
            printf("fsize=%" PRIu64 "\n", s.bt_fsize);
            printf("expected dsize=%" PRIu64 "\n", dsize); 
        }
        assert(0 < s.bt_nkeys && s.bt_nkeys <= N);
        assert(s.bt_ndata == s.bt_nkeys);
        assert(0 < s.bt_dsize && s.bt_dsize <= dsize);
        assert(s.bt_fsize > N);
    }
    /* r=txn->commit(txn, 0); CKERR(r); */

    r=db->close(db, 0); CKERR(r);

    r=env->close(env, 0); CKERR(r);
}

int
test_main (int argc, char *const argv[])
{
    parse_args(argc, argv);
    test_stat64(40000);
    test_stat64(400000);
    return 0;
}
