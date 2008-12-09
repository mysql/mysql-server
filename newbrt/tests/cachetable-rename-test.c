/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

#include "includes.h"
#include "test.h"

// this mutex is used by some of the tests to serialize access to some
// global data, especially between the test thread and the cachetable
// writeback threads

toku_pthread_mutex_t  test_mutex;

static inline void test_mutex_init() {
    int r = toku_pthread_mutex_init(&test_mutex, 0); assert(r == 0);
}

static inline void test_mutex_destroy() {
    int r = toku_pthread_mutex_destroy(&test_mutex); assert(r == 0);
}

static inline void test_mutex_lock() {
    int r = toku_pthread_mutex_lock(&test_mutex); assert(r == 0);
}

static inline void test_mutex_unlock() {
    int r = toku_pthread_mutex_unlock(&test_mutex); assert(r == 0);
}

enum { KEYLIMIT = 4, TRIALLIMIT=256000 };
static CACHEKEY  keys[KEYLIMIT];
static void*     vals[KEYLIMIT];
static int       n_keys=0;

static void r_flush (CACHEFILE f      __attribute__((__unused__)),
		     CACHEKEY k,
		     void *value,
		     void *extra      __attribute__((__unused__)),
		     long size        __attribute__((__unused__)),
		     BOOL write_me    __attribute__((__unused__)),
		     BOOL keep_me,
		     LSN modified_lsn __attribute__((__unused__)),
		     BOOL rename_p    __attribute__((__unused__))) {
    int i;
    //printf("Flush\n");
    if (keep_me) return;

    test_mutex_lock();
    for (i=0; i<n_keys; i++) {
	if (keys[i].b==k.b) {
	    assert(vals[i]==value);
	    if (!keep_me) {
                if (verbose) printf("%s: %d/%d %" PRIx64 "\n", __FUNCTION__, i, n_keys, k.b);
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
    r = toku_cachetable_openf(&f, t, fname, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO);
    assert(r==0);
    for (i=0; i<TRIALLIMIT; i++) {
	int ra = random()%3;
	if (ra<=1) {
	    // Insert something
	    CACHEKEY nkey = make_blocknum(random());
	    long     nval = random();
	    if (verbose) printf("n_keys=%d Insert %08" PRIx64 "\n", n_keys, nkey.b);
	    u_int32_t hnkey = toku_cachetable_hash(f, nkey);
	    r = toku_cachetable_put(f, nkey, hnkey,
				    (void*)nval, 1,
				    r_flush, r_fetch, 0);
	    assert(r==0);
            test_mutex_lock();
            while (n_keys >= KEYLIMIT) {
                test_mutex_unlock();
                toku_pthread_yield();
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
	    CACHEKEY nkey = make_blocknum(random());
            test_mutex_lock();
	    CACHEKEY okey = keys[objnum];
            test_mutex_unlock();
	    void *current_value;
	    long current_size;
	    if (verbose) printf("Rename %" PRIx64 " to %" PRIx64 "\n", okey.b, nkey.b);
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
                if (keys[j].b == okey.b)
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
        okey = make_blocknum(random());
        void *v;
        r = toku_cachetable_maybe_get_and_pin(f, okey, toku_cachetable_hash(f, okey), &v);
        if (r != 0)
            break;
        r = toku_cachetable_unpin(f, okey, toku_cachetable_hash(f, okey), CACHETABLE_CLEAN, 1);
        assert(r == 0);
    }
    nkey = make_blocknum(random());
    r = toku_cachetable_rename(f, okey, nkey);
    assert(r != 0);

    r = toku_cachefile_close(&f, 0);
    assert(r == 0);
    r = toku_cachetable_close(&t);
    assert(r == 0);
    test_mutex_destroy();
    assert(n_keys == 0);
}

int
test_main (int argc, const char *argv[]) {
    // parse args
    default_parse_args(argc, argv);
    toku_os_initialize_settings(verbose);

    // run tests
    int i;
    for (i=0; i<1; i++) 
        test_rename();
    return 0;
}
