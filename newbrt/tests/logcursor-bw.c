/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id$"
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

#include "test.h"

#include "includes.h"

// walk forward through the log files found in the current directory

int
test_main (int argc, const char *argv[]) {
    default_parse_args(argc, argv);

    int r;

    // verify the log backwards
    TOKULOGCURSOR lc = NULL;
    r = toku_logcursor_create(&lc, ".");
    assert(r == 0 && lc != NULL);

    int n = 0;
    while (1) {
        struct log_entry *le = NULL;
        r = toku_logcursor_prev(lc, &le);
        if (r != 0)
            break;
        n++;
    }

    printf("n=%d\n", n);

    r = toku_logcursor_destroy(&lc);
    assert(r == 0 && lc == NULL);

    return 0;
}
