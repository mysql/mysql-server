/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2011 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
/* Test an overflow condition on the leaf.  See #632. */


#include "test.h"


static const char *fname = TOKU_TEST_FILENAME;

static TOKUTXN const null_txn = 0;
static DB * const null_db = 0;

static void
test_overflow (void) {
    FT_HANDLE t;
    CACHETABLE ct;
    uint32_t nodesize = 1<<20; 
    int r;
    unlink(fname);
    toku_cachetable_create(&ct, 0, ZERO_LSN, NULL_LOGGER);
    r = toku_open_ft_handle(fname, 1, &t, nodesize, nodesize / 8, TOKU_DEFAULT_COMPRESSION_METHOD, ct, null_txn, toku_builtin_compare_fun); assert(r==0);

    DBT k,v;
    uint32_t vsize = nodesize/8;
    char buf[vsize];
    memset(buf, 'a', vsize);
    int i;
    for (i=0; i<8; i++) {
	char key[]={(char)('a'+i), 0};
	toku_ft_insert(t, toku_fill_dbt(&k, key, 2), toku_fill_dbt(&v,buf,sizeof(buf)), null_txn);
    }
    r = toku_close_ft_handle_nolsn(t, 0);        assert(r==0);
    toku_cachetable_close(&ct);
}

int
test_main (int argc, const char *argv[]) {
    int i;
    for (i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (0 == strcmp(arg, "-v") || 0 == strcmp(arg, "--verbose"))
            verbose = 1;
        else if (0 == strcmp(arg, "-q") || 0 == strcmp(arg, "--quiet"))
            verbose = 0;
    }
    test_overflow();
    
    return 0;
}
