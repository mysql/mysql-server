/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

#include "test.h"

#include "includes.h"

#define dname __FILE__ ".dir"
#define rmrf "rm -rf " dname "/"

// a logcursor in an empty directory should not find any log entries

int
test_main (int argc, const char *argv[]) {
    default_parse_args(argc, argv);

    int r;
    r = system(rmrf);
    CKERR(r);
    r = toku_os_mkdir(dname, S_IRWXU);    assert(r==0);

    // verify the log is empty

    TOKULOGCURSOR lc = NULL;
    struct log_entry *le;
    
    r = toku_logcursor_create(&lc, dname);
    assert(r == 0 && lc != NULL);

    r = toku_logcursor_next(lc, &le);
    assert(r != 0);
    
    r = toku_logcursor_prev(lc, &le);
    assert(r != 0);

    r = toku_logcursor_destroy(&lc);
    assert(r == 0 && lc == NULL);

    r = system(rmrf);
    CKERR(r);

    return 0;
}
