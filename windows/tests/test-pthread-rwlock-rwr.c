#include <toku_assert.h>
#include <test.h>
#include <toku_pthread.h>
#include <stdio.h>
#include <unistd.h>

static void *f(void *arg) {
    int r;
    toku_pthread_rwlock_t *mylock = arg;
    printf("%s:%d\n", __FUNCTION__, __LINE__); fflush(stdout);
    r = toku_pthread_rwlock_wrlock(mylock); assert(r == 0);
    printf("%s:%d\n", __FUNCTION__, __LINE__); fflush(stdout);
    r = toku_pthread_rwlock_wrunlock(mylock); assert(r == 0);
    printf("%s:%d\n", __FUNCTION__, __LINE__); fflush(stdout);
    return arg;
}

int test_main(int argc  __attribute__((__unused__)), char *argv[]  __attribute__((__unused__))) {
    int r;
    toku_pthread_rwlock_t rwlock;
    toku_pthread_t tid;
    void *retptr;

    r = toku_pthread_rwlock_init(&rwlock, NULL); assert(r == 0);
    printf("%s:%d\n", __FUNCTION__, __LINE__); fflush(stdout);
    r = toku_pthread_rwlock_rdlock(&rwlock); assert(r == 0);

    r = toku_pthread_create(&tid, NULL, f, &rwlock); assert(r == 0);

    printf("%s:%d\n", __FUNCTION__, __LINE__); fflush(stdout);
    sleep(10);
    printf("%s:%d\n", __FUNCTION__, __LINE__); fflush(stdout);
    r = toku_pthread_rwlock_rdlock(&rwlock); assert(r == 0);
    printf("%s:%d\n", __FUNCTION__, __LINE__); fflush(stdout);
    r = toku_pthread_rwlock_rdunlock(&rwlock); assert(r == 0);
    printf("%s:%d\n", __FUNCTION__, __LINE__); fflush(stdout);
    r = toku_pthread_rwlock_rdunlock(&rwlock); assert(r == 0);
    printf("%s:%d\n", __FUNCTION__, __LINE__); fflush(stdout);

    r = toku_pthread_join(tid, &retptr); assert(r == 0);

    r = toku_pthread_rwlock_destroy(&rwlock); assert(r == 0);
    
    return 0;
}
