/* Testing of connecting to MySQL from X threads */

#include <windows.h>
#include <process.h>
#include <stdio.h>
#include <mysql.h>
#include <errno.h>

#define TEST_COUNT 20

/*****************************************************************************
** The following is to emulate the posix thread interface
*****************************************************************************/

typedef HANDLE  pthread_t;
typedef struct thread_attr {
	DWORD	dwStackSize ;
	DWORD	dwCreatingFlag ;
	int	priority ;
} pthread_attr_t ;

typedef struct { int dummy; } pthread_condattr_t;
typedef unsigned int uint;

typedef struct {
  uint waiting;
  HANDLE semaphore;
} pthread_cond_t;

typedef CRITICAL_SECTION pthread_mutex_t;

#define pthread_mutex_init(A,B)  InitializeCriticalSection(A)
#define pthread_mutex_lock(A)    (EnterCriticalSection(A),0)
#define pthread_mutex_unlock(A)  LeaveCriticalSection(A)
#define pthread_mutex_destroy(A) DeleteCriticalSection(A)
#define pthread_handler_t unsigned __cdecl *
typedef unsigned (__cdecl *pthread_handler)(void *);
#define pthread_self() GetCurrentThread()

static unsigned int thread_count;
static pthread_cond_t COND_thread_count;
static pthread_mutex_t LOCK_thread_count;

pthread_mutex_t THR_LOCK_malloc,THR_LOCK_open,THR_LOCK_keycache,
		THR_LOCK_lock,THR_LOCK_isam;

/*
** We have tried to use '_beginthreadex' instead of '_beginthread' here
** but in this case the program leaks about 512 characters for each
** created thread !
*/

int pthread_create(pthread_t *thread_id, pthread_attr_t *attr,
		   pthread_handler func, void *param)
{
  HANDLE hThread;
 
  hThread=(HANDLE)_beginthread(func,
			       attr->dwStackSize ? attr->dwStackSize :
			       65535,param);
  if ((long) hThread == -1L)
  {
    return(errno ? errno : -1);
  }
  *thread_id=hThread;
  return(0);
}

void pthread_exit(unsigned A)
{
  _endthread();
}

/*
** The following simple implementation of conds works as long as
** only one thread uses pthread_cond_wait at a time.
** This is coded very carefully to work with thr_lock.
*/

/*****************************************************************************
** The following is a simple implementation of posix conditions
*****************************************************************************/

int pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr)
{
  cond->waiting=0;
  cond->semaphore=CreateSemaphore(NULL,0,0x7FFFFFFF,(char*) 0);
  if (!cond->semaphore)
    return ENOMEM;
  return 0;
}

int pthread_cond_destroy(pthread_cond_t *cond)
{
	return CloseHandle(cond->semaphore) ? 0 : EINVAL;
}

int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex)
{
  InterlockedIncrement(&cond->waiting);
  LeaveCriticalSection(mutex);
  WaitForSingleObject(cond->semaphore,INFINITE);
  InterlockedDecrement(&cond->waiting);
  EnterCriticalSection(mutex);
  return 0 ;
}

int pthread_cond_signal(pthread_cond_t *cond)
{
  long prev_count;
  if (cond->waiting)
    ReleaseSemaphore(cond->semaphore,1,&prev_count);
  return 0;
}

int pthread_attr_init(pthread_attr_t *connect_att)
{
  connect_att->dwStackSize	= 0;
  connect_att->dwCreatingFlag	= 0;
  connect_att->priority		= 0;
  return 0;
}

int pthread_attr_setstacksize(pthread_attr_t *connect_att,DWORD stack)
{
  connect_att->dwStackSize=stack;
  return 0;
}

int pthread_attr_setprio(pthread_attr_t *connect_att,int priority)
{
  connect_att->priority=priority;
  return 0;
}

int pthread_attr_destroy(pthread_attr_t *connect_att)
{
  return 0;
}

/* from my_pthread.c */

#ifndef REMOVE_BUG

__declspec(thread) int THR_KEY_my_errno;

int _my_errno(void)
{
  return THR_KEY_my_errno;
}
#endif


/*****************************************************************************
** The test program
*****************************************************************************/

pthread_handler_t test_thread(void *arg)
{
  MYSQL mysql;
  MYSQL_RES *res;

  mysql_init(&mysql);
  if (!mysql_real_connect(&mysql,NULL,0,0,NULL,0,NULL,0))
  {
    fprintf(stderr,"Couldn't connect to engine!\n%s\n\n",mysql_error(&mysql));
    perror("");
    goto end;
  }
  mysql.reconnect= 1;
  if (mysql_query(&mysql,"select 1") < 0)
  {
    fprintf(stderr,"Query failed (%s)\n",mysql_error(&mysql));
    goto end;
  }
  if (!(res=mysql_store_result(&mysql)))
  {
    fprintf(stderr,"Couldn't get result from query failed\n",
	    mysql_error(&mysql));
    goto end;
  }
  mysql_free_result(res);

end:

  Sleep(1000); /* Win32 sleep */
  mysql_close(&mysql);

  pthread_mutex_lock(&LOCK_thread_count);
  thread_count--;
  pthread_cond_signal(&COND_thread_count); /* Tell main we are ready */
  pthread_mutex_unlock(&LOCK_thread_count);
  pthread_exit(0);
  return 0;
}

int main(int argc,char **argv)
{
  pthread_t tid;
  pthread_attr_t thr_attr;
  int i,error;

  if ((error=pthread_cond_init(&COND_thread_count,NULL)))
  {
    fprintf(stderr,"Got error: %d from pthread_cond_init (errno: %d)",
	    error,errno);
    exit(1);
  }
  pthread_mutex_init(&LOCK_thread_count,NULL);
  if ((error=pthread_attr_init(&thr_attr)))
  {
    fprintf(stderr,"Got error: %d from pthread_attr_init (errno: %d)",
	    error,errno);
    exit(1);
  }
  if ((error=pthread_attr_setstacksize(&thr_attr,65536L)))
  {
    fprintf(stderr,"Got error: %d from pthread_attr_setstacksize (errno: %d)",
	    error,errno);
    exit(1);
  }

  printf("Init ok. Creating %d threads\n",TEST_COUNT);

  for (i=1 ; i <= TEST_COUNT ; i++)
  {
    int *param= &i;
    if ((error=pthread_mutex_lock(&LOCK_thread_count)))
    {
      fprintf(stderr,"\nGot error: %d from pthread_mutex_lock (errno: %d)",
	      error,errno);
      exit(1);
    }
    if ((error=pthread_create(&tid,&thr_attr,test_thread,(void*) param)))
    {
      fprintf(stderr,"\nGot error: %d from pthread_create (errno: %d)\n",
	      error,errno);
      pthread_mutex_unlock(&LOCK_thread_count);
      exit(1);
    }
    thread_count++;
    pthread_mutex_unlock(&LOCK_thread_count);
  }

  if ((error=pthread_mutex_lock(&LOCK_thread_count)))
    fprintf(stderr,"\nGot error: %d from pthread_mutex_lock\n",error);
  while (thread_count)
  {
    if ((error=pthread_cond_wait(&COND_thread_count,&LOCK_thread_count)))
      fprintf(stderr,"\nGot error: %d from pthread_cond_wait\n",error);
  }
  pthread_mutex_unlock(&LOCK_thread_count);
  pthread_attr_destroy(&thr_attr);
  printf("\nend\n");
  return 0;
}
