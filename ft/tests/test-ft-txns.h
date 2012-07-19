/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ifndef TEST_FT_TXNS_H
#define TEST_FT_TXNS_H

#ident "$Id$"
#ident "Copyright (c) 2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#if !defined(TESTDIR) || !defined(FILENAME)
# error "must define TESTDIR and FILENAME"
#endif

static inline void
test_setup(TOKULOGGER *loggerp, CACHETABLE *ctp) {
    *loggerp = NULL;
    *ctp = NULL;
    int r;
    r = system("rm -rf " TESTDIR);
    CKERR(r);
    r = toku_os_mkdir(TESTDIR, S_IRWXU);
    CKERR(r);

    r = toku_logger_create(loggerp);
    CKERR(r);
    TOKULOGGER logger = *loggerp;

    r = toku_logger_open(TESTDIR, logger);
    CKERR(r);

    r = toku_create_cachetable(ctp, 0, ZERO_LSN, logger);
    CKERR(r);
    CACHETABLE ct = *ctp;
    toku_cachetable_set_env_dir(ct, TESTDIR);

    toku_logger_set_cachetable(logger, ct);

    r = toku_logger_open_rollback(logger, ct, TRUE);
    CKERR(r);

    r = toku_checkpoint(ct, logger, NULL, NULL, NULL, NULL, STARTUP_CHECKPOINT);
    CKERR(r);
}

static inline void
xid_lsn_keep_cachetable_callback (DB_ENV *env, CACHETABLE cachetable) {
    CACHETABLE *CAST_FROM_VOIDP(ctp, (void *) env);
    *ctp = cachetable;
}

static inline void test_setup_and_recover(TOKULOGGER *loggerp, CACHETABLE *ctp) {
    int r;
    TOKULOGGER logger = NULL;
    CACHETABLE ct = NULL;
    r = toku_logger_create(&logger);
    CKERR(r);

    DB_ENV *CAST_FROM_VOIDP(ctv, (void *) &ct);  // Use intermediate to avoid compiler warning.
    r = tokudb_recover(ctv,
                       NULL_prepared_txn_callback,
                       xid_lsn_keep_cachetable_callback,
                       logger,
                       TESTDIR, TESTDIR, 0, 0, 0, NULL, 0);
    CKERR(r);
    if (!toku_logger_is_open(logger)) {
        //Did not need recovery.
        invariant(ct==NULL);
        r = toku_logger_open(TESTDIR, logger);
        CKERR(r);
        r = toku_create_cachetable(&ct, 0, ZERO_LSN, logger);
        CKERR(r);
        toku_logger_set_cachetable(logger, ct);
    }
    *ctp = ct;
    *loggerp = logger;
}

static inline void clean_shutdown(TOKULOGGER *loggerp, CACHETABLE *ctp) {
    int r;
    r = toku_checkpoint(*ctp, *loggerp, NULL, NULL, NULL, NULL, SHUTDOWN_CHECKPOINT);
    CKERR(r);

    r = toku_logger_close_rollback(*loggerp, false);
    CKERR(r);

    r = toku_checkpoint(*ctp, *loggerp, NULL, NULL, NULL, NULL, SHUTDOWN_CHECKPOINT);
    CKERR(r);

    r = toku_logger_shutdown(*loggerp);
    CKERR(r);

    r = toku_cachetable_close(ctp);
    CKERR(r);

    r = toku_logger_close(loggerp);
    CKERR(r);
}

static inline void shutdown_after_recovery(TOKULOGGER *loggerp, CACHETABLE *ctp) {
    int r;
    r = toku_logger_close_rollback(*loggerp, false);
    CKERR(r);
    r = toku_cachetable_close(ctp);
    CKERR(r);
    r = toku_logger_close(loggerp);
    CKERR(r);
}

#endif /* TEST_FT_TXNS_H */
