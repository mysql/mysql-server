/* How expensive is
 *   - Obtaining a read-only lock for the first obtainer.
 *   - Obtaining it for the second one?
 *   - The third one? */

#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/time.h>

float tdiff (struct timeval *start, struct timeval *end) {
    return 1e6*(end->tv_sec-start->tv_sec) +(end->tv_usec - start->tv_usec);
}

/* My own rwlock implementation. */
struct brwl {
    int mutex;
    int state; // 0 for unlocked, -1 for a writer, otherwise many readers
};

static inline int xchg(volatile int *ptr, int x)
{
    __asm__("xchgl %0,%1" :"=r" (x) :"m" (*(ptr)), "0" (x) :"memory");
    return x;
}

static inline void sfence (void) {
    asm volatile ("sfence":::"memory");
}

static inline void brwl_rlock_fence (struct brwl *l) {
    while (xchg(&l->mutex, 1)) ;
    l->state++;
    sfence();
    l->mutex=0;
}

static inline void brwl_rlock_xchg (struct brwl *l) {
    while (xchg(&l->mutex, 1)) ;
    l->state++;
    xchg(&l->mutex, 0);
}


enum {K=100000};
pthread_rwlock_t rwlocks[K];
struct brwl blocks[K];
pthread_mutex_t mlocks[K];

int main (int argc __attribute__((__unused__)), char *argv[] __attribute__((__unused__))) {
    int j;
    int i;
    int r;
    struct timeval start, end;
    for (j=0; j<3; j++) {
	for (i=0; i<K; i++) {
	    r=pthread_mutex_init(&mlocks[i], NULL);
	    assert(r==0);
	}
	gettimeofday(&start, 0);
	for (i=0; i<K; i++) {
	    r = pthread_mutex_lock(&mlocks[i]);
	    assert(r==0);
	}
	gettimeofday(&end, 0);
	printf("pthread_mutex_lock       took %9.3fus for %d ops:  %9.3fus/lock (%9.3fMops/s)\n", tdiff(&start,&end), K, tdiff(&start,&end)/K, K/tdiff(&start,&end));

	gettimeofday(&start, 0);
	for (i=0; i<K; i++) {
	    r = pthread_mutex_unlock(&mlocks[i]);
	    assert(r==0);
	}
	gettimeofday(&end, 0);
	printf("pthread_mutex_unlock     took %9.3fus for %d ops:  %9.3fus/lock (%9.3fMops/s)\n", tdiff(&start,&end), K, tdiff(&start,&end)/K, K/tdiff(&start,&end));
    }

    for (j=0; j<3; j++) {
	for (i=0; i<K; i++) {
	    r=pthread_rwlock_init(&rwlocks[i], NULL);
	    assert(r==0);
	}
	gettimeofday(&start, 0);
	for (i=0; i<K; i++) {
	    r = pthread_rwlock_tryrdlock(&rwlocks[i]);
	    assert(r==0);
	}
	gettimeofday(&end, 0);
	printf("pthread_rwlock_tryrdlock took %9.3fus for %d ops:  %9.3fus/lock (%9.3fMops/s)\n", tdiff(&start,&end), K, tdiff(&start,&end)/K, K/tdiff(&start,&end));
    }

    for (j=0; j<3; j++) {
	for (i=0; i<K; i++) {
	    r=pthread_rwlock_init(&rwlocks[i], NULL);
	    assert(r==0);
	}
	gettimeofday(&start, 0);
	for (i=0; i<K; i++) {
	    r = pthread_rwlock_rdlock(&rwlocks[i]);
	    assert(r==0);
	}
	gettimeofday(&end, 0);
	printf("pthread_rwlock_rdlock    took %9.3fus for %d ops:  %9.3fus/lock (%9.3fMops/s)\n", tdiff(&start,&end), K, tdiff(&start,&end)/K, K/tdiff(&start,&end));
    }

    for (j=0; j<3; j++) {
	for (i=0; i<K; i++) {
	    blocks[i].state=0;
	    blocks[i].mutex=0;
	}
	gettimeofday(&start, 0);
	for (i=0; i<K; i++) {
	    brwl_rlock_xchg(&blocks[i]);
	}
	gettimeofday(&end, 0);
	printf("brwl_rlock_xchg          took %9.3fus for %d ops:  %9.3fus/lock (%9.3fMops/s)\n", tdiff(&start,&end), K, tdiff(&start,&end)/K, K/tdiff(&start,&end));
    }

    for (j=0; j<3; j++) {
	for (i=0; i<K; i++) {
	    blocks[i].state=0;
	    blocks[i].mutex=0;
	}
	gettimeofday(&start, 0);
	for (i=0; i<K; i++) {
	    brwl_rlock_fence(&blocks[i]);
	}
	gettimeofday(&end, 0);
	printf("brwl_rlock_fence         took %9.3fus for %d ops:  %9.3fus/lock (%9.3fMops/s)\n", tdiff(&start,&end), K, tdiff(&start,&end)/K, K/tdiff(&start,&end));
    }
    return 0;
}
