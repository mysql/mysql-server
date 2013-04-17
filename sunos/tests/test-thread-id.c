#include <stdio.h>
#include <assert.h>
#include <test.h>
#include <toku_pthread.h>
#include <toku_os.h>
#include <stdio.h>
#include <unistd.h>

void *f(void *arg) {
    int r;
    toku_pthread_rwlock_t *mylock = arg;
    printf("%s:%d:%d\n", __FUNCTION__, __LINE__, toku_os_gettid()); fflush(stdout);
    r = toku_pthread_rwlock_wrlock(mylock); assert(r == 0);
    printf("%s:%d:%d\n", __FUNCTION__, __LINE__, toku_os_gettid()); fflush(stdout);
    r = toku_pthread_rwlock_wrunlock(mylock); assert(r == 0);
    printf("%s:%d:%d\n", __FUNCTION__, __LINE__, toku_os_gettid()); fflush(stdout);
    return arg;
}

int test_main(int argc, char *argv[]) {
    int r;
    toku_pthread_rwlock_t rwlock;
    const int nthreads = 2;
    toku_pthread_t tid[nthreads];
    void *retptr;
    int i;

    r = toku_pthread_rwlock_init(&rwlock, NULL); assert(r == 0);
    printf("%s:%d:%d\n", __FUNCTION__, __LINE__, toku_os_gettid()); fflush(stdout);
    r = toku_pthread_rwlock_rdlock(&rwlock); assert(r == 0);

    for (i=0; i<nthreads; i++) {
        r = toku_pthread_create(&tid[i], NULL, f, &rwlock); assert(r == 0);
    }

    printf("%s:%d:%d\n", __FUNCTION__, __LINE__, toku_os_gettid()); fflush(stdout);
    sleep(10);
    printf("%s:%d:%d\n", __FUNCTION__, __LINE__, toku_os_gettid()); fflush(stdout);
    r = toku_pthread_rwlock_rdlock(&rwlock); assert(r == 0);
    printf("%s:%d:%d\n", __FUNCTION__, __LINE__, toku_os_gettid()); fflush(stdout);
    r = toku_pthread_rwlock_rdunlock(&rwlock); assert(r == 0);
    printf("%s:%d:%d\n", __FUNCTION__, __LINE__, toku_os_gettid()); fflush(stdout);
    r = toku_pthread_rwlock_rdunlock(&rwlock); assert(r == 0);
    printf("%s:%d:%d\n", __FUNCTION__, __LINE__, toku_os_gettid()); fflush(stdout);

    for (i=0; i<nthreads; i++) {
        r = toku_pthread_join(tid[i], &retptr); assert(r == 0);
    }
    printf("%s:%d:%d\n", __FUNCTION__, __LINE__, toku_os_gettid()); fflush(stdout);

    r = toku_pthread_rwlock_destroy(&rwlock); assert(r == 0);
    
    return 0;
}
