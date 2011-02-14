/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2011 Tokutek Inc.  All rights reserved."

// verify that the log file trimmer does not delete the log file containing the
// begin checkpoint when the checkpoint log entries span multiple log files.

#include "test.h"
#include "includes.h"

#define dname __FILE__ ".dir"
#define rmrf "rm -rf " dname "/"

int
test_main (int argc __attribute__((__unused__)),
	  const char *argv[] __attribute__((__unused__))) {
    int r;
    r = system(rmrf); CKERR(r);
    r = toku_os_mkdir(dname, S_IRWXU);    assert(r==0);

    TOKULOGGER logger;
    r = toku_logger_create(&logger); assert(r == 0);
    r = toku_logger_set_lg_max(logger, 32); assert(r == 0);
    r = toku_logger_open(dname, logger); assert(r == 0);
    BYTESTRING hello = (BYTESTRING) { 5, "hello"};
    LSN comment_lsn;
    r = toku_log_comment(logger, &comment_lsn, TRUE, 0, hello);
    LSN begin_lsn;
    r = toku_log_begin_checkpoint(logger, &begin_lsn, TRUE, 0); assert(r == 0);
    LSN end_lsn;
    r = toku_log_end_checkpoint(logger, &end_lsn, TRUE, begin_lsn.lsn, 0, 0, 0); assert(r == 0);
    r = toku_logger_maybe_trim_log(logger, begin_lsn); assert(r == 0);
    r = toku_logger_close(&logger); assert(r == 0);

    // verify all log entries prior the begin checkpoint are trimmed
    TOKULOGCURSOR lc = NULL;
    r = toku_logcursor_create(&lc, dname); assert(r == 0);
    struct log_entry *le = NULL;
    r = toku_logcursor_first(lc, &le); assert(r == 0);
    assert(le->cmd == LT_begin_checkpoint);
    r = toku_logcursor_destroy(&lc); assert(r == 0);
    
    r = system(rmrf); CKERR(r);
    return 0;
}
