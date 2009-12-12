/* Copyright (C) 2000 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/*****************************************************************************
** Simulation of posix threads calls for WIN95 and NT
*****************************************************************************/

/* SAFE_MUTEX will not work until the thread structure is up to date */
#undef SAFE_MUTEX

#include "mysys_priv.h"
#if defined(THREAD) && defined(__WIN__)
#include <m_string.h>
#undef getpid
#include <process.h>

static pthread_mutex_t THR_LOCK_thread;

struct pthread_map
{
  HANDLE pthreadself;
  pthread_handler func;
  void *param;
};

void win_pthread_init(void)
{
  pthread_mutex_init(&THR_LOCK_thread,MY_MUTEX_INIT_FAST);
}


/**
   Adapter to @c pthread_mutex_trylock()

   @retval 0      Mutex was acquired
   @retval EBUSY  Mutex was already locked by a thread
 */
int
win_pthread_mutex_trylock(pthread_mutex_t *mutex)
{
  if (TryEnterCriticalSection(mutex))
  {
    /* Don't allow recursive lock */
    if (mutex->RecursionCount > 1){
      LeaveCriticalSection(mutex);
      return EBUSY;
    }
    return 0;
  }
  return EBUSY;
}


/*
** We have tried to use '_beginthreadex' instead of '_beginthread' here
** but in this case the program leaks about 512 characters for each
** created thread !
** As we want to save the created thread handler for other threads to
** use and to be returned by pthread_self() (instead of the Win32 pseudo
** handler), we have to go trough pthread_start() to catch the returned handler
** in the new thread.
*/

pthread_handler_t pthread_start(void *param)
{
  pthread_handler func=((struct pthread_map *) param)->func;
  void *func_param=((struct pthread_map *) param)->param;
  my_thread_init();			/* Will always succeed in windows */
  pthread_mutex_lock(&THR_LOCK_thread);	  /* Wait for beginthread to return */
  win_pthread_self=((struct pthread_map *) param)->pthreadself;
  pthread_mutex_unlock(&THR_LOCK_thread);
  free((char*) param);			  /* Free param from create */
  pthread_exit((void*) (*func)(func_param));
  return 0;				  /* Safety */
}


int pthread_create(pthread_t *thread_id, pthread_attr_t *attr,
		   pthread_handler func, void *param)
{
  HANDLE hThread;
  struct pthread_map *map;
  DBUG_ENTER("pthread_create");

  if (!(map=malloc(sizeof(*map))))
    DBUG_RETURN(-1);
  map->func=func;
  map->param=param;
  pthread_mutex_lock(&THR_LOCK_thread);
#ifdef __BORLANDC__
  hThread=(HANDLE)_beginthread((void(_USERENTRY *)(void *)) pthread_start,
			       attr->dwStackSize ? attr->dwStackSize :
			       65535, (void*) map);
#else
  hThread=(HANDLE)_beginthread((void( __cdecl *)(void *)) pthread_start,
			       attr->dwStackSize ? attr->dwStackSize :
			       65535, (void*) map);
#endif
  DBUG_PRINT("info", ("hThread=%lu",(long) hThread));
  *thread_id=map->pthreadself=hThread;
  pthread_mutex_unlock(&THR_LOCK_thread);

  if (hThread == (HANDLE) -1)
  {
    int error=errno;
    DBUG_PRINT("error",
	       ("Can't create thread to handle request (error %d)",error));
    DBUG_RETURN(error ? error : -1);
  }
  VOID(SetThreadPriority(hThread, attr->priority)) ;
  DBUG_RETURN(0);
}


void pthread_exit(void *a)
{
  _endthread();
}

/* This is neaded to get the macro pthread_setspecific to work */

int win_pthread_setspecific(void *a,void *b,uint length)
{
  memcpy(a,b,length);
  return 0;
}


/*
 One time initialization. For simplicity, we assume initializer thread
 does not exit within init_routine().
*/
int my_pthread_once(my_pthread_once_t *once_control, 
    void (*init_routine)(void))
{
  LONG state= InterlockedCompareExchange(once_control, MY_PTHREAD_ONCE_INPROGRESS,
                                          MY_PTHREAD_ONCE_INIT);
  switch(state)
  {
  case MY_PTHREAD_ONCE_INIT:
    /* This is initializer thread */
    (*init_routine)();
    *once_control= MY_PTHREAD_ONCE_DONE;
    break;

  case MY_PTHREAD_ONCE_INPROGRESS:
    /* init_routine in progress. Wait for its completion */
    while(*once_control == MY_PTHREAD_ONCE_INPROGRESS)
    {
      Sleep(1);
    }
    break;
  case MY_PTHREAD_ONCE_DONE:
    /* Nothing to do */
    break;
  }
  return 0;
}

#endif
