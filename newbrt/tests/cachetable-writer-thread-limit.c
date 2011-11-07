#ident "$Id$"
#ident "Copyright (c) 2007-2011 Tokutek Inc.  All rights reserved."
#include "includes.h"
#include "test.h"


static int total_size;
static int test_limit;


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
    if (w) {
        int curr_size = __sync_fetch_and_add(&total_size, -1);
        assert(curr_size <= 200);
        usleep(500*1000);
    }
}


static void
cachetable_test (void) {
    total_size = 0;
    int num_entries = 100;
    test_limit = 6;
    int r;
    CACHETABLE ct;
    r = toku_create_cachetable(&ct, test_limit, ZERO_LSN, NULL_LOGGER); assert(r == 0);
    char fname1[] = __FILE__ "test1.dat";
    unlink(fname1);
    CACHEFILE f1;
    r = toku_cachetable_openf(&f1, ct, fname1, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO); assert(r == 0);

    for (int64_t i = 0; i < num_entries; i++) {
        r = toku_cachetable_put(f1, make_blocknum(i), i, NULL, make_pair_attr(1), flush, def_pe_est_callback, def_pe_callback, def_cleaner_callback, NULL);
        int curr_size = __sync_fetch_and_add(&total_size, 1);
        assert(curr_size <= test_limit + test_limit/2+1);
        r = toku_cachetable_unpin(f1, make_blocknum(i), i, CACHETABLE_DIRTY, make_pair_attr(4));
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
