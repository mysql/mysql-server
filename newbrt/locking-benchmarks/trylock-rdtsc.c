/* Like trylock, except use rdstc */
#define _MULTI_THREADED
#include <pthread.h>
#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/time.h>
#include <unistd.h>
#include <rdtsc.h>

float tdiff (struct timeval *start, struct timeval *end) {
    return 1e6*(end->tv_sec-start->tv_sec) +(end->tv_usec - start->tv_usec);
}

unsigned long long rtdiff (unsigned long long a, unsigned long long b) {
    return (b-a);
}

/* Simple function to check the return code and exit the program
   if the function call failed
   */
static void compResults(char *string, int rc) {
  if (rc) {
    printf("Error on : %s, rc=%d",
           string, rc);
    exit(EXIT_FAILURE);
  }
  return;
}

pthread_rwlock_t       rwlock = PTHREAD_RWLOCK_INITIALIZER;

void *rdlockThread(void *arg __attribute__((unused)))
{
  int             rc;
  int             count=0;

  unsigned long long t_start, t_end;

  printf("Entered thread, getting read lock with mp wait\n");
  Retry:

  t_start = rdtsc();
  rc = pthread_rwlock_tryrdlock(&rwlock);
  t_end = rdtsc();
  printf("pthread_rwlock_tryrdlock took %llu clocks\n", rtdiff(t_start,t_end));
  if (rc == EBUSY) {
    if (count >= 10) {
      printf("Retried too many times, failure!\n");

      exit(EXIT_FAILURE);
    }
    ++count;
    printf("Could not get lock, do other work, then RETRY...\n");
    sleep(1);
    goto Retry;
  }
  compResults("pthread_rwlock_tryrdlock() 1\n", rc);

  sleep(2);

  printf("unlock the read lock\n");
  t_start = rdtsc();
  rc = pthread_rwlock_unlock(&rwlock);
  t_end = rdtsc();
  compResults("pthread_rwlock_unlock()\n", rc);
  printf("Took %llu clocks\n", rtdiff(t_start, t_end));

  printf("Secondary thread complete\n");
  return NULL;
}

int main(int argc __attribute__((unused)), char **argv)
{
  int                   rc=0;
  pthread_t             thread;
  unsigned long long t_start, t_end;

  printf("Enter Testcase - %s\n", argv[0]);

  t_start = rdtsc();
  t_end   = rdtsc();
  printf("nop Took %llu clocks\n", rtdiff(t_start, t_end));

  {
      int N=1000;
      int i;
      printf("Main, get and release the write lock %d times\n", N);
      t_start = rdtsc();
      for (i=0; i<N; i++) {
	  rc = pthread_rwlock_wrlock(&rwlock);
	  rc = pthread_rwlock_unlock(&rwlock);
      }
      t_end = rdtsc();
      compResults("pthread_rwlock_wrlock()\n", rc);
      printf("Took %5.2f clocks/op\n", ((double)(t_end-t_start))/N);
  }

  printf("Main, get the write lock\n");
  t_start = rdtsc();
  rc = pthread_rwlock_wrlock(&rwlock);
  t_end   = rdtsc();
  compResults("pthread_rwlock_wrlock()\n", rc);
  printf("Took %llu clocks\n", rtdiff(t_start, t_end));

  printf("Main, create the try read lock thread\n");
  rc = pthread_create(&thread, NULL, rdlockThread, NULL);
  compResults("pthread_create\n", rc);

  printf("Main, wait a bit holding the write lock\n");
  sleep(5);

  printf("Main, Now unlock the write lock\n");
  t_start = rdtsc();
  rc = pthread_rwlock_unlock(&rwlock);
  t_end   = rdtsc();
  compResults("pthread_rwlock_unlock()\n", rc);
  printf("Took %llu clocks\n", rtdiff(t_start, t_end));

  printf("Main, wait for the thread to end\n");
  rc = pthread_join(thread, NULL);
  compResults("pthread_join\n", rc);

  rc = pthread_rwlock_destroy(&rwlock);
  compResults("pthread_rwlock_destroy()\n", rc);
  printf("Main completed\n");

  
  {
      static int lock_for_lock_and_unlock;
      t_start = rdtsc();
      (void)__sync_lock_test_and_set(&lock_for_lock_and_unlock, 1);
      t_end   = rdtsc();
      printf("sync_lock_test_and_set took %llu clocks\n", t_end-t_start);

      t_start = rdtsc();
      __sync_lock_release(&lock_for_lock_and_unlock);
      t_end   = rdtsc();
      printf("sync_lock_release      took %llu clocks\n", t_end-t_start);
  }


  {
      t_start = rdtsc();
      (void)__sync_synchronize();
      t_end   = rdtsc();
      printf("sync_synchornize took %llu clocks\n", t_end-t_start);
  }

  t_start = rdtsc();
  sleep(1);
  t_end   = rdtsc();
  printf("sleep(1) took %llu clocks\n", t_end-t_start);
  return 0;
}
