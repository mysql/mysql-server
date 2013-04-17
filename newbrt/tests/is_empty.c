/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id$"
#ident "Copyright (c) 2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "test.h"
#include "includes.h"
#include "toku_os.h"
#include "checkpoint.h"


#define TESTDIR "dir." __FILE__ 
#define FILENAME "test0.brt"

static void do_yield (voidfp f, void *fv, void *UU(v)) {
    if (f) f(fv);
}


static void test_it (int N) {
    BRT brt;
    int r;
    system("rm -rf " TESTDIR);
    r = toku_os_mkdir(TESTDIR, S_IRWXU);                                                                    CKERR(r);

    TOKULOGGER logger;
    r = toku_logger_create(&logger);                                                                        CKERR(r);
    r = toku_logger_open(TESTDIR, logger);                                                                  CKERR(r);


    CACHETABLE ct;
    r = toku_brt_create_cachetable(&ct, 0, ZERO_LSN, logger);                                               CKERR(r);
    toku_cachetable_set_env_dir(ct, TESTDIR);

    toku_logger_set_cachetable(logger, ct);

    r = toku_logger_open_rollback(logger, ct, TRUE);                                                        CKERR(r);

    TOKUTXN txn;
    r = toku_txn_begin_txn((DB_TXN*)NULL, (TOKUTXN)0, &txn, logger, TXN_SNAPSHOT_ROOT);                     CKERR(r);

    r = toku_open_brt(FILENAME, 1, &brt, 1024, ct, txn, toku_builtin_compare_fun, NULL);                    CKERR(r);

    r = toku_txn_commit_txn(txn, FALSE, do_yield, NULL, NULL, NULL);                                        CKERR(r);
    toku_txn_close_txn(txn);

    r = toku_checkpoint(ct, logger, NULL, NULL, NULL, NULL);                                                CKERR(r);
    r = toku_close_brt(brt, NULL);                                                                          CKERR(r);

    unsigned int rands[N];
    for (int i=0; i<N; i++) {
	r = toku_txn_begin_txn((DB_TXN*)NULL, (TOKUTXN)0, &txn, logger, TXN_SNAPSHOT_ROOT);                 CKERR(r);
	r = toku_open_brt(FILENAME, 0, &brt, 1024, ct, txn, toku_builtin_compare_fun, NULL);                CKERR(r);
	r = toku_txn_commit_txn(txn, FALSE, do_yield, NULL, NULL, NULL);                                        CKERR(r);
	toku_txn_close_txn(txn);

	r = toku_txn_begin_txn((DB_TXN*)NULL, (TOKUTXN)0, &txn, logger, TXN_SNAPSHOT_ROOT);                 CKERR(r);
	char key[100],val[300];
	DBT k, v;
	rands[i] = random();
	snprintf(key, sizeof(key), "key%x.%x", rands[i], i);
	memset(val, 'v', sizeof(val));
	val[sizeof(val)-1]=0;
	r = toku_brt_insert(brt, toku_fill_dbt(&k, key, 1+strlen(key)), toku_fill_dbt(&v, val, 1+strlen(val)), txn);
	r = toku_txn_commit_txn(txn, FALSE, do_yield, NULL, NULL, NULL);                                        CKERR(r);
	toku_txn_close_txn(txn);


	r = toku_checkpoint(ct, logger, NULL, NULL, NULL, NULL);                                                CKERR(r);
	r = toku_close_brt(brt, NULL);                                                                          CKERR(r);

	if (verbose) printf("i=%d\n", i);
    }
    for (int i=0; i<N; i++) {
	r = toku_txn_begin_txn((DB_TXN*)NULL, (TOKUTXN)0, &txn, logger, TXN_SNAPSHOT_ROOT);                     CKERR(r);
	r = toku_open_brt(FILENAME, 0, &brt, 1024, ct, txn, toku_builtin_compare_fun, NULL);                CKERR(r);
	r = toku_txn_commit_txn(txn, FALSE, do_yield, NULL, NULL, NULL);                                        CKERR(r);
	toku_txn_close_txn(txn);

	r = toku_txn_begin_txn((DB_TXN*)NULL, (TOKUTXN)0, &txn, logger, TXN_SNAPSHOT_ROOT);                     CKERR(r);
	char key[100];
	DBT k;
	snprintf(key, sizeof(key), "key%x.%x", rands[i], i);
	r = toku_brt_delete(brt, toku_fill_dbt(&k, key, 1+strlen(key)), txn);

	if (0) {
	BOOL is_empty;
        is_empty = toku_brt_is_empty_fast(brt);
	assert(!is_empty);
	}
	
	r = toku_txn_commit_txn(txn, FALSE, do_yield, NULL, NULL, NULL);                                        CKERR(r);
	toku_txn_close_txn(txn);


	r = toku_checkpoint(ct, logger, NULL, NULL, NULL, NULL);                                                CKERR(r);
	r = toku_close_brt(brt, NULL);                                                                          CKERR(r);

	if (verbose) printf("d=%d\n", i);
    }
    r = toku_txn_begin_txn((DB_TXN*)NULL, (TOKUTXN)0, &txn, logger, TXN_SNAPSHOT_ROOT);                        CKERR(r);
    r = toku_open_brt(FILENAME, 0, &brt, 1024, ct, txn, toku_builtin_compare_fun, NULL);                       CKERR(r);
    r = toku_txn_commit_txn(txn, FALSE, do_yield, NULL, NULL, NULL);                                           CKERR(r);
    toku_txn_close_txn(txn);

    if (0) {
    BOOL is_empty;
    is_empty = toku_brt_is_empty_fast(brt);
    assert(is_empty);
    }

    r = toku_checkpoint(ct, logger, NULL, NULL, NULL, NULL);                                                   CKERR(r);
    r = toku_close_brt(brt, NULL);                                                                             CKERR(r);

    r = toku_checkpoint(ct, logger, NULL, NULL, NULL, NULL);                                                   CKERR(r);
    r = toku_logger_close_rollback(logger, FALSE);                                                             CKERR(r);
    r = toku_checkpoint(ct, logger, NULL, NULL, NULL, NULL);                                                   CKERR(r);
    r = toku_cachetable_close(&ct);                                                                            CKERR(r);
    r = toku_logger_close(&logger);                                                        assert(r==0);

}
    

int test_main (int argc, const char *argv[]) {
    default_parse_args(argc, argv);
    for (int i=1; i<=64; i++) {
	test_it(i);
    }
    return 0;
}
