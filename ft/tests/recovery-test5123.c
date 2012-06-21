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

#include "test-ft-txns.h"

static void test_5123(void) {
    TOKULOGGER logger;
    CACHETABLE ct;
    test_setup(&logger, &ct);

    int r;

    TOKUTXN txn[3];
    r = toku_txn_begin_txn((DB_TXN*)NULL, (TOKUTXN)0, &txn[0], logger, TXN_SNAPSHOT_ROOT);
    CKERR(r);
    r = toku_txn_begin_txn((DB_TXN*)NULL, (TOKUTXN)0, &txn[1], logger, TXN_SNAPSHOT_ROOT);
    CKERR(r);
    r = toku_txn_begin_txn((DB_TXN*)NULL, (TOKUTXN)0, &txn[2], logger, TXN_SNAPSHOT_ROOT);
    CKERR(r);

    toku_maybe_log_begin_txn_for_write_operation(txn[0]);
    toku_maybe_log_begin_txn_for_write_operation(txn[2]);
    toku_maybe_log_begin_txn_for_write_operation(txn[1]);

    r = toku_txn_commit_txn(txn[1], FALSE, NULL, NULL);
    CKERR(r);

    r = toku_logger_close_rollback(logger, false);
    CKERR(r);

    r = toku_cachetable_close(&ct);
    CKERR(r);
    // "Crash"
    r = toku_logger_close(&logger);
    CKERR(r);
    ct = NULL;
    logger = NULL;

    // "Recover"
    test_setup_and_recover(&logger, &ct);

    shutdown_after_recovery(&logger, &ct);
}

int test_main (int argc, const char *argv[]) {
    default_parse_args(argc, argv);
    test_5123();
    return 0;
}
