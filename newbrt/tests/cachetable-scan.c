/* How fast can we read a file usng the cachetable interface. */
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>

#include "cachetable.h"

enum { KEYLIMIT = 4, BLOCKSIZE=1<<20, N=2048};

static double tdiff (struct timeval *a, struct timeval *b) {
    return (a->tv_sec-b->tv_sec)+1e-6*(a->tv_usec-b->tv_usec);
}

static void f_flush (CACHEFILE f,
		     CACHEKEY key,
		     void *value,
		     long size,
		     BOOL write_me,
		     BOOL keep_me,
		     LSN  modified_lsn __attribute__((__unused__)),
		     BOOL rename_p    __attribute__((__unused__))) {
    assert(size==BLOCKSIZE);
    if (write_me) {
	int r = pwrite(toku_cachefile_fd(f), value, BLOCKSIZE, key);
	assert(r==BLOCKSIZE);
    }
    if (!keep_me) {
	free(value);
    }
}

static int f_fetch (CACHEFILE f,
		    CACHEKEY key,
		    u_int32_t fullhash __attribute__((__unused__)),
		    void**value,
		    long *sizep,
		    void*extraargs     __attribute__((__unused__)),
		    LSN *modified_lsn  __attribute__((__unused__))) {
    void *buf = malloc(BLOCKSIZE);
    int r = pread(toku_cachefile_fd(f), buf, BLOCKSIZE, key);
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
    r = toku_cachetable_openf(&f, t, fname, O_RDWR|O_CREAT, 0777);   assert(r==0);
}

static void writeit (void) {
    struct timeval start, end;
    gettimeofday(&start, 0);
    int i, r;
    for (i=0; i<N; i++) {
	void *buf = malloc(BLOCKSIZE);
	CACHEKEY key = i*BLOCKSIZE;
	u_int32_t fullhash = toku_cachetable_hash(f, key);
	int j;
	for (j=0; j<BLOCKSIZE; j++) ((char*)buf)[j]=(i+j)%256;
	r = toku_cachetable_put(f, key, fullhash, buf, BLOCKSIZE, f_flush, f_fetch, 0);	assert(r==0);
	r = toku_cachetable_unpin(f, key, fullhash, 0, BLOCKSIZE);                      assert(r==0);
    }
    gettimeofday(&end, 0);
    double diff = tdiff(&end, &start);
    printf("writeit %d blocks of size %d in %6.2fs at %6.2fMB/s\n", N, BLOCKSIZE, diff, (N/diff)*(BLOCKSIZE*1e-6));
}

static void readit (void) {
    struct timeval start, end;
    struct rusage start_usage, end_usage;
    getrusage(RUSAGE_SELF, &start_usage);
    gettimeofday(&start, 0);
    int i, r;
    void *block;
    long  current_size;
    CACHEKEY key;
    for (i=0; i<N; i++) {
	key = i*BLOCKSIZE;
	u_int32_t fullhash = toku_cachetable_hash(f, key);
	r=toku_cachetable_get_and_pin(f, key, fullhash, &block, &current_size, f_flush, f_fetch, 0); assert(r==0);
	r=toku_cachetable_unpin(f, key, fullhash, 0, BLOCKSIZE);                                      assert(r==0);
    }
    r = toku_cachefile_close(&f, 0);    assert(r == 0);
    r = toku_cachetable_close(&t);      assert(r == 0);
    gettimeofday(&end, 0);
    getrusage(RUSAGE_SELF, &end_usage);
    double diff = tdiff(&end, &start);
    double udiff = tdiff(&end_usage.ru_utime, &start_usage.ru_utime);
    double sdiff = tdiff(&end_usage.ru_stime, &start_usage.ru_stime);
    printf("readit  %d blocks of size %d in %6.2fs at %6.2fMB/s   user=%6.2fs sys=%6.2fs\n",
	   N, BLOCKSIZE, diff, (N/diff)*(BLOCKSIZE*1e-6), udiff, sdiff);
}

int main (int argc __attribute__((__unused__)), char *argv[] __attribute__((__unused__))) {
    open_file();
    writeit();
    readit();
    return 0;
}
