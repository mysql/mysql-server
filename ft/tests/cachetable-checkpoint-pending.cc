/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
// Make sure that the pending stuff gets checkpointed, but subsequent changes don't, even with concurrent updates.
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."

#include "test.h"
#include <stdio.h>
#include <unistd.h>
#include "cachetable-test.h"
#include "checkpoint.h"
#include <portability/toku_atomic.h>

static int N; // how many items in the table
static CACHEFILE cf;
static CACHETABLE ct;
int    *values;

static const int item_size = sizeof(int);

static volatile int n_flush, n_write_me, n_keep_me, n_fetch;

static void
sleep_random (void)
{
#if TOKU_WINDOWS
    usleep(random() % 1000); //Will turn out to be almost always 1ms.
#else
    toku_timespec_t req = {.tv_sec  = 0,
			   .tv_nsec = random()%1000000}; //Max just under 1ms
    nanosleep(&req, NULL);
#endif
}

int expect_value = 42; // initially 42, later 43

static void
flush (
    CACHEFILE UU(thiscf), 
    int UU(fd), 
    CACHEKEY UU(key), 
    void *value, 
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
    // printf("f");
    assert(size.size== item_size);
    int *CAST_FROM_VOIDP(v, value);
    if (*v!=expect_value) printf("got %d expect %d\n", *v, expect_value);
    assert(*v==expect_value);
    (void)toku_sync_fetch_and_add(&n_flush, 1);
    if (write_me) (void)toku_sync_fetch_and_add(&n_write_me, 1);
    if (keep_me)  (void)toku_sync_fetch_and_add(&n_keep_me, 1);
    sleep_random();
}

static void*
do_update (void *UU(ignore))
{
    while (n_flush==0); // wait until the first checkpoint ran
    int i;
    for (i=0; i<N; i++) {
	CACHEKEY key = make_blocknum(i);
        uint32_t hi = toku_cachetable_hash(cf, key);
        void *vv;
	long size;
        CACHETABLE_WRITE_CALLBACK wc = def_write_callback(NULL);
        wc.flush_callback = flush;
        int r = toku_cachetable_get_and_pin(cf, key, hi, &vv, &size, wc, fetch_die, def_pf_req_callback, def_pf_callback, true, 0);
	//printf("g");
	assert(r==0);
	assert(size==sizeof(int));
	int *CAST_FROM_VOIDP(v, vv);
	assert(*v==42);
	*v = 43;
	//printf("[%d]43\n", i);
	r = toku_test_cachetable_unpin(cf, key, hi, CACHETABLE_DIRTY, make_pair_attr(item_size));
	sleep_random();
    }
    return 0;
}

static void*
do_checkpoint (void *UU(v))
{
    CHECKPOINTER cp = toku_cachetable_get_checkpointer(ct);
    int r = toku_checkpoint(cp, NULL, NULL, NULL, NULL, NULL, CLIENT_CHECKPOINT);
    assert(r == 0);
    return 0;
}

// put n items into the cachetable, mark them dirty, and then concurently
//   do a checkpoint (in which the callback functions are slow)
//   replace the n items with new values
// make sure that the stuff that was checkpointed includes only the old versions
// then do a flush and make sure the new items are written

static void checkpoint_pending(void) {
    if (verbose) { printf("%s:%d n=%d\n", __FUNCTION__, __LINE__, N); fflush(stdout); }
    const int test_limit = N;
    int r;
    toku_cachetable_create(&ct, test_limit*sizeof(int), ZERO_LSN, NULL_LOGGER);
    const char *fname1 = TOKU_TEST_FILENAME;
    r = unlink(fname1); if (r!=0) CKERR2(get_error_errno(), ENOENT);
    r = toku_cachetable_openf(&cf, ct, fname1, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO); assert(r == 0);
    create_dummy_functions(cf);
    
    // Insert items into the cachetable. All dirty.
    int i;
    for (i=0; i<N; i++) {
        CACHEKEY key = make_blocknum(i);
        uint32_t hi = toku_cachetable_hash(cf, key);
	values[i] = 42;
        CACHETABLE_WRITE_CALLBACK wc = def_write_callback(NULL);
        wc.flush_callback = flush;
        toku_cachetable_put(cf, key, hi, &values[i], make_pair_attr(sizeof(int)), wc, put_callback_nop);
        assert(r == 0);

        r = toku_test_cachetable_unpin(cf, key, hi, CACHETABLE_DIRTY, make_pair_attr(item_size));
        assert(r == 0);
    }

    // the checkpoint should cause n writes, but since n <= the cachetable size,
    // all items should be kept in the cachetable
    n_flush = n_write_me = n_keep_me = n_fetch = 0; expect_value = 42;
    //printf("E42\n");
    toku_pthread_t checkpoint_thread, update_thread;
    r = toku_pthread_create(&checkpoint_thread, NULL, do_checkpoint, NULL);  assert(r==0);
    r = toku_pthread_create(&update_thread,     NULL, do_update,     NULL);  assert(r==0);
    r = toku_pthread_join(checkpoint_thread, 0);                             assert(r==0);
    r = toku_pthread_join(update_thread, 0);                                 assert(r==0);
    
    assert(n_flush == N && n_write_me == N && n_keep_me == N);

    // after the checkpoint, all of the items should be 43
    //printf("E43\n");
    n_flush = n_write_me = n_keep_me = n_fetch = 0; expect_value = 43;
    CHECKPOINTER cp = toku_cachetable_get_checkpointer(ct);
    r = toku_checkpoint(cp, NULL, NULL, NULL, NULL, NULL, CLIENT_CHECKPOINT);
    assert(r == 0);
    assert(n_flush == N && n_write_me == N && n_keep_me == N);

    // a subsequent checkpoint should cause no flushes, or writes since all of the items are clean
    n_flush = n_write_me = n_keep_me = n_fetch = 0;

    r = toku_checkpoint(cp, NULL, NULL, NULL, NULL, NULL, CLIENT_CHECKPOINT);
    assert(r == 0);
    assert(n_flush == 0 && n_write_me == 0 && n_keep_me == 0);

    toku_cachefile_close(&cf, false, ZERO_LSN);
    toku_cachetable_close(&ct);
}

int
test_main(int argc, const char *argv[]) {
    {
	struct timeval tv;
	gettimeofday(&tv, 0);
	srandom(tv.tv_sec * 1000000 + tv.tv_usec);
    }	
    {
	int i;
	for (i=1; i<argc; i++) {
	    if (strcmp(argv[i], "-v") == 0) {
		verbose++;
		continue;
	    }
	}
    }
    for (N=1; N<=128; N*=2) {
	int myvalues[N];
	values = myvalues;
        checkpoint_pending();
	//printf("\n");
    }
    return 0;
}
