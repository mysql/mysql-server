// Make sure that the pending stuff gets checkpointed, but subsequent changes don't, even with concurrent updates.

#include <stdio.h>
#include <unistd.h>
#include <toku_assert.h>

#include "test.h"
#include "checkpoint.h"

static int N; // how many items in the table
static CACHEFILE cf;
static CACHETABLE ct;
int    *values;

static const int item_size = sizeof(int);

static volatile int n_flush, n_write_me, n_keep_me, n_fetch;

#if TOKU_WINDOWS
//This is NOT correct, but close enough for now.
//Obviously has race conditions.
static void
__sync_fetch_and_add(volatile int *num, int incr) {
    *num += incr;
}
#endif

static void
sleep_random (void)
{
#if TOKU_WINDOWS
    sleep(1);
#else
    toku_timespec_t req = {.tv_sec  = 0,
			   .tv_nsec = random()%1000000};
    nanosleep(&req, NULL);
#endif
}

int expect_value = 42; // initially 42, later 43

static void
flush (CACHEFILE UU(thiscf), CACHEKEY UU(key), void *value, void *UU(extraargs), long size, BOOL write_me, BOOL keep_me, BOOL UU(for_checkpoint))
{
    // printf("f");
    assert(size == item_size);
    int *v = value;
    if (*v!=expect_value) printf("got %d expect %d\n", *v, expect_value);
    assert(*v==expect_value);
    (void)__sync_fetch_and_add(&n_flush, 1);
    if (write_me) (void)__sync_fetch_and_add(&n_write_me, 1);
    if (keep_me)  (void)__sync_fetch_and_add(&n_keep_me,  1);
    sleep_random();
}

static int
fetch (CACHEFILE UU(thiscf), CACHEKEY UU(key), u_int32_t UU(fullhash), void **UU(value), long *UU(sizep), void *UU(extraargs))
{
    assert(0); // should not be called
    return 0;
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
        int r = toku_cachetable_get_and_pin(cf, key, hi, &vv, &size, flush, fetch, 0);
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
    int r = toku_checkpoint(ct, NULL, NULL, NULL, NULL, NULL, NULL);
    assert(r == 0);
    return 0;
}

// put n items into the cachetable, mark them dirty, and then concurently
//   do a checkpoint (in which the callback functions are slow)
//   replace the n items with new values
// make sure that the stuff that was checkpointed includes only the old versions
// then do a flush and make sure the new items are written

static void checkpoint_pending(void) {
    if (verbose) printf("%s:%d n=%d\n", __FUNCTION__, __LINE__, N);
    const int test_limit = N;
    int r;
    r = toku_create_cachetable(&ct, test_limit*sizeof(int), ZERO_LSN, NULL_LOGGER); assert(r == 0);
    char fname1[] = __FILE__ "test1.dat";
    unlink(fname1);
    r = toku_cachetable_openf(&cf, ct, fname1, fname1, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO); assert(r == 0);

    // Insert items into the cachetable. All dirty.
    int i;
    for (i=0; i<N; i++) {
        CACHEKEY key = make_blocknum(i);
        u_int32_t hi = toku_cachetable_hash(cf, key);
	values[i] = 42;
        r = toku_cachetable_put(cf, key, hi, &values[i], sizeof(int), flush, fetch, 0);
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

    r = toku_checkpoint(ct, NULL, NULL, NULL, NULL, NULL, NULL);
    assert(r == 0);
    assert(n_flush == N && n_write_me == N && n_keep_me == N);

    // a subsequent checkpoint should cause no flushes, or writes since all of the items are clean
    n_flush = n_write_me = n_keep_me = n_fetch = 0;

    r = toku_checkpoint(ct, NULL, NULL, NULL, NULL, NULL, NULL);
    assert(r == 0);
    assert(n_flush == 0 && n_write_me == 0 && n_keep_me == 0);

    r = toku_cachefile_close(&cf, NULL_LOGGER, 0, ZERO_LSN); assert(r == 0 && cf == 0);
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
