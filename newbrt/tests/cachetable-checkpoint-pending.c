// Make sure that the pending stuff gets checkpointed, but subsequent changes don't, even with concurrent updates.
#ident "$Id$"
#ident "Copyright (c) 2007-2011 Tokutek Inc.  All rights reserved."

#include "test.h"
#include <stdio.h>
#include <unistd.h>
#include "checkpoint.h"
#include "toku_atomic.h"

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
flush (CACHEFILE UU(thiscf), int UU(fd), CACHEKEY UU(key), void *value, void *UU(extraargs), long size, long* UU(new_size), BOOL write_me, BOOL keep_me, BOOL UU(for_checkpoint))
{
    // printf("f");
    assert(size == item_size);
    int *v = value;
    if (*v!=expect_value) printf("got %d expect %d\n", *v, expect_value);
    assert(*v==expect_value);
    (void)toku_sync_fetch_and_increment_int32(&n_flush);
    if (write_me) (void)toku_sync_fetch_and_increment_int32(&n_write_me);
    if (keep_me)  (void)toku_sync_fetch_and_increment_int32(&n_keep_me);
    sleep_random();
}

static int
fetch (CACHEFILE UU(thiscf), int UU(fd), CACHEKEY UU(key), u_int32_t UU(fullhash), void **UU(value), long *UU(sizep), int *UU(dirtyp), void *UU(extraargs))
{
    assert(0); // should not be called
    return 0;
}

static void 
pe_est_callback(
    void* UU(brtnode_pv), 
    long* bytes_freed_estimate, 
    enum partial_eviction_cost *cost, 
    void* UU(write_extraargs)
    )
{
    *bytes_freed_estimate = 0;
    *cost = PE_CHEAP;
}

static int 
pe_callback (
    void *brtnode_pv __attribute__((__unused__)), 
    long bytes_to_free __attribute__((__unused__)), 
    long* bytes_freed, 
    void* extraargs __attribute__((__unused__))
    ) 
{
    *bytes_freed = bytes_to_free;
    return 0;
}

static BOOL pf_req_callback(void* UU(brtnode_pv), void* UU(read_extraargs)) {
    // placeholder for now
    return FALSE;
}

static int pf_callback(void* UU(brtnode_pv), void* UU(read_extraargs), int UU(fd), long* UU(sizep)) {
    assert(FALSE);
}

static void*
do_update (void *UU(ignore))
{
    while (n_flush==0); // wait until the first checkpoint ran
    int i;
    for (i=0; i<N; i++) {
	CACHEKEY key = make_blocknum(i);
        u_int32_t hi = toku_cachetable_hash(cf, key);
        void *vv;
	long size;
        int r = toku_cachetable_get_and_pin(cf, key, hi, &vv, &size, flush, fetch, pe_est_callback, pe_callback, pf_req_callback, pf_callback, 0, 0);
	//printf("g");
	assert(r==0);
	assert(size==sizeof(int));
	int *v = vv;
	assert(*v==42);
	*v = 43;
	//printf("[%d]43\n", i);
	r = toku_cachetable_unpin(cf, key, hi, CACHETABLE_DIRTY, item_size);
	sleep_random();
    }
    return 0;
}

static void*
do_checkpoint (void *UU(v))
{
    int r = toku_checkpoint(ct, NULL, NULL, NULL, NULL, NULL);
    assert(r == 0);
    return 0;
}

// put n items into the cachetable, mark them dirty, and then concurently
//   do a checkpoint (in which the callback functions are slow)
//   replace the n items with new values
// make sure that the stuff that was checkpointed includes only the old versions
// then do a flush and make sure the new items are written

static int dummy_pin_unpin(CACHEFILE UU(cfu), void* UU(v)) {
    return 0;
}

static void checkpoint_pending(void) {
    if (verbose) { printf("%s:%d n=%d\n", __FUNCTION__, __LINE__, N); fflush(stdout); }
    const int test_limit = N;
    int r;
    r = toku_create_cachetable(&ct, test_limit*sizeof(int), ZERO_LSN, NULL_LOGGER); assert(r == 0);
    char fname1[] = __FILE__ "test1.dat";
    r = unlink(fname1); if (r!=0) CKERR2(errno, ENOENT);
    r = toku_cachetable_openf(&cf, ct, fname1, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO); assert(r == 0);
    toku_cachefile_set_userdata(cf, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                                dummy_pin_unpin, dummy_pin_unpin);

    // Insert items into the cachetable. All dirty.
    int i;
    for (i=0; i<N; i++) {
        CACHEKEY key = make_blocknum(i);
        u_int32_t hi = toku_cachetable_hash(cf, key);
	values[i] = 42;
        r = toku_cachetable_put(cf, key, hi, &values[i], sizeof(int), flush, pe_est_callback, pe_callback, 0);
        assert(r == 0);

        r = toku_cachetable_unpin(cf, key, hi, CACHETABLE_DIRTY, item_size);
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

    r = toku_checkpoint(ct, NULL, NULL, NULL, NULL, NULL);
    assert(r == 0);
    assert(n_flush == N && n_write_me == N && n_keep_me == N);

    // a subsequent checkpoint should cause no flushes, or writes since all of the items are clean
    n_flush = n_write_me = n_keep_me = n_fetch = 0;

    r = toku_checkpoint(ct, NULL, NULL, NULL, NULL, NULL);
    assert(r == 0);
    assert(n_flush == 0 && n_write_me == 0 && n_keep_me == 0);

    r = toku_cachefile_close(&cf, 0, FALSE, ZERO_LSN); assert(r == 0 && cf == 0);
    r = toku_cachetable_close(&ct); assert(r == 0 && ct == 0);
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
