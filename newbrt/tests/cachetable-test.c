/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

#include "includes.h"
#include "test.h"

// this mutex is used by some of the tests to serialize access to some
// global data, especially between the test thread and the cachetable
// writeback threads

toku_pthread_mutex_t  test_mutex;

static inline void test_mutex_init(void) {
    int r = toku_pthread_mutex_init(&test_mutex, 0); assert(r == 0);
}

static inline void test_mutex_destroy(void) {
    int r = toku_pthread_mutex_destroy(&test_mutex); assert(r == 0);
}

static inline void test_mutex_lock(void) {
    int r = toku_pthread_mutex_lock(&test_mutex); assert(r == 0);
}

static inline void test_mutex_unlock(void) {
    int r = toku_pthread_mutex_unlock(&test_mutex); assert(r == 0);
}

// hook my_malloc_always_fails into malloc to control malloc and verify
// the correct recovery from malloc failures
#if defined(__linux__)
#define DO_MALLOC_HOOK 1
#else
#define DO_MALLOC_HOOK 0
#endif
#if DO_MALLOC_HOOK
static void *my_malloc_always_fails(size_t n, const __malloc_ptr_t p) {
    n = n; p = p;
    return 0;
}
#endif

// verify that cachetable creation and close works

static void
test_cachetable_create(void) {
    CACHETABLE ct = 0;
    int r;
    r = toku_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER);
    assert(r == 0);
    r = toku_cachetable_close(&ct);
    assert(r == 0 && ct == 0);
}

// verify that cachetable create with no memory returns ENOMEM
#if DO_MALLOC_HOOK

static void
test_cachetable_create_no_memory (void) {
    void *(*orig_malloc_hook)(size_t, const __malloc_ptr_t) = __malloc_hook;
    __malloc_hook = my_malloc_always_fails;
    CACHETABLE ct = 0;
    int r;
    r = toku_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER);
    assert(r == ENOMEM);
    __malloc_hook = orig_malloc_hook;
}

#endif

static const int test_object_size = 1;

struct item {
    CACHEKEY key;
    char *something;
};

static volatile int expect_n_flushes=0;
static volatile CACHEKEY flushes[100];

static void expect_init(void) {
    test_mutex_lock();
    expect_n_flushes = 0;
    test_mutex_unlock();
}

static void expect1(int64_t blocknum_n) {
    test_mutex_lock();
    expect_n_flushes=1;
    flushes[0].b=blocknum_n;
    //if (verbose) printf("%s:%d %lld\n", __FUNCTION__, 0, key.b);
    test_mutex_unlock();
}
static void expectN(int64_t blocknum_n) {
    test_mutex_lock();
    //if (verbose) printf("%s:%d %lld\n", __FUNCTION__, expect_n_flushes, key);
    flushes[expect_n_flushes++].b=blocknum_n;
    test_mutex_unlock();
}

static CACHEFILE expect_f;

static void flush (CACHEFILE f,
                   int UU(fd),
		   CACHEKEY key,
		   void*value,
		   void *extra __attribute__((__unused__)),
		   long size __attribute__((__unused__)),
		   BOOL write_me __attribute__((__unused__)),
		   BOOL keep_me __attribute__((__unused__)),
		   BOOL for_checkpoint __attribute__((__unused__))) {
    struct item *it = value;
    int i;

    if (keep_me) return;

    if (verbose) printf("Flushing %" PRId64 " (it=>key=%" PRId64 ")\n", key.b, it->key.b);

    test_mutex_lock();
    assert(expect_f==f);
    assert(strcmp(it->something,"something")==0);
    assert(it->key.b==key.b);

    /* Verify that we expected the flush. */
    for (i=0; i<expect_n_flushes; i++) {
	if (key.b==flushes[i].b) {
	    flushes[i] = flushes[expect_n_flushes-1];
	    expect_n_flushes--;
	    goto found_flush;
	}
    }
    fprintf(stderr, "%" PRId64 " was flushed, but I didn't expect it\n", key.b);
    abort();
 found_flush:
    test_mutex_unlock();
    toku_free(value);
}

static struct item *make_item (u_int64_t key) {
    struct item *MALLOC(it);
    it->key.b=key;
    it->something="something";
    return it;
}

static CACHEKEY did_fetch={-1};
static int fetch (CACHEFILE f, int UU(fd), CACHEKEY key, u_int32_t fullhash __attribute__((__unused__)), void**value, long *sizep __attribute__((__unused__)), void*extraargs) {
    if (verbose) printf("Fetch %" PRId64 "\n", key.b);
    assert (expect_f==f);
    assert((long)extraargs==23);
    *value = make_item(key.b);
    *sizep = test_object_size;
    did_fetch=key;
    return 0;
}

static void maybe_flush(CACHETABLE t) {
#if !TOKU_CACHETABLE_DO_EVICT_FROM_WRITER
    toku_cachetable_maybe_flush_some(t);
#endif
}

// verify that a sequence of cachetable operations causes a particular sequence of
// callbacks

static void test0 (void) {
    void* t3=(void*)23;
    CACHETABLE t;
    CACHEFILE f;
    int r;
    char fname[] = __FILE__ "test.dat";

    r=toku_create_cachetable(&t, 5, ZERO_LSN, NULL_LOGGER);
    assert(r==0);
    unlink(fname);
    r = toku_cachetable_openf(&f, t, fname, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO);
    assert(r==0);

    TOKULOGGER logger = toku_cachefile_logger(f);
    assert(logger == NULL_LOGGER);

    expect_f = f;

    expect_n_flushes=0;
    u_int32_t h1 = toku_cachetable_hash(f, make_blocknum(1));
    u_int32_t h2 = toku_cachetable_hash(f, make_blocknum(2));
    u_int32_t h3 = toku_cachetable_hash(f, make_blocknum(3));
    u_int32_t h4 = toku_cachetable_hash(f, make_blocknum(4));
    u_int32_t h5 = toku_cachetable_hash(f, make_blocknum(5));
    u_int32_t h6 = toku_cachetable_hash(f, make_blocknum(6));
    u_int32_t h7 = toku_cachetable_hash(f, make_blocknum(7));
    r=toku_cachetable_put(f, make_blocknum(1), h1, make_item(1), test_object_size, flush, fetch, t3);   /* 1P */        /* this is the lru list.  1 is pinned. */
    assert(r==0);
    assert(expect_n_flushes==0);

    expect_init();
    r=toku_cachetable_put(f, make_blocknum(2), h2, make_item(2), test_object_size, flush, fetch, t3);
    assert(r==0);
    r=toku_cachetable_unpin(f, make_blocknum(2), h2, CACHETABLE_DIRTY, 1);           /* 2U 1P */
    assert(expect_n_flushes==0);

    expect_init(); 
    r=toku_cachetable_put(f, make_blocknum(3), h3, make_item(3), test_object_size, flush, fetch, t3);
    assert(r==0);
    assert(expect_n_flushes==0);            /* 3P 2U 1P */   /* 3 is most recently used (pinned), 2 is next (unpinned), 1 is least recent (pinned) */

    expect_init(); 
    r=toku_cachetable_put(f, make_blocknum(4), h4, make_item(4), test_object_size, flush, fetch, t3);
    assert(r==0);
    assert(expect_n_flushes==0);            /* 4P 3P 2U 1P */

    expect_init();
    r=toku_cachetable_put(f, make_blocknum(5), h5, make_item(5), test_object_size, flush, fetch, t3);
    assert(r==0);
    r=toku_cachetable_unpin(f, make_blocknum(5), h5, CACHETABLE_DIRTY, test_object_size);
    assert(r==0);
    r=toku_cachetable_unpin(f, make_blocknum(3), h3, CACHETABLE_DIRTY, test_object_size);
    assert(r==0);
    assert(expect_n_flushes==0);            /* 5U 4P 3U 2U 1P */

    expect1(2); /* 2 is the oldest unpinned item. */
    r=toku_cachetable_put(f, make_blocknum(6), h6, make_item(6), test_object_size, flush, fetch, t3);   /* 6P 5U 4P 3U 1P */
    assert(r==0);
    test_mutex_lock();
    while (expect_n_flushes != 0) {
        test_mutex_unlock(); toku_pthread_yield(); maybe_flush(t); test_mutex_lock();
    }
    assert(expect_n_flushes==0);
    test_mutex_unlock();

    expect1(3);
    r=toku_cachetable_put(f, make_blocknum(7), h7, make_item(7), test_object_size, flush, fetch, t3);
    assert(r==0);
    test_mutex_lock();
    while (expect_n_flushes != 0) {
        test_mutex_unlock(); toku_pthread_yield(); maybe_flush(t); test_mutex_lock();
    }
    assert(expect_n_flushes==0);
    test_mutex_unlock();
    r=toku_cachetable_unpin(f, make_blocknum(7), h7, CACHETABLE_DIRTY, test_object_size);           /* 7U 6P 5U 4P 1P */
    assert(r==0);

    {
	void *item_v=0;
	expect_init(); 
	r=toku_cachetable_get_and_pin(f, make_blocknum(5), toku_cachetable_hash(f, make_blocknum(5)), &item_v, NULL, flush, fetch, t3);  /* 5P 7U 6P 4P 1P */
	assert(r==0);
	assert(((struct item *)item_v)->key.b==5);
	assert(strcmp(((struct item *)item_v)->something,"something")==0);
        test_mutex_lock();
	assert(expect_n_flushes==0);
        test_mutex_unlock();
    }

    {
	void *item_v=0;
	r=toku_cachetable_unpin(f, make_blocknum(4), h4, CACHETABLE_DIRTY, test_object_size);
	assert(r==0);
	expect1(4);
	did_fetch=make_blocknum(-1);
	r=toku_cachetable_get_and_pin(f, make_blocknum(2), toku_cachetable_hash(f, make_blocknum(2)), &item_v, NULL, flush, fetch, t3);  /* 2p 5P 7U 6P 1P */
	assert(r==0);
	assert(did_fetch.b==2); /* Expect that 2 is fetched in. */
	assert(((struct item *)item_v)->key.b==2);
	assert(strcmp(((struct item *)item_v)->something,"something")==0);
        test_mutex_lock();
        while (expect_n_flushes != 0) {
            test_mutex_unlock(); toku_pthread_yield(); maybe_flush(t); test_mutex_lock();
        }
        assert(expect_n_flushes==0);
        test_mutex_unlock();
    }
	
    r=toku_cachetable_unpin(f, make_blocknum(2), h2, CACHETABLE_DIRTY, test_object_size);
    assert(r==0);
    r=toku_cachetable_unpin(f, make_blocknum(5), h5, CACHETABLE_DIRTY, test_object_size);
    assert(r==0);
    r=toku_cachetable_unpin(f, make_blocknum(6), h6, CACHETABLE_DIRTY, test_object_size);
    assert(r==0);
    r=toku_cachetable_unpin(f, make_blocknum(1), h1, CACHETABLE_DIRTY, test_object_size);
    assert(r==0);
    r=toku_cachetable_assert_all_unpinned(t);
    assert(r==0);

    if (verbose) printf("Closing\n");
    expect1(2);
    expectN(5);
    expectN(7);
    expectN(6);
    expectN(1);
    r=toku_cachefile_close(&f, 0, FALSE, ZERO_LSN);
    assert(r==0);
    r=toku_cachetable_close(&t);
    assert(r==0);
    assert(expect_n_flushes==0);
    expect_f = 0;
    toku_memory_check_all_free();
}

static void flush_n (CACHEFILE f __attribute__((__unused__)), int UU(fd), CACHEKEY key __attribute__((__unused__)),
		     void *value,
		     void *extra  __attribute__((__unused__)),
                     long size __attribute__((__unused__)),
		     BOOL write_me __attribute__((__unused__)),    BOOL keep_me __attribute__((__unused__)),
		     BOOL for_checkpoint __attribute__ ((__unused__))) {
    int *v = value;
    assert(*v==0);
}
static int fetch_n (CACHEFILE f __attribute__((__unused__)), int UU(fd), CACHEKEY key __attribute__((__unused__)),
		    u_int32_t fullhash  __attribute__((__unused__)),
                    void**value, long *sizep __attribute__((__unused__)), void*extraargs) {
    assert((long)extraargs==42);
    *value=0;
    return 0;
}


static void test_nested_pin (void) {
    void *f2=(void*)42;
    CACHETABLE t;
    CACHEFILE f;
    int i0, i1;
    int r;
    void *vv,*vv2;
    char fname[] = __FILE__ "test_ct.dat";
    r = toku_create_cachetable(&t, 1, ZERO_LSN, NULL_LOGGER);
    assert(r==0);
    unlink(fname);
    r = toku_cachetable_openf(&f, t, fname, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO);
    assert(r==0);
    expect_f = f;

    i0=0; i1=0;
    u_int32_t f1hash = toku_cachetable_hash(f, make_blocknum(1));
    r = toku_cachetable_put(f, make_blocknum(1), f1hash, &i0, 1, flush_n, fetch_n, f2);
    assert(r==0);
    r = toku_cachetable_get_and_pin(f, make_blocknum(1), f1hash, &vv, NULL, flush_n, fetch_n, f2);
    assert(r==0);
    assert(vv==&i0);
    assert(i0==0);
    r = toku_cachetable_unpin(f, make_blocknum(1), f1hash, CACHETABLE_CLEAN, test_object_size);
    assert(r==0);
    r = toku_cachetable_maybe_get_and_pin(f, make_blocknum(1), f1hash, &vv2);
    assert(r==0);
    assert(vv2==vv);
    r = toku_cachetable_unpin(f, make_blocknum(1), f1hash, CACHETABLE_CLEAN, test_object_size);
    assert(r==0);
    u_int32_t f2hash = toku_cachetable_hash(f, make_blocknum(2));
    r = toku_cachetable_put(f, make_blocknum(2), f2hash, &i1, test_object_size, flush_n, fetch_n, f2);
    assert(r==0); // The other one is pinned, but now the cachetable fails gracefully:  It allows the pin to happen
    r = toku_cachetable_unpin(f, make_blocknum(1), f1hash, CACHETABLE_CLEAN, test_object_size);
    assert(r==0);
    r = toku_cachetable_unpin(f, make_blocknum(2), f2hash, CACHETABLE_CLEAN, test_object_size);
    assert(r==0);
    //    toku_os_usleep(1*1000000);
    r = toku_cachefile_close(&f, 0, FALSE, ZERO_LSN); assert(r==0);
    r = toku_cachetable_close(&t); assert(r==0);
}


static void null_flush (CACHEFILE cf     __attribute__((__unused__)),
                        int UU(fd),
                        CACHEKEY k       __attribute__((__unused__)),
                        void *v          __attribute__((__unused__)),
                        void *extra      __attribute__((__unused__)),
                        long size        __attribute__((__unused__)),
                        BOOL write_me    __attribute__((__unused__)),
                        BOOL keep_me     __attribute__((__unused__)),
                        BOOL for_checkpoint __attribute__((__unused__))) {
}

static int add123_fetch (CACHEFILE cf, int UU(fd), CACHEKEY key, u_int32_t fullhash, void **value, long *sizep __attribute__((__unused__)), void*extraargs) {
    assert(fullhash==toku_cachetable_hash(cf,key));
    assert((long)extraargs==123);
    *value = (void*)((unsigned long)key.b+123L);
    return 0;
}

static int add222_fetch (CACHEFILE cf, int UU(fd), CACHEKEY key, u_int32_t fullhash, void **value, long *sizep __attribute__((__unused__)), void*extraargs) {
    assert(fullhash==toku_cachetable_hash(cf,key));
    assert((long)extraargs==222);
    *value = (void*)((unsigned long)key.b+222L);
    return 0;
}

#if !TOKU_WINDOWS

static void test_multi_filehandles (void) {
    CACHETABLE t;
    CACHEFILE f1,f2,f3;
    char fname1[]= __FILE__ "test_ct.dat";
    char fname2[]= __FILE__ "test2_ct.dat";
    char fname3[]= __FILE__ "test3_ct.dat";
    int r;
    void *v;
    unlink(fname1);
    unlink(fname2);

    r = toku_create_cachetable(&t, 4, ZERO_LSN, NULL_LOGGER);          assert(r==0);
    r = toku_cachetable_openf(&f1, t, fname1, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO);   assert(r==0);
    r = link(fname1, fname2);                                     assert(r==0);
    r = toku_cachetable_openf(&f2, t, fname2, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO);   assert(r==0);
    r = toku_cachetable_openf(&f3, t, fname3, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO);   assert(r==0);

    assert(f1==f2);
    assert(f1!=f3);
    
    r = toku_cachetable_put(f1, make_blocknum(1), toku_cachetable_hash(f1, make_blocknum(1)), (void*)124, test_object_size, null_flush, add123_fetch, (void*)123); assert(r==0);
    r = toku_cachetable_get_and_pin(f2, make_blocknum(1), toku_cachetable_hash(f2, make_blocknum(1)), &v, NULL, null_flush, add123_fetch, (void*)123); assert(r==0);
    assert((unsigned long)v==124);
    r = toku_cachetable_get_and_pin(f2, make_blocknum(2), toku_cachetable_hash(f2, make_blocknum(2)), &v, NULL, null_flush, add123_fetch, (void*)123); assert(r==0);
    assert((unsigned long)v==125);
    r = toku_cachetable_get_and_pin(f3, make_blocknum(2), toku_cachetable_hash(f3, make_blocknum(2)), &v, NULL, null_flush, add222_fetch, (void*)222); assert(r==0);
    assert((unsigned long)v==224);
    r = toku_cachetable_maybe_get_and_pin_clean(f1, make_blocknum(2), toku_cachetable_hash(f1, make_blocknum(2)), &v); assert(r==0);
    assert((unsigned long)v==125);

    r = toku_cachetable_unpin(f1, make_blocknum(1), toku_cachetable_hash(f1, make_blocknum(1)), CACHETABLE_CLEAN, 0); assert(r==0);
    r = toku_cachetable_unpin(f1, make_blocknum(2), toku_cachetable_hash(f1, make_blocknum(2)), CACHETABLE_CLEAN, 0); assert(r==0);
    r = toku_cachefile_close(&f1, 0, FALSE, ZERO_LSN); assert(r==0);

    r = toku_cachetable_unpin(f2, make_blocknum(1), toku_cachetable_hash(f2, make_blocknum(1)), CACHETABLE_CLEAN, 0); assert(r==0);
    r = toku_cachetable_unpin(f2, make_blocknum(2), toku_cachetable_hash(f2, make_blocknum(2)), CACHETABLE_CLEAN, 0); assert(r==0);
    r = toku_cachefile_close(&f2, 0, FALSE, ZERO_LSN); assert(r==0);

    r = toku_cachetable_unpin(f3, make_blocknum(2), toku_cachetable_hash(f3, make_blocknum(2)), CACHETABLE_CLEAN, 0); assert(r==0);
    r = toku_cachefile_close(&f3, 0, FALSE, ZERO_LSN); assert(r==0);

    r = toku_cachetable_close(&t); assert(r==0);
}

#endif

static void test_dirty_flush(CACHEFILE f,
                             int UU(fd),
			     CACHEKEY key,
			     void *value,
			     void *extra __attribute__((__unused__)),
			     long size,
			     BOOL do_write,
			     BOOL keep,
			     BOOL for_checkpoint __attribute__((__unused__))) {
    if (verbose) printf("test_dirty_flush %p %" PRId64 " %p %ld %u %u\n", f, key.b, value, size, (unsigned)do_write, (unsigned)keep);
}

static int test_dirty_fetch(CACHEFILE f, int UU(fd), CACHEKEY key, u_int32_t fullhash, void **value_ptr, long *size_ptr, void *arg) {
    *value_ptr = arg;
    assert(fullhash==toku_cachetable_hash(f,key));
    if (verbose) printf("test_dirty_fetch %p %" PRId64 " %p %ld %p\n", f, key.b, *value_ptr, *size_ptr, arg);
    return 0;
}

static void test_dirty(void) {
    if (verbose) printf("test_dirty\n");

    CACHETABLE t;
    CACHEFILE f;
    CACHEKEY key; void *value;
    int dirty; long long pinned; long entry_size;
    int r;

    r = toku_create_cachetable(&t, 4, ZERO_LSN, NULL_LOGGER);
    assert(r == 0);

    char *fname = __FILE__ "test.dat";
    unlink(fname);
    r = toku_cachetable_openf(&f, t, fname, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO);
    assert(r == 0);

    key = make_blocknum(1); value = (void*)1;
    u_int32_t hkey = toku_cachetable_hash(f, key);
    r = toku_cachetable_put(f, key, hkey, value, test_object_size, test_dirty_flush, 0, 0);
    assert(r == 0);

    // cachetable_print_state(t);
    r = toku_cachetable_get_key_state(t, key, f, &value, &dirty, &pinned, &entry_size);
    assert(r == 0);
    assert(dirty == 1);
    assert(pinned == 1);

    r = toku_cachetable_unpin(f, key, hkey, CACHETABLE_CLEAN, 0);
    assert(r == 0);
    r = toku_cachetable_get_key_state(t, key, f, &value, &dirty, &pinned, &entry_size);
    assert(r == 0);
    assert(dirty == 1);
    assert(pinned == 0);

    r = toku_cachetable_get_and_pin(f, key, hkey, &value, NULL, test_dirty_flush,
				    test_dirty_fetch, 0);
    assert(r == 0);

    // cachetable_print_state(t);
    r = toku_cachetable_get_key_state(t, key, f, &value, &dirty, &pinned, &entry_size);
    assert(r == 0);
    assert(dirty == 1);
    assert(pinned == 1);

    r = toku_cachetable_unpin(f, key, hkey, CACHETABLE_CLEAN, test_object_size);
    assert(r == 0);

    // cachetable_print_state(t);
    r = toku_cachetable_get_key_state(t, key, f, &value, &dirty, &pinned, &entry_size);
    assert(r == 0);
    assert(dirty == 1);
    assert(pinned == 0);

    key = make_blocknum(2);
    hkey = toku_cachetable_hash(f, key);
    r = toku_cachetable_get_and_pin(f, key, hkey,
				    &value, NULL, test_dirty_flush,
				    test_dirty_fetch, 0);
    assert(r == 0);

    // cachetable_print_state(t);
    r = toku_cachetable_get_key_state(t, key, f, &value, &dirty, &pinned, &entry_size);
    assert(r == 0);
    assert(dirty == 0);
    assert(pinned == 1);

    r = toku_cachetable_unpin(f, key, hkey, CACHETABLE_CLEAN, test_object_size);
    assert(r == 0);

    // cachetable_print_state(t);
    r = toku_cachetable_get_key_state(t, key, f, &value, &dirty, &pinned, &entry_size);
    assert(r == 0);
    assert(dirty == 0);
    assert(pinned == 0);

    r = toku_cachetable_get_and_pin(f, key, hkey,
				    &value, NULL, test_dirty_flush,
				    test_dirty_fetch, 0);
    assert(r == 0);

    // cachetable_print_state(t);
    r = toku_cachetable_get_key_state(t, key, f, &value, &dirty, &pinned, &entry_size);
    assert(r == 0);
    assert(dirty == 0);
    assert(pinned == 1);

    r = toku_cachetable_unpin(f, key, hkey, CACHETABLE_DIRTY, test_object_size);
    assert(r == 0);

    // cachetable_print_state(t);
    r = toku_cachetable_get_key_state(t, key, f, &value, &dirty, &pinned, &entry_size);
    assert(r == 0);
    assert(dirty == 1);
    assert(pinned == 0);
     
    r = toku_cachefile_close(&f, 0, FALSE, ZERO_LSN);
    assert(r == 0);
    r = toku_cachetable_close(&t);
    assert(r == 0);
}

static int test_size_debug;
static CACHEKEY test_size_flush_key;

static void test_size_flush_callback(CACHEFILE f,
                                     int UU(fd),
				     CACHEKEY key,
				     void *value,
				     void *extra __attribute__((__unused__)),
				     long size,
				     BOOL do_write,
				     BOOL keep,
				     BOOL for_checkpoint __attribute__((__unused__))) {
    if (test_size_debug && verbose) printf("test_size_flush %p %" PRId64 " %p %ld %u %u\n", f, key.b, value, size, (unsigned)do_write, (unsigned)keep);
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

    r = toku_create_cachetable(&t, n*size, ZERO_LSN, NULL_LOGGER);
    assert(r == 0);

    char *fname = __FILE__ "test.dat";
    unlink(fname);
    r = toku_cachetable_openf(&f, t, fname, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO);
    assert(r == 0);

    CACHEKEY key = make_blocknum(42);
    void *value = (void *) -42;

    u_int32_t hkey = toku_cachetable_hash(f, key);

    r = toku_cachetable_put(f, key, hkey, value, size, test_size_flush_callback, 0, 0);
    assert(r == 0);

    void *entry_value; int dirty; long long pinned; long entry_size;
    r = toku_cachetable_get_key_state(t, key, f, &entry_value, &dirty, &pinned, &entry_size);
    assert(r == 0);
    assert(dirty == 1);
    assert(pinned == 1);
    assert(entry_value == value);
    assert(entry_size == size);

    long long new_size = 2*size;
    r = toku_cachetable_unpin(f, key, hkey, CACHETABLE_CLEAN, new_size);
    assert(r == 0);

    void *current_value;
    long current_size;
    r = toku_cachetable_get_and_pin(f, key, hkey, &current_value, &current_size, test_size_flush_callback, 0, 0);
    assert(r == 0);
    assert(current_value == value);
    assert(current_size == new_size);

    r = toku_cachetable_unpin(f, key, hkey, CACHETABLE_CLEAN, new_size);
    assert(r == 0);

    r = toku_cachefile_close(&f, 0, FALSE, ZERO_LSN);
    assert(r == 0);
    r = toku_cachetable_close(&t);
    assert(r == 0);
}

static int min2(int a, int b) { return a < b ? a : b; }

static void test_size_flush(void) {
    if (verbose) printf("test_size_flush\n");

    CACHETABLE t;
    CACHEFILE f;
    int r;

    const int n = 8;
    long long size = 1*1024*1024;
    r = toku_create_cachetable(&t, n*size, ZERO_LSN, NULL_LOGGER);
    assert(r == 0);

    char *fname = __FILE__ "test.dat";
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
	u_int32_t hkey = toku_cachetable_hash(f, key);
        r = toku_cachetable_put(f, key, hkey, value, size, test_size_flush_callback, 0, 0);
        assert(r == 0);

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

        r = toku_cachetable_unpin(f, key, hkey, CACHETABLE_CLEAN, size);
        assert(r == 0);
    }
    
    r = toku_cachefile_close(&f, 0, FALSE, ZERO_LSN);
    assert(r == 0);
    r = toku_cachetable_close(&t);
    assert(r == 0);
}

int
test_main (int argc, const char *argv[]) {
    // defaults
#if defined(__linux__)
    int do_malloc_fail = 0;
#endif

    // parse args
    default_parse_args(argc, argv);
    int i;
    for (i=1; i<argc; i++) {
        const char *arg = argv[i];
        if (strcmp(arg, "-v") == 0) {
            verbose++;
            continue;
        }
#if defined(__linux__)
        if (strcmp(arg, "-malloc-fail") == 0) {
            do_malloc_fail = 1;
            continue;
        }
#endif
    }

    test_mutex_init();

    // run tests
#if !TOKU_WINDOWS
    test_multi_filehandles();
#endif
    test_cachetable_create();
#if DO_MALLOC_HOOK
    if (do_malloc_fail)
        test_cachetable_create_no_memory();    // fails with valgrind
#endif
    for (i=0; i<1; i++) {
        test0();
        test_nested_pin();
#if !TOKU_WINDOWS
        test_multi_filehandles ();
#endif
        test_dirty();
        test_size_resize();
        test_size_flush();
    }

    test_mutex_destroy();
    toku_malloc_cleanup();
    if (verbose) printf("ok\n");
    return 0;
}
