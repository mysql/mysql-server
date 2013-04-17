/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

#include "includes.h"
#include "test.h"

// this mutex is used by some of the tests to serialize access to some
// global data, especially between the test thread and the cachetable
// writeback threads

toku_mutex_t  test_mutex;

static inline void test_mutex_init(void) {
    toku_mutex_init(&test_mutex, 0);
}

static inline void test_mutex_destroy(void) {
    toku_mutex_destroy(&test_mutex);
}

static inline void test_mutex_lock(void) {
    toku_mutex_lock(&test_mutex);
}

static inline void test_mutex_unlock(void) {
    toku_mutex_unlock(&test_mutex);
}

// verify that cachetable creation and close works

static void
test_cachetable_create(void) {
    CACHETABLE ct = NULL;
    toku_cachetable_create(&ct, 0, ZERO_LSN, NULL_LOGGER);
    toku_cachetable_close(&ct);
}

static const int test_object_size = 1;

struct item {
    CACHEKEY key;
    const char *something;
};

static CACHEFILE expect_f;

static void maybe_flush(CACHETABLE t) {
    toku_cachetable_maybe_flush_some(t);
}


static void flush_n (CACHEFILE f __attribute__((__unused__)), int UU(fd), CACHEKEY key __attribute__((__unused__)),
		     void *value,
		     void** UU(dd),
		     void *extra  __attribute__((__unused__)),
                     PAIR_ATTR size __attribute__((__unused__)),
        PAIR_ATTR* new_size      __attribute__((__unused__)),
		     bool write_me __attribute__((__unused__)),    bool keep_me __attribute__((__unused__)),
		     bool for_checkpoint __attribute__ ((__unused__)),
        bool UU(is_clone)
		     ) {
    int *CAST_FROM_VOIDP(v, value);
    assert(*v==0);
}
static int fetch_n (CACHEFILE f __attribute__((__unused__)), PAIR UU(p), int UU(fd), CACHEKEY key __attribute__((__unused__)),
		    uint32_t fullhash  __attribute__((__unused__)),
                    void**value, 
		    void** UU(dd),
PAIR_ATTR *sizep __attribute__((__unused__)), 
		    int * dirtyp, void*extraargs) {
    assert((long)extraargs==42);
    *value=0;
    *dirtyp = 0;
    *sizep = make_pair_attr(0);
    return 0;
}


static void test_nested_pin (void) {
    void *f2=(void*)42;
    CACHETABLE t;
    CACHEFILE f;
    int i0, i1;
    int r;
    void *vv,*vv2;
    char fname[] = __SRCFILE__ "test_ct.dat";
    if (verbose) printf("creating cachetable\n");
    toku_cachetable_create(&t, 1, ZERO_LSN, NULL_LOGGER);
    unlink(fname);
    r = toku_cachetable_openf(&f, t, fname, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO);
    assert(r==0);
    expect_f = f;

    i0=0; i1=0;
    uint32_t f1hash = toku_cachetable_hash(f, make_blocknum(1));
    CACHETABLE_WRITE_CALLBACK wc = def_write_callback(f2);
    wc.flush_callback = flush_n;
    toku_cachetable_put(f, make_blocknum(1), f1hash, &i0, make_pair_attr(1), wc, put_callback_nop);
    r = toku_test_cachetable_unpin(f, make_blocknum(1), f1hash, CACHETABLE_CLEAN, make_pair_attr(test_object_size));
    r = toku_cachetable_get_and_pin(f, make_blocknum(1), f1hash, &vv, NULL, wc, fetch_n, def_pf_req_callback, def_pf_callback, true, f2);
    assert(r==0);
    assert(vv==&i0);
    assert(i0==0);
    r = toku_test_cachetable_unpin(f, make_blocknum(1), f1hash, CACHETABLE_CLEAN, make_pair_attr(test_object_size));
    assert(r==0);
    r = toku_cachetable_maybe_get_and_pin(f, make_blocknum(1), f1hash, &vv2);
    assert(r==0);
    assert(vv2==vv);
    r = toku_test_cachetable_unpin(f, make_blocknum(1), f1hash, CACHETABLE_CLEAN, make_pair_attr(test_object_size));
    assert(r==0);
    uint32_t f2hash = toku_cachetable_hash(f, make_blocknum(2));
    toku_cachetable_put(f, make_blocknum(2), f2hash, &i1, make_pair_attr(test_object_size), wc, put_callback_nop);
    r = toku_test_cachetable_unpin(f, make_blocknum(2), f2hash, CACHETABLE_CLEAN, make_pair_attr(test_object_size));
    assert(r==0);
    r = toku_cachefile_close(&f, false, ZERO_LSN); assert(r==0);
    toku_cachetable_close(&t);
}


static void null_flush (CACHEFILE cf     __attribute__((__unused__)),
                        int UU(fd),
                        CACHEKEY k       __attribute__((__unused__)),
                        void *v          __attribute__((__unused__)),
			void** UU(dd),
                        void *extra      __attribute__((__unused__)),
                        PAIR_ATTR size        __attribute__((__unused__)),
        PAIR_ATTR* new_size      __attribute__((__unused__)),
                        bool write_me    __attribute__((__unused__)),
                        bool keep_me     __attribute__((__unused__)),
                        bool for_checkpoint __attribute__((__unused__)),
        bool UU(is_clone)
                        ) {
}

static int add123_fetch (CACHEFILE cf, PAIR UU(p), int UU(fd), CACHEKEY key, uint32_t fullhash, void **value, 
			 void** UU(dd),
PAIR_ATTR *sizep __attribute__((__unused__)), int * dirtyp, void*extraargs) {
    assert(fullhash==toku_cachetable_hash(cf,key));
    assert((long)extraargs==123);
    *value = (void*)((unsigned long)key.b+123L);
    *dirtyp = 0;
    *sizep = make_pair_attr(0);
    return 0;
}

static int add222_fetch (CACHEFILE cf, PAIR UU(p), int UU(fd), CACHEKEY key, uint32_t fullhash, void **value, 
			 void** UU(dd),
PAIR_ATTR *sizep __attribute__((__unused__)), int * dirtyp, void*extraargs) {
    assert(fullhash==toku_cachetable_hash(cf,key));
    assert((long)extraargs==222);
    *value = (void*)((unsigned long)key.b+222L);
    *dirtyp = 0;
    *sizep = make_pair_attr(0);
    return 0;
}

#if !TOKU_WINDOWS

static void test_multi_filehandles (void) {
    CACHETABLE t;
    CACHEFILE f1,f2,f3;
    char fname1[]= __SRCFILE__ "test_ct.dat";
    char fname2[]= __SRCFILE__ "test2_ct.dat";
    char fname3[]= __SRCFILE__ "test3_ct.dat";
    int r;
    void *v;
    unlink(fname1);
    unlink(fname2);

    toku_cachetable_create(&t, 4, ZERO_LSN, NULL_LOGGER);
    r = toku_cachetable_openf(&f1, t, fname1, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO);   assert(r==0);
    r = link(fname1, fname2);                                     assert(r==0);
    r = toku_cachetable_openf(&f2, t, fname2, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO);   assert(r==0);
    r = toku_cachetable_openf(&f3, t, fname3, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO);   assert(r==0);

    assert(f1==f2);
    assert(f1!=f3);
    
    CACHETABLE_WRITE_CALLBACK wc = def_write_callback((void*)123);
    wc.flush_callback = null_flush;
    toku_cachetable_put(f1, make_blocknum(1), toku_cachetable_hash(f1, make_blocknum(1)), (void*)124, make_pair_attr(test_object_size), wc, put_callback_nop);
    r = toku_test_cachetable_unpin(f1, make_blocknum(1), toku_cachetable_hash(f1, make_blocknum(1)), CACHETABLE_DIRTY, make_pair_attr(0)); assert(r==0);
    r = toku_cachetable_get_and_pin(f2, make_blocknum(1), toku_cachetable_hash(f2, make_blocknum(1)), &v, NULL, wc, add123_fetch, def_pf_req_callback, def_pf_callback, true, (void*)123); assert(r==0);
    assert((unsigned long)v==124);
    r = toku_cachetable_get_and_pin(f2, make_blocknum(2), toku_cachetable_hash(f2, make_blocknum(2)), &v, NULL, wc, add123_fetch, def_pf_req_callback, def_pf_callback, true, (void*)123); assert(r==0);
    assert((unsigned long)v==125);
    wc.write_extraargs = (void*)222;
    r = toku_cachetable_get_and_pin(f3, make_blocknum(2), toku_cachetable_hash(f3, make_blocknum(2)), &v, NULL, wc, add222_fetch, def_pf_req_callback, def_pf_callback, true, (void*)222); assert(r==0);
    assert((unsigned long)v==224);

    // we support only one close for a file handle
    r = toku_test_cachetable_unpin(f1, make_blocknum(1), toku_cachetable_hash(f1, make_blocknum(1)), CACHETABLE_CLEAN, make_pair_attr(0)); assert(r==0);
    r = toku_test_cachetable_unpin(f2, make_blocknum(2), toku_cachetable_hash(f2, make_blocknum(2)), CACHETABLE_CLEAN, make_pair_attr(0)); assert(r==0);
    r = toku_cachefile_close(&f2, false, ZERO_LSN); assert(r==0);

    r = toku_test_cachetable_unpin(f3, make_blocknum(2), toku_cachetable_hash(f3, make_blocknum(2)), CACHETABLE_CLEAN, make_pair_attr(0)); assert(r==0);
    r = toku_cachefile_close(&f3, false, ZERO_LSN); assert(r==0);

    toku_cachetable_close(&t);
}

#endif

static void test_dirty_flush(CACHEFILE f,
                             int UU(fd),
			     CACHEKEY key,
			     void *value,
			     void** UU(dd),
			     void *extra __attribute__((__unused__)),
			     PAIR_ATTR size,
        PAIR_ATTR* new_size      __attribute__((__unused__)),
			     bool do_write,
			     bool keep,
			     bool for_checkpoint __attribute__((__unused__)),
        bool UU(is_clone)
			     ) {
    if (verbose) printf("test_dirty_flush %p %" PRId64 " %p %ld %u %u\n", f, key.b, value, size.size, (unsigned)do_write, (unsigned)keep);
}

static int test_dirty_fetch(CACHEFILE f, PAIR UU(p), int UU(fd), CACHEKEY key, uint32_t fullhash, void **value_ptr, 
			    void** UU(dd),
PAIR_ATTR *size_ptr, int * dirtyp, void *arg) {
    *value_ptr = arg;
    *dirtyp = 0;
    *size_ptr = make_pair_attr(0);
    assert(fullhash==toku_cachetable_hash(f,key));
    if (verbose) printf("test_dirty_fetch %p %" PRId64 " %p %ld %p\n", f, key.b, *value_ptr, size_ptr->size, arg);
    return 0;
}

static void test_dirty(void) {
    if (verbose) printf("test_dirty\n");

    CACHETABLE t;
    CACHEFILE f;
    CACHEKEY key; void *value;
    int dirty; long long pinned; long entry_size;
    int r;

    toku_cachetable_create(&t, 4, ZERO_LSN, NULL_LOGGER);

    const char *fname = __SRCFILE__ "test.dat";
    unlink(fname);
    r = toku_cachetable_openf(&f, t, fname, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO);
    assert(r == 0);

    key = make_blocknum(1); value = (void*)1;
    uint32_t hkey = toku_cachetable_hash(f, key);
    CACHETABLE_WRITE_CALLBACK wc = def_write_callback(NULL);
    wc.flush_callback = test_dirty_flush;
    toku_cachetable_put(f, key, hkey, value, make_pair_attr(test_object_size), wc, put_callback_nop);

    // cachetable_print_state(t);
    r = toku_cachetable_get_key_state(t, key, f, &value, &dirty, &pinned, &entry_size);
    assert(r == 0);
    assert(dirty == 1);
    assert(pinned == 1);

    r = toku_test_cachetable_unpin(f, key, hkey, CACHETABLE_CLEAN, make_pair_attr(0));
    assert(r == 0);
    r = toku_cachetable_get_key_state(t, key, f, &value, &dirty, &pinned, &entry_size);
    assert(r == 0);
    assert(dirty == 1);
    assert(pinned == 0);

    r = toku_cachetable_get_and_pin(f, key, hkey, &value, NULL, wc,
				    test_dirty_fetch, def_pf_req_callback, def_pf_callback, true, 0);
    assert(r == 0);

    // cachetable_print_state(t);
    r = toku_cachetable_get_key_state(t, key, f, &value, &dirty, &pinned, &entry_size);
    assert(r == 0);
    assert(dirty == 1);
    assert(pinned == 1);

    r = toku_test_cachetable_unpin(f, key, hkey, CACHETABLE_CLEAN, make_pair_attr(test_object_size));
    assert(r == 0);

    // cachetable_print_state(t);
    r = toku_cachetable_get_key_state(t, key, f, &value, &dirty, &pinned, &entry_size);
    assert(r == 0);
    assert(dirty == 1);
    assert(pinned == 0);

    key = make_blocknum(2);
    hkey = toku_cachetable_hash(f, key);
    r = toku_cachetable_get_and_pin(f, key, hkey,
				    &value, NULL, wc,
				    test_dirty_fetch, def_pf_req_callback, def_pf_callback, true, 0);
    assert(r == 0);

    // cachetable_print_state(t);
    r = toku_cachetable_get_key_state(t, key, f, &value, &dirty, &pinned, &entry_size);
    assert(r == 0);
    assert(dirty == 0);
    assert(pinned == 1);

    r = toku_test_cachetable_unpin(f, key, hkey, CACHETABLE_CLEAN, make_pair_attr(test_object_size));
    assert(r == 0);

    // cachetable_print_state(t);
    r = toku_cachetable_get_key_state(t, key, f, &value, &dirty, &pinned, &entry_size);
    assert(r == 0);
    assert(dirty == 0);
    assert(pinned == 0);

    r = toku_cachetable_get_and_pin(f, key, hkey,
				    &value, NULL, wc,
				    test_dirty_fetch, def_pf_req_callback, def_pf_callback, true, 0);
    assert(r == 0);

    // cachetable_print_state(t);
    r = toku_cachetable_get_key_state(t, key, f, &value, &dirty, &pinned, &entry_size);
    assert(r == 0);
    assert(dirty == 0);
    assert(pinned == 1);

    r = toku_test_cachetable_unpin(f, key, hkey, CACHETABLE_DIRTY, make_pair_attr(test_object_size));
    assert(r == 0);

    // cachetable_print_state(t);
    r = toku_cachetable_get_key_state(t, key, f, &value, &dirty, &pinned, &entry_size);
    assert(r == 0);
    assert(dirty == 1);
    assert(pinned == 0);

    toku_cachefile_close(&f, false, ZERO_LSN);

    toku_cachetable_close(&t);
}

static int test_size_debug;
static CACHEKEY test_size_flush_key;

static void test_size_flush_callback(CACHEFILE f,
                                     int UU(fd),
				     CACHEKEY key,
				     void *value,
				     void** UU(dd),
				     void *extra __attribute__((__unused__)),
				     PAIR_ATTR size,
        PAIR_ATTR* new_size      __attribute__((__unused__)),
				     bool do_write,
				     bool keep,
				     bool for_checkpoint __attribute__((__unused__)),
        bool UU(is_clone)
				     ) {
    if (test_size_debug && verbose) printf("test_size_flush %p %" PRId64 " %p %ld %u %u\n", f, key.b, value, size.size, (unsigned)do_write, (unsigned)keep);
    if (keep) {
        if (do_write) {
            test_mutex_lock();
            test_size_flush_key = key;
            test_mutex_unlock();
        }
    } else {
        assert(!do_write);
    }
}

static void test_size_resize(void) {
    if (verbose) printf("test_size_resize\n");

    CACHETABLE t;
    CACHEFILE f;
    int r;

    int n = 3;
    long size = 1;

    toku_cachetable_create(&t, n*size, ZERO_LSN, NULL_LOGGER);

    const char *fname = __SRCFILE__ "test.dat";
    unlink(fname);
    r = toku_cachetable_openf(&f, t, fname, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO);
    assert(r == 0);

    CACHEKEY key = make_blocknum(42);
    void *value = (void *) -42;

    uint32_t hkey = toku_cachetable_hash(f, key);

    CACHETABLE_WRITE_CALLBACK wc = def_write_callback(NULL);
    wc.flush_callback = test_size_flush_callback;
    toku_cachetable_put(f, key, hkey, value, make_pair_attr(size), wc, put_callback_nop);

    void *entry_value; int dirty; long long pinned; long entry_size;
    r = toku_cachetable_get_key_state(t, key, f, &entry_value, &dirty, &pinned, &entry_size);
    assert(r == 0);
    assert(dirty == 1);
    assert(pinned == 1);
    assert(entry_value == value);
    assert(entry_size == size);

    long long new_size = 2*size;
    r = toku_test_cachetable_unpin(f, key, hkey, CACHETABLE_CLEAN, make_pair_attr(new_size));
    assert(r == 0);

    void *current_value;
    long current_size;
    r = toku_cachetable_get_and_pin(f, key, hkey, &current_value, &current_size, wc, 0, def_pf_req_callback, def_pf_callback, true, 0);
    assert(r == 0);
    assert(current_value == value);
    assert(current_size == new_size);

    r = toku_test_cachetable_unpin(f, key, hkey, CACHETABLE_CLEAN, make_pair_attr(new_size));
    assert(r == 0);

    toku_cachefile_close(&f, false, ZERO_LSN);
    toku_cachetable_close(&t);
}

static int min2(int a, int b) { return a < b ? a : b; }

__attribute__((unused))
static void test_size_flush(void) {
    if (verbose) printf("test_size_flush\n");

    CACHETABLE t;
    CACHEFILE f;
    int r;

    const int n = 8;
    long long size = 1*1024*1024;
    toku_cachetable_create(&t, n*size, ZERO_LSN, NULL_LOGGER);

    const char *fname = __SRCFILE__ "test.dat";
    unlink(fname);
    r = toku_cachetable_openf(&f, t, fname, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO);
    assert(r == 0);

    /* put 2*n keys into the table, ensure flushes occur in key order */
    test_mutex_lock();
    test_size_flush_key = make_blocknum(-1);
    test_mutex_unlock();
    
    int i;
    CACHEKEY expect_flush_key = make_blocknum(0);
    for (i=0; i<2*n; i++) {
        CACHEKEY key = make_blocknum(i);
        void *value = (void *)(long)-i;
        //        printf("test_size put %lld %p %lld\n", key, value, size);
	uint32_t hkey = toku_cachetable_hash(f, key);
        CACHETABLE_WRITE_CALLBACK wc = def_write_callback(NULL);
        wc.flush_callback = test_size_flush_callback;
        toku_cachetable_put(f, key, hkey, value, make_pair_attr(size), wc, put_callback_nop);

        int n_entries, hash_size; long size_current, size_limit;
        toku_cachetable_get_state(t, &n_entries, &hash_size, &size_current, &size_limit);
        while (n_entries != min2(i+1, n)) {
            toku_pthread_yield(); maybe_flush(t);
            toku_cachetable_get_state(t, &n_entries, 0, 0, 0);
        }
        assert(n_entries == min2(i+1, n));

        void *entry_value; int dirty; long long pinned; long entry_size;
        r = toku_cachetable_get_key_state(t, key, f, &entry_value, &dirty, &pinned, &entry_size);
        assert(r == 0);
        assert(dirty == 1);
        assert(pinned == 1);
        assert(entry_value == value);
        assert(entry_size == size);

        test_mutex_lock();
        if (test_size_flush_key.b != -1) {
            assert(test_size_flush_key.b == expect_flush_key.b);
            assert(expect_flush_key.b == i-n);
            expect_flush_key.b += 1;
        }
        test_mutex_unlock();

        r = toku_test_cachetable_unpin(f, key, hkey, CACHETABLE_CLEAN, make_pair_attr(size));
        assert(r == 0);
    }
    
    r = toku_cachefile_close(&f, false, ZERO_LSN);
    assert(r == 0);
    toku_cachetable_close(&t);
}

int
test_main (int argc, const char *argv[]) {
    // parse args
    int i;
    for (i=1; i<argc; i++) {
        const char *arg = argv[i];
        if (strcmp(arg, "-v") == 0) {
            verbose++;
            continue;
        }
        if (strcmp(arg, "-q") == 0) {
            if (verbose > 0) verbose--;
            continue;
        }
    }

    test_mutex_init();

    // run tests
#if !TOKU_WINDOWS
    test_multi_filehandles();
#endif
    test_cachetable_create();
    for (i=0; i<1; i++) {
        test_nested_pin();
#if !TOKU_WINDOWS
        test_multi_filehandles ();
#endif
        test_dirty();
        test_size_resize();
        //test_size_flush();
    }

    test_mutex_destroy();
    
    if (verbose) printf("ok\n");
    return 0;
}
