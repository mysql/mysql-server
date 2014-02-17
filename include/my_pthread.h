/* Copyright (c) 2000, 2013, Oracle and/or its affiliates.
   Copyright (c) 2009, 2013, Monty Program Ab.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA */

/* Defines to make different thread packages compatible */

#ifndef _my_pthread_h
#define _my_pthread_h

#include "my_global.h"                          /* myf */

#ifndef ETIME
#define ETIME ETIMEDOUT				/* For FreeBSD */
#endif

#ifdef  __cplusplus
#define EXTERNC extern "C"
extern "C" {
#else
#define EXTERNC
#endif /* __cplusplus */ 

#if defined(__WIN__)
typedef CRITICAL_SECTION pthread_mutex_t;
typedef DWORD		 pthread_t;
typedef struct thread_attr {
    DWORD dwStackSize ;
    DWORD dwCreatingFlag ;
} pthread_attr_t ;

typedef struct { int dummy; } pthread_condattr_t;

/* Implementation of posix conditions */

typedef struct st_pthread_link {
  DWORD thread_id;
  struct st_pthread_link *next;
} pthread_link;

/**
  Implementation of Windows condition variables.
  We use native conditions on Vista and later, and fallback to own 
  implementation on earlier OS version.
*/
typedef union
{
  /* Native condition (used on Vista and later) */
  CONDITION_VARIABLE native_cond;

  /* Own implementation (used on XP) */
  struct
  { 
    uint32 waiting;
    CRITICAL_SECTION lock_waiting;
    enum 
    {
      SIGNAL= 0,
      BROADCAST= 1,
      MAX_EVENTS= 2
    } EVENTS;
    HANDLE events[MAX_EVENTS];
    HANDLE broadcast_block_event;
  };
} pthread_cond_t;


typedef int pthread_mutexattr_t;
#define pthread_self() GetCurrentThreadId()
#define pthread_handler_t EXTERNC void * __cdecl
typedef void * (__cdecl *pthread_handler)(void *);

typedef volatile LONG my_pthread_once_t;
#define MY_PTHREAD_ONCE_INIT  0
#define MY_PTHREAD_ONCE_INPROGRESS 1
#define MY_PTHREAD_ONCE_DONE 2

struct timespec {
  time_t tv_sec;
  long tv_nsec;
};

int win_pthread_mutex_trylock(pthread_mutex_t *mutex);
int pthread_create(pthread_t *, const pthread_attr_t *, pthread_handler, void *);
int pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr);
int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex);
int pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex,
			   const struct timespec *abstime);
int pthread_cond_signal(pthread_cond_t *cond);
int pthread_cond_broadcast(pthread_cond_t *cond);
int pthread_cond_destroy(pthread_cond_t *cond);
int pthread_attr_init(pthread_attr_t *connect_att);
int pthread_attr_setstacksize(pthread_attr_t *connect_att,DWORD stack);
int pthread_attr_destroy(pthread_attr_t *connect_att);
int my_pthread_once(my_pthread_once_t *once_control,void (*init_routine)(void));
struct tm *localtime_r(const time_t *timep,struct tm *tmp);
struct tm *gmtime_r(const time_t *timep,struct tm *tmp);

void pthread_exit(void *a);
int pthread_join(pthread_t thread, void **value_ptr);
int pthread_cancel(pthread_t thread);

#ifndef ETIMEDOUT
#define ETIMEDOUT 145		    /* Win32 doesn't have this */
#endif

#define getpid() GetCurrentThreadId()
#define HAVE_LOCALTIME_R		1
#define _REENTRANT			1
#define HAVE_PTHREAD_ATTR_SETSTACKSIZE	1

#undef SAFE_MUTEX				/* This will cause conflicts */
#define pthread_key(T,V)  DWORD V
#define pthread_key_create(A,B) ((*A=TlsAlloc())==0xFFFFFFFF)
#define pthread_key_delete(A) TlsFree(A)
#define my_pthread_setspecific_ptr(T,V) (!TlsSetValue((T),(V)))
#define pthread_setspecific(A,B) (!TlsSetValue((A),(B)))
#define pthread_getspecific(A) (TlsGetValue(A))
#define my_pthread_getspecific(T,A) ((T) TlsGetValue(A))
#define my_pthread_getspecific_ptr(T,V) ((T) TlsGetValue(V))

#define pthread_equal(A,B) ((A) == (B))
#define pthread_mutex_init(A,B)  (InitializeCriticalSection(A),0)
#define pthread_mutex_lock(A)	 (EnterCriticalSection(A),0)
#define pthread_mutex_trylock(A) win_pthread_mutex_trylock((A))
#define pthread_mutex_unlock(A)  (LeaveCriticalSection(A), 0)
#define pthread_mutex_destroy(A) (DeleteCriticalSection(A), 0)
#define pthread_kill(A,B) pthread_dummy((A) ? 0 : ESRCH)

/* Dummy defines for easier code */
#define pthread_attr_setdetachstate(A,B) pthread_dummy(0)
#define pthread_attr_setscope(A,B)
#define pthread_detach_this_thread()
#define pthread_condattr_init(A)
#define pthread_condattr_destroy(A)
#define pthread_yield() SwitchToThread()
#define my_sigset(A,B) signal(A,B)

#else /* Normal threads */

#ifdef HAVE_rts_threads
#define sigwait org_sigwait
#include <signal.h>
#undef sigwait
#endif
#include <pthread.h>
#ifndef _REENTRANT
#define _REENTRANT
#endif
#ifdef HAVE_THR_SETCONCURRENCY
#include <thread.h>			/* Probably solaris */
#endif
#ifdef HAVE_SCHED_H
#include <sched.h>
#endif
#ifdef HAVE_SYNCH_H
#include <synch.h>
#endif

#define pthread_key(T,V) pthread_key_t V
#define my_pthread_getspecific_ptr(T,V) my_pthread_getspecific(T,(V))
#define my_pthread_setspecific_ptr(T,V) pthread_setspecific(T,(void*) (V))
#define pthread_detach_this_thread()
#define pthread_handler_t EXTERNC void *
typedef void *(* pthread_handler)(void *);

#define my_pthread_once_t pthread_once_t
#if defined(PTHREAD_ONCE_INITIALIZER)
#define MY_PTHREAD_ONCE_INIT PTHREAD_ONCE_INITIALIZER
#else
#define MY_PTHREAD_ONCE_INIT PTHREAD_ONCE_INIT
#endif
#define my_pthread_once(C,F) pthread_once(C,F)

/* Test first for RTS or FSU threads */

#if defined(PTHREAD_SCOPE_GLOBAL) && !defined(PTHREAD_SCOPE_SYSTEM)
#define HAVE_rts_threads
extern int my_pthread_create_detached;
#define pthread_sigmask(A,B,C) sigprocmask((A),(B),(C))
#define PTHREAD_CREATE_DETACHED &my_pthread_create_detached
#define PTHREAD_SCOPE_SYSTEM  PTHREAD_SCOPE_GLOBAL
#define PTHREAD_SCOPE_PROCESS PTHREAD_SCOPE_LOCAL
#define USE_ALARM_THREAD
#endif /* defined(PTHREAD_SCOPE_GLOBAL) && !defined(PTHREAD_SCOPE_SYSTEM) */

#if defined(_BSDI_VERSION) && _BSDI_VERSION < 199910
int sigwait(sigset_t *set, int *sig);
#endif

#define my_sigwait(A,B) sigwait((A),(B))

#if defined(HAVE_SIGTHREADMASK) && !defined(HAVE_PTHREAD_SIGMASK)
#define pthread_sigmask(A,B,C) sigthreadmask((A),(B),(C))
#endif

#if !defined(HAVE_SIGWAIT) && !defined(HAVE_rts_threads) && !defined(sigwait) && !defined(alpha_linux_port) && !defined(_AIX)
int sigwait(sigset_t *setp, int *sigp);		/* Use our implemention */
#endif


/*
  We define my_sigset() and use that instead of the system sigset() so that
  we can favor an implementation based on sigaction(). On some systems, such
  as Mac OS X, sigset() results in flags such as SA_RESTART being set, and
  we want to make sure that no such flags are set.
*/
#if defined(HAVE_SIGACTION) && !defined(my_sigset)
#define my_sigset(A,B) do { struct sigaction l_s; sigset_t l_set;           \
                            DBUG_ASSERT((A) != 0);                          \
                            sigemptyset(&l_set);                            \
                            l_s.sa_handler = (B);                           \
                            l_s.sa_mask   = l_set;                          \
                            l_s.sa_flags   = 0;                             \
                            sigaction((A), &l_s, NULL);                     \
                          } while (0)
#elif defined(HAVE_SIGSET) && !defined(my_sigset)
#define my_sigset(A,B) sigset((A),(B))
#elif !defined(my_sigset)
#define my_sigset(A,B) signal((A),(B))
#endif

#if !defined(HAVE_PTHREAD_ATTR_SETSCOPE)
#define pthread_attr_setscope(A,B)
#undef	HAVE_GETHOSTBYADDR_R			/* No definition */
#endif

#define my_pthread_getspecific(A,B) ((A) pthread_getspecific(B))

#ifndef HAVE_LOCALTIME_R
struct tm *localtime_r(const time_t *clock, struct tm *res);
#endif

#ifndef HAVE_GMTIME_R
struct tm *gmtime_r(const time_t *clock, struct tm *res);
#endif

#ifdef HAVE_PTHREAD_CONDATTR_CREATE
/* DCE threads on HPUX 10.20 */
#define pthread_condattr_init pthread_condattr_create
#define pthread_condattr_destroy pthread_condattr_delete
#endif

/* FSU THREADS */
#if !defined(HAVE_PTHREAD_KEY_DELETE) && !defined(pthread_key_delete)
#define pthread_key_delete(A) pthread_dummy(0)
#endif

#if defined(HAVE_PTHREAD_ATTR_CREATE) && !defined(HAVE_SIGWAIT)
/* This is set on AIX_3_2 and Siemens unix (and DEC OSF/1 3.2 too) */
#define pthread_key_create(A,B) \
		pthread_keycreate(A,(B) ?\
				  (pthread_destructor_t) (B) :\
				  (pthread_destructor_t) pthread_dummy)
#define pthread_attr_init(A) pthread_attr_create(A)
#define pthread_attr_destroy(A) pthread_attr_delete(A)
#define pthread_attr_setdetachstate(A,B) pthread_dummy(0)
#define pthread_create(A,B,C,D) pthread_create((A),*(B),(C),(D))
#ifndef pthread_sigmask
#define pthread_sigmask(A,B,C) sigprocmask((A),(B),(C))
#endif
#define pthread_kill(A,B) pthread_dummy((A) ? 0 : ESRCH)
#undef	pthread_detach_this_thread
#define pthread_detach_this_thread() { pthread_t tmp=pthread_self() ; pthread_detach(&tmp); }
#else /* HAVE_PTHREAD_ATTR_CREATE && !HAVE_SIGWAIT */
#define HAVE_PTHREAD_KILL
#endif

#endif /* defined(__WIN__) */

#if defined(HPUX10) && !defined(DONT_REMAP_PTHREAD_FUNCTIONS)
#undef pthread_cond_timedwait
#define pthread_cond_timedwait(a,b,c) my_pthread_cond_timedwait((a),(b),(c))
int my_pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex,
			      struct timespec *abstime);
#endif

#if defined(HPUX10)
#define pthread_attr_getstacksize(A,B) my_pthread_attr_getstacksize(A,B)
void my_pthread_attr_getstacksize(pthread_attr_t *attrib, size_t *size);
#endif

#if defined(HAVE_POSIX1003_4a_MUTEX) && !defined(DONT_REMAP_PTHREAD_FUNCTIONS)
#undef pthread_mutex_trylock
#define pthread_mutex_trylock(a) my_pthread_mutex_trylock((a))
int my_pthread_mutex_trylock(pthread_mutex_t *mutex);
#endif

#if !defined(HAVE_PTHREAD_YIELD_ZERO_ARG)
/* no pthread_yield() available */
#ifdef HAVE_SCHED_YIELD
#define pthread_yield() sched_yield()
#elif defined(HAVE_PTHREAD_YIELD_NP) /* can be Mac OS X */
#define pthread_yield() pthread_yield_np()
#elif defined(HAVE_THR_YIELD)
#define pthread_yield() thr_yield()
#endif
#endif

/*
  The defines set_timespec and set_timespec_nsec should be used
  for calculating an absolute time at which
  pthread_cond_timedwait should timeout
*/

#define set_timespec(ABSTIME,SEC) set_timespec_nsec((ABSTIME),(SEC)*1000000000ULL)

#ifndef set_timespec_nsec
#define set_timespec_nsec(ABSTIME,NSEC)                                 \
  set_timespec_time_nsec((ABSTIME), my_hrtime().val*1000 + (NSEC))
#endif /* !set_timespec_nsec */

/* adapt for two different flavors of struct timespec */
#ifdef HAVE_TIMESPEC_TS_SEC
#define MY_tv_sec  ts_sec
#define MY_tv_nsec ts_nsec
#else
#define MY_tv_sec  tv_sec
#define MY_tv_nsec tv_nsec
#endif /* HAVE_TIMESPEC_TS_SEC */

/**
   Compare two timespec structs.

   @retval  1 If TS1 ends after TS2.

   @retval  0 If TS1 is equal to TS2.

   @retval -1 If TS1 ends before TS2.
*/
#ifndef cmp_timespec
#define cmp_timespec(TS1, TS2) \
  ((TS1.MY_tv_sec > TS2.MY_tv_sec || \
    (TS1.MY_tv_sec == TS2.MY_tv_sec && TS1.MY_tv_nsec > TS2.MY_tv_nsec)) ? 1 : \
   ((TS1.MY_tv_sec < TS2.MY_tv_sec || \
     (TS1.MY_tv_sec == TS2.MY_tv_sec && TS1.MY_tv_nsec < TS2.MY_tv_nsec)) ? -1 : 0))
#endif /* !cmp_timespec */

#ifndef set_timespec_time_nsec
#define set_timespec_time_nsec(ABSTIME,NSEC) do {    \
  ulonglong _now_= (NSEC);                             \
  (ABSTIME).MY_tv_sec=  (_now_ / 1000000000ULL);       \
  (ABSTIME).MY_tv_nsec= (_now_ % 1000000000ULL);       \
} while(0)
#endif /* !set_timespec_time_nsec */

/* safe_mutex adds checking to mutex for easier debugging */
struct st_hash;
typedef struct st_safe_mutex_t
{
  pthread_mutex_t global,mutex;
  const char *file, *name;
  uint line,count;
  myf create_flags, active_flags;
  ulong id;
  pthread_t thread;
  struct st_hash *locked_mutex, *used_mutex;
  struct st_safe_mutex_t *prev, *next;
#ifdef SAFE_MUTEX_DETECT_DESTROY
  struct st_safe_mutex_info_t *info;	/* to track destroying of mutexes */
#endif
} safe_mutex_t;

typedef struct st_safe_mutex_deadlock_t
{
  const char *file, *name;
  safe_mutex_t *mutex;
  uint line;
  ulong count;
  ulong id;
  my_bool warning_only;
} safe_mutex_deadlock_t;

#ifdef SAFE_MUTEX_DETECT_DESTROY
/*
  Used to track the destroying of mutexes. This needs to be a seperate
  structure because the safe_mutex_t structure could be freed before
  the mutexes are destroyed.
*/

typedef struct st_safe_mutex_info_t
{
  struct st_safe_mutex_info_t *next;
  struct st_safe_mutex_info_t *prev;
  const char *init_file;
  uint32 init_line;
} safe_mutex_info_t;
#endif /* SAFE_MUTEX_DETECT_DESTROY */

int safe_mutex_init(safe_mutex_t *mp, const pthread_mutexattr_t *attr,
                    const char *name, const char *file, uint line);
int safe_mutex_lock(safe_mutex_t *mp, myf my_flags, const char *file,
                    uint line);
int safe_mutex_unlock(safe_mutex_t *mp,const char *file, uint line);
int safe_mutex_destroy(safe_mutex_t *mp,const char *file, uint line);
int safe_cond_wait(pthread_cond_t *cond, safe_mutex_t *mp,const char *file,
		   uint line);
int safe_cond_timedwait(pthread_cond_t *cond, safe_mutex_t *mp,
                        const struct timespec *abstime,
                        const char *file, uint line);
void safe_mutex_global_init(void);
void safe_mutex_end(FILE *file);
void safe_mutex_free_deadlock_data(safe_mutex_t *mp);

	/* Wrappers if safe mutex is actually used */
#define MYF_TRY_LOCK              1
#define MYF_NO_DEADLOCK_DETECTION 2

#ifdef SAFE_MUTEX
#undef pthread_mutex_init
#undef pthread_mutex_lock
#undef pthread_mutex_unlock
#undef pthread_mutex_destroy
#undef pthread_mutex_wait
#undef pthread_mutex_timedwait
#undef pthread_mutex_t
#undef pthread_cond_wait
#undef pthread_cond_timedwait
#undef pthread_mutex_trylock
#define pthread_mutex_init(A,B) safe_mutex_init((A),(B),#A,__FILE__,__LINE__)
#define pthread_mutex_lock(A) safe_mutex_lock((A),0,__FILE__, __LINE__)
#define pthread_mutex_unlock(A) safe_mutex_unlock((A),__FILE__,__LINE__)
#define pthread_mutex_destroy(A) safe_mutex_destroy((A),__FILE__,__LINE__)
#define pthread_cond_wait(A,B) safe_cond_wait((A),(B),__FILE__,__LINE__)
#define pthread_cond_timedwait(A,B,C) safe_cond_timedwait((A),(B),(C),__FILE__,__LINE__)
#define pthread_mutex_trylock(A) safe_mutex_lock((A), MYF_TRY_LOCK, __FILE__, __LINE__)
#define pthread_mutex_t safe_mutex_t
#define safe_mutex_assert_owner(mp) \
          DBUG_ASSERT((mp)->count > 0 && \
                      pthread_equal(pthread_self(), (mp)->thread))
#define safe_mutex_assert_not_owner(mp) \
          DBUG_ASSERT(! (mp)->count || \
                      ! pthread_equal(pthread_self(), (mp)->thread))
#define safe_mutex_setflags(mp, F)      do { (mp)->create_flags|= (F); } while (0)
#else
#define my_pthread_mutex_init(A,B,C,D) pthread_mutex_init((A),(B))
#define safe_mutex_assert_owner(mp)    do {} while(0)
#define safe_mutex_assert_not_owner(mp) do {} while(0)
#define safe_mutex_free_deadlock_data(mp) do {} while(0)
#define safe_mutex_setflags(mp, F) do {} while (0)
#endif /* SAFE_MUTEX */

#if defined(MY_PTHREAD_FASTMUTEX) && !defined(SAFE_MUTEX)
typedef struct st_my_pthread_fastmutex_t
{
  pthread_mutex_t mutex;
  uint spins;
  uint rng_state;
} my_pthread_fastmutex_t;
void fastmutex_global_init(void);

int my_pthread_fastmutex_init(my_pthread_fastmutex_t *mp, 
                              const pthread_mutexattr_t *attr);
int my_pthread_fastmutex_lock(my_pthread_fastmutex_t *mp);

#undef pthread_mutex_init
#undef pthread_mutex_lock
#undef pthread_mutex_unlock
#undef pthread_mutex_destroy
#undef pthread_mutex_wait
#undef pthread_mutex_timedwait
#undef pthread_mutex_t
#undef pthread_cond_wait
#undef pthread_cond_timedwait
#undef pthread_mutex_trylock
#define pthread_mutex_init(A,B) my_pthread_fastmutex_init((A),(B))
#define pthread_mutex_lock(A) my_pthread_fastmutex_lock(A)
#define pthread_mutex_unlock(A) pthread_mutex_unlock(&(A)->mutex)
#define pthread_mutex_destroy(A) pthread_mutex_destroy(&(A)->mutex)
#define pthread_cond_wait(A,B) pthread_cond_wait((A),&(B)->mutex)
#define pthread_cond_timedwait(A,B,C) pthread_cond_timedwait((A),&(B)->mutex,(C))
#define pthread_mutex_trylock(A) pthread_mutex_trylock(&(A)->mutex)
#define pthread_mutex_t my_pthread_fastmutex_t
#endif /* defined(MY_PTHREAD_FASTMUTEX) && !defined(SAFE_MUTEX) */

	/* READ-WRITE thread locking */

#if defined(USE_MUTEX_INSTEAD_OF_RW_LOCKS)
/* use these defs for simple mutex locking */
#define rw_lock_t pthread_mutex_t
#define my_rwlock_init(A,B) pthread_mutex_init((A),(B))
#define rw_rdlock(A) pthread_mutex_lock((A))
#define rw_wrlock(A) pthread_mutex_lock((A))
#define rw_tryrdlock(A) pthread_mutex_trylock((A))
#define rw_trywrlock(A) pthread_mutex_trylock((A))
#define rw_unlock(A) pthread_mutex_unlock((A))
#define rwlock_destroy(A) pthread_mutex_destroy((A))
#elif defined(HAVE_PTHREAD_RWLOCK_RDLOCK)
#define rw_lock_t pthread_rwlock_t
#define my_rwlock_init(A,B) pthread_rwlock_init((A),(B))
#define rw_rdlock(A) pthread_rwlock_rdlock(A)
#define rw_wrlock(A) pthread_rwlock_wrlock(A)
#define rw_tryrdlock(A) pthread_rwlock_tryrdlock((A))
#define rw_trywrlock(A) pthread_rwlock_trywrlock((A))
#define rw_unlock(A) pthread_rwlock_unlock(A)
#define rwlock_destroy(A) pthread_rwlock_destroy(A)
#elif defined(HAVE_RWLOCK_INIT)
#ifdef HAVE_RWLOCK_T				/* For example Solaris 2.6-> */
#define rw_lock_t rwlock_t
#endif
#define my_rwlock_init(A,B) rwlock_init((A),USYNC_THREAD,0)
#else
/* Use our own version of read/write locks */
#define NEED_MY_RW_LOCK 1
#define rw_lock_t my_rw_lock_t
#define my_rwlock_init(A,B) my_rw_init((A))
#define rw_rdlock(A) my_rw_rdlock((A))
#define rw_wrlock(A) my_rw_wrlock((A))
#define rw_tryrdlock(A) my_rw_tryrdlock((A))
#define rw_trywrlock(A) my_rw_trywrlock((A))
#define rw_unlock(A) my_rw_unlock((A))
#define rwlock_destroy(A) my_rw_destroy((A))
#define rw_lock_assert_write_owner(A) my_rw_lock_assert_write_owner((A))
#define rw_lock_assert_not_write_owner(A) my_rw_lock_assert_not_write_owner((A))
#endif /* USE_MUTEX_INSTEAD_OF_RW_LOCKS */


/**
  Portable implementation of special type of read-write locks.

  These locks have two properties which are unusual for rwlocks:
  1) They "prefer readers" in the sense that they do not allow
     situations in which rwlock is rd-locked and there is a
     pending rd-lock which is blocked (e.g. due to pending
     request for wr-lock).
     This is a stronger guarantee than one which is provided for
     PTHREAD_RWLOCK_PREFER_READER_NP rwlocks in Linux.
     MDL subsystem deadlock detector relies on this property for
     its correctness.
  2) They are optimized for uncontended wr-lock/unlock case.
     This is scenario in which they are most oftenly used
     within MDL subsystem. Optimizing for it gives significant
     performance improvements in some of tests involving many
     connections.

  Another important requirement imposed on this type of rwlock
  by the MDL subsystem is that it should be OK to destroy rwlock
  object which is in unlocked state even though some threads might
  have not yet fully left unlock operation for it (of course there
  is an external guarantee that no thread will try to lock rwlock
  which is destroyed).
  Putting it another way the unlock operation should not access
  rwlock data after changing its state to unlocked.

  TODO/FIXME: We should consider alleviating this requirement as
  it blocks us from doing certain performance optimizations.
*/

typedef struct st_rw_pr_lock_t {
  /**
    Lock which protects the structure.
    Also held for the duration of wr-lock.
  */
  pthread_mutex_t lock;
  /**
    Condition variable which is used to wake-up
    writers waiting for readers to go away.
  */
  pthread_cond_t no_active_readers;
  /** Number of active readers. */
  uint active_readers;
  /** Number of writers waiting for readers to go away. */
  uint writers_waiting_readers;
  /** Indicates whether there is an active writer. */
  my_bool active_writer;
#ifdef SAFE_MUTEX
  /** Thread holding wr-lock (for debug purposes only). */
  pthread_t writer_thread;
#endif
} rw_pr_lock_t;

extern int rw_pr_init(rw_pr_lock_t *);
extern int rw_pr_rdlock(rw_pr_lock_t *);
extern int rw_pr_wrlock(rw_pr_lock_t *);
extern int rw_pr_unlock(rw_pr_lock_t *);
extern int rw_pr_destroy(rw_pr_lock_t *);
#ifdef SAFE_MUTEX
#define rw_pr_lock_assert_write_owner(A) \
  DBUG_ASSERT((A)->active_writer && pthread_equal(pthread_self(), \
                                                  (A)->writer_thread))
#define rw_pr_lock_assert_not_write_owner(A) \
  DBUG_ASSERT(! (A)->active_writer || ! pthread_equal(pthread_self(), \
                                                      (A)->writer_thread))
#else
#define rw_pr_lock_assert_write_owner(A)
#define rw_pr_lock_assert_not_write_owner(A)
#endif /* SAFE_MUTEX */


#ifdef NEED_MY_RW_LOCK

#ifdef _WIN32

/**
  Implementation of Windows rwlock.

  We use native (slim) rwlocks on Win7 and later, and fallback to  portable
  implementation on earlier Windows.

  slim rwlock are also available on Vista/WS2008, but we do not use it
  ("trylock" APIs are missing on Vista)
*/
typedef union
{
  /* Native rwlock (is_srwlock == TRUE) */
  struct 
  {
    SRWLOCK srwlock;             /* native reader writer lock */
    BOOL have_exclusive_srwlock; /* used for unlock */
  };

  /*
    Portable implementation (is_srwlock == FALSE)
    Fields are identical with Unix my_rw_lock_t fields.
  */
  struct 
  {
    pthread_mutex_t lock;       /* lock for structure		*/
    pthread_cond_t  readers;    /* waiting readers		*/
    pthread_cond_t  writers;    /* waiting writers		*/
    int state;                  /* -1:writer,0:free,>0:readers	*/
    int waiters;                /* number of waiting writers	*/
#ifdef SAFE_MUTEX
    pthread_t  write_thread;
#endif
  };
} my_rw_lock_t;


#else /* _WIN32 */

/*
  On systems which don't support native read/write locks we have
  to use own implementation.
*/
typedef struct st_my_rw_lock_t {
	pthread_mutex_t lock;		/* lock for structure		*/
	pthread_cond_t	readers;	/* waiting readers		*/
	pthread_cond_t	writers;	/* waiting writers		*/
	int		state;		/* -1:writer,0:free,>0:readers	*/
	int             waiters;        /* number of waiting writers	*/
#ifdef SAFE_MUTEX
        pthread_t       write_thread;
#endif
} my_rw_lock_t;

#endif /*! _WIN32 */

extern int my_rw_init(my_rw_lock_t *);
extern int my_rw_destroy(my_rw_lock_t *);
extern int my_rw_rdlock(my_rw_lock_t *);
extern int my_rw_wrlock(my_rw_lock_t *);
extern int my_rw_unlock(my_rw_lock_t *);
extern int my_rw_tryrdlock(my_rw_lock_t *);
extern int my_rw_trywrlock(my_rw_lock_t *);
#ifdef SAFE_MUTEX
#define my_rw_lock_assert_write_owner(A) \
  DBUG_ASSERT((A)->state == -1 && pthread_equal(pthread_self(), \
                                                (A)->write_thread))
#define my_rw_lock_assert_not_write_owner(A) \
  DBUG_ASSERT((A)->state >= 0 || ! pthread_equal(pthread_self(), \
                                                 (A)->write_thread))
#else
#define my_rw_lock_assert_write_owner(A)
#define my_rw_lock_assert_not_write_owner(A)
#endif
#endif /* NEED_MY_RW_LOCK */


#define GETHOSTBYADDR_BUFF_SIZE 2048

#ifndef HAVE_THR_SETCONCURRENCY
#define thr_setconcurrency(A) pthread_dummy(0)
#endif
#if !defined(HAVE_PTHREAD_ATTR_SETSTACKSIZE) && ! defined(pthread_attr_setstacksize)
#define pthread_attr_setstacksize(A,B) pthread_dummy(0)
#endif

/* Define mutex types, see my_thr_init.c */
#define MY_MUTEX_INIT_SLOW   NULL
#ifdef PTHREAD_ADAPTIVE_MUTEX_INITIALIZER_NP
extern pthread_mutexattr_t my_fast_mutexattr;
#define MY_MUTEX_INIT_FAST &my_fast_mutexattr
#else
#define MY_MUTEX_INIT_FAST   NULL
#endif
#ifdef PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP
extern pthread_mutexattr_t my_errorcheck_mutexattr;
#define MY_MUTEX_INIT_ERRCHK &my_errorcheck_mutexattr
#else
#define MY_MUTEX_INIT_ERRCHK   NULL
#endif

#ifndef ESRCH
/* Define it to something */
#define ESRCH 1
#endif

typedef ulong my_thread_id;

extern void my_threadattr_global_init(void);
extern my_bool my_thread_global_init(void);
extern void my_thread_global_reinit(void);
extern void my_thread_global_end(void);
extern my_bool my_thread_init(void);
extern void my_thread_end(void);
extern const char *my_thread_name(void);
extern my_thread_id my_thread_dbug_id(void);
extern int pthread_dummy(int);
extern void my_mutex_init();
extern void my_mutex_end();

/* All thread specific variables are in the following struct */

#define THREAD_NAME_SIZE 10
#ifndef DEFAULT_THREAD_STACK
/*
  We need to have at least 256K stack to handle calls to myisamchk_init()
  with the current number of keys and key parts.
*/
#define DEFAULT_THREAD_STACK	(288*1024L)
#endif

#define MY_PTHREAD_LOCK_READ 0
#define MY_PTHREAD_LOCK_WRITE 1

#include <mysql/psi/mysql_thread.h>

#define INSTRUMENT_ME 0

struct st_my_thread_var
{
  int thr_errno;
  mysql_cond_t suspend;
  mysql_mutex_t mutex;
  mysql_mutex_t * volatile current_mutex;
  mysql_cond_t * volatile current_cond;
  pthread_t pthread_self;
  my_thread_id id;
  int cmp_length;
  int volatile abort;
  my_bool init;
  struct st_my_thread_var *next,**prev;
  void *opt_info;
  uint  lock_type; /* used by conditional release the queue */
  void  *stack_ends_here;
  safe_mutex_t *mutex_in_use;
#ifndef DBUG_OFF
  void *dbug;
  char name[THREAD_NAME_SIZE+1];
#endif
};

extern struct st_my_thread_var *_my_thread_var(void) __attribute__ ((const));
extern void **my_thread_var_dbug();
extern safe_mutex_t **my_thread_var_mutex_in_use();
extern uint my_thread_end_wait_time;
extern my_bool safe_mutex_deadlock_detector;
#define my_thread_var (_my_thread_var())
#define my_errno my_thread_var->thr_errno
/*
  Keep track of shutdown,signal, and main threads so that my_end() will not
  report errors with them
*/

/* Which kind of thread library is in use */

#define THD_LIB_OTHER 1
#define THD_LIB_NPTL  2
#define THD_LIB_LT    4

extern uint thd_lib_detected;

/*
  thread_safe_xxx functions are for critical statistic or counters.
  The implementation is guaranteed to be thread safe, on all platforms.
  Note that the calling code should *not* assume the counter is protected
  by the mutex given, as the implementation of these helpers may change
  to use my_atomic operations instead.
*/

#ifndef thread_safe_increment
#ifdef _WIN32
#define thread_safe_increment(V,L) InterlockedIncrement((long*) &(V))
#define thread_safe_decrement(V,L) InterlockedDecrement((long*) &(V))
#else
#define thread_safe_increment(V,L) \
        (mysql_mutex_lock((L)), (V)++, mysql_mutex_unlock((L)))
#define thread_safe_decrement(V,L) \
        (mysql_mutex_lock((L)), (V)--, mysql_mutex_unlock((L)))
#endif
#endif

#ifndef thread_safe_add
#ifdef _WIN32
#define thread_safe_add(V,C,L) InterlockedExchangeAdd((long*) &(V),(C))
#define thread_safe_sub(V,C,L) InterlockedExchangeAdd((long*) &(V),-(long) (C))
#else
#define thread_safe_add(V,C,L) \
        (mysql_mutex_lock((L)), (V)+=(C), mysql_mutex_unlock((L)))
#define thread_safe_sub(V,C,L) \
        (mysql_mutex_lock((L)), (V)-=(C), mysql_mutex_unlock((L)))
#endif
#endif


/*
  statistics_xxx functions are for non critical statistic,
  maintained in global variables.
  When compiling with SAFE_STATISTICS:
  - race conditions can not occur.
  - some locking occurs, which may cause performance degradation.

  When compiling without SAFE_STATISTICS:
  - race conditions can occur, making the result slightly inaccurate.
  - the lock given is not honored.
*/
#ifdef SAFE_STATISTICS
#define statistic_increment(V,L) thread_safe_increment((V),(L))
#define statistic_decrement(V,L) thread_safe_decrement((V),(L))
#define statistic_add(V,C,L)     thread_safe_add((V),(C),(L))
#define statistic_sub(V,C,L)     thread_safe_sub((V),(C),(L))
#else
#define statistic_decrement(V,L) (V)--
#define statistic_increment(V,L) (V)++
#define statistic_add(V,C,L)     (V)+=(C)
#define statistic_sub(V,C,L)     (V)-=(C)
#endif /* SAFE_STATISTICS */

/*
  No locking needed, the counter is owned by the thread
*/
#define status_var_increment(V) (V)++
#define status_var_decrement(V) (V)--
#define status_var_add(V,C)     (V)+=(C)
#define status_var_sub(V,C)     (V)-=(C)

#ifdef SAFE_MUTEX
#define mysql_mutex_record_order(A,B)                   \
  do {                                                  \
    mysql_mutex_lock(A); mysql_mutex_lock(B);           \
    mysql_mutex_unlock(B); mysql_mutex_unlock(A);       \
  }  while(0)
#else
#define mysql_mutex_record_order(A,B) do { } while(0) 
#endif

/* At least Windows and NetBSD do not have this definition */
#ifndef PTHREAD_STACK_MIN
#define PTHREAD_STACK_MIN 65536
#endif

#ifdef  __cplusplus
}
#endif
#endif /* _my_ptread_h */
