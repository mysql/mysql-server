#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <toku_assert.h>
#include <toku_pthread.h>
#include "queue.h"

static int verbose=1;

static int count_0 = 0;
static u_int64_t e_max_weight=0, d_max_weight = 0; // max weight seen by enqueue thread and dequeue thread respectively.

static void *start_0 (void *arg) {
    QUEUE q = (QUEUE)arg;
    void *item;
    u_int64_t weight;
    long count = 0;
    while (1) {
	u_int64_t this_max_weight;
	int r=queue_deq(q, &item, &weight, &this_max_weight);
	if (r==EOF) break;
	assert(r==0);
	if (this_max_weight>d_max_weight) d_max_weight=this_max_weight;
	long v = (long)item;
	//printf("D(%ld)=%ld %ld\n", v, this_max_weight, d_max_weight);
	assert(v==count);
	count_0++;
	count++;
    }
    return NULL;
}

static void enq (QUEUE q, long v, u_int64_t weight) {
    u_int64_t this_max_weight;
    int r = queue_enq(q, (void*)v, (weight==0)?0:1, &this_max_weight);
    assert(r==0);
    if (this_max_weight>e_max_weight) e_max_weight=this_max_weight;
    //printf("E(%ld)=%ld %ld\n", v, this_max_weight, e_max_weight);
}

static void queue_test_0 (u_int64_t weight)
// Test a queue that can hold WEIGHT items.
{
    //printf("\n");
    count_0 = 0;
    e_max_weight = 0;
    d_max_weight = 0;
    QUEUE q;
    int r;
    r = queue_create(&q, weight);                               assert(r==0);
    toku_pthread_t thread;
    r = toku_pthread_create(&thread, NULL, start_0, q); assert(r==0);
    enq(q, 0L, weight);
    enq(q, 1L, weight);
    enq(q, 2L, weight);
    enq(q, 3L, weight);
    sleep(1);
    enq(q, 4L, weight);
    enq(q, 5L, weight);
    r = queue_eof(q);                                      assert(r==0);
    void *result;
    r = toku_pthread_join(thread, &result);	           assert(r==0);
    assert(result==NULL);
    assert(count_0==6);
    r = queue_destroy(q);
    assert(d_max_weight <= weight);
    assert(e_max_weight <= weight);
}


static void parse_args (int argc, const char *argv[]) {
    const char *progname=argv[0];
    argc--; argv++;
    while (argc>0) {
	if (strcmp(argv[0],"-v")==0) {
	    verbose++;
	} else if (strcmp(argv[0],"-q")==0) {
	    verbose--;
	} else {
	    fprintf(stderr, "Usage:\n %s [-v] [-q]\n", progname);
	    exit(1);
	}
	argc--; argv++;
    }
    if (verbose<0) verbose=0;
}

int main (int argc, const char *argv[]) {
    parse_args(argc, argv);
    queue_test_0(0LL);
    queue_test_0(1LL);
    queue_test_0(2LL);
    return 0;
}
