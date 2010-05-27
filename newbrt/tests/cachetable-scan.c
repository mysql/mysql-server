/* How fast can we read a file usng the cachetable interface. */
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

#include "test.h"

#include "includes.h"
#include "toku_time.h"

enum { KEYLIMIT = 4, BLOCKSIZE=1<<20, N=2048};

static void f_flush (CACHEFILE f,
                     int UU(fd),
		     CACHEKEY key,
		     void *value,
		     void *extra       __attribute__((__unused__)),
		     long size,
		     BOOL write_me,
		     BOOL keep_me,
		     BOOL for_checkpoint     __attribute__((__unused__))) {
    assert(size==BLOCKSIZE);
    if (write_me) {
	toku_os_full_pwrite(toku_cachefile_get_and_pin_fd(f), value, BLOCKSIZE, key.b);
        toku_cachefile_unpin_fd(f);
    }
    if (!keep_me) {
	toku_free(value);
    }
}

static int f_fetch (CACHEFILE f,
                    int UU(fd),
		    CACHEKEY key,
		    u_int32_t fullhash __attribute__((__unused__)),
		    void**value,
		    long *sizep,
		    void*extraargs     __attribute__((__unused__))) {
    void *buf = toku_malloc(BLOCKSIZE);
    int r = pread(toku_cachefile_get_and_pin_fd(f), buf, BLOCKSIZE, key.b);
    toku_cachefile_unpin_fd(f);
    assert(r==BLOCKSIZE);
    *value = buf;
    *sizep = BLOCKSIZE;
    return 0;
}

const char fname[] = __FILE__ ".dat";

CACHETABLE t;
CACHEFILE f;

static void open_file (void ) {
    int r;
    r = toku_create_cachetable(&t, KEYLIMIT, ZERO_LSN, NULL_LOGGER); assert(r==0);
    r = toku_cachetable_openf(&f, t, fname, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO);   assert(r==0);
}

static void writeit (void) {
    struct timeval start, end;
    gettimeofday(&start, 0);
    int i, r;
    for (i=0; i<N; i++) {
	void *buf = toku_malloc(BLOCKSIZE);
	CACHEKEY key = make_blocknum(i*BLOCKSIZE);
	u_int32_t fullhash = toku_cachetable_hash(f, key);
	int j;
	for (j=0; j<BLOCKSIZE; j++) ((char*)buf)[j]=(char)((i+j)%256);
	r = toku_cachetable_put(f, key, fullhash, buf, BLOCKSIZE, f_flush, f_fetch, 0);	assert(r==0);
	r = toku_cachetable_unpin(f, key, fullhash, CACHETABLE_CLEAN, BLOCKSIZE); assert(r==0);
    }
    gettimeofday(&end, 0);
    double diff = toku_tdiff(&end, &start);
    if (verbose)
	printf("writeit %d blocks of size %d in %6.2fs at %6.2fMB/s\n", N, BLOCKSIZE, diff, (N/diff)*(BLOCKSIZE*1e-6));
}

static void readit (void) {
    struct timeval start, end;
    struct timeval start_usertime, start_systime;
    struct timeval end_usertime, end_systime;
    toku_os_get_process_times(&start_usertime, &start_systime);
    gettimeofday(&start, 0);
    int i, r;
    void *block;
    long  current_size;
    for (i=0; i<N; i++) {
	CACHEKEY key = make_blocknum(i*BLOCKSIZE);
	u_int32_t fullhash = toku_cachetable_hash(f, key);
	r=toku_cachetable_get_and_pin(f, key, fullhash, &block, &current_size, f_flush, f_fetch, 0); assert(r==0);
	r=toku_cachetable_unpin(f, key, fullhash, CACHETABLE_CLEAN, BLOCKSIZE);                                      assert(r==0);
    }
    r = toku_cachefile_close(&f, 0, FALSE, ZERO_LSN);    assert(r == 0);
    r = toku_cachetable_close(&t);      assert(r == 0);
    gettimeofday(&end, 0);
    toku_os_get_process_times(&end_usertime, &end_systime);
    double diff = toku_tdiff(&end, &start);
    double udiff = toku_tdiff(&end_usertime, &start_usertime);
    double sdiff = toku_tdiff(&end_systime, &start_systime);
    if (verbose)
	printf("readit  %d blocks of size %d in %6.2fs at %6.2fMB/s   user=%6.2fs sys=%6.2fs\n",
	       N, BLOCKSIZE, diff, (N/diff)*(BLOCKSIZE*1e-6), udiff, sdiff);
}

int
test_main (int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    open_file();
    writeit();
    readit();
    return 0;
}
