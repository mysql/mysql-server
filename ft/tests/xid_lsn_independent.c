/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "test.h"
#include "includes.h"
#include "toku_os.h"
#include "checkpoint.h"


#define TESTDIR __SRCFILE__ ".dir"
#define FILENAME "test0.ft"

static void test_it (int N) {
    FT_HANDLE brt;
    int r;
    r = system("rm -rf " TESTDIR);
    CKERR(r);
    r = toku_os_mkdir(TESTDIR, S_IRWXU);                                                                    CKERR(r);

    TOKULOGGER logger;
    r = toku_logger_create(&logger);                                                                        CKERR(r);
    r = toku_logger_open(TESTDIR, logger);                                                                  CKERR(r);


    CACHETABLE ct;
    r = toku_create_cachetable(&ct, 0, ZERO_LSN, logger);                                               CKERR(r);
    toku_cachetable_set_env_dir(ct, TESTDIR);

    toku_logger_set_cachetable(logger, ct);

    r = toku_logger_open_rollback(logger, ct, TRUE);                                                        CKERR(r);

    TOKUTXN txn;
    r = toku_txn_begin_txn((DB_TXN*)NULL, (TOKUTXN)0, &txn, logger, TXN_SNAPSHOT_ROOT);                     CKERR(r);

    r = toku_open_ft_handle(FILENAME, 1, &brt, 1024, 256, TOKU_DEFAULT_COMPRESSION_METHOD, ct, txn, toku_builtin_compare_fun);                    CKERR(r);

    r = toku_txn_commit_txn(txn, FALSE, NULL, NULL);                                 CKERR(r);
    toku_txn_close_txn(txn);

    r = toku_txn_begin_txn((DB_TXN*)NULL, (TOKUTXN)0, &txn, logger, TXN_SNAPSHOT_ROOT);                 CKERR(r);
    TXNID xid_first = txn->txnid64;
    unsigned int rands[N];
    for (int i=0; i<N; i++) {
	char key[100],val[300];
	DBT k, v;
	rands[i] = random();
	snprintf(key, sizeof(key), "key%x.%x", rands[i], i);
	memset(val, 'v', sizeof(val));
	val[sizeof(val)-1]=0;
	r = toku_ft_insert(brt, toku_fill_dbt(&k, key, 1+strlen(key)), toku_fill_dbt(&v, val, 1+strlen(val)), txn);
        CKERR(r);
    }
    {
        TOKUTXN txn2;
        r = toku_txn_begin_txn((DB_TXN*)NULL, (TOKUTXN)0, &txn2, logger, TXN_SNAPSHOT_ROOT);                 CKERR(r);
        // Verify the txnid has gone up only by one (even though many log entries were done)
        invariant(txn2->txnid64 == xid_first + 1);
        r = toku_txn_commit_txn(txn2, FALSE, NULL, NULL);                                 CKERR(r);
        toku_txn_close_txn(txn2);
    }
    r = toku_txn_commit_txn(txn, FALSE, NULL, NULL);                                 CKERR(r);
    toku_txn_close_txn(txn);
    {
        //TODO(yoni) #5067 will break this portion of the test. (End ids are also assigned, so it would increase by 4 instead of 2.)
        // Verify the txnid has gone up only by two (even though many log entries were done)
        TOKUTXN txn3;
        r = toku_txn_begin_txn((DB_TXN*)NULL, (TOKUTXN)0, &txn3, logger, TXN_SNAPSHOT_ROOT);                 CKERR(r);
        invariant(txn3->txnid64 == xid_first + 2);
        r = toku_txn_commit_txn(txn3, FALSE, NULL, NULL);                                 CKERR(r);
        toku_txn_close_txn(txn3);
    }

    r = toku_checkpoint(ct, logger, NULL, NULL, NULL, NULL, CLIENT_CHECKPOINT);                                CKERR(r);
    r = toku_close_ft_handle_nolsn(brt, NULL);                                                                             CKERR(r);

    r = toku_checkpoint(ct, logger, NULL, NULL, NULL, NULL, CLIENT_CHECKPOINT);                                CKERR(r);
    r = toku_logger_close_rollback(logger, FALSE);                                                             CKERR(r);
    r = toku_checkpoint(ct, logger, NULL, NULL, NULL, NULL, CLIENT_CHECKPOINT);                                CKERR(r);
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
