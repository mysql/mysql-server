#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "memory.h"
#include "fifo.h"

int verbose;

void test_fifo_create() {
    int r;
    FIFO f;

    f = 0;
    r = toku_fifo_create(&f); 
    assert(r == 0); assert(f != 0);

    toku_fifo_free(&f);
    assert(f == 0);
}

void test_fifo_enq(int n) {
    int r;
    FIFO f;

    f = 0;
    r = toku_fifo_create(&f); 
    assert(r == 0); assert(f != 0);

    char *thekey = 0; int thekeylen;
    char *theval = 0; int thevallen;
    
    void buildkey(int len) {
        thekeylen = len;
        thekey = realloc(thekey, thekeylen);
        memset(thekey, len, thekeylen);
    }

    void buildval(int len) {
        thevallen = len+1;
        theval = realloc(theval, thevallen);
        memset(theval, ~len, thevallen);
    }

    int i;
    for (i=0; i<n; i++) {
        buildkey(i);
        buildval(i);
        r = toku_fifo_enq(f, thekey, thekeylen, theval, thevallen, i); assert(r == 0);
    }

    void checkit(bytevec key, ITEMLEN keylen, bytevec val, ITEMLEN vallen, int type, void *arg) {
        if (verbose) printf("checkit %d %d\n", i, type);
        assert(arg == 0);
        buildkey(i);
        buildval(i);
        assert((int) keylen == thekeylen); assert(memcmp(key, thekey, keylen) == 0);
        assert((int) vallen == thevallen); assert(memcmp(val, theval, vallen) == 0);
        assert(i % 256 == type);
        
        i += 1;
    }

    i = 0;
    toku_fifo_iterate(f, checkit, 0);
    assert(i == n);

    if (thekey) free(thekey);
    if (theval) free(theval);

    while (toku_fifo_deq(f) == 0)
        ;

    toku_fifo_free(&f);
    assert(f == 0);
}

int main(int argc, char *argv[]) {
    int i;
    for (i = 1; i < argc; i++) {
        char *arg = argv[i];
        if (0 == strcmp(arg, "-v") || 0 == strcmp(arg, "--verbose")) {
            verbose = 1; continue;
        }
    }

    test_fifo_create();
    test_fifo_enq(512);
    toku_malloc_cleanup();
    return 0;
}
