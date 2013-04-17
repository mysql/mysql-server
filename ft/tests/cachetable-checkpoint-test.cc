/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#include "test.h"
#include "cachetable-test.h"
#include <stdio.h>
#include <unistd.h>


#include "checkpoint.h"

static const int item_size = 1;

static int n_flush, n_write_me, n_keep_me, n_fetch;

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
        bool UU(is_clone)
    ) 
{
    //cf = cf; key = key; value = value; extraargs = extraargs; 
    // assert(key == make_blocknum((long)value));
    assert(size.size == item_size);
    n_flush++;
    if (write_me) n_write_me++;
    if (keep_me) n_keep_me++;
}

static int callback_was_called = 0;
static int callback2_was_called = 0;

static void checkpoint_callback(void * extra) {
    int * x = (int*) extra;
    (*x)++;
    if (verbose) printf("checkpoint_callback called %d (should be 1-16)\n", *x);
}

static void checkpoint_callback2(void * extra) {
    int * x = (int*) extra;
    (*x)++;
    if (verbose) printf("checkpoint_callback2 called %d (should be 1-16)\n", *x);
}

// put n items into the cachetable, maybe mark them dirty, do a checkpoint, and
// verify that all of the items have been written and are clean.

static void cachetable_checkpoint_test(int n, enum cachetable_dirty dirty) {
    if (verbose) printf("%s:%d n=%d dirty=%d\n", __FUNCTION__, __LINE__, n, (int) dirty);
    const int test_limit = n;
    int r;
    CACHETABLE ct;
    toku_cachetable_create(&ct, test_limit, ZERO_LSN, NULL_LOGGER);
    char fname1[] = __SRCFILE__ "test1.dat";
    unlink(fname1);
    CACHEFILE f1;
    r = toku_cachetable_openf(&f1, ct, fname1, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO); assert(r == 0);
    create_dummy_functions(f1);
    
    // insert items into the cachetable. all should be dirty
    int i;
    for (i=0; i<n; i++) {
        CACHEKEY key = make_blocknum(i);
        uint32_t hi = toku_cachetable_hash(f1, key);
        CACHETABLE_WRITE_CALLBACK wc = def_write_callback(NULL);
        wc.flush_callback = flush;
        toku_cachetable_put(f1, key, hi, (void *)(long)i, make_pair_attr(1), wc, put_callback_nop);

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
    r = toku_checkpoint(cp, NULL, checkpoint_callback, &callback_was_called, checkpoint_callback2, &callback2_was_called, CLIENT_CHECKPOINT);
    assert(r == 0);
    assert(callback_was_called  != 0);
    assert(callback2_was_called != 0);
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

    r = toku_cachefile_close(&f1, false, ZERO_LSN); assert(r == 0 );
    toku_cachetable_close(&ct);
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
        cachetable_checkpoint_test(i, CACHETABLE_CLEAN);
        cachetable_checkpoint_test(i, CACHETABLE_DIRTY);
    }
    return 0;
}
