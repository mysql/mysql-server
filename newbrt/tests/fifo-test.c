#include "includes.h"


#include "test.h"
int verbose;

static void
test_fifo_create (void) {
    int r;
    FIFO f;

    f = 0;
    r = toku_fifo_create(&f); 
    assert(r == 0); assert(f != 0);

    toku_fifo_free(&f);
    assert(f == 0);
}

static void
test_fifo_enq (int n) {
    int r;
    FIFO f;

    f = 0;
    r = toku_fifo_create(&f); 
    assert(r == 0); assert(f != 0);

    char *thekey = 0; int thekeylen;
    char *theval = 0; int thevallen;

    // this was a function but icc cant handle it    
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

    int i;
    for (i=0; i<n; i++) {
        buildkey(i);
        buildval(i);
        r = toku_fifo_enq(f, thekey, thekeylen, theval, thevallen, i, (TXNID)i); assert(r == 0);
    }

    i = 0;
    FIFO_ITERATE(f, key, keylen, val, vallen, type, xid, {
        if (verbose) printf("checkit %d %d\n", i, type);
        buildkey(i);
        buildval(i);
        assert((int) keylen == thekeylen); assert(memcmp(key, thekey, keylen) == 0);
        assert((int) vallen == thevallen); assert(memcmp(val, theval, vallen) == 0);
        assert(i % 256 == type);
	assert((TXNID)i==xid);
        i += 1;
    });
    assert(i == n);

    if (thekey) toku_free(thekey);
    if (theval) toku_free(theval);

    while (toku_fifo_deq(f) == 0)
        ;

    toku_fifo_free(&f);
    assert(f == 0);
}

int
test_main(int argc, const char *argv[]) {
    default_parse_args(argc, argv);

    test_fifo_create();
    test_fifo_enq(4);
    test_fifo_enq(512);
    toku_malloc_cleanup();
    return 0;
}
