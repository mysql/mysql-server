// check if write locks are fair

#include <stdio.h>
#include <toku_assert.h>
#include <unistd.h>
#include <pthread.h>
#include <util/frwlock.h>

toku_mutex_t rwlock_mutex;
toku::frwlock rwlock;
volatile int killed = 0;

static void *t1_func(void *arg) {
    int i;
    for (i = 0; !killed; i++) {
        toku_mutex_lock(&rwlock_mutex);
        rwlock.write_lock(false);
        toku_mutex_unlock(&rwlock_mutex);
        usleep(10000);
        toku_mutex_lock(&rwlock_mutex);
        rwlock.write_unlock();
        toku_mutex_unlock(&rwlock_mutex);
    }
    printf("%lu %d\n", (unsigned long) pthread_self(), i);
    return arg;
}

int main(void) {
    int r;

    toku_mutex_init(&rwlock_mutex, NULL);
    rwlock.init(&rwlock_mutex);
    
    const int nthreads = 2;
    pthread_t tids[nthreads];
    for (int i = 0; i < nthreads; i++) {
        r = pthread_create(&tids[i], NULL, t1_func, NULL); 
        assert(r == 0);
    }
    sleep(10);
    killed = 1;
    for (int i = 0; i < nthreads; i++) {
        void *ret;
        r = pthread_join(tids[i], &ret);
        assert(r == 0);
    }

    rwlock.deinit();
    toku_mutex_destroy(&rwlock_mutex);

    return 0;
}
