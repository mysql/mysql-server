/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."
#ident "Id:"

// Test for #3748 (FIFO's do not shrink their size after a flush)
// This test makes sure that the code that stabilizes fifos works.

#include "includes.h"
#include "test.h"
int verbose;

static void test_3748 (void) {
    FIFO f;
    int r;
    f = NULL;
    r = toku_fifo_create(&f);

    char *thekey = 0; int thekeylen;
    char *theval = 0; int thevallen;

#define buildkey(len) { \
        thekeylen = len; \
        thekey = toku_realloc(thekey, thekeylen); \
        memset(thekey, len, thekeylen); \
    }

#define buildval(len) { \
        thevallen = len+1; \
        theval = toku_realloc(theval, thevallen); \
        memset(theval, ~len, thevallen); \
    }

    MSN startmsn = ZERO_MSN;

    int N=1000;

    // enqueue some stuff
    for (int i=0; i<N; i++) {
	buildkey(i);
        buildval(i);

	XIDS xids = xids_get_root_xids();
	MSN msn = next_dummymsn();

	if (startmsn.msn == ZERO_MSN.msn)
	    startmsn = msn;
	r = toku_fifo_enq(f, thekey, thekeylen, theval, thevallen, i, msn, xids); assert(r == 0);
	xids_destroy(&xids);
    }
    for (int i=N/10; i<N; i++) {
	r = toku_fifo_deq(f);
    }
    unsigned int msize0 = toku_fifo_memory_size(f);
    toku_fifo_size_is_stabilized(f);
    unsigned int msize1 = toku_fifo_memory_size(f);
    assert(msize1 < msize0);

    while (toku_fifo_deq(f) == 0)
        /*nothing*/;

    toku_fifo_size_is_stabilized(f);
    unsigned int msize2 = toku_fifo_memory_size(f);
    assert(msize2 < msize1);

    toku_fifo_free(&f);
    assert(f == 0);

    if (thekey) toku_free(thekey);
    if (theval) toku_free(theval);

}

int
test_main(int argc, const char *argv[]) {
    default_parse_args(argc, argv);
    test_3748();
   
    return 0;
}
