#define _MULTI_THREADED
#include <pthread.h>
#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/time.h>
#include <unistd.h>

float tdiff (struct timeval *start, struct timeval *end) {
    return 1e6*(end->tv_sec-start->tv_sec) +(end->tv_usec - start->tv_usec);
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

  struct timeval start, end;

  printf("Entered thread, getting read lock with mp wait\n");
  Retry:

  gettimeofday(&start, 0);
  rc = pthread_rwlock_tryrdlock(&rwlock);
  gettimeofday(&end, 0);
  printf("pthread_rwlock_tryrdlock took %9.3fus\n", tdiff(&start,&end));
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
  gettimeofday(&start, 0);
  rc = pthread_rwlock_unlock(&rwlock);
  gettimeofday(&end, 0);
  compResults("pthread_rwlock_unlock()\n", rc);
  printf("%lu.%6lu to %lu.%6lu is %9.2f\n", start.tv_sec, start.tv_usec, end.tv_sec, end.tv_usec, tdiff(&start, &end));

  printf("Secondary thread complete\n");
  return NULL;
}

int main(int argc __attribute__((unused)), char **argv)
{
  int                   rc=0;
  pthread_t             thread;
  struct timeval start, end;

  printf("Enter Testcase - %s\n", argv[0]);

  gettimeofday(&start, 0);
  gettimeofday(&end, 0);
  printf("nop Took %9.2f\n", tdiff(&start, &end));

  {
      int N=1000;
      int i;
      printf("Main, get and release the write lock %d times\n", N);
      gettimeofday(&start, 0);
      for (i=0; i<N; i++) {
	  rc = pthread_rwlock_wrlock(&rwlock);
	  rc = pthread_rwlock_unlock(&rwlock);
      }
      gettimeofday(&end, 0);
      compResults("pthread_rwlock_wrlock()\n", rc);
      printf("Took %9.2fns/op\n", 1000*tdiff(&start, &end)/N);
  }

  printf("Main, get the write lock\n");
  gettimeofday(&start, 0);
  rc = pthread_rwlock_wrlock(&rwlock);
  gettimeofday(&end, 0);
  compResults("pthread_rwlock_wrlock()\n", rc);
  printf("Took %9.2f\n", tdiff(&start, &end));

  printf("Main, create the try read lock thread\n");
  rc = pthread_create(&thread, NULL, rdlockThread, NULL);
  compResults("pthread_create\n", rc);

  printf("Main, wait a bit holding the write lock\n");
  sleep(5);

  printf("Main, Now unlock the write lock\n");
  gettimeofday(&start, 0);
  rc = pthread_rwlock_unlock(&rwlock);
  gettimeofday(&end, 0);
  compResults("pthread_rwlock_unlock()\n", rc);
  printf("Took %9.2f\n", tdiff(&start, &end));

  printf("Main, wait for the thread to end\n");
  rc = pthread_join(thread, NULL);
  compResults("pthread_join\n", rc);

  rc = pthread_rwlock_destroy(&rwlock);
  compResults("pthread_rwlock_destroy()\n", rc);
  printf("Main completed\n");
  return 0;
}
