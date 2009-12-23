#include <toku_portability.h>
#include "test.h"
#include "minicron.h"
#include <unistd.h>

#include <string.h>
#include <stdlib.h>

static double
tdiff (struct timeval *a, struct timeval *b) {
    return (a->tv_sec-b->tv_sec) + (a->tv_usec-b->tv_usec)*1e-6;
}

struct timeval starttime;
static double elapsed (void) {
    struct timeval now;
    gettimeofday(&now, 0);
    return tdiff(&now, &starttime);
}

static int __attribute__((__noreturn__))
never_run (void *a) {
    assert(a==0);
    assert(0);
#if TOKU_WINDOWS
    return 0; //ICC ignores the noreturn attribute.
#endif
}

// Can we start something with period=0 (the function should never run) and shut it down.
static void*
test1 (void* v)
{
    struct minicron m;
    int r = toku_minicron_setup(&m, 0, never_run, 0);   assert(r==0);
    sleep(1);
    r = toku_minicron_shutdown(&m);                     assert(r==0);
    return v;
}

// Can we start something with period=10 and shut it down after 2 seconds (the function should never run) .
static void*
test2 (void* v)
{
    struct minicron m;
    int r = toku_minicron_setup(&m, 10, never_run, 0);   assert(r==0);
    sleep(2);
    r = toku_minicron_shutdown(&m);                     assert(r==0);
    return v;
}

struct tenx {
    struct timeval tv;
    int counter;
};

static int
run_5x (void *v) {
    struct tenx *tx=v;
    struct timeval now;
    gettimeofday(&now, 0);
    double diff = tdiff(&now, &tx->tv);
    if (verbose) printf("T=%f\n", diff);
    assert(diff>0.5 + tx->counter);
    assert(diff<1.5 + tx->counter);
    tx->counter++;
    return 0;
}

// Start something with period=1 and run it a few times
static void*
test3 (void* v)
{
    struct minicron m;
    struct tenx tx;
    gettimeofday(&tx.tv, 0);
    tx.counter=0;
    int r = toku_minicron_setup(&m, 1, run_5x, &tx);   assert(r==0);
    sleep(5);
    r = toku_minicron_shutdown(&m);                     assert(r==0);
    assert(tx.counter>=4 && tx.counter<=5); // after 5 seconds it could have run 4 or 5 times.
    return v;
}

static int
run_3sec (void *v) {
    if (verbose) printf("start3sec at %.6f\n", elapsed());
    int *counter = v;
    (*counter)++;
    sleep(3);
    if (verbose) printf("end3sec at %.6f\n", elapsed());
    return 0;
}

// make sure that if f is really slow that it doesn't run too many times
static void*
test4 (void *v) {
    struct minicron m;
    int counter = 0;
    int r = toku_minicron_setup(&m, 2, run_3sec, &counter); assert(r==0);
    sleep(9);
    r = toku_minicron_shutdown(&m);                     assert(r==0);
    assert(counter==2);
    return v;
}

static void*
test5 (void *v) {
    struct minicron m;
    int counter = 0;
    int r = toku_minicron_setup(&m, 10, run_3sec, &counter); assert(r==0);
    r = toku_minicron_change_period(&m, 2);                  assert(r==0);
    sleep(9);
    r = toku_minicron_shutdown(&m);                     assert(r==0);
    assert(counter==2);
    return v;
}

static void*
test6 (void *v) {
    struct minicron m;
    int r = toku_minicron_setup(&m, 5, never_run, 0); assert(r==0);
    r = toku_minicron_change_period(&m, 0);                  assert(r==0);
    sleep(7);
    r = toku_minicron_shutdown(&m);                          assert(r==0);
    return v;
}

typedef void*(*ptf)(void*);
int
test_main (int argc, const char *argv[]) {
    default_parse_args(argc,argv);
    gettimeofday(&starttime, 0);

    ptf testfuns[] = {test1, test2, test3,
		      test4,
		      test5,
		      test6
    };
#define N (sizeof(testfuns)/sizeof(testfuns[0]))
    toku_pthread_t tests[N];

    unsigned int i;
    for (i=0; i<N; i++) {
	int r=toku_pthread_create(tests+i, 0, testfuns[i], 0);
	assert(r==0);
    }
    for (i=0; i<N; i++) {
	void *v;
	int r=toku_pthread_join(tests[i], &v);
	assert(r==0);
	assert(v==0);
    }
    return 0;
}
