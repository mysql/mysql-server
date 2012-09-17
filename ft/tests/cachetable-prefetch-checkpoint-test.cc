/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

// verify that the cache table checkpoint with prefetched blocks active works.
// the blocks in the reading state should be ignored.
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#include "test.h"
#include <stdio.h>
#include <unistd.h>
#include "cachetable-test.h"

#include "checkpoint.h"

const int item_size = 1;

int n_flush, n_write_me, n_keep_me, n_fetch;

static void flush(
    CACHEFILE UU(cf), 
    int UU(fd), 
    CACHEKEY UU(key), 
    void *UU(value), 
    void** UU(dd),
    void *UU(extraargs), 
    PAIR_ATTR size, 
    PAIR_ATTR* UU(new_size), 
    bool write_me, 
    bool keep_me, 
    bool UU(for_checkpoint),
        bool UU(is_clone), bool UU(aggressive)
    ) 
{
    // assert(key == make_blocknum((long)value));
    assert(size.size == item_size);
    n_flush++;
    if (write_me) n_write_me++;
    if (keep_me) n_keep_me++;
}

static int fetch(
    CACHEFILE UU(cf), 
    PAIR UU(p),
    int UU(fd), 
    CACHEKEY UU(key), 
    uint32_t UU(fullhash), 
    void **UU(value), 
    void** UU(dd),
    PAIR_ATTR *UU(sizep), 
    int *dirtyp, 
    void *UU(extraargs)
    ) 
{
    n_fetch++;
    sleep(10);
    *value = 0;
    *sizep = make_pair_attr(item_size);
    *dirtyp = 0;
    return 0;
}

// put n items into the cachetable, maybe mark them dirty, do a checkpoint, and
// verify that all of the items have been written and are clean.
static void cachetable_prefetch_checkpoint_test(int n, enum cachetable_dirty dirty) {
    if (verbose) printf("%s:%d n=%d dirty=%d\n", __FUNCTION__, __LINE__, n, (int) dirty);
    const int test_limit = n;
    int r;
    CACHETABLE ct;
    CACHETABLE_WRITE_CALLBACK wc = def_write_callback(NULL);
    wc.flush_callback = flush;
    r = toku_create_cachetable(&ct, test_limit, ZERO_LSN, NULL_LOGGER); assert(r == 0);
    char fname1[] = __SRCFILE__ "test1.dat";
    unlink(fname1);
    CACHEFILE f1;
    r = toku_cachetable_openf(&f1, ct, fname1, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO); assert(r == 0);
    create_dummy_functions(f1);

    // prefetch block n+1. this will take 10 seconds.
    {
        CACHEKEY key = make_blocknum(n+1);
        uint32_t fullhash = toku_cachetable_hash(f1, key);
        r = toku_cachefile_prefetch(f1, key, fullhash, wc, fetch, def_pf_req_callback, def_pf_callback, 0, NULL);
        toku_cachetable_verify(ct);
    }

    // insert items into the cachetable. all should be dirty
    int i;
    for (i=0; i<n; i++) {
        CACHEKEY key = make_blocknum(i);
        uint32_t hi = toku_cachetable_hash(f1, key);
        r = toku_cachetable_put(f1, key, hi, (void *)(long)i, make_pair_attr(1), wc, put_callback_nop);
        assert(r == 0);

        r = toku_test_cachetable_unpin(f1, key, hi, dirty, make_pair_attr(item_size));
        assert(r == 0);

        void *v;
        int its_dirty;
        long long its_pin;
        long its_size;
        r = toku_cachetable_get_key_state(ct, key, f1, &v, &its_dirty, &its_pin, &its_size);
        if (r != 0) 
            continue;
        assert(its_dirty == CACHETABLE_DIRTY);
        assert(its_pin == 0);
        assert(its_size == item_size);
    }

    // the checkpoint should cause n writes, but since n <= the cachetable size,
    // all items should be kept in the cachetable
    n_flush = n_write_me = n_keep_me = n_fetch = 0;
    CHECKPOINTER cp = toku_cachetable_get_checkpointer(ct);
    r = toku_checkpoint(cp, NULL, NULL, NULL, NULL, NULL, CLIENT_CHECKPOINT);
    assert(r == 0);
    assert(n_flush == n && n_write_me == n && n_keep_me == n);

    // after the checkpoint, all of the items should be clean
    for (i=0; i<n; i++) {
        CACHEKEY key = make_blocknum(i);
        uint32_t hi = toku_cachetable_hash(f1, key);
        void *v;
        r = toku_cachetable_maybe_get_and_pin(f1, key, hi, &v);
        if (r != 0) 
            continue;
        r = toku_test_cachetable_unpin(f1, key, hi, CACHETABLE_CLEAN, make_pair_attr(item_size));
        assert(r == 0);
        
        int its_dirty;
        long long its_pin;
        long its_size;
        r = toku_cachetable_get_key_state(ct, key, f1, &v, &its_dirty, &its_pin, &its_size);
        if (r != 0) 
            continue;
        assert(its_dirty == CACHETABLE_CLEAN);
        assert(its_pin == 0);
        assert(its_size == item_size);
    }

    // a subsequent checkpoint should cause no flushes, or writes since all of the items are clean
    n_flush = n_write_me = n_keep_me = n_fetch = 0;

    r = toku_checkpoint(cp, NULL, NULL, NULL, NULL, NULL, CLIENT_CHECKPOINT);
    assert(r == 0);
    assert(n_flush == 0 && n_write_me == 0 && n_keep_me == 0);

    r = toku_cachefile_close(&f1, 0, false, ZERO_LSN); assert(r == 0);
    r = toku_cachetable_close(&ct); assert(r == 0 && ct == 0);
}

int
test_main(int argc, const char *argv[]) {
    int i;
    for (i=1; i<argc; i++) {
        if (strcmp(argv[i], "-v") == 0) {
            verbose++;
            continue;
        }
    }
    for (i=0; i<8; i++) {
        cachetable_prefetch_checkpoint_test(i, CACHETABLE_CLEAN);
        cachetable_prefetch_checkpoint_test(i, CACHETABLE_DIRTY);
    }
    return 0;
}
