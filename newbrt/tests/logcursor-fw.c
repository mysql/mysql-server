/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

#include "test.h"

#include "includes.h"

#define dname __FILE__ ".dir"
#define rmrf "rm -rf " dname "/"

// log a couple of timestamp entries and verify the log by walking 
// a cursor through the log entries

int
test_main (int argc, const char *argv[]) {
    default_parse_args(argc, argv);

    int r;
    system(rmrf);
    r = toku_os_mkdir(dname, S_IRWXU);    assert(r==0);

    // verify the log backwards
    TOKULOGCURSOR lc = NULL;
    r = toku_logcursor_create(&lc, "/tmp");
    assert(r == 0 && lc != NULL);

    int n = 0;
    while (1) {
        struct log_entry *le = NULL;
        r = toku_logcursor_next(lc, &le);
        if (r != 0)
            break;
        n++;
    }

    printf("n=%d\n", n);

    r = toku_logcursor_destroy(&lc);
    assert(r == 0 && lc == NULL);

    return 0;
}
