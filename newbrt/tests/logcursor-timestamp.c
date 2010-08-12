/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

#include "test.h"

#include "includes.h"

static u_int64_t now(void) {
    struct timeval tv;
    int r = gettimeofday(&tv, NULL);
    assert(r == 0);
    return tv.tv_sec * 1000000ULL + tv.tv_usec;
}

#define dname __FILE__ ".dir"
#define rmrf "rm -rf " dname "/"

// log a couple of timestamp entries and verify the log by walking 
// a cursor through the log entries

int
test_main (int argc, const char *argv[]) {
    default_parse_args(argc, argv);

    int r;
    r = system(rmrf);
    CKERR(r);
    r = toku_os_mkdir(dname, S_IRWXU);    assert(r==0);
    TOKULOGGER logger;
    LSN lsn = ZERO_LSN;

    // log a couple of timestamp log entries

    r = toku_logger_create(&logger);
    assert(r == 0);

    r = toku_logger_open(dname, logger);
    assert(r == 0);

    BYTESTRING bs0 = { .data = "hello", .len = 5 };
    r = toku_log_comment(logger, &lsn, 0, now(), bs0);
    assert(r == 0);

    sleep(10);

    BYTESTRING bs1 = { .data = "world", .len = 5 };
    r = toku_log_comment(logger, &lsn, 0, now(), bs1);
    assert(r == 0);

    r = toku_logger_close(&logger);
    assert(r == 0);

    // verify the log forwards
    TOKULOGCURSOR lc = NULL;
    struct log_entry *le;
    
    r = toku_logcursor_create(&lc, dname);
    assert(r == 0 && lc != NULL);

    r = toku_logcursor_next(lc, &le);
    assert(r == 0 && le->cmd == LT_comment);
    assert(le->u.comment.comment.len == 5 && memcmp(le->u.comment.comment.data, "hello", 5) == 0);
    u_int64_t t = le->u.comment.timestamp;
    
    r = toku_logcursor_next(lc, &le);
    assert(r == 0 && le->cmd == LT_comment);
    assert(le->u.comment.comment.len == 5 && memcmp(le->u.comment.comment.data, "world", 5) == 0);
    if (verbose)
        printf("%"PRIu64"\n", le->u.comment.timestamp - t);
    assert(le->u.comment.timestamp - t >= 10*1000000);

    r = toku_logcursor_next(lc, &le);
    assert(r != 0);

    r = toku_logcursor_destroy(&lc);
    assert(r == 0 && lc == NULL);

    // verify the log backwards
    r = toku_logcursor_create(&lc, dname);
    assert(r == 0 && lc != NULL);

    r = toku_logcursor_prev(lc, &le);
    assert(r == 0 && le->cmd == LT_comment);
    assert(le->u.comment.comment.len == 5 && memcmp(le->u.comment.comment.data, "world", 5) == 0);
    t = le->u.comment.timestamp;
    
    r = toku_logcursor_prev(lc, &le);
    assert(r == 0 && le->cmd == LT_comment);
    assert(le->u.comment.comment.len == 5 && memcmp(le->u.comment.comment.data, "hello", 5) == 0);
    if (verbose)
        printf("%"PRIu64"\n", t - le->u.comment.timestamp);
    assert(t - le->u.comment.timestamp >= 10*1000000);

    r = toku_logcursor_prev(lc, &le);
    assert(r != 0);

    r = toku_logcursor_destroy(&lc);
    assert(r == 0 && lc == NULL);

    r = system(rmrf);
    CKERR(r);

    return 0;
}
