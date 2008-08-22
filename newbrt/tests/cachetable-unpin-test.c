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

void cachetable_unpin_test(int n) {
    const int test_limit = 2*n;
    int r;
    CACHETABLE ct;
    r = toku_create_cachetable(&ct, test_limit, ZERO_LSN, NULL_LOGGER); assert(r == 0);
    char fname1[] = __FILE__ "test1.dat";
    unlink(fname1);
    CACHEFILE f1;
    r = toku_cachetable_openf(&f1, ct, fname1, O_RDWR|O_CREAT, 0777); assert(r == 0);

    int i;
    for (i=1; i<=n; i++) {
        u_int32_t hi;
        hi = toku_cachetable_hash(f1, i);
        r = toku_cachetable_put(f1, i, hi, (void *)(long)i, 1, flush, fetch, 0);
        assert(r == 0);
        assert(toku_cachefile_count_pinned(f1, 0) == i);

        void *v;
        r = toku_cachetable_maybe_get_and_pin(f1, i, hi, &v);
        assert(r == 0);
        assert(toku_cachefile_count_pinned(f1, 0) == i);

        r = toku_cachetable_unpin(f1, i, hi, CACHETABLE_CLEAN, 1);
        assert(r == 0);
        assert(toku_cachefile_count_pinned(f1, 0) == i);
    }
    for (i=n; i>0; i--) {
        u_int32_t hi;
        hi = toku_cachetable_hash(f1, i);
        r = toku_cachetable_unpin(f1, i, hi, CACHETABLE_CLEAN, 1);
        assert(r == 0);
        assert(toku_cachefile_count_pinned(f1, 0) == i-1);
    }
    assert(toku_cachefile_count_pinned(f1, 1) == 0);
    toku_cachetable_verify(ct);

    CACHEKEY k = n+1;
    r = toku_cachetable_unpin(f1, k, toku_cachetable_hash(f1, k), CACHETABLE_CLEAN, 1);
    assert(r != 0);

    r = toku_cachefile_close(&f1, NULL_LOGGER); assert(r == 0 && f1 == 0);
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
    cachetable_unpin_test(8);
    return 0;
}
