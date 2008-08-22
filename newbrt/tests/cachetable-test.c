/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <errno.h>
#include <pthread.h>

#include "toku_assert.h"
#include "memory.h"
#include "cachetable.h"
#include "test.h"

// this mutex is used by some of the tests to serialize access to some
// global data, especially between the test thread and the cachetable
// writeback threads

pthread_mutex_t  test_mutex;

static inline void test_mutex_init() {
    int r = pthread_mutex_init(&test_mutex, 0); assert(r == 0);
}

static inline void test_mutex_destroy() {
    int r = pthread_mutex_destroy(&test_mutex); assert(r == 0);
}

static inline void test_mutex_lock() {
    int r = pthread_mutex_lock(&test_mutex); assert(r == 0);
}

static inline void test_mutex_unlock() {
    int r = pthread_mutex_unlock(&test_mutex); assert(r == 0);
}

// hook my_malloc_always_fails into malloc to control malloc and verify
// the correct recovery from malloc failures

#define DO_MALLOC_HOOK 1
#if DO_MALLOC_HOOK
static void *my_malloc_always_fails(size_t n, const __malloc_ptr_t p) {
    n = n; p = p;
    return 0;
}
#endif

// verify that cachetable creation and close works

void test_cachetable_create() {
    CACHETABLE ct = 0;
    int r;
    r = toku_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER);
    assert(r == 0);
    r = toku_cachetable_close(&ct);
    assert(r == 0 && ct == 0);
}

// verify that cachetable create with no memory returns ENOMEM

void test_cachetable_create_no_memory() {
    void *(*orig_malloc_hook)(size_t, const __malloc_ptr_t) = __malloc_hook;
    __malloc_hook = my_malloc_always_fails;
    CACHETABLE ct = 0;
    int r;
    r = toku_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER);
    assert(r == ENOMEM);
    __malloc_hook = orig_malloc_hook;
}

static const int test_object_size = 1;

struct item {
    CACHEKEY key;
    char *something;
};

static volatile int expect_n_flushes=0;
static volatile CACHEKEY flushes[100];

static void expect1(CACHEKEY key) {
    expect_n_flushes=1;
    flushes[0]=key;
    if (verbose) printf("%s:%d %lld\n", __FUNCTION__, 0, key);
}
static void expectN(CACHEKEY key) {
    if (verbose) printf("%s:%d %lld\n", __FUNCTION__, expect_n_flushes, key);
    flushes[expect_n_flushes++]=key;
}

static CACHEFILE expect_f;

static void flush (CACHEFILE f, CACHEKEY key, void*value, long size __attribute__((__unused__)), BOOL write_me __attribute__((__unused__)), BOOL keep_me __attribute__((__unused__)), LSN modified_lsn __attribute__((__unused__)), BOOL rename_p __attribute__((__unused__))) {
    struct item *it = value;
    int i;

    if (keep_me) return;

    if (verbose) printf("Flushing %lld (it=>key=%lld)\n", key, it->key);

    assert(expect_f==f);
    assert(strcmp(it->something,"something")==0);
    assert(it->key==key);

    /* Verify that we expected the flush. */
    for (i=0; i<expect_n_flushes; i++) {
	if (key==flushes[i]) {
	    flushes[i] = flushes[expect_n_flushes-1];
	    expect_n_flushes--;
	    goto found_flush;
	}
    }
    fprintf(stderr, "%lld was flushed, but I didn't expect it\n", key);
    abort();
 found_flush:
    toku_free(value);
}

static struct item *make_item (CACHEKEY key) {
    struct item *MALLOC(it);
    it->key=key;
    it->something="something";
    return it;
}

static CACHEKEY did_fetch=-1;
static int fetch (CACHEFILE f, CACHEKEY key, u_int32_t fullhash __attribute__((__unused__)), void**value, long *sizep __attribute__((__unused__)), void*extraargs, LSN *written_lsn) {
    if (verbose) printf("Fetch %lld\n", key);
    assert (expect_f==f);
    assert((long)extraargs==23);
    *value = make_item(key);
    did_fetch=key;
    written_lsn->lsn = 0;
    return 0;
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
    r = toku_cachetable_openf(&f, t, fname, O_RDWR|O_CREAT, 0777);
    assert(r==0);
    expect_f = f;

    expect_n_flushes=0;
    u_int32_t h1 = toku_cachetable_hash(f, 1);
    u_int32_t h2 = toku_cachetable_hash(f, 2);
    u_int32_t h3 = toku_cachetable_hash(f, 3);
    u_int32_t h4 = toku_cachetable_hash(f, 4);
    u_int32_t h5 = toku_cachetable_hash(f, 5);
    u_int32_t h6 = toku_cachetable_hash(f, 6);
    u_int32_t h7 = toku_cachetable_hash(f, 7);
    r=toku_cachetable_put(f, 1, h1, make_item(1), test_object_size, flush, fetch, t3);   /* 1P */        /* this is the lru list.  1 is pinned. */
    assert(r==0);
    assert(expect_n_flushes==0);

    expect_n_flushes=0;
    r=toku_cachetable_put(f, 2, h2, make_item(2), test_object_size, flush, fetch, t3);
    assert(r==0);
    r=toku_cachetable_unpin(f, 2, h2, CACHETABLE_DIRTY, 1);           /* 2U 1P */
    assert(expect_n_flushes==0);

    expect_n_flushes=0;
    r=toku_cachetable_put(f, 3, h3, make_item(3), test_object_size, flush, fetch, t3);
    assert(r==0);
    assert(expect_n_flushes==0);            /* 3P 2U 1P */   /* 3 is most recently used (pinned), 2 is next (unpinned), 1 is least recent (pinned) */

    expect_n_flushes=0;
    r=toku_cachetable_put(f, 4, h4, make_item(4), test_object_size, flush, fetch, t3);
    assert(r==0);
    assert(expect_n_flushes==0);            /* 4P 3P 2U 1P */

    expect_n_flushes=0;
    r=toku_cachetable_put(f, 5, h5, make_item(5), test_object_size, flush, fetch, t3);
    assert(r==0);
    r=toku_cachetable_unpin(f, 5, h5, CACHETABLE_DIRTY, test_object_size);
    assert(r==0);
    r=toku_cachetable_unpin(f, 3, h3, CACHETABLE_DIRTY, test_object_size);
    assert(r==0);
    assert(expect_n_flushes==0);            /* 5U 4P 3U 2U 1P */

    expect1(2); /* 2 is the oldest unpinned item. */
    r=toku_cachetable_put(f, 6, h6, make_item(6), test_object_size, flush, fetch, t3);   /* 6P 5U 4P 3U 1P */
    assert(r==0);
    while (expect_n_flushes != 0) pthread_yield();
    assert(expect_n_flushes==0);


    expect1(3);
    r=toku_cachetable_put(f, 7, h7, make_item(7), test_object_size, flush, fetch, t3);
    assert(r==0);
    while (expect_n_flushes != 0) pthread_yield();
    assert(expect_n_flushes==0);
    r=toku_cachetable_unpin(f, 7, h7, CACHETABLE_DIRTY, test_object_size);           /* 7U 6P 5U 4P 1P */
    assert(r==0);

    {
	void *item_v=0;
	expect_n_flushes=0;
	r=toku_cachetable_get_and_pin(f, 5, toku_cachetable_hash(f, 5), &item_v, NULL, flush, fetch, t3);  /* 5P 7U 6P 4P 1P */
	assert(r==0);
	assert(((struct item *)item_v)->key==5);
	assert(strcmp(((struct item *)item_v)->something,"something")==0);
	assert(expect_n_flushes==0);
    }

    {
	void *item_v=0;
	r=toku_cachetable_unpin(f, 4, h4, CACHETABLE_DIRTY, test_object_size);
	assert(r==0);
	expect1(4);
	did_fetch=-1;
	r=toku_cachetable_get_and_pin(f, 2, toku_cachetable_hash(f, 2), &item_v, NULL, flush, fetch, t3);  /* 2p 5P 7U 6P 1P */
	assert(r==0);
	assert(did_fetch==2); /* Expect that 2 is fetched in. */
	assert(((struct item *)item_v)->key==2);
	assert(strcmp(((struct item *)item_v)->something,"something")==0);
        while (expect_n_flushes != 0) pthread_yield();
        assert(expect_n_flushes==0);
    }
	
    r=toku_cachetable_unpin(f, 2, h2, CACHETABLE_DIRTY, test_object_size);
    assert(r==0);
    r=toku_cachetable_unpin(f, 5, h5, CACHETABLE_DIRTY, test_object_size);
    assert(r==0);
    r=toku_cachetable_unpin(f, 6, h6, CACHETABLE_DIRTY, test_object_size);
    assert(r==0);
    r=toku_cachetable_unpin(f, 1, h1, CACHETABLE_DIRTY, test_object_size);
    assert(r==0);
    r=toku_cachetable_assert_all_unpinned(t);
    assert(r==0);

    if (verbose) printf("Closing\n");
    expect1(2);
    expectN(5);
    expectN(7);
    expectN(6);
    expectN(1);
    r=toku_cachefile_close(&f, 0);
    assert(r==0);
    r=toku_cachetable_close(&t);
    assert(r==0);
    assert(expect_n_flushes==0);
    expect_f = 0;
    toku_memory_check_all_free();
}

static void flush_n (CACHEFILE f __attribute__((__unused__)), CACHEKEY key __attribute__((__unused__)), void *value,
                     long size __attribute__((__unused__)),
		     BOOL write_me __attribute__((__unused__)),    BOOL keep_me __attribute__((__unused__)),
		     LSN modified_lsn __attribute__((__unused__)), BOOL rename_p __attribute ((__unused__))) {
    int *v = value;
    assert(*v==0);
}
static int fetch_n (CACHEFILE f __attribute__((__unused__)), CACHEKEY key __attribute__((__unused__)),
		    u_int32_t fullhash  __attribute__((__unused__)),
                    void**value, long *sizep __attribute__((__unused__)), void*extraargs, LSN *written_lsn) {
    assert((long)extraargs==42);
    *value=0;
    written_lsn->lsn = 0;
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
    r = toku_cachetable_openf(&f, t, fname, O_RDWR|O_CREAT, 0777);
    assert(r==0);
    expect_f = f;

    i0=0; i1=0;
    u_int32_t f1hash = toku_cachetable_hash(f, 1);
    r = toku_cachetable_put(f, 1, f1hash, &i0, 1, flush_n, fetch_n, f2);
    assert(r==0);
    r = toku_cachetable_get_and_pin(f, 1, f1hash, &vv, NULL, flush_n, fetch_n, f2);
    assert(r==0);
    assert(vv==&i0);
    assert(i0==0);
    r = toku_cachetable_unpin(f, 1, f1hash, 0, test_object_size);
    assert(r==0);
    r = toku_cachetable_maybe_get_and_pin(f, 1, f1hash, &vv2);
    assert(r==0);
    assert(vv2==vv);
    r = toku_cachetable_unpin(f, 1, f1hash, 0, test_object_size);
    assert(r==0);
    u_int32_t f2hash = toku_cachetable_hash(f, 2);
    r = toku_cachetable_put(f, 2, f2hash, &i1, test_object_size, flush_n, fetch_n, f2);
    assert(r==0); // The other one is pinned, but now the cachetable fails gracefully:  It allows the pin to happen
    r = toku_cachetable_unpin(f, 1, f1hash, 0, test_object_size);
    assert(r==0);
    r = toku_cachetable_unpin(f, 2, f2hash, 0, test_object_size);
    assert(r==0);
    //    sleep(1);
    r = toku_cachefile_close(&f, 0); assert(r==0);
    r = toku_cachetable_close(&t); assert(r==0);
}


static void null_flush (CACHEFILE cf     __attribute__((__unused__)),
                        CACHEKEY k       __attribute__((__unused__)),
                        void *v          __attribute__((__unused__)),
                        long size        __attribute__((__unused__)),
                        BOOL write_me    __attribute__((__unused__)),
                        BOOL keep_me     __attribute__((__unused__)),
                        LSN modified_lsn __attribute__((__unused__)),
                        BOOL rename_p    __attribute__((__unused__))) {
}

static int add123_fetch (CACHEFILE cf, CACHEKEY key, u_int32_t fullhash, void **value, long *sizep __attribute__((__unused__)), void*extraargs, LSN *written_lsn) {
    assert(fullhash==toku_cachetable_hash(cf,key));
    assert((long)extraargs==123);
    *value = (void*)((unsigned long)key+123L);
    written_lsn->lsn = 0;
    return 0;
}

static int add222_fetch (CACHEFILE cf, CACHEKEY key, u_int32_t fullhash, void **value, long *sizep __attribute__((__unused__)), void*extraargs, LSN *written_lsn) {
    assert(fullhash==toku_cachetable_hash(cf,key));
    assert((long)extraargs==222);
    *value = (void*)((unsigned long)key+222L);
    written_lsn->lsn = 0;
    return 0;
}

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
    r = toku_cachetable_openf(&f1, t, fname1, O_RDWR|O_CREAT, 0777);   assert(r==0);
    r = link(fname1, fname2);                                     assert(r==0);
    r = toku_cachetable_openf(&f2, t, fname2, O_RDWR|O_CREAT, 0777);   assert(r==0);
    r = toku_cachetable_openf(&f3, t, fname3, O_RDWR|O_CREAT, 0777);   assert(r==0);

    assert(f1==f2);
    assert(f1!=f3);
    
    r = toku_cachetable_put(f1, 1, toku_cachetable_hash(f1, 1), (void*)124, test_object_size, null_flush, add123_fetch, (void*)123); assert(r==0);
    r = toku_cachetable_get_and_pin(f2, 1, toku_cachetable_hash(f2, 1), &v, NULL, null_flush, add123_fetch, (void*)123); assert(r==0);
    assert((unsigned long)v==124);
    r = toku_cachetable_get_and_pin(f2, 2, toku_cachetable_hash(f2, 2), &v, NULL, null_flush, add123_fetch, (void*)123); assert(r==0);
    assert((unsigned long)v==125);
    r = toku_cachetable_get_and_pin(f3, 2, toku_cachetable_hash(f3, 2), &v, NULL, null_flush, add222_fetch, (void*)222); assert(r==0);
    assert((unsigned long)v==224);
    r = toku_cachetable_maybe_get_and_pin(f1, 2, toku_cachetable_hash(f1, 2), &v); assert(r==0);
    assert((unsigned long)v==125);

    r = toku_cachetable_unpin(f1, 1, toku_cachetable_hash(f1, 1), CACHETABLE_CLEAN, 0); assert(r==0);
    r = toku_cachetable_unpin(f1, 2, toku_cachetable_hash(f1, 2), CACHETABLE_CLEAN, 0); assert(r==0);
    r = toku_cachefile_close(&f1, 0); assert(r==0);

    r = toku_cachetable_unpin(f2, 1, toku_cachetable_hash(f2, 1), CACHETABLE_CLEAN, 0); assert(r==0);
    r = toku_cachetable_unpin(f2, 2, toku_cachetable_hash(f2, 2), CACHETABLE_CLEAN, 0); assert(r==0);
    r = toku_cachefile_close(&f2, 0); assert(r==0);

    r = toku_cachetable_unpin(f3, 2, toku_cachetable_hash(f3, 2), CACHETABLE_CLEAN, 0); assert(r==0);
    r = toku_cachefile_close(&f3, 0); assert(r==0);

    r = toku_cachetable_close(&t); assert(r==0);
}

static void test_dirty_flush(CACHEFILE f, CACHEKEY key, void *value, long size, BOOL do_write, BOOL keep, LSN modified_lsn __attribute__((__unused__)), BOOL rename_p __attribute__((__unused__))) {
    if (verbose) printf("test_dirty_flush %p %lld %p %ld %d %d\n", f, key, value, size, do_write, keep);
}

static int test_dirty_fetch(CACHEFILE f, CACHEKEY key, u_int32_t fullhash, void **value_ptr, long *size_ptr, void *arg, LSN *written_lsn) {
    *value_ptr = arg;
    written_lsn->lsn = 0;
    assert(fullhash==toku_cachetable_hash(f,key));
    if (verbose) printf("test_dirty_fetch %p %lld %p %ld %p\n", f, key, *value_ptr, *size_ptr, arg);
    return 0;
}

static void test_dirty() {
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
    r = toku_cachetable_openf(&f, t, fname, O_RDWR|O_CREAT, 0777);
    assert(r == 0);

    key = 1; value = (void*)1;
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

    key = 2;
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
     
    r = toku_cachefile_close(&f, 0);
    assert(r == 0);
    r = toku_cachetable_close(&t);
    assert(r == 0);
}

static int test_size_debug;
static CACHEKEY test_size_flush_key;

static void test_size_flush_callback(CACHEFILE f, CACHEKEY key, void *value, long size, BOOL do_write, BOOL keep, LSN modified_lsn __attribute__((__unused__)), BOOL rename_p __attribute__((__unused__))) {
    if (test_size_debug && verbose) printf("test_size_flush %p %lld %p %ld %d %d\n", f, key, value, size, do_write, keep);
    if (keep) {
        assert(do_write != 0);
        test_size_flush_key = key;
    }
}

static void test_size_resize() {
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
    r = toku_cachetable_openf(&f, t, fname, O_RDWR|O_CREAT, 0777);
    assert(r == 0);

    CACHEKEY key = 42;
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

    r = toku_cachefile_close(&f, 0);
    assert(r == 0);
    r = toku_cachetable_close(&t);
    assert(r == 0);
}

static void test_size_flush() {
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
    r = toku_cachetable_openf(&f, t, fname, O_RDWR|O_CREAT, 0777);
    assert(r == 0);

    /* put 2*n keys into the table, ensure flushes occur in key order */
    test_size_flush_key = -1;
    
    int i;
    CACHEKEY expect_flush_key = 0;
    for (i=0; i<2*n; i++) {
        CACHEKEY key = i;
        void *value = (void *)(long)-i;
        //        printf("test_size put %lld %p %lld\n", key, value, size);
	u_int32_t hkey = toku_cachetable_hash(f, key);
        r = toku_cachetable_put(f, key, hkey, value, size, test_size_flush_callback, 0, 0);
        assert(r == 0);

        int n_entries, hash_size; long size_current, size_limit;
        toku_cachetable_get_state(t, &n_entries, &hash_size, &size_current, &size_limit);
        int min2(int a, int b) { return a < b ? a : b; }
        while (n_entries != min2(i+1, n)) {
            pthread_yield();
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

        if (test_size_flush_key != -1) {
            assert(test_size_flush_key == expect_flush_key);
            assert(expect_flush_key == i-n);
            expect_flush_key += 1;
        }

        r = toku_cachetable_unpin(f, key, hkey, CACHETABLE_CLEAN, size);
        assert(r == 0);
    }
    
    r = toku_cachefile_close(&f, 0);
    assert(r == 0);
    r = toku_cachetable_close(&t);
    assert(r == 0);
}

int main (int argc, const char *argv[]) {
    // defaults
    int do_malloc_fail = 0;

    // parse args
    int i;
    for (i=1; i<argc; i++) {
        const char *arg = argv[i];
        if (strcmp(arg, "-v") == 0) {
            verbose++;
            continue;
        }
        if (strcmp(arg, "-malloc-fail") == 0) {
            do_malloc_fail = 1;
            continue;
        }
    }

    // run tests
    test_multi_filehandles();
    test_cachetable_create();
    if (do_malloc_fail)
        test_cachetable_create_no_memory();    // fails with valgrind
    for (i=0; i<1; i++) {
        test0();
        test_nested_pin();
        test_multi_filehandles ();
        test_dirty();
        test_size_resize();
        test_size_flush();
    }
    toku_malloc_cleanup();
    if (verbose) printf("ok\n");
    return 0;
}
