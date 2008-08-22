#include <stdio.h>
#include <unistd.h>
#include <assert.h>

#include "test.h"
#include "cachetable.h"

const int item_size = 1;

int n_flush, n_write_me, n_keep_me, n_fetch;

void flush(CACHEFILE cf, CACHEKEY key, void *value, long size, BOOL write_me, BOOL keep_me, LSN modified_lsn, BOOL rename_p) {
    cf = cf; modified_lsn = modified_lsn; rename_p = rename_p;
    assert(key == (CACHEKEY)(long)value);
    assert(size == item_size);
    n_flush++;
    if (write_me) n_write_me++;
    if (keep_me) n_keep_me++;
}

int fetch() {
    n_fetch++;
    return 0;
}

// put n items into the cachetable, maybe mark them dirty, do a checkpoint, and
// verify that all of the items have been written and are clean.

void cachetable_checkpoint_test(int n, int dirty) {
    const int test_limit = n;
    int r;
    CACHETABLE ct;
    r = toku_create_cachetable(&ct, test_limit, ZERO_LSN, NULL_LOGGER); assert(r == 0);
    char fname1[] = __FILE__ "test1.dat";
    unlink(fname1);
    CACHEFILE f1;
    r = toku_cachetable_openf(&f1, ct, fname1, O_RDWR|O_CREAT, 0777); assert(r == 0);

    // insert items into the cachetable. all should be dirty
    int i;
    for (i=0; i<n; i++) {
        u_int32_t hi = toku_cachetable_hash(f1, i);
        r = toku_cachetable_put(f1, i, hi, (void *)(long)i, 1, flush, fetch, 0);
        assert(r == 0);

        r = toku_cachetable_unpin(f1, i, hi, dirty, item_size);
        assert(r == 0);

        void *v;
        int its_dirty;
        long long its_pin;
        long its_size;
        r = toku_cachetable_get_key_state(ct, i, f1, &v, &its_dirty, &its_pin, &its_size);
        if (r != 0) 
            continue;
        assert(its_dirty == CACHETABLE_DIRTY);
        assert(its_pin == 0);
        assert(its_size == item_size);
    }

    // the checkpoint should cause n writes, but since n <= the cachetable size,
    // all items should be kept in the cachetable
    n_flush = n_write_me = n_keep_me = n_fetch = 0;
    r = toku_cachetable_checkpoint(ct);
    assert(r == 0);
    assert(n_flush == n && n_write_me == n && n_keep_me == n);

    // after the checkpoint, all of the items should be clean
    for (i=0; i<n; i++) {
        u_int32_t hi = toku_cachetable_hash(f1, i);
        void *v;
        r = toku_cachetable_maybe_get_and_pin(f1, i, hi, &v);
        if (r != 0) 
            continue;
        r = toku_cachetable_unpin(f1, i, hi, CACHETABLE_CLEAN, item_size);
        assert(r == 0);
        
        int its_dirty;
        long long its_pin;
        long its_size;
        r = toku_cachetable_get_key_state(ct, i, f1, &v, &its_dirty, &its_pin, &its_size);
        if (r != 0) 
            continue;
        assert(its_dirty == CACHETABLE_CLEAN);
        assert(its_pin == 0);
        assert(its_size == item_size);
    }

    // a subsequent checkpoint should cause n flushes, but no writes since all
    // of the items are clean
    n_flush = n_write_me = n_keep_me = n_fetch = 0;
    r = toku_cachetable_checkpoint(ct);
    assert(r == 0);
    assert(n_flush == n && n_write_me == 0 && n_keep_me == n);

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
    for (i=0; i<8; i++) {
        cachetable_checkpoint_test(i, CACHETABLE_CLEAN);
        cachetable_checkpoint_test(i, CACHETABLE_DIRTY);
    }
    return 0;
}
