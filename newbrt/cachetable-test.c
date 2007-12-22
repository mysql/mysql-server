/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "memory.h"
#include "cachetable.h"
#include "test.h"

static const int test_object_size = 1;

struct item {
    CACHEKEY key;
    char *something;
};

static int expect_n_flushes=0;
static CACHEKEY flushes[100];

static void expect1(CACHEKEY key) {
    expect_n_flushes=1;
    flushes[0]=key;
}
static void expectN(CACHEKEY key) {
    flushes[expect_n_flushes++]=key;
}

static CACHEFILE expect_f;

static void flush (CACHEFILE f, CACHEKEY key, void*value, long size __attribute__((__unused__)), BOOL write_me __attribute__((__unused__)), BOOL keep_me __attribute__((__unused__)), LSN modified_lsn __attribute__((__unused__)), BOOL rename_p __attribute__((__unused__))) {
    struct item *it = value;
    int i;

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
static int fetch (CACHEFILE f, CACHEKEY key, void**value, long *sizep __attribute__((__unused__)), void*extraargs, LSN *written_lsn) {
    if (verbose) printf("Fetch %lld\n", key);
    assert (expect_f==f);
    assert((long)extraargs==23);
    *value = make_item(key);
    did_fetch=key;
    written_lsn->lsn = 0;
    return 0;
}


static void test0 (void) {
    void* t3=(void*)23;
    CACHETABLE t;
    CACHEFILE f;
    int r;
    char fname[] = "test.dat";
    r=toku_create_cachetable(&t, 5, ZERO_LSN, NULL_LOGGER);
    assert(r==0);
    unlink(fname);
    r = toku_cachetable_openf(&f, t, fname, O_RDWR|O_CREAT, 0777);
    assert(r==0);
    expect_f = f;

    expect_n_flushes=0;
    r=toku_cachetable_put(f, 1, make_item(1), test_object_size, flush, fetch, t3);   /* 1P */        /* this is the lru list.  1 is pinned. */
    assert(r==0);
    assert(expect_n_flushes==0);

    expect_n_flushes=0;
    r=toku_cachetable_put(f, 2, make_item(2), test_object_size, flush, fetch, t3);
    assert(r==0);
    r=toku_cachetable_unpin(f, 2, CACHETABLE_DIRTY, 1);           /* 2U 1P */
    assert(expect_n_flushes==0);

    expect_n_flushes=0;
    r=toku_cachetable_put(f, 3, make_item(3), test_object_size, flush, fetch, t3);
    assert(r==0);
    assert(expect_n_flushes==0);            /* 3P 2U 1P */   /* 3 is most recently used (pinned), 2 is next (unpinned), 1 is least recent (pinned) */

    expect_n_flushes=0;
    r=toku_cachetable_put(f, 4, make_item(4), test_object_size, flush, fetch, t3);
    assert(r==0);
    assert(expect_n_flushes==0);            /* 4P 3P 2U 1P */

    expect_n_flushes=0;
    r=toku_cachetable_put(f, 5, make_item(5), test_object_size, flush, fetch, t3);
    assert(r==0);
    r=toku_cachetable_unpin(f, 5, CACHETABLE_DIRTY, test_object_size);
    assert(r==0);
    r=toku_cachetable_unpin(f, 3, CACHETABLE_DIRTY, test_object_size);
    assert(r==0);
    assert(expect_n_flushes==0);            /* 5U 4P 3U 2U 1P */

    expect1(2); /* 2 is the oldest unpinned item. */
    r=toku_cachetable_put(f, 6, make_item(6), test_object_size, flush, fetch, t3);   /* 6P 5U 4P 3U 1P */
    assert(r==0);
    assert(expect_n_flushes==0);


    expect1(3);
    r=toku_cachetable_put(f, 7, make_item(7), test_object_size, flush, fetch, t3);
    assert(r==0);
    assert(expect_n_flushes==0);
    r=toku_cachetable_unpin(f, 7, CACHETABLE_DIRTY, test_object_size);           /* 7U 6P 5U 4P 1P */
    assert(r==0);

    {
	void *item_v=0;
	expect_n_flushes=0;
	r=toku_cachetable_get_and_pin(f, 5, &item_v, NULL, flush, fetch, t3);  /* 5P 7U 6P 4P 1P */
	assert(r==0);
	assert(((struct item *)item_v)->key==5);
	assert(strcmp(((struct item *)item_v)->something,"something")==0);
	assert(expect_n_flushes==0);
    }

    {
	void *item_v=0;
	r=toku_cachetable_unpin(f, 4, CACHETABLE_DIRTY, test_object_size);
	assert(r==0);
	expect1(4);
	did_fetch=-1;
	r=toku_cachetable_get_and_pin(f, 2, &item_v, NULL, flush, fetch, t3);  /* 2p 5P 7U 6P 1P */
	assert(r==0);
	assert(did_fetch==2); /* Expect that 2 is fetched in. */
	assert(((struct item *)item_v)->key==2);
	assert(strcmp(((struct item *)item_v)->something,"something")==0);
        assert(expect_n_flushes==0);
    }
	
    r=toku_cachetable_unpin(f, 2, CACHETABLE_DIRTY, test_object_size);
    assert(r==0);
    r=toku_cachetable_unpin(f ,5, CACHETABLE_DIRTY, test_object_size);
    assert(r==0);
    r=toku_cachetable_unpin(f, 6, CACHETABLE_DIRTY, test_object_size);
    assert(r==0);
    r=toku_cachetable_unpin(f, 1, CACHETABLE_DIRTY, test_object_size);
    assert(r==0);
    r=toku_cachetable_assert_all_unpinned(t);
    assert(r==0);

    if (verbose) printf("Closing\n");
    expect1(2);
    expectN(5);
    expectN(7);
    expectN(6);
    expectN(1);
    r=toku_cachefile_close(&f);
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
    void *vv;
    char fname[] = "test_ct.dat";
    r = toku_create_cachetable(&t, 1, ZERO_LSN, NULL_LOGGER);
    assert(r==0);
    unlink(fname);
    r = toku_cachetable_openf(&f, t, fname, O_RDWR|O_CREAT, 0777);
    assert(r==0);
    expect_f = f;

    i0=0; i1=0;
    r = toku_cachetable_put(f, 1, &i0, 1, flush_n, fetch_n, f2);
    assert(r==0);
    r = toku_cachetable_get_and_pin(f, 1, &vv, NULL, flush_n, fetch_n, f2);
    assert(r==0);
    assert(vv==&i0);
    assert(i0==0);
    r = toku_cachetable_unpin(f, 1, 0, test_object_size);
    assert(r==0);
    r = toku_cachetable_put(f, 2, &i1, test_object_size, flush_n, fetch_n, f2);
    assert(r!=0); // previously pinned, we shouldn't be able to put.
    r = toku_cachetable_unpin(f, 1, 0, test_object_size);
    assert(r==0);
    r = toku_cachetable_put(f, 2, &i1, test_object_size, flush_n, fetch_n, f2);
    assert(r==0); // now it is unpinned, we can put it.

    r = toku_cachefile_close(&f); assert(r==0);
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
static int add123_fetch (CACHEFILE cf __attribute__((__unused__)), CACHEKEY key, void **value, long *sizep __attribute__((__unused__)), void*extraargs, LSN *written_lsn) {
    assert((long)extraargs==123);
    *value = (void*)((unsigned long)key+123L);
    written_lsn->lsn = 0;
    return 0;
}
static int add222_fetch (CACHEFILE cf __attribute__((__unused__)), CACHEKEY key, void **value, long *sizep __attribute__((__unused__)), void*extraargs, LSN *written_lsn) {
    assert((long)extraargs==222);
    *value = (void*)((unsigned long)key+222L);
    written_lsn->lsn = 0;
    return 0;
}


static void test_multi_filehandles (void) {
    CACHETABLE t;
    CACHEFILE f1,f2,f3;
    char fname1[]="test_ct.dat";
    char fname2[]="test2_ct.dat";
    char fname3[]="test3_ct.dat";
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
    
    r = toku_cachetable_put(f1, 1, (void*)124, test_object_size, null_flush, add123_fetch, (void*)123); assert(r==0);
    r = toku_cachetable_get_and_pin(f2, 1, &v, NULL, null_flush, add123_fetch, (void*)123); assert(r==0);
    assert((unsigned long)v==124);
    r = toku_cachetable_get_and_pin(f2, 2, &v, NULL, null_flush, add123_fetch, (void*)123); assert(r==0);
    assert((unsigned long)v==125);
    r = toku_cachetable_get_and_pin(f3, 2, &v, NULL, null_flush, add222_fetch, (void*)222); assert(r==0);
    assert((unsigned long)v==224);
    r = toku_cachetable_maybe_get_and_pin(f1, 2, &v); assert(r==0);
    assert((unsigned long)v==125);
    
    r = toku_cachefile_close(&f1); assert(r==0);
    r = toku_cachefile_close(&f2); assert(r==0);
    r = toku_cachefile_close(&f3); assert(r==0);
    r = toku_cachetable_close(&t); assert(r==0);
}

static void test_dirty_flush(CACHEFILE f, CACHEKEY key, void *value, long size, BOOL do_write, BOOL keep, LSN modified_lsn __attribute__((__unused__)), BOOL rename_p __attribute__((__unused__))) {
    if (verbose) printf("test_dirty_flush %p %lld %p %ld %d %d\n", f, key, value, size, do_write, keep);
}

static int test_dirty_fetch(CACHEFILE f, CACHEKEY key, void **value_ptr, long *size_ptr, void *arg, LSN *written_lsn) {
    *value_ptr = arg;
    written_lsn->lsn = 0;
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

    char *fname = "test.dat";
    unlink(fname);
    r = toku_cachetable_openf(&f, t, fname, O_RDWR|O_CREAT, 0777);   
    assert(r == 0);

    key = 1; value = (void*)1;
    r = toku_cachetable_put(f, key, value, test_object_size, test_dirty_flush, 0, 0);
    assert(r == 0);

    // cachetable_print_state(t);
    r = toku_cachetable_get_key_state(t, key, &value, &dirty, &pinned, &entry_size);
    assert(r == 0);
    assert(dirty == 1);
    assert(pinned == 1);

    r = toku_cachetable_unpin(f, key, CACHETABLE_CLEAN, 0);
    assert(r == 0);
    r = toku_cachetable_get_key_state(t, key, &value, &dirty, &pinned, &entry_size);
    assert(r == 0);
    assert(dirty == 1);
    assert(pinned == 0);

    r = toku_cachetable_get_and_pin(f, key, &value, NULL, test_dirty_flush,
                               test_dirty_fetch, 0);
    assert(r == 0);

    // cachetable_print_state(t);
    r = toku_cachetable_get_key_state(t, key, &value, &dirty, &pinned, &entry_size);
    assert(r == 0);
    assert(dirty == 1);
    assert(pinned == 1);

    r = toku_cachetable_unpin(f, key, CACHETABLE_CLEAN, test_object_size);
    assert(r == 0);

    // cachetable_print_state(t);
    r = toku_cachetable_get_key_state(t, key, &value, &dirty, &pinned, &entry_size);
    assert(r == 0);
    assert(dirty == 1);
    assert(pinned == 0);

    key = 2;
    r = toku_cachetable_get_and_pin(f, key, &value, NULL, test_dirty_flush,
                               test_dirty_fetch, 0);
    assert(r == 0);

    // cachetable_print_state(t);
    r = toku_cachetable_get_key_state(t, key, &value, &dirty, &pinned, &entry_size);
    assert(r == 0);
    assert(dirty == 0);
    assert(pinned == 1);

    r = toku_cachetable_unpin(f, key, CACHETABLE_CLEAN, test_object_size);
    assert(r == 0);

    // cachetable_print_state(t);
    r = toku_cachetable_get_key_state(t, key, &value, &dirty, &pinned, &entry_size);
    assert(r == 0);
    assert(dirty == 0);
    assert(pinned == 0);

    r = toku_cachetable_get_and_pin(f, key, &value, NULL, test_dirty_flush,
				    test_dirty_fetch, 0);
    assert(r == 0);

    // cachetable_print_state(t);
    r = toku_cachetable_get_key_state(t, key, &value, &dirty, &pinned, &entry_size);
    assert(r == 0);
    assert(dirty == 0);
    assert(pinned == 1);

    r = toku_cachetable_unpin(f, key, CACHETABLE_DIRTY, test_object_size);
    assert(r == 0);

    // cachetable_print_state(t);
    r = toku_cachetable_get_key_state(t, key, &value, &dirty, &pinned, &entry_size);
    assert(r == 0);
    assert(dirty == 1);
    assert(pinned == 0);
     
    r = toku_cachefile_close(&f);
    assert(r == 0);
    r = toku_cachetable_close(&t);
    assert(r == 0);
}

static int test_size_debug;
static CACHEKEY test_size_flush_key;

static void test_size_flush_callback(CACHEFILE f, CACHEKEY key, void *value, long size, BOOL do_write, BOOL keep, LSN modified_lsn __attribute__((__unused__)), BOOL rename_p __attribute__((__unused__))) {
    if (test_size_debug && verbose) printf("test_size_flush %p %lld %p %ld %d %d\n", f, key, value, size, do_write, keep);
    assert(do_write != 0);
    test_size_flush_key = key;
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

    char *fname = "test.dat";
    unlink(fname);
    r = toku_cachetable_openf(&f, t, fname, O_RDWR|O_CREAT, 0777);   
    assert(r == 0);

    CACHEKEY key = 42;
    void *value = (void *) -42;

    r = toku_cachetable_put(f, key, value, size, test_size_flush_callback, 0, 0);
    assert(r == 0);

    void *entry_value; int dirty; long long pinned; long entry_size;
    r = toku_cachetable_get_key_state(t, key, &entry_value, &dirty, &pinned, &entry_size);
    assert(r == 0);
    assert(dirty == 1);
    assert(pinned == 1);
    assert(entry_value == value);
    assert(entry_size == size);

    long long new_size = 2*size;
    r = toku_cachetable_unpin(f, key, CACHETABLE_CLEAN, new_size);
    assert(r == 0);

    void *current_value;
    long current_size;
    r = toku_cachetable_get_and_pin(f, key, &current_value, &current_size, test_size_flush_callback, 0, 0);
    assert(r == 0);
    assert(current_value == value);
    assert(current_size == new_size);

    r = toku_cachetable_unpin(f, key, CACHETABLE_CLEAN, new_size);
    assert(r == 0);

    r = toku_cachefile_close(&f);
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

    char *fname = "test.dat";
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
        r = toku_cachetable_put(f, key, value, size, test_size_flush_callback, 0, 0);
        assert(r == 0);

        int n_entries;
        toku_cachetable_get_state(t, &n_entries, 0, 0, 0);
        int min2(int a, int b) { return a < b ? a : b; }
        assert(n_entries == min2(i+1, n));

        void *entry_value; int dirty; long long pinned; long entry_size;
        r = toku_cachetable_get_key_state(t, key, &entry_value, &dirty, &pinned, &entry_size);
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

        r = toku_cachetable_unpin(f, key, CACHETABLE_CLEAN, size);
        assert(r == 0);
    }
    
    r = toku_cachefile_close(&f);
    assert(r == 0);
    r = toku_cachetable_close(&t);
    assert(r == 0);
}

enum { KEYLIMIT = 4, TRIALLIMIT=64 };
static CACHEKEY  keys[KEYLIMIT];
static void*     vals[KEYLIMIT];
static int       n_keys=0;

static void r_flush (CACHEFILE f __attribute__((__unused__)),
		     CACHEKEY k, void *value,
		     long size __attribute__((__unused__)),
		     BOOL write_me  __attribute__((__unused__)),
		     BOOL keep_me,
		     LSN modified_lsn __attribute__((__unused__)),
		     BOOL rename_p    __attribute__((__unused__))) {
    int i;
    //printf("Flush\n");
    for (i=0; i<n_keys; i++) {
	if (keys[i]==k) {
	    assert(vals[i]==value);
	    if (!keep_me) {
		keys[i]=keys[n_keys-1];
		vals[i]=vals[n_keys-1];
		n_keys--;
		return;
	    }
	}
    }
    fprintf(stderr, "Whoops\n");
    abort();
}

static int r_fetch (CACHEFILE f      __attribute__((__unused__)),
	     CACHEKEY key     __attribute__((__unused__)),
	     void**value      __attribute__((__unused__)),
	     long *sizep      __attribute__((__unused__)),
	     void*extraargs   __attribute__((__unused__)),
	     LSN *modified_lsn __attribute__((__unused__))) {
    fprintf(stderr, "Whoops, this should never be called");
    return 0;
}

static void test_rename (void) {
    CACHETABLE t;
    CACHEFILE f;
    int i;
    int r;
    const char fname[] = "ct-test-rename.dat";
    r=toku_create_cachetable(&t, KEYLIMIT, ZERO_LSN, NULL_LOGGER); assert(r==0);
    unlink(fname);
    r = toku_cachetable_openf(&f, t, fname, O_RDWR|O_CREAT, 0777);
    assert(r==0);
  
    for (i=0; i<TRIALLIMIT; i++) {
	int ra = random()%3;
	if (ra<=1) {
	    // Insert something
	    CACHEKEY nkey = random();
	    long     nval = random();
	    //printf("n_keys=%d Insert %08llx\n", n_keys, nkey);
	    r = toku_cachetable_put(f, nkey, (void*)nval, 1,
				    r_flush, r_fetch, 0);
	    assert(r==0);
	    assert(n_keys<KEYLIMIT);
	    keys[n_keys] = nkey;
	    vals[n_keys] = (void*)nval;
	    n_keys++;
	    r = toku_cachetable_unpin(f, nkey, CACHETABLE_DIRTY, 1);
	    assert(r==0);
	} else if (ra==2 && n_keys>0) {
	    // Rename something
	    int objnum = random()%n_keys;
	    CACHEKEY okey = keys[objnum];
	    CACHEKEY nkey = random();
	    void *current_value;
	    long current_size;
	    keys[objnum]=nkey;
	    //printf("Rename %llx to %llx\n", okey, nkey);
	    r = toku_cachetable_get_and_pin(f, okey, &current_value, &current_size, r_flush, r_fetch, 0);
	    assert(r==0);
	    r = toku_cachetable_rename(f, okey, nkey);
	    assert(r==0);
	    r = toku_cachetable_unpin(f, nkey, CACHETABLE_DIRTY, 1);
	}
    }

    r = toku_cachefile_close(&f);
    assert(r == 0);
    r = toku_cachetable_close(&t);
    assert(r == 0);

    assert(n_keys == 0);
}

int main (int argc, const char *argv[]) {
    default_parse_args(argc, argv);
    test_rename();
    test0();
    test_nested_pin();
    test_multi_filehandles ();
    test_dirty();
    test_size_resize();
    test_size_flush();
    toku_malloc_cleanup();
    if (verbose) printf("ok\n");
    return 0;
}
