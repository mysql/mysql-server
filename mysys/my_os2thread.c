/* Copyright (C) Yuri Dario & 2000 MySQL AB
   All the above parties has a full, independent copyright to
   the following code, including the right to use the code in
   any manner without any demands from the other parties.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA */

/*****************************************************************************
** Simulation of posix threads calls for OS/2
*****************************************************************************/

#include "mysys_priv.h"
#if defined(THREAD) && defined(OS2)
#include <m_string.h>
#include <process.h>

static pthread_mutex_t THR_LOCK_thread;

struct pthread_map
{
  HANDLE          pthreadself;
  pthread_handler func;
  void *          param;
};

void win_pthread_init(void)
{
  pthread_mutex_init(&THR_LOCK_thread,NULL);
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

static pthread_handler_decl(pthread_start,param)
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

  if (!(map=(struct pthread_map *)malloc(sizeof(*map))))
    DBUG_RETURN(-1);
  map->func=func;
  map->param=param;
  pthread_mutex_lock(&THR_LOCK_thread);
#ifdef __BORLANDC__
  hThread=(HANDLE)_beginthread((void(_USERENTRY *)(void *)) pthread_start,
			       attr->dwStackSize ? attr->dwStackSize :
			       65535, (void*) map);
#elif defined( OS2)
  hThread=(HANDLE)_beginthread((void( _Optlink *)(void *)) pthread_start, NULL,
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
#ifdef OS2
  my_pthread_setprio(hThread, attr->priority);
#else
  VOID(SetThreadPriority(hThread, attr->priority)) ;
#endif
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

#endif
