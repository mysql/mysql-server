/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
#include <test.h>
#include <toku_assert.h>
#include <toku_pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

// write a test see if things happen in the right order.

volatile int state = 0;
int verbose = 0;

static void *f(void *arg) {
    toku_pthread_rwlock_t *mylock = arg;
    sleep(2);
    assert(state==42); state = 16; if (verbose) printf("%s:%d\n", __FUNCTION__, __LINE__);
    toku_pthread_rwlock_wrlock(mylock);
    assert(state==49); state = 17; if (verbose) printf("%s:%d\n", __FUNCTION__, __LINE__);
    toku_pthread_rwlock_wrunlock(mylock);
    sleep(10);
    assert(state==52); state = 20; if (verbose) printf("%s:%d\n", __FUNCTION__, __LINE__);
    return arg;
}

int test_main(int argc , char *const argv[] ) {
    assert(argc==1 || argc==2);
    if (argc==2) {
	assert(strcmp(argv[1],"-v")==0);
	verbose = 1;
    }
    int r;
    toku_pthread_rwlock_t rwlock;
    toku_pthread_t tid;
    void *retptr;

    toku_pthread_rwlock_init(&rwlock, NULL);
    state = 37; if (verbose) printf("%s:%d\n", __FUNCTION__, __LINE__);
    toku_pthread_rwlock_rdlock(&rwlock);

    r = toku_pthread_create(&tid, NULL, f, &rwlock); assert(r == 0);

    assert(state==37); state = 42; if (verbose) printf("%s:%d\n", __FUNCTION__, __LINE__);
    sleep(4);
    assert(state==16); state = 44; if (verbose) printf("%s:%d\n", __FUNCTION__, __LINE__);
    toku_pthread_rwlock_rdlock(&rwlock);
    assert(state==44); state = 46; if (verbose) printf("%s:%d\n", __FUNCTION__, __LINE__);
    toku_pthread_rwlock_rdunlock(&rwlock);
    sleep(4);
    assert(state==46); state=49; if (verbose) printf("%s:%d\n", __FUNCTION__, __LINE__); // still have a read lock
    toku_pthread_rwlock_rdunlock(&rwlock);
    sleep(6);
    assert(state==17); state=52; if (verbose) printf("%s:%d\n", __FUNCTION__, __LINE__);

    r = toku_pthread_join(tid, &retptr); assert(r == 0);

    toku_pthread_rwlock_destroy(&rwlock);
    
    return 0;
}
