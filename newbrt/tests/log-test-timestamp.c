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
test_main (int argc __attribute__((__unused__)),
	  const char *argv[] __attribute__((__unused__))) {
    int r;
    system(rmrf);
    r = toku_os_mkdir(dname, S_IRWXU);    assert(r==0);
    TOKULOGGER logger;
    LSN lsn = ZERO_LSN;

    // log a couple of timestamp log entries

    r = toku_logger_create(&logger);
    assert(r == 0);

    r = toku_logger_open(dname, logger);
    assert(r == 0);

    BYTESTRING bs0 = { .data = "hello", .len = 5 };
    r = toku_log_timestamp(logger, &lsn, 0, now(), bs0);
    assert(r == 0);

    sleep(10);

    BYTESTRING bs1 = { .data = "world", .len = 5 };
    r = toku_log_timestamp(logger, &lsn, 0, now(), bs1);
    assert(r == 0);

    r = toku_logger_close(&logger);
    assert(r == 0);

    // TODO verify the log
    TOKULOGCURSOR lc = NULL;
    r = toku_logcursor_create(&lc, dname);
    assert(r == 0 && lc != NULL);

    struct log_entry le;
    r = toku_logcursor_next(lc, &le);
    assert(r == 0 && le.cmd == LT_timestamp);
    
    r = toku_logcursor_next(lc, &le);
    assert(r == 0 && le.cmd == LT_timestamp);

    r = toku_logcursor_next(lc, &le);
    assert(r != 0);

    r = toku_logcursor_destroy(&lc);
    assert(r == 0 && lc == NULL);

    return 0;
}
