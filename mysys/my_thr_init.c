/* Copyright (C) 2000 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/*
  Functions to handle initializating and allocationg of all mysys & debug
  thread variables.
*/

#include "mysys_priv.h"
#include <m_string.h>

#ifdef THREAD
#ifdef USE_TLS
pthread_key(struct st_my_thread_var*, THR_KEY_mysys);
#else
pthread_key(struct st_my_thread_var, THR_KEY_mysys);
#endif /* USE_TLS */
pthread_mutex_t THR_LOCK_malloc,THR_LOCK_open,
	        THR_LOCK_lock,THR_LOCK_isam,THR_LOCK_myisam,THR_LOCK_heap,
	        THR_LOCK_net, THR_LOCK_charset; 
#if !defined(HAVE_LOCALTIME_R) || !defined(HAVE_GMTIME_R)
pthread_mutex_t LOCK_localtime_r;
#endif
#ifndef HAVE_GETHOSTBYNAME_R
pthread_mutex_t LOCK_gethostbyname_r;
#endif
#ifdef PTHREAD_ADAPTIVE_MUTEX_INITIALIZER_NP
pthread_mutexattr_t my_fast_mutexattr;
#endif
#ifdef PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP
pthread_mutexattr_t my_errchk_mutexattr;
#endif

/*
  initialize thread environment

  SYNOPSIS
    my_thread_global_init()

  RETURN
    0  ok
    1  error (Couldn't create THR_KEY_mysys)
*/

my_bool my_thread_global_init(void)
{
  if (pthread_key_create(&THR_KEY_mysys,0))
  {
    fprintf(stderr,"Can't initialize threads: error %d\n",errno);
    return 1;
  }
#ifdef PTHREAD_ADAPTIVE_MUTEX_INITIALIZER_NP
  pthread_mutexattr_init(&my_fast_mutexattr);
  /*
    Note that the following statement may give a compiler warning under
    some configurations, but there isn't anything we can do about this as
    this is a bug in the header files for the thread implementation
  */
  pthread_mutexattr_setkind_np(&my_fast_mutexattr,PTHREAD_MUTEX_ADAPTIVE_NP);
#endif
#ifdef PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP
  pthread_mutexattr_init(&my_errchk_mutexattr);
  pthread_mutexattr_setkind_np(&my_errchk_mutexattr,
			       PTHREAD_MUTEX_ERRORCHECK_NP);
#endif

  pthread_mutex_init(&THR_LOCK_malloc,MY_MUTEX_INIT_FAST);
  pthread_mutex_init(&THR_LOCK_open,MY_MUTEX_INIT_FAST);
  pthread_mutex_init(&THR_LOCK_lock,MY_MUTEX_INIT_FAST);
  pthread_mutex_init(&THR_LOCK_isam,MY_MUTEX_INIT_SLOW);
  pthread_mutex_init(&THR_LOCK_myisam,MY_MUTEX_INIT_SLOW);
  pthread_mutex_init(&THR_LOCK_heap,MY_MUTEX_INIT_FAST);
  pthread_mutex_init(&THR_LOCK_net,MY_MUTEX_INIT_FAST);
  pthread_mutex_init(&THR_LOCK_charset,MY_MUTEX_INIT_FAST);
#if defined( __WIN__) || defined(OS2)
  win_pthread_init();
#endif
#if !defined(HAVE_LOCALTIME_R) || !defined(HAVE_GMTIME_R)
  pthread_mutex_init(&LOCK_localtime_r,MY_MUTEX_INIT_SLOW);
#endif
#ifndef HAVE_GETHOSTBYNAME_R
  pthread_mutex_init(&LOCK_gethostbyname_r,MY_MUTEX_INIT_SLOW);
#endif
  if (my_thread_init())
  {
    my_thread_global_end();			/* Clean up */
    return 1;
  }
  return 0;
}


void my_thread_global_end(void)
{
  pthread_key_delete(THR_KEY_mysys);
#ifdef PTHREAD_ADAPTIVE_MUTEX_INITIALIZER_NP
  pthread_mutexattr_destroy(&my_fast_mutexattr);
#endif
#ifdef PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP
  pthread_mutexattr_destroy(&my_errchk_mutexattr);
#endif
  pthread_mutex_destroy(&THR_LOCK_malloc);
  pthread_mutex_destroy(&THR_LOCK_open);
  pthread_mutex_destroy(&THR_LOCK_lock);
  pthread_mutex_destroy(&THR_LOCK_isam);
  pthread_mutex_destroy(&THR_LOCK_myisam);
  pthread_mutex_destroy(&THR_LOCK_heap);
  pthread_mutex_destroy(&THR_LOCK_net);
  pthread_mutex_destroy(&THR_LOCK_charset);
#if !defined(HAVE_LOCALTIME_R) || !defined(HAVE_GMTIME_R)
  pthread_mutex_destroy(&LOCK_localtime_r);
#endif
#ifndef HAVE_GETHOSTBYNAME_R
  pthread_mutex_destroy(&LOCK_gethostbyname_r);
#endif
}

static long thread_id=0;

/*
  We can't use mutex_locks here if we are using windows as
  we may have compiled the program with SAFE_MUTEX, in which
  case the checking of mutex_locks will not work until
  the pthread_self thread specific variable is initialized.
*/

my_bool my_thread_init(void)
{
  struct st_my_thread_var *tmp;
  my_bool error=0;

#ifdef EXTRA_DEBUG_THREADS
  fprintf(stderr,"my_thread_init(): thread_id=%ld\n",pthread_self());
#endif  
#if !defined(__WIN__) || defined(USE_TLS) || ! defined(SAFE_MUTEX)
  pthread_mutex_lock(&THR_LOCK_lock);
#endif

#if !defined(__WIN__) || defined(USE_TLS)
  if (my_pthread_getspecific(struct st_my_thread_var *,THR_KEY_mysys))
  {
#ifdef EXTRA_DEBUG_THREADS
    fprintf(stderr,"my_thread_init() called more than once in thread %ld\n",
	        pthread_self());
#endif    
    goto end;
  }
  if (!(tmp= (struct st_my_thread_var *) calloc(1, sizeof(*tmp))))
  {
    error= 1;
    goto end;
  }
  pthread_setspecific(THR_KEY_mysys,tmp);

#else
  /*
    Skip initialization if the thread specific variable is already initialized
  */
  if (THR_KEY_mysys.id)
    goto end;
  tmp= &THR_KEY_mysys;
#endif
  tmp->id= ++thread_id;
#if defined(__WIN__) && defined(EMBEDDED_LIBRARY)
  tmp->thread_self= (pthread_t)getpid();
#endif
  pthread_mutex_init(&tmp->mutex,MY_MUTEX_INIT_FAST);
  pthread_cond_init(&tmp->suspend, NULL);
  tmp->init= 1;

end:
#if !defined(__WIN__) || defined(USE_TLS) || ! defined(SAFE_MUTEX)
  pthread_mutex_unlock(&THR_LOCK_lock);
#endif
  return error;
}


void my_thread_end(void)
{
  struct st_my_thread_var *tmp;
  tmp= my_pthread_getspecific(struct st_my_thread_var*,THR_KEY_mysys);

#ifdef EXTRA_DEBUG_THREADS
  fprintf(stderr,"my_thread_end(): tmp=%p,thread_id=%ld\n",
	  tmp,pthread_self());
#endif  
  if (tmp && tmp->init)
  {
#if !defined(DBUG_OFF)
    /* tmp->dbug is allocated inside DBUG library */
    if (tmp->dbug)
    {
      free(tmp->dbug);
      tmp->dbug=0;
    }
#endif
#if !defined(__bsdi__) && !defined(__OpenBSD__) || defined(HAVE_mit_thread)
 /* bsdi and openbsd 3.5 dumps core here */
    pthread_cond_destroy(&tmp->suspend);
#endif
    pthread_mutex_destroy(&tmp->mutex);
#if (!defined(__WIN__) && !defined(OS2)) || defined(USE_TLS)
    free(tmp);
#else
    tmp->init= 0;
#endif
  }
  /* The following free has to be done, even if my_thread_var() is 0 */
#if (!defined(__WIN__) && !defined(OS2)) || defined(USE_TLS)
  pthread_setspecific(THR_KEY_mysys,0);
#endif
}

struct st_my_thread_var *_my_thread_var(void)
{
  struct st_my_thread_var *tmp=
    my_pthread_getspecific(struct st_my_thread_var*,THR_KEY_mysys);
#if defined(USE_TLS)
  /* This can only happen in a .DLL */
  if (!tmp)
  {
    my_thread_init();
    tmp=my_pthread_getspecific(struct st_my_thread_var*,THR_KEY_mysys);
  }
#endif
  return tmp;
}


/****************************************************************************
  Get name of current thread.
****************************************************************************/

#define UNKNOWN_THREAD -1

long my_thread_id()
{
#if defined(HAVE_PTHREAD_GETSEQUENCE_NP)
  return pthread_getsequence_np(pthread_self());
#elif (defined(__sun) || defined(__sgi) || defined(__linux__)) && !defined(HAVE_mit_thread)
  return pthread_self();
#else
  return my_thread_var->id;
#endif
}

#ifdef DBUG_OFF
const char *my_thread_name(void)
{
  return "no_name";
}

#else

const char *my_thread_name(void)
{
  char name_buff[100];
  struct st_my_thread_var *tmp=my_thread_var;
  if (!tmp->name[0])
  {
    long id=my_thread_id();
    sprintf(name_buff,"T@%ld", id);
    strmake(tmp->name,name_buff,THREAD_NAME_SIZE);
  }
  return tmp->name;
}
#endif /* DBUG_OFF */

#endif /* THREAD */
