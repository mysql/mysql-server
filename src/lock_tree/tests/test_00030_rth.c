/* We are going to test whether create and close properly check their input. */

#include "test.h"

toku_rth* rth;

int main(int argc, const char *argv[]) {
    int r;
    parse_args(argc, argv);
    

    rth = NULL;
    for (failon = 1; failon <= 2; failon++) {
        mallocced = 0;
        r = toku_rth_create(&rth, fail_malloc, toku_free, toku_realloc);
        CKERR2(r, ENOMEM);
        assert(rth==NULL);
    }
    r = toku_rth_create(&rth, toku_malloc, toku_free, toku_realloc);
    CKERR(r);
    assert(rth);
    toku_rth_close(rth);
    rth = NULL;

    size_t i;
    size_t iterations = 512 << 2;
    
    r = toku_rth_create(&rth, toku_malloc, toku_free, toku_realloc);
    CKERR(r);
    assert(rth);
    for (i = 1; i < iterations; i++) {
        r = toku_rth_insert(rth, (DB_TXN*)i);
        CKERR(r);
    }
    toku_rt_forest* f;
    for (i = 1; i < iterations; i++) {
        f = toku_rth_find(rth, (DB_TXN*)i);
        assert(f);
    }
    f = toku_rth_find(rth, (DB_TXN*)i);
    assert(!f);
    for (i = iterations - 1; i >= 1; i--) {
        toku_rth_delete(rth, (DB_TXN*)i);
    }
    toku_rth_close(rth);
    rth = NULL;

    r = toku_rth_create(&rth, toku_malloc, toku_free, toku_realloc);
    CKERR(r);
    assert(rth);
    for (i = 1; i < iterations; i++) {
        r = toku_rth_insert(rth, (DB_TXN*)i);
        CKERR(r);
    }
    for (i = 1; i < iterations; i++) {
        toku_rth_delete(rth, (DB_TXN*)i);
    }
    toku_rth_close(rth);
    rth = NULL;


    r = toku_rth_create(&rth, toku_malloc, toku_free, toku_realloc);
    CKERR(r);
    assert(rth);
    for (i = iterations - 1; i >= 1; i--) {
        r = toku_rth_insert(rth, (DB_TXN*)i);
        CKERR(r);
    }
    toku_rth_close(rth);
    rth = NULL;

    failon = 3;
    mallocced = 0;
    r = toku_rth_create(&rth, fail_malloc, toku_free, toku_realloc);
    CKERR(r);
    assert(rth);
    r = toku_rth_insert(rth, (DB_TXN*)1);
    CKERR2(r, ENOMEM);
    toku_rth_close(rth);
    rth = NULL;
    return 0;
}
