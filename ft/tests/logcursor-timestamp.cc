/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

#include "logcursor.h"
#include "test.h"

static uint64_t now(void) {
    struct timeval tv;
    int r = gettimeofday(&tv, NULL);
    assert(r == 0);
    return tv.tv_sec * 1000000ULL + tv.tv_usec;
}

// log a couple of timestamp entries and verify the log by walking 
// a cursor through the log entries

int
test_main (int argc, const char *argv[]) {
    default_parse_args(argc, argv);

    int r;
    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    r = toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU);    assert(r==0);
    TOKULOGGER logger;
    LSN lsn = ZERO_LSN;

    // log a couple of timestamp log entries

    r = toku_logger_create(&logger);
    assert(r == 0);

    r = toku_logger_open(TOKU_TEST_FILENAME, logger);
    assert(r == 0);

    BYTESTRING bs0 = { .len = 5, .data = (char *) "hello" };
    toku_log_comment(logger, &lsn, 0, now(), bs0);

    sleep(11);

    BYTESTRING bs1 = { .len = 5, .data = (char *) "world" };
    toku_log_comment(logger, &lsn, 0, now(), bs1);

    r = toku_logger_close(&logger);
    assert(r == 0);

    // verify the log forwards
    TOKULOGCURSOR lc = NULL;
    struct log_entry *le;
    
    r = toku_logcursor_create(&lc, TOKU_TEST_FILENAME);
    assert(r == 0 && lc != NULL);

    r = toku_logcursor_next(lc, &le);
    assert(r == 0 && le->cmd == LT_comment);
    assert(le->u.comment.comment.len == 5 && memcmp(le->u.comment.comment.data, "hello", 5) == 0);
    uint64_t t = le->u.comment.timestamp;
    
    r = toku_logcursor_next(lc, &le);
    assert(r == 0 && le->cmd == LT_comment);
    assert(le->u.comment.comment.len == 5 && memcmp(le->u.comment.comment.data, "world", 5) == 0);
    if (verbose)
        printf("%" PRIu64 "\n", le->u.comment.timestamp - t);
    assert(le->u.comment.timestamp - t >= 10*1000000);

    r = toku_logcursor_next(lc, &le);
    assert(r != 0);

    r = toku_logcursor_destroy(&lc);
    assert(r == 0 && lc == NULL);

    // verify the log backwards
    r = toku_logcursor_create(&lc, TOKU_TEST_FILENAME);
    assert(r == 0 && lc != NULL);

    r = toku_logcursor_prev(lc, &le);
    assert(r == 0 && le->cmd == LT_comment);
    assert(le->u.comment.comment.len == 5 && memcmp(le->u.comment.comment.data, "world", 5) == 0);
    t = le->u.comment.timestamp;
    
    r = toku_logcursor_prev(lc, &le);
    assert(r == 0 && le->cmd == LT_comment);
    assert(le->u.comment.comment.len == 5 && memcmp(le->u.comment.comment.data, "hello", 5) == 0);
    if (verbose)
        printf("%" PRIu64 "\n", t - le->u.comment.timestamp);
    assert(t - le->u.comment.timestamp >= 10*1000000);

    r = toku_logcursor_prev(lc, &le);
    assert(r != 0);

    r = toku_logcursor_destroy(&lc);
    assert(r == 0 && lc == NULL);

    toku_os_recursive_delete(TOKU_TEST_FILENAME);

    return 0;
}
