/* Test pthread rwlocks in multiprocess environment. */

/* How expensive is
 *   - Obtaining a read-only lock for the first obtainer.
 *   - Obtaining it for the second one?
 *   - The third one? */

#include <toku_assert.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>


float tdiff (struct timeval *start, struct timeval *end) {
    return 1e6*(end->tv_sec-start->tv_sec) +(end->tv_usec - start->tv_usec);
}

#define FILE "process.data"

int main (int argc __attribute__((__unused__)), char *argv[] __attribute__((__unused__))) {
    int r;
    int fd;
    void *p;
    fd=open(FILE, O_CREAT|O_RDWR|O_TRUNC, 0666); assert(fd>=0);
    int i;
    for (i=0; i<4096; i++) {
	r=write(fd, "\000", 1);
	assert(r==1);
    }
    p=mmap(0, 4096, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (p==MAP_FAILED) {
	printf("err=%d %s (EPERM=%d)\n", errno, strerror(errno), EPERM);
    }
    assert(p!=MAP_FAILED);
    r=close(fd); assert(r==0);

    pthread_rwlockattr_t attr;
    pthread_rwlock_t *lock=p;
    r=pthread_rwlockattr_init(&attr);                               assert(r==0);
    r=pthread_rwlockattr_setpshared(&attr, PTHREAD_PROCESS_SHARED); assert(r==0);
    r=pthread_rwlock_init(lock, &attr);                             assert(r==0);
    r=pthread_rwlock_init(lock+1, &attr);                             assert(r==0);

    r=pthread_rwlock_wrlock(lock);

    pid_t pid;
    if ((pid=fork())==0) {
	// I'm the child
	r = munmap(p, 4096); assert(r==0);
	fd = open(FILE, O_RDWR, 0666); assert(fd>=0);
	p=mmap(0, 4096, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	assert(p!=MAP_FAILED);
	r=close(fd); assert(r==0);
	
	printf("A0\n");
	r=pthread_rwlock_wrlock(lock);
	printf("C\n");
	sleep(1);
    
	r=pthread_rwlock_unlock(lock);
	printf("D\n");

	r=pthread_rwlock_rdlock(lock);
	printf("E0\n");
	sleep(1);

    } else {
	printf("A1\n");
	sleep(1);
	printf("B\n");
	r=pthread_rwlock_unlock(lock); // release the lock grabbed before the fork
	assert(r==0);

	sleep(1);
	r=pthread_rwlock_rdlock(lock);
	assert(r==0);
	printf("E1\n");
	sleep(1);

	int status;
	pid_t waited=wait(&status);
	assert(waited==pid);
    }
    return 0;    
    
    
#if 0    

    int j;
    int i;
    int r;
    struct timeval start, end;
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
	    brwl_rlock(&blocks[i]);
	}
	gettimeofday(&end, 0);
	printf("brwl_rlock               took %9.3fus for %d ops:  %9.3fus/lock (%9.3fMops/s)\n", tdiff(&start,&end), K, tdiff(&start,&end)/K, K/tdiff(&start,&end));
    }
    return 0;
#endif
}
