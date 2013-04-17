/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

#include "logcursor.h"
#include "test.h"

const int N = 2;

int
test_main (int argc, const char *argv[]) {
    default_parse_args(argc, argv);

    int r;
    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    r = toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU);    assert(r==0);
    TOKULOGGER logger;
    LSN lsn = ZERO_LSN;

    int helloseq = 0;

    // create N log files with a hello message
    for (int i=0; i<N; i++) {
        r = toku_logger_create(&logger);
        assert(r == 0);

        r = toku_logger_open(TOKU_TEST_FILENAME, logger);
        assert(r == 0);

        char str[32];
        sprintf(str, "hello%d", helloseq++);
        BYTESTRING bs0 = { .len = (uint32_t) strlen(str), .data = str };
        toku_log_comment(logger, &lsn, 0, 0, bs0);

        r = toku_logger_close(&logger);
        assert(r == 0);
    }    

    // create N empty log files
    for (int i=0; i<N; i++) {
        r = toku_logger_create(&logger);
        assert(r == 0);

        r = toku_logger_open(TOKU_TEST_FILENAME, logger);
        assert(r == 0);

        r = toku_logger_close(&logger);
        assert(r == 0);
    }

    // CREATE AN EMPTY FILE (tests [t:2384])
    {
        long long nexti;
        r = toku_logger_find_next_unused_log_file(TOKU_TEST_FILENAME, &nexti);
        assert(r == 0);
        char mt_fname[TOKU_PATH_MAX+1];
        snprintf(mt_fname, TOKU_PATH_MAX, "%s/log%012lld.tokulog%d", TOKU_TEST_FILENAME, nexti, TOKU_LOG_VERSION);
        int mt_fd = open(mt_fname, O_CREAT+O_WRONLY+O_TRUNC+O_EXCL+O_BINARY, S_IRWXU);
        assert(mt_fd != -1);
        r = close(mt_fd);
    }

    // create N log files with a hello message
    for (int i=0; i<N; i++) {
        r = toku_logger_create(&logger);
        assert(r == 0);

        r = toku_logger_open(TOKU_TEST_FILENAME, logger);
        assert(r == 0);

        char str[32];
        sprintf(str, "hello%d", helloseq++);
        BYTESTRING bs0 = { .len = (uint32_t) strlen(str), .data = str };
        toku_log_comment(logger, &lsn, 0, 0, bs0);

        r = toku_logger_close(&logger);
        assert(r == 0);
    }

    // CREATE AN EMPTY FILE (tests [t:2384])
    {
        long long nexti;
        r = toku_logger_find_next_unused_log_file(TOKU_TEST_FILENAME, &nexti);
        assert(r == 0);
        char mt_fname[TOKU_PATH_MAX+1];
        snprintf(mt_fname, TOKU_PATH_MAX, "%s/log%012lld.tokulog%d", TOKU_TEST_FILENAME, nexti, TOKU_LOG_VERSION);
        int mt_fd = open(mt_fname, O_CREAT+O_WRONLY+O_TRUNC+O_EXCL+O_BINARY, S_IRWXU);
        assert(mt_fd != -1);
        r = close(mt_fd);
    }

    // verify the log forwards
    TOKULOGCURSOR lc = NULL;
    struct log_entry *le;

    r = toku_logcursor_create(&lc, TOKU_TEST_FILENAME);
    assert(r == 0 && lc != NULL);

    helloseq = 0;
    for (int i=0; i<2*N; i++) {

        r = toku_logcursor_next(lc, &le);
        assert(r == 0 && le->cmd == LT_comment);
        char expect[32];
        sprintf(expect, "hello%d", helloseq++);
        assert(le->u.comment.comment.len == strlen(expect) && memcmp(le->u.comment.comment.data, expect, le->u.comment.comment.len) == 0);
    }

    r = toku_logcursor_next(lc, &le);
    assert(r != 0);

    r = toku_logcursor_destroy(&lc);
    assert(r == 0 && lc == NULL);

    // verify the log backwards
    r = toku_logcursor_create(&lc, TOKU_TEST_FILENAME);
    assert(r == 0 && lc != NULL);

    helloseq = 2*N;
    for (int i=0; i<2*N; i++) {

        r = toku_logcursor_prev(lc, &le);
        assert(r == 0 && le->cmd == LT_comment);
        char expect[32];
        sprintf(expect, "hello%d", --helloseq);
        assert(le->u.comment.comment.len == strlen(expect) && memcmp(le->u.comment.comment.data, expect, le->u.comment.comment.len) == 0);
    }

    r = toku_logcursor_prev(lc, &le);
    assert(r != 0);

    r = toku_logcursor_destroy(&lc);
    assert(r == 0 && lc == NULL);

    // VERIFY TRIM WORKS WITH ZERO LENGTH FILE [t:2384]
    {
        LSN trim_lsn;
        trim_lsn.lsn = (2*N)-1;
        r = toku_logger_create(&logger);  assert(r==0);
        r = toku_logger_open(TOKU_TEST_FILENAME, logger);  assert(r==0);

        toku_logger_maybe_trim_log(logger, trim_lsn);
        assert( toku_logfilemgr_num_logfiles(logger->logfilemgr) == 4 ); // untrimmed log, empty log, plus newly openned log

        r = toku_logger_close(&logger);
    }

    toku_os_recursive_delete(TOKU_TEST_FILENAME);

    return 0;
}
