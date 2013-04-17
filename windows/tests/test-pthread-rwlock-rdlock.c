#include <test.h>
#include <toku_assert.h>
#include <toku_pthread.h>
#include <stdio.h>
#include <unistd.h>

int test_main(int argc, char *const argv[]) {
    int r;
    toku_pthread_rwlock_t rwlock;

    r = toku_pthread_rwlock_init(&rwlock, NULL); assert(r == 0);
    r = toku_pthread_rwlock_rdlock(&rwlock); assert(r == 0);
    r = toku_pthread_rwlock_rdlock(&rwlock); assert(r == 0);
    r = toku_pthread_rwlock_rdunlock(&rwlock); assert(r == 0);
    r = toku_pthread_rwlock_rdunlock(&rwlock); assert(r == 0);
    r = toku_pthread_rwlock_destroy(&rwlock); assert(r == 0);

    return 0;
}

