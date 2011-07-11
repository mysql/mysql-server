/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id$"
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

#include "test.h"

#include "includes.h"

#define dname __FILE__ ".dir"
#define rmrf "rm -rf " dname "/"

// log a couple of timestamp entries and verify the log by walking 
// a cursor through the log entries

static void corrupt_the_checksum(void) {
    // change the LSN in the first log entry of log 0.  this will cause an checksum error.
    char logname[PATH_MAX];
    int r;
    sprintf(logname, dname "/" "log000000000000.tokulog%d", TOKU_LOG_VERSION);
    FILE *f = fopen(logname, "r+b"); assert(f);
    r = fseek(f, 025, SEEK_SET); assert(r == 0);
    char c = 100;
    size_t n = fwrite(&c, sizeof c, 1, f); assert(n == sizeof c);
    r = fclose(f); assert(r == 0);
}

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
    r = toku_log_comment(logger, &lsn, 0, 0, bs0);
    assert(r == 0);

    r = toku_logger_close(&logger);
    assert(r == 0);

    // change the LSN and corrupt the checksum
    corrupt_the_checksum();

    if (!verbose) {
        // redirect stderr
        int devnul = open(DEV_NULL_FILE, O_WRONLY);
        assert(devnul >= 0);
        r = toku_dup2(devnul, fileno(stderr)); assert(r == fileno(stderr));
        r = close(devnul); assert(r == 0);
    }

    // walk forwards
    TOKULOGCURSOR lc = NULL;
    struct log_entry *le;
    
    r = toku_logcursor_create(&lc, dname);
    assert(r == 0 && lc != NULL);

    r = toku_logcursor_next(lc, &le);
    assert(r != 0);

    r = toku_logcursor_destroy(&lc);
    assert(r == 0 && lc == NULL);

    // walk backwards
    r = toku_logcursor_create(&lc, dname);
    assert(r == 0 && lc != NULL);

    r = toku_logcursor_prev(lc, &le);
    assert(r != 0);

    r = toku_logcursor_destroy(&lc);
    assert(r == 0 && lc == NULL);

    r = system(rmrf);
    CKERR(r);

    return 0;
}
