/*
  This is written by Sergei Golubchik for MySQL AB and is in public domain.

  Simple implementation of semaphores, needed to compile MySQL with
  MIT-pthreads.
*/

typedef struct {
  pthread_mutex_t mutex;
  pthread_cond_t  cond;
  uint            count;
} sem_t;

int sem_init(sem_t * sem, int pshared, uint value);
int sem_destroy(sem_t * sem);
int sem_wait(sem_t * sem);
int sem_trywait(sem_t * sem);
int sem_post (sem_t * sem);
int sem_post_multiple(sem_t * sem, uint count);
int sem_getvalue (sem_t * sem, uint *sval);
