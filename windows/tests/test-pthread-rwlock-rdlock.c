#include <test.h>
#include <toku_assert.h>
#include <toku_pthread.h>
#include <stdio.h>
#include <unistd.h>

int test_main(int argc __attribute__((__unused__)), char *const argv[] __attribute__((__unused__))) {
    toku_pthread_rwlock_t rwlock;

    toku_pthread_rwlock_init(&rwlock, NULL);
    toku_pthread_rwlock_rdlock(&rwlock);
    toku_pthread_rwlock_rdlock(&rwlock);
    toku_pthread_rwlock_rdunlock(&rwlock);
    toku_pthread_rwlock_rdunlock(&rwlock);
    toku_pthread_rwlock_destroy(&rwlock);

    return 0;
}

