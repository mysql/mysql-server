#ident "$Id$"
#ident "Copyright (c) 2007-2011 Tokutek Inc.  All rights reserved."
#include "includes.h"
#include "test.h"



static void
flush (CACHEFILE f __attribute__((__unused__)),
       int UU(fd),
       CACHEKEY k  __attribute__((__unused__)),
       void *v     __attribute__((__unused__)),
       void *e     __attribute__((__unused__)),
       PAIR_ATTR s      __attribute__((__unused__)),
       PAIR_ATTR* new_size      __attribute__((__unused__)),
       BOOL w      __attribute__((__unused__)),
       BOOL keep   __attribute__((__unused__)),
       BOOL c      __attribute__((__unused__))
       ) {
}



static void
cachetable_test (void) {
    int num_entries = 100;
    int test_limit = 6;
    int r;
    CACHETABLE ct;
    r = toku_create_cachetable(&ct, test_limit, ZERO_LSN, NULL_LOGGER); assert(r == 0);
    char fname1[] = __FILE__ "test1.dat";
    unlink(fname1);
    CACHEFILE f1;
    r = toku_cachetable_openf(&f1, ct, fname1, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO); assert(r == 0);

    // test that putting something too big in the cachetable works fine
    r = toku_cachetable_put(f1, make_blocknum(num_entries+1), num_entries+1, NULL, make_pair_attr(test_limit*2), flush, def_pe_est_callback, def_pe_callback, def_cleaner_callback, NULL);
    assert(r==0);
    r = toku_cachetable_unpin(f1, make_blocknum(num_entries+1), num_entries+1, CACHETABLE_DIRTY, make_pair_attr(test_limit*2));
    assert(r==0);


    for (int64_t i = 0; i < num_entries; i++) {
        r = toku_cachetable_put(f1, make_blocknum(i), i, NULL, make_pair_attr(1), flush, def_pe_est_callback, def_pe_callback, def_cleaner_callback, NULL);
        assert(toku_cachefile_count_pinned(f1, 0) == (i+1));
    }
    for (int64_t i = 0; i < num_entries; i++) {
        r = toku_cachetable_unpin(f1, make_blocknum(i), i, CACHETABLE_DIRTY, make_pair_attr(1));
    }

    
    r = toku_cachefile_close(&f1, 0, FALSE, ZERO_LSN); assert(r == 0 && f1 == 0);
    r = toku_cachetable_close(&ct); assert(r == 0 && ct == 0);
}

int
test_main(int argc, const char *argv[]) {
    default_parse_args(argc, argv);
    cachetable_test();
    return 0;
}
