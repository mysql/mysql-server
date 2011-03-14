/* Copyright (C) 2000 MySQL AB, 2008-2009 Sun Microsystems, Inc

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

/*
  Functions to handle initializating and allocationg of all mysys & debug
  thread variables.
*/

#include "mysys_priv.h"
#include <m_string.h>
#include <signal.h>

pthread_key(struct st_my_thread_var*, THR_KEY_mysys);
mysql_mutex_t THR_LOCK_malloc, THR_LOCK_open,
              THR_LOCK_lock, THR_LOCK_isam, THR_LOCK_myisam, THR_LOCK_heap,
              THR_LOCK_net, THR_LOCK_charset, THR_LOCK_threads, THR_LOCK_time,
              THR_LOCK_myisam_mmap;

mysql_cond_t  THR_COND_threads;
uint            THR_thread_count= 0;
uint 		my_thread_end_wait_time= 5;
#if !defined(HAVE_LOCALTIME_R) || !defined(HAVE_GMTIME_R)
mysql_mutex_t LOCK_localtime_r;
#endif
#ifndef HAVE_GETHOSTBYNAME_R
mysql_mutex_t LOCK_gethostbyname_r;
#endif
#ifdef PTHREAD_ADAPTIVE_MUTEX_INITIALIZER_NP
pthread_mutexattr_t my_fast_mutexattr;
#endif
#ifdef PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP
pthread_mutexattr_t my_errorcheck_mutexattr;
#endif
#ifdef _MSC_VER
static void install_sigabrt_handler();
#endif
#ifdef TARGET_OS_LINUX

/*
  Dummy thread spawned in my_thread_global_init() below to avoid
  race conditions in NPTL pthread_exit code.
*/

static pthread_handler_t
nptl_pthread_exit_hack_handler(void *arg __attribute((unused)))
{
  /* Do nothing! */
  pthread_exit(0);
  return 0;
}

#endif /* TARGET_OS_LINUX */


static uint get_thread_lib(void);

/** True if @c my_thread_basic_global_init() has been called. */
static my_bool my_thread_basic_global_init_done= 0;

/**
  Perform a minimal initialisation of mysys, when compiled with threads.
  The initialisation performed is sufficient to:
  - allocate memory
  - perform file operations
  - use charsets
  - use my_errno
  @sa my_basic_init
  @sa my_thread_basic_global_reinit
*/
my_bool my_thread_basic_global_init(void)
{
  int pth_ret;

  if (my_thread_basic_global_init_done)
    return 0;
  my_thread_basic_global_init_done= 1;

#ifdef PTHREAD_ADAPTIVE_MUTEX_INITIALIZER_NP
  /*
    Set mutex type to "fast" a.k.a "adaptive"

    In this case the thread may steal the mutex from some other thread
    that is waiting for the same mutex.  This will save us some
    context switches but may cause a thread to 'starve forever' while
    waiting for the mutex (not likely if the code within the mutex is
    short).
  */
  pthread_mutexattr_init(&my_fast_mutexattr);
  pthread_mutexattr_settype(&my_fast_mutexattr,
                            PTHREAD_MUTEX_ADAPTIVE_NP);
#endif

#ifdef PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP
  /*
    Set mutex type to "errorcheck"
  */
  pthread_mutexattr_init(&my_errorcheck_mutexattr);
  pthread_mutexattr_settype(&my_errorcheck_mutexattr,
                            PTHREAD_MUTEX_ERRORCHECK);
#endif

  mysql_mutex_init(key_THR_LOCK_malloc, &THR_LOCK_malloc, MY_MUTEX_INIT_FAST);
  mysql_mutex_init(key_THR_LOCK_open, &THR_LOCK_open, MY_MUTEX_INIT_FAST);
  mysql_mutex_init(key_THR_LOCK_charset, &THR_LOCK_charset, MY_MUTEX_INIT_FAST);
  mysql_mutex_init(key_THR_LOCK_threads, &THR_LOCK_threads, MY_MUTEX_INIT_FAST);

  if ((pth_ret= pthread_key_create(&THR_KEY_mysys, NULL)) != 0)
  {
    fprintf(stderr, "Can't initialize threads: error %d\n", pth_ret);
    return 1;
  }

  if (my_thread_init())
    return 1;

  return 0;
}

/**
  Re-initialize components initialized early with @c my_thread_basic_global_init.
  Some mutexes were initialized before the instrumentation.
  Destroy + create them again, now that the instrumentation
  is in place.
  This is safe, since this function() is called before creating new threads,
  so the mutexes are not in use.
*/
void my_thread_basic_global_reinit(void)
{
  struct st_my_thread_var *tmp;

  DBUG_ASSERT(my_thread_basic_global_init_done);

#ifdef HAVE_PSI_INTERFACE
  my_init_mysys_psi_keys();
#endif

  mysql_mutex_destroy(&THR_LOCK_malloc);
  mysql_mutex_init(key_THR_LOCK_malloc, &THR_LOCK_malloc, MY_MUTEX_INIT_FAST);

  mysql_mutex_destroy(&THR_LOCK_open);
  mysql_mutex_init(key_THR_LOCK_open, &THR_LOCK_open, MY_MUTEX_INIT_FAST);

  mysql_mutex_destroy(&THR_LOCK_charset);
  mysql_mutex_init(key_THR_LOCK_charset, &THR_LOCK_charset, MY_MUTEX_INIT_FAST);

  mysql_mutex_destroy(&THR_LOCK_threads);
  mysql_mutex_init(key_THR_LOCK_threads, &THR_LOCK_threads, MY_MUTEX_INIT_FAST);

  tmp= my_pthread_getspecific(struct st_my_thread_var*, THR_KEY_mysys);
  DBUG_ASSERT(tmp);

  mysql_mutex_destroy(&tmp->mutex);
  mysql_mutex_init(key_my_thread_var_mutex, &tmp->mutex, MY_MUTEX_INIT_FAST);

  mysql_cond_destroy(&tmp->suspend);
  mysql_cond_init(key_my_thread_var_suspend, &tmp->suspend, NULL);
}

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
  if (my_thread_basic_global_init())
    return 1;

  thd_lib_detected= get_thread_lib();

#ifdef TARGET_OS_LINUX
  /*
    BUG#24507: Race conditions inside current NPTL pthread_exit()
    implementation.

    To avoid a possible segmentation fault during concurrent
    executions of pthread_exit(), a dummy thread is spawned which
    initializes internal variables of pthread lib. See bug description
    for a full explanation.

    TODO: Remove this code when fixed versions of glibc6 are in common
    use.
  */
  if (thd_lib_detected == THD_LIB_NPTL)
  {
    pthread_t       dummy_thread;
    pthread_attr_t  dummy_thread_attr;

    pthread_attr_init(&dummy_thread_attr);
    pthread_attr_setdetachstate(&dummy_thread_attr, PTHREAD_CREATE_JOINABLE);

    if (pthread_create(&dummy_thread,&dummy_thread_attr,
                       nptl_pthread_exit_hack_handler, NULL) == 0)
      (void)pthread_join(dummy_thread, NULL);
  }
#endif /* TARGET_OS_LINUX */

  mysql_mutex_init(key_THR_LOCK_lock, &THR_LOCK_lock, MY_MUTEX_INIT_FAST);
  mysql_mutex_init(key_THR_LOCK_isam, &THR_LOCK_isam, MY_MUTEX_INIT_SLOW);
  mysql_mutex_init(key_THR_LOCK_myisam, &THR_LOCK_myisam, MY_MUTEX_INIT_SLOW);
  mysql_mutex_init(key_THR_LOCK_myisam_mmap, &THR_LOCK_myisam_mmap, MY_MUTEX_INIT_FAST);
  mysql_mutex_init(key_THR_LOCK_heap, &THR_LOCK_heap, MY_MUTEX_INIT_FAST);
  mysql_mutex_init(key_THR_LOCK_net, &THR_LOCK_net, MY_MUTEX_INIT_FAST);
  mysql_mutex_init(key_THR_LOCK_time, &THR_LOCK_time, MY_MUTEX_INIT_FAST);
  mysql_cond_init(key_THR_COND_threads, &THR_COND_threads, NULL);

#if !defined(HAVE_LOCALTIME_R) || !defined(HAVE_GMTIME_R)
  mysql_mutex_init(key_LOCK_localtime_r, &LOCK_localtime_r, MY_MUTEX_INIT_SLOW);
#endif
#ifndef HAVE_GETHOSTBYNAME_R
  mysql_mutex_init(key_LOCK_gethostbyname_r,
                   &LOCK_gethostbyname_r, MY_MUTEX_INIT_SLOW);
#endif

#ifdef _MSC_VER
  install_sigabrt_handler();
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
  struct timespec abstime;
  my_bool all_threads_killed= 1;

  set_timespec(abstime, my_thread_end_wait_time);
  mysql_mutex_lock(&THR_LOCK_threads);
  while (THR_thread_count > 0)
  {
    int error= mysql_cond_timedwait(&THR_COND_threads, &THR_LOCK_threads,
                                    &abstime);
    if (error == ETIMEDOUT || error == ETIME)
    {
#ifdef HAVE_PTHREAD_KILL
      /*
        We shouldn't give an error here, because if we don't have
        pthread_kill(), programs like mysqld can't ensure that all threads
        are killed when we enter here.
      */
      if (THR_thread_count)
        fprintf(stderr,
                "Error in my_thread_global_end(): %d threads didn't exit\n",
                THR_thread_count);
#endif
      all_threads_killed= 0;
      break;
    }
  }
  mysql_mutex_unlock(&THR_LOCK_threads);

  pthread_key_delete(THR_KEY_mysys);
#ifdef PTHREAD_ADAPTIVE_MUTEX_INITIALIZER_NP
  pthread_mutexattr_destroy(&my_fast_mutexattr);
#endif
#ifdef PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP
  pthread_mutexattr_destroy(&my_errorcheck_mutexattr);
#endif
  mysql_mutex_destroy(&THR_LOCK_malloc);
  mysql_mutex_destroy(&THR_LOCK_open);
  mysql_mutex_destroy(&THR_LOCK_lock);
  mysql_mutex_destroy(&THR_LOCK_isam);
  mysql_mutex_destroy(&THR_LOCK_myisam);
  mysql_mutex_destroy(&THR_LOCK_myisam_mmap);
  mysql_mutex_destroy(&THR_LOCK_heap);
  mysql_mutex_destroy(&THR_LOCK_net);
  mysql_mutex_destroy(&THR_LOCK_time);
  mysql_mutex_destroy(&THR_LOCK_charset);
  if (all_threads_killed)
  {
    mysql_mutex_destroy(&THR_LOCK_threads);
    mysql_cond_destroy(&THR_COND_threads);
  }
#if !defined(HAVE_LOCALTIME_R) || !defined(HAVE_GMTIME_R)
  mysql_mutex_destroy(&LOCK_localtime_r);
#endif
#ifndef HAVE_GETHOSTBYNAME_R
  mysql_mutex_destroy(&LOCK_gethostbyname_r);
#endif

  my_thread_basic_global_init_done= 0;
}

static my_thread_id thread_id= 0;

/*
  Allocate thread specific memory for the thread, used by mysys and dbug

  SYNOPSIS
    my_thread_init()

  NOTES
    We can't use mutex_locks here if we are using windows as
    we may have compiled the program with SAFE_MUTEX, in which
    case the checking of mutex_locks will not work until
    the pthread_self thread specific variable is initialized.

   This function may called multiple times for a thread, for example
   if one uses my_init() followed by mysql_server_init().

  RETURN
    0  ok
    1  Fatal error; mysys/dbug functions can't be used
*/

my_bool my_thread_init(void)
{
  struct st_my_thread_var *tmp;
  my_bool error=0;

#ifdef EXTRA_DEBUG_THREADS
  fprintf(stderr,"my_thread_init(): thread_id: 0x%lx\n",
          (ulong) pthread_self());
#endif  

  if (my_pthread_getspecific(struct st_my_thread_var *,THR_KEY_mysys))
  {
#ifdef EXTRA_DEBUG_THREADS
    fprintf(stderr,"my_thread_init() called more than once in thread 0x%lx\n",
            (long) pthread_self());
#endif    
    goto end;
  }

#ifdef _MSC_VER
  install_sigabrt_handler();
#endif

  if (!(tmp= (struct st_my_thread_var *) calloc(1, sizeof(*tmp))))
  {
    error= 1;
    goto end;
  }
  pthread_setspecific(THR_KEY_mysys,tmp);
  tmp->pthread_self= pthread_self();
  mysql_mutex_init(key_my_thread_var_mutex, &tmp->mutex, MY_MUTEX_INIT_FAST);
  mysql_cond_init(key_my_thread_var_suspend, &tmp->suspend, NULL);

  tmp->stack_ends_here= (char*)&tmp +
                         STACK_DIRECTION * (long)my_thread_stack_size;

  mysql_mutex_lock(&THR_LOCK_threads);
  tmp->id= ++thread_id;
  ++THR_thread_count;
  mysql_mutex_unlock(&THR_LOCK_threads);
  tmp->init= 1;
#ifndef DBUG_OFF
  /* Generate unique name for thread */
  (void) my_thread_name();
#endif

end:
  return error;
}


/*
  Deallocate memory used by the thread for book-keeping

  SYNOPSIS
    my_thread_end()

  NOTE
    This may be called multiple times for a thread.
    This happens for example when one calls 'mysql_server_init()'
    mysql_server_end() and then ends with a mysql_end().
*/

void my_thread_end(void)
{
  struct st_my_thread_var *tmp;
  tmp= my_pthread_getspecific(struct st_my_thread_var*,THR_KEY_mysys);

#ifdef EXTRA_DEBUG_THREADS
  fprintf(stderr,"my_thread_end(): tmp: 0x%lx  pthread_self: 0x%lx  thread_id: %ld\n",
	  (long) tmp, (long) pthread_self(), tmp ? (long) tmp->id : 0L);
#endif  

#ifdef HAVE_PSI_INTERFACE
  /*
    Remove the instrumentation for this thread.
    This must be done before trashing st_my_thread_var,
    because the LF_HASH depends on it.
  */
  if (PSI_server)
    PSI_server->delete_current_thread();
#endif

  if (tmp && tmp->init)
  {
#if !defined(DBUG_OFF)
    /* tmp->dbug is allocated inside DBUG library */
    if (tmp->dbug)
    {
      DBUG_POP();
      free(tmp->dbug);
      tmp->dbug=0;
    }
#endif
#if !defined(__bsdi__) && !defined(__OpenBSD__)
 /* bsdi and openbsd 3.5 dumps core here */
    mysql_cond_destroy(&tmp->suspend);
#endif
    mysql_mutex_destroy(&tmp->mutex);
    free(tmp);

    /*
      Decrement counter for number of running threads. We are using this
      in my_thread_global_end() to wait until all threads have called
      my_thread_end and thus freed all memory they have allocated in
      my_thread_init() and DBUG_xxxx
    */
    mysql_mutex_lock(&THR_LOCK_threads);
    DBUG_ASSERT(THR_thread_count != 0);
    if (--THR_thread_count == 0)
      mysql_cond_signal(&THR_COND_threads);
    mysql_mutex_unlock(&THR_LOCK_threads);
  }
  pthread_setspecific(THR_KEY_mysys,0);
}

struct st_my_thread_var *_my_thread_var(void)
{
  return  my_pthread_getspecific(struct st_my_thread_var*,THR_KEY_mysys);
}


/****************************************************************************
  Get name of current thread.
****************************************************************************/

my_thread_id my_thread_dbug_id()
{
  return my_thread_var->id;
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
    my_thread_id id= my_thread_dbug_id();
    sprintf(name_buff,"T@%lu", (ulong) id);
    strmake(tmp->name,name_buff,THREAD_NAME_SIZE);
  }
  return tmp->name;
}

/* Return pointer to DBUG for holding current state */

extern void **my_thread_var_dbug()
{
  struct st_my_thread_var *tmp=
    my_pthread_getspecific(struct st_my_thread_var*,THR_KEY_mysys);
  return tmp && tmp->init ? &tmp->dbug : 0;
}
#endif /* DBUG_OFF */


static uint get_thread_lib(void)
{
#ifdef _CS_GNU_LIBPTHREAD_VERSION
  char buff[64];
    
  confstr(_CS_GNU_LIBPTHREAD_VERSION, buff, sizeof(buff));

  if (!strncasecmp(buff, "NPTL", 4))
    return THD_LIB_NPTL;
  if (!strncasecmp(buff, "linuxthreads", 12))
    return THD_LIB_LT;
#endif
  return THD_LIB_OTHER;
}

#ifdef _WIN32
/*
  In Visual Studio 2005 and later, default SIGABRT handler will overwrite
  any unhandled exception filter set by the application  and will try to
  call JIT debugger. This is not what we want, this we calling __debugbreak
  to stop in debugger, if process is being debugged or to generate 
  EXCEPTION_BREAKPOINT and then handle_segfault will do its magic.
*/

#if (_MSC_VER >= 1400)
static void my_sigabrt_handler(int sig)
{
  __debugbreak();
}
#endif /*_MSC_VER >=1400 */

static void install_sigabrt_handler(void)
{
#if (_MSC_VER >=1400)
  /*abort() should not override our exception filter*/
  _set_abort_behavior(0,_CALL_REPORTFAULT);
  signal(SIGABRT,my_sigabrt_handler);
#endif /* _MSC_VER >=1400 */
}
#endif

