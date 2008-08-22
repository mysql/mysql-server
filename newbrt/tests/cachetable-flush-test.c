#include <stdio.h>
#include <unistd.h>
#include <assert.h>

#include "test.h"
#include "cachetable.h"

void flush() {
}

int fetch() {
    return 0;
}

void test_cachetable_flush(int n) {
    const int test_limit = 2*n;
    int r;
    CACHETABLE ct;
    r = toku_create_cachetable(&ct, test_limit, ZERO_LSN, NULL_LOGGER); assert(r == 0);
    char fname1[] = __FILE__ "test1.dat";
    unlink(fname1);
    CACHEFILE f1;
    r = toku_cachetable_openf(&f1, ct, fname1, O_RDWR|O_CREAT, 0777); assert(r == 0);

    char fname2[] = __FILE__ "test2.dat";
    unlink(fname2);
    CACHEFILE f2;
    r = toku_cachetable_openf(&f2, ct, fname2, O_RDWR|O_CREAT, 0777); assert(r == 0);

    // insert keys 0..n-1 
    int i;
    for (i=0; i<n; i++) {
        u_int32_t hi;
        hi = toku_cachetable_hash(f1, i);
        r = toku_cachetable_put(f1, i, hi, (void *)(long)i, 1, flush, fetch, 0);
        assert(r == 0);
        r = toku_cachetable_unpin(f1, i, hi, CACHETABLE_CLEAN, 1);
        assert(r == 0);
        hi = toku_cachetable_hash(f2, i);
        r = toku_cachetable_put(f2, i, hi, (void *)(long)i, 1, flush, fetch, 0);
        assert(r == 0);
        r = toku_cachetable_unpin(f2, i, hi, CACHETABLE_CLEAN, 1);
        assert(r == 0);
    }
    toku_cachetable_verify(ct);

    // verify keys exists
    for (i=0; i<n; i++) {
        u_int32_t hi;
        void *v;
        hi = toku_cachetable_hash(f1, i);
        r = toku_cachetable_maybe_get_and_pin(f1, i, hi, &v);
        assert(r == 0 && v == (void *)(long)i);
        r = toku_cachetable_unpin(f1, i, hi, CACHETABLE_CLEAN, 1);
        assert(r == 0);
        hi = toku_cachetable_hash(f2, i);
        r = toku_cachetable_maybe_get_and_pin(f2, i, hi, &v);
        assert(r == 0 && v == (void *)(long)i);
        r = toku_cachetable_unpin(f2, i, hi, CACHETABLE_CLEAN, 1);
        assert(r == 0);
    }

    // flush 
    r = toku_cachefile_flush(f1); assert(r == 0);
    toku_cachefile_verify(f1);

    // verify keys do not exist in f1 but do exist in f2
    for (i=0; i<n; i++) {
        u_int32_t hi;
        void *v;
        hi = toku_cachetable_hash(f1, i);
        r = toku_cachetable_maybe_get_and_pin(f1, i, hi, &v);
        assert(r != 0);
        hi = toku_cachetable_hash(f2, i);
        r = toku_cachetable_maybe_get_and_pin(f2, i, hi, &v);
        assert(r == 0);
        r = toku_cachetable_unpin(f2, i, hi, CACHETABLE_CLEAN, 1);
        assert(r == 0);
    }

    r = toku_cachefile_close(&f1, NULL_LOGGER); assert(r == 0 && f1 == 0);
    r = toku_cachefile_close(&f2, NULL_LOGGER); assert(r == 0 && f2 == 0);
    r = toku_cachetable_close(&ct); assert(r == 0 && ct == 0);
}

int main(int argc, const char *argv[]) {
    int i;
    for (i=1; i<argc; i++) {
        if (strcmp(argv[i], "-v") == 0) {
            verbose++;
            continue;
        }
    }
    test_cachetable_flush(8);
    return 0;
}
