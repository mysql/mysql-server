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

enum { KEYLIMIT = 4, TRIALLIMIT=256000 };
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
    if (keep_me) return;

    test_mutex_lock();
    for (i=0; i<n_keys; i++) {
	if (keys[i]==k) {
	    assert(vals[i]==value);
	    if (!keep_me) {
                if (verbose) printf("%s: %d/%d %llx\n", __FUNCTION__, i, n_keys, k);
		keys[i]=keys[n_keys-1];
		vals[i]=vals[n_keys-1];
		n_keys--;
                test_mutex_unlock();
		return;
	    }
	}
    }
    fprintf(stderr, "Whoops\n");
    abort();
    test_mutex_unlock();
}

static int r_fetch (CACHEFILE f        __attribute__((__unused__)),
		    CACHEKEY key       __attribute__((__unused__)),
		    u_int32_t fullhash __attribute__((__unused__)),
		    void**value        __attribute__((__unused__)),
		    long *sizep        __attribute__((__unused__)),
		    void*extraargs     __attribute__((__unused__)),
		    LSN *modified_lsn  __attribute__((__unused__))) {
    // fprintf(stderr, "Whoops, this should never be called");
    return -42;
}

static void test_rename (void) {
    CACHETABLE t;
    CACHEFILE f;
    int i;
    int r;
    test_mutex_init();
    const char fname[] = __FILE__ "rename.dat";
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
	    if (verbose) printf("n_keys=%d Insert %08llx\n", n_keys, nkey);
	    u_int32_t hnkey = toku_cachetable_hash(f, nkey);
	    r = toku_cachetable_put(f, nkey, hnkey,
				    (void*)nval, 1,
				    r_flush, r_fetch, 0);
	    assert(r==0);
            test_mutex_lock();
            while (n_keys >= KEYLIMIT) {
                test_mutex_unlock();
                pthread_yield();
                test_mutex_lock();
            }
	    assert(n_keys<KEYLIMIT);
	    keys[n_keys] = nkey;
	    vals[n_keys] = (void*)nval;
	    n_keys++;
            test_mutex_unlock();
	    r = toku_cachetable_unpin(f, nkey, hnkey, CACHETABLE_DIRTY, 1);
	    assert(r==0);
	} else if (ra==2 && n_keys>0) {
	    // Rename something
	    int objnum = random()%n_keys;
	    CACHEKEY nkey = random();
            test_mutex_lock();
	    CACHEKEY okey = keys[objnum];
            test_mutex_unlock();
	    void *current_value;
	    long current_size;
	    if (verbose) printf("Rename %llx to %llx\n", okey, nkey);
	    r = toku_cachetable_get_and_pin(f, okey, toku_cachetable_hash(f, okey), &current_value, &current_size, r_flush, r_fetch, 0);
	    if (r == -42) continue;
            assert(r==0);
	    r = toku_cachetable_rename(f, okey, nkey);
	    assert(r==0);
            test_mutex_lock();
            // assert(objnum < n_keys && keys[objnum] == okey);
            // get_and_pin may reorganize the keys[], so we need to find it again
            int j;
            for (j=0; j < n_keys; j++)
                if (keys[j] == okey)
                    break;
            assert(j < n_keys);
	    keys[j]=nkey;
            test_mutex_unlock();
	    r = toku_cachetable_unpin(f, nkey, toku_cachetable_hash(f, nkey), CACHETABLE_DIRTY, 1);
	}
    }

    // test rename fails if old key does not exist in the cachetable
    CACHEKEY okey, nkey;
    while (1) {
        okey = random();
        void *v;
        r = toku_cachetable_maybe_get_and_pin(f, okey, toku_cachetable_hash(f, okey), &v);
        if (r != 0)
            break;
        r = toku_cachetable_unpin(f, okey, toku_cachetable_hash(f, okey), CACHETABLE_CLEAN, 1);
        assert(r == 0);
    }
    nkey = random();
    r = toku_cachetable_rename(f, okey, nkey);
    assert(r != 0);

    r = toku_cachefile_close(&f, 0);
    assert(r == 0);
    r = toku_cachetable_close(&t);
    assert(r == 0);
    test_mutex_destroy();
    assert(n_keys == 0);
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
    for (i=0; i<1; i++) 
        test_rename();
    return 0;
}
