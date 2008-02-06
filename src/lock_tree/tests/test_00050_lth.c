/* We are going to test whether create and close properly check their input. */

#include "test.h"

toku_lth* lth;

int main(int argc, const char *argv[]) {
    int r;
    parse_args(argc, argv);
    

    lth = NULL;
    for (failon = 1; failon <= 2; failon++) {
        mallocced = 0;
        r = toku_lth_create(&lth, fail_malloc, toku_free, toku_realloc);
        CKERR2(r, ENOMEM);
        assert(lth==NULL);
    }
    r = toku_lth_create(&lth, toku_malloc, toku_free, toku_realloc);
    CKERR(r);
    assert(lth);
    toku_lth_close(lth);
    lth = NULL;

    size_t i;
    size_t iterations = 512 << 2;
    
    r = toku_lth_create(&lth, toku_malloc, toku_free, toku_realloc);
    CKERR(r);
    assert(lth);
    for (i = 1; i < iterations; i++) {
        r = toku_lth_insert(lth, (toku_lock_tree*)i);
        CKERR(r);
    }
    toku_lock_tree* f;
    for (i = 1; i < iterations; i++) {
        f = toku_lth_find(lth, (toku_lock_tree*)i);
        assert(f == (toku_lock_tree*) i);
    }
    f = toku_lth_find(lth, (toku_lock_tree*)i);
    assert(!f);
    
    toku_lth_start_scan(lth);
    for (i = 1; i < iterations; i++) {
        f = toku_lth_next(lth);
        assert(f);
    }
    f = toku_lth_next(lth);
    assert(!f);

    for (i = iterations - 1; i >= 1; i--) {
        toku_lth_delete(lth, (toku_lock_tree*)i);
    }
    toku_lth_close(lth);
    lth = NULL;

    r = toku_lth_create(&lth, toku_malloc, toku_free, toku_realloc);
    CKERR(r);
    assert(lth);
    for (i = 1; i < iterations; i++) {
        r = toku_lth_insert(lth, (toku_lock_tree*)i);
        CKERR(r);
    }
    for (i = 1; i < iterations; i++) {
        toku_lth_delete(lth, (toku_lock_tree*)i);
    }
    toku_lth_close(lth);
    lth = NULL;


    r = toku_lth_create(&lth, toku_malloc, toku_free, toku_realloc);
    CKERR(r);
    assert(lth);
    for (i = iterations - 1; i >= 1; i--) {
        r = toku_lth_insert(lth, (toku_lock_tree*)i);
        CKERR(r);
    }
    toku_lth_close(lth);
    lth = NULL;

    failon = 3;
    mallocced = 0;
    r = toku_lth_create(&lth, fail_malloc, toku_free, toku_realloc);
    CKERR(r);
    assert(lth);
    r = toku_lth_insert(lth, (toku_lock_tree*)1);
    CKERR2(r, ENOMEM);
    toku_lth_close(lth);
    lth = NULL;
    return 0;
}
