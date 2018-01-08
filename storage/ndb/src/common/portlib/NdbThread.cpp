/*
   Copyright (c) 2003, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/


#include <ndb_global.h>
#include <NdbThread.h>
#include "my_thread.h"
#include <NdbMutex.h>
#include <NdbCondition.h>

#ifdef HAVE_LINUX_SCHEDULING
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <sys/types.h>
#include <unistd.h>
#include <sched.h>
#include <sys/syscall.h>
#elif defined(HAVE_SOLARIS_AFFINITY) || defined(HAVE_PROCESSOR_AFFINITY)
#include <sys/types.h>
#include <sys/lwp.h>
#include <sys/processor.h>
#include <sys/procset.h>
#include <sys/pset.h>
#include <thread.h>
#elif defined HAVE_CPUSET_SETAFFINITY
#include <sys/param.h>
#include <sys/cpuset.h>
#include <sys/thr.h>
#endif
#ifdef HAVE_PRIOCNTL
#include <sys/types.h>
#include <sys/priocntl.h>
#include <sys/fxpriocntl.h>
#endif
#ifdef HAVE_SETPRIORITY
#include <sys/time.h>
#include <sys/resource.h>
#endif
#ifdef _WIN32
#include <Windows.h>
#ifdef HAVE_PROCESSTOPOLOGYAPI_H
#include <Processtopologyapi.h>
#endif
#ifdef HAVE_PROCESSTHREADSAPI_H
#include <Processthreadsapi.h>
#endif
#endif

#ifdef NDB_MUTEX_DEADLOCK_DETECTOR
#include "NdbMutex_DeadlockDetector.h"
#endif

#if defined(HAVE_LINUX_SCHEDULING) || defined(HAVE_PTHREAD_SET_SCHEDPARAM)
static int g_min_prio = 0;
static int g_max_prio = 0;
static bool get_prio_first = TRUE;
#endif

static NdbMutex *ndb_thread_mutex = 0;
static struct NdbCondition * ndb_thread_condition = 0;

#ifdef NDB_SHM_TRANSPORTER
int ndb_shm_signum= 0;
#endif

static int f_high_prio_set = 0;
static int f_high_prio_policy;
static int f_high_prio_prio;

struct NdbThread 
{ 
  volatile int inited;
  my_thread_t thread;
  /* Have we called any lock to CPU function yet for this thread */
  bool first_lock_call_exclusive;
  bool first_lock_call_non_exclusive;
#ifdef _WIN32
  /*
    Problem in mysys on certain MySQL versions where the thread id is
    used as the thread identifier. Since the thread id may be reused
    by another thread(when the current thread exits) one may end up
    waiting for the wrong thread.

    Workaround by using a HANDLE which is opened by thread itself in
    'ndb_thread_wrapper' and subsequently used to wait for the
    thread in 'NdbThread_WaitFor'.

    NOTE: Windows implementation of 'pthread_join' and support for non
    detached threads as implemented in MySQL Cluster 7.0 and 7.1 is not
    affected, only MySQL Cluster based on 5.5+ affected(slightly
    different implementation). The current workaround in NdbThread is
    generic and works regardless of pthread_join implementation in mysys.
  */
  HANDLE thread_handle;
  /**
   * Need to know which processor group number we have used
   * Also need old processor group number and mask of this
   * processor for original locking.
   */
  unsigned int usedProcessorGroupNumber;
  WORD oldProcessorGroupNumber;
  KAFFINITY oldProcessorMask;
#endif
#if defined HAVE_LINUX_SCHEDULING
  pid_t tid;
#elif defined HAVE_CPUSET_SETAFFINITY
  id_t tid;
#elif defined HAVE_SOLARIS_AFFINITY
  /* Our thread id */
  id_t tid;
#elif defined _WIN32
  unsigned tid;
#else
  int tid;
#endif
  const struct processor_set_handler *cpu_set_key;
  char thread_name[16];
  NDB_THREAD_FUNC * func;
  void * object;
  void *thread_key;
#ifdef NDB_MUTEX_DEADLOCK_DETECTOR
  struct ndb_mutex_thr_state m_mutex_thr_state;
#endif
};

#ifdef NDB_SHM_TRANSPORTER
void NdbThread_set_shm_sigmask(bool block)
{
  if (ndb_shm_signum)
  {
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, ndb_shm_signum);
    if (block)
      pthread_sigmask(SIG_BLOCK, &mask, 0);
    else
      pthread_sigmask(SIG_UNBLOCK, &mask, 0);
  }
  return;
}
#endif

#if defined HAVE_LINUX_SCHEDULING
#define THREAD_ID_TYPE pid_t
#elif defined HAVE_CPUSET_SETAFFINITY
#define THREAD_ID_TYPE id_t
#elif defined HAVE_SOLARIS_AFFINITY
#define THREAD_ID_TYPE id_t
#elif defined _WIN32
#define THREAD_ID_TYPE unsigned
#else
#define THREAD_ID_TYPE int
#endif

static
THREAD_ID_TYPE
NdbThread_GetMyThreadId()
{
  THREAD_ID_TYPE tid;
#if defined HAVE_LINUX_SCHEDULING
  /* Linux */
  tid = syscall(SYS_gettid);
  if (tid == (pid_t)-1)
  {
    /*
      This extra check is from suggestion by Kristian Nielsen
      to handle cases when running binaries on LinuxThreads
      compiled with NPTL threads
    */
    tid = getpid();
  }
#elif defined HAVE_SOLARIS_AFFINITY
  /* Solaris */
  tid = (id_t)_lwp_self();
#elif defined HAVE_CPUSET_SETAFFINITY
  /* FreeBSD */
  thr_self(&tid);
#elif defined _WIN32
  /* Windows */
  tid = GetCurrentThreadId();
#else
  /* Mac OS X */
  tid = -1;
#endif
  return tid;
}

static void
settid(struct NdbThread *thr)
{
  thr->tid = NdbThread_GetMyThreadId();
}

int
NdbThread_GetTid(struct NdbThread* thr)
{
#if defined HAVE_LINUX_SCHEDULING
  return (int)thr->tid;
#elif defined HAVE_SOLARIS_AFFINITY
  return (int)thr->tid;
#elif defined HAVE_CPUSET_SETAFFINITY
  return (int)thr->tid;
#elif defined _WIN32
  return (int)thr->tid;
#else
  (void)thr;
  return -1;
#endif
}

static
void*
ndb_thread_wrapper(void* _ss){
  my_thread_init();
  {
    DBUG_ENTER("ndb_thread_wrapper");
#ifdef NDB_SHM_TRANSPORTER
    NdbThread_set_shm_sigmask(TRUE);
#endif

#ifdef HAVE_PTHREAD_SIGMASK
    {
      /**
       * Block all signals to thread by default
       *   let them go to main process instead
       */
      sigset_t mask;
      sigfillset(&mask);
      /** Unblock SIGBUS, SIGFPE, SIGILL and SIGSEGV
          because behaviour is undefined when any of
          these signals is blocked
       */
      sigdelset(&mask, SIGBUS);
      sigdelset(&mask, SIGFPE);
      sigdelset(&mask, SIGILL);
      sigdelset(&mask, SIGSEGV);
      pthread_sigmask(SIG_BLOCK, &mask, 0);
    }      
#endif

    {
      void *ret;
      struct NdbThread * ss = (struct NdbThread *)_ss;
      settid(ss);

#ifdef _WIN32
      /*
        Create the thread handle, ignore failure since it's unlikely
        to fail and the functions using this handle checks for NULL.
      */
      ss->thread_handle =
        OpenThread(SYNCHRONIZE |
                     THREAD_SET_INFORMATION |
                     THREAD_QUERY_INFORMATION,
                   FALSE,
                   GetCurrentThreadId());
#endif

#ifdef NDB_MUTEX_DEADLOCK_DETECTOR
      ndb_mutex_thread_init(&ss->m_mutex_thr_state);
#endif
      NDB_THREAD_TLS_NDB_THREAD = ss;
      NdbMutex_Lock(ndb_thread_mutex);
      ss->inited = 1;
      NdbCondition_Signal(ndb_thread_condition);
      NdbMutex_Unlock(ndb_thread_mutex);
      ret= (* ss->func)(ss->object);
      DBUG_POP();
      NdbThread_Exit(ret);
    }
  /* will never be reached */
    DBUG_RETURN(0);
  }
}

static struct NdbThread* g_main_thread = 0;

struct NdbThread*
NdbThread_CreateObject(const char * name)
{
  struct NdbThread* tmpThread;
  DBUG_ENTER("NdbThread_CreateObject");

  if (g_main_thread != 0)
  {
    settid(g_main_thread);
    if (name)
    {
      my_stpnmov(g_main_thread->thread_name, name, sizeof(tmpThread->thread_name));
    }
    DBUG_RETURN(g_main_thread);
  }

  tmpThread = (struct NdbThread*)malloc(sizeof(struct NdbThread));
  if (tmpThread == NULL)
    DBUG_RETURN(NULL);

  bzero(tmpThread, sizeof(* tmpThread));
  if (name)
  {
    my_stpnmov(tmpThread->thread_name, name, sizeof(tmpThread->thread_name));
  }
  else
  {
    my_stpnmov(tmpThread->thread_name, "main", sizeof(tmpThread->thread_name));
  }

#ifdef HAVE_PTHREAD_SELF
  tmpThread->thread = pthread_self();
#elif defined HAVE_GETPID
  tmpThread->thread = getpid();
#endif
  settid(tmpThread);
  tmpThread->inited = 1;

#ifdef NDB_MUTEX_DEADLOCK_DETECTOR
  ndb_mutex_thread_init(&tmpThread->m_mutex_thr_state);
#endif

  g_main_thread = tmpThread;
  DBUG_RETURN(tmpThread);
}

struct NdbThread*
NdbThread_Create(NDB_THREAD_FUNC *p_thread_func,
                 NDB_THREAD_ARG *p_thread_arg,
                 const NDB_THREAD_STACKSIZE _stack_size,
                 const char* p_thread_name,
                 NDB_THREAD_PRIO thread_prio)
{
  struct NdbThread* tmpThread;
  int result;
  my_thread_attr_t thread_attr;
  my_thread_handle thread_handle;
  NDB_THREAD_STACKSIZE thread_stack_size;

  DBUG_ENTER("NdbThread_Create");

  /* Use default stack size if 0 specified */
  if (_stack_size == 0)
    thread_stack_size = 64 * 1024 * SIZEOF_CHARP/4;
  else
    thread_stack_size = _stack_size * SIZEOF_CHARP/4;

  (void)thread_prio; /* remove warning for unused parameter */

  if (p_thread_func == NULL)
    DBUG_RETURN(NULL);

  tmpThread = (struct NdbThread*)malloc(sizeof(struct NdbThread));
  if (tmpThread == NULL)
    DBUG_RETURN(NULL);

  DBUG_PRINT("info",("thread_name: %s", p_thread_name));

  my_stpnmov(tmpThread->thread_name,p_thread_name,sizeof(tmpThread->thread_name));

  my_thread_attr_init(&thread_attr);
#ifdef PTHREAD_STACK_MIN
  if (thread_stack_size < PTHREAD_STACK_MIN)
    thread_stack_size = PTHREAD_STACK_MIN;
#endif
  DBUG_PRINT("info", ("stack_size: %llu", (ulonglong)thread_stack_size));
#ifndef _WIN32
  pthread_attr_setstacksize(&thread_attr, thread_stack_size);
#else
  my_thread_attr_setstacksize(&thread_attr, (DWORD)thread_stack_size);
#endif
#ifdef USE_PTHREAD_EXTRAS
  /* Guard stack overflow with a 2k databuffer */
  pthread_attr_setguardsize(&thread_attr, 2048);
#endif

#ifdef PTHREAD_CREATE_JOINABLE /* needed on SCO */
  pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_JOINABLE);
#endif
  tmpThread->inited = 0;
  tmpThread->func= p_thread_func;
  tmpThread->object= p_thread_arg;
  tmpThread->cpu_set_key = NULL;
  tmpThread->first_lock_call_exclusive = FALSE;
  tmpThread->first_lock_call_non_exclusive = FALSE;

  NdbMutex_Lock(ndb_thread_mutex);
  result = my_thread_create(&thread_handle,
			    &thread_attr,
  		            ndb_thread_wrapper,
  		            tmpThread);
  tmpThread->thread= thread_handle.thread;

  my_thread_attr_destroy(&thread_attr);

  if (result != 0)
  {
    free(tmpThread);
    NdbMutex_Unlock(ndb_thread_mutex);
    DBUG_RETURN(0);
  }

  if (thread_prio == NDB_THREAD_PRIO_HIGH && f_high_prio_set)
  {
#ifdef HAVE_PTHREAD_SETSCHEDPARAM
    struct sched_param param;
    bzero(&param, sizeof(param));
    param.sched_priority = f_high_prio_prio;
    if (pthread_setschedparam(tmpThread->thread, f_high_prio_policy, &param))
      perror("pthread_setschedparam failed");
#endif
  }

  do
  {
    NdbCondition_WaitTimeout(ndb_thread_condition, ndb_thread_mutex, 100);
  } while (tmpThread->inited == 0);

  NdbMutex_Unlock(ndb_thread_mutex);

  DBUG_PRINT("exit", ("Returning thread pointer: %p", tmpThread));
  DBUG_RETURN(tmpThread);
}

struct NdbThread*
NdbThread_CreateLockObject(int tid)
{
  struct NdbThread* tmpThread;
  DBUG_ENTER("NdbThread_CreateLockObject");

  tmpThread = (struct NdbThread*)malloc(sizeof(struct NdbThread));
  if (tmpThread == NULL)
    DBUG_RETURN(NULL);

  bzero(tmpThread, sizeof(* tmpThread));

#if defined HAVE_LINUX_SCHEDULING
  tmpThread->tid= (pid_t)tid;
#elif defined HAVE_CPUSET_SETAFFINITY
  tmpThread->tid= (id_t)tid;
#elif defined HAVE_SOLARIS_AFFINITY
  tmpThread->tid= (id_t)tid;
#elif defined _WIN32
  tmpThread->tid = (unsigned)tid;
#else
  tmpThread->tid= tid;
#endif
  tmpThread->inited = 1;

#ifdef _WIN32
      /*
        Create the thread handle, ignore failure since it's unlikely
        to fail and the functions using this handle checks for NULL.
      */
  tmpThread->thread_handle = OpenThread(SYNCHRONIZE |
                                          THREAD_SET_INFORMATION |
                                          THREAD_QUERY_INFORMATION,
                                        FALSE,
                                        (DWORD)tid);
#endif

#ifdef NDB_MUTEX_DEADLOCK_DETECTOR
  ndb_mutex_thread_init(&tmpThread->m_mutex_thr_state);
#endif

  DBUG_RETURN(tmpThread);
}

void NdbThread_Destroy(struct NdbThread** p_thread)
{
  DBUG_ENTER("NdbThread_Destroy");
  if (*p_thread != NULL){
#ifdef _WIN32
    HANDLE thread_handle = (*p_thread)->thread_handle;
    if (thread_handle)
      CloseHandle(thread_handle);
#endif
    DBUG_PRINT("info", ("Destroying thread pointer: %p", *p_thread));
    free(* p_thread);
    * p_thread = 0;
  }
  DBUG_VOID_RETURN;
}


int NdbThread_WaitFor(struct NdbThread* p_wait_thread, void** status)
{
  if (p_wait_thread == NULL)
    return 0;

  if (p_wait_thread->thread == 0)
    return 0;

#ifdef _WIN32
  {
    DWORD ret;
    HANDLE thread_handle = p_wait_thread->thread_handle;

    if (thread_handle == NULL)
    {
      return -1;
    }

    ret = WaitForSingleObject(thread_handle, INFINITE);
    if (ret != WAIT_OBJECT_0)
    {
      return -1;
    }

    /* Don't fill in "status", never used anyway */

    return 0;
  }
#else
  return pthread_join(p_wait_thread->thread, status);
#endif
}


void NdbThread_Exit(void *status)
{
  my_thread_end();
  my_thread_exit(status);
}


int NdbThread_SetConcurrencyLevel(int level)
{
#ifdef USE_PTHREAD_EXTRAS
  return pthread_setconcurrency(level);
#else
  (void)level; /* remove warning for unused parameter */
  return 0;
#endif
}

#if defined(HAVE_LINUX_SCHEDULING) || defined(HAVE_PTHREAD_SET_SCHEDPARAM)
static int
get_max_prio(int policy)
{
  int max_prio;
#ifdef HAVE_SCHED_GET_PRIORITY_MAX
  max_prio = sched_get_priority_max(policy);
#else
  /*
     Should normally not be used, on Linux RT-prio is between 1 and 100
     so choose 90 mostly from Linux point of view
   */
  max_prio = 90;
#endif
  return max_prio;
}

static int
get_min_prio(int policy)
{
  int min_prio;
#ifdef HAVE_SCHED_GET_PRIORITY_MIN
  min_prio = sched_get_priority_min(policy);
#else
  /* 1 seems like a natural minimum priority level */
  min_prio = 1;
#endif
  return min_prio;
}

static int
get_prio(bool high_prio, int policy)
{
  int prio;

  if (get_prio_first)
  {
    g_max_prio = get_max_prio(policy);
    g_min_prio = get_min_prio(policy);
    get_prio_first = FALSE;
  }
  /*
    We need to distinguish between high and low priority threads. High
    priority threads are the threads that don't execute the main thread.
    It's important that these threads are at a higher priority than the
    main thread since the main thread can execute for a very long time.
    There are no reasons to put these priorities higher than the lowest
    priority and the distance between them being two to enable for future
    extensions where a new priority level is required.
  */
  if (high_prio)
    prio = g_min_prio + 3;
  else
    prio = g_min_prio + 1;
  if (prio < g_min_prio)
    prio = g_min_prio;
  return prio;
}
#endif

/**
 * When running in real-time mode we stop everyone else from running,
 * we make a short break to give others a chance to execute that are
 * on lower prio. If we don't do this regularly and run for a long
 * time on real-time prio then we can easily crash the system.
 */
int
NdbThread_yield_rt(struct NdbThread* pThread, bool high_prio)
{
  int res = NdbThread_SetScheduler(pThread, FALSE, high_prio);
  int res1 = NdbThread_SetScheduler(pThread, TRUE, high_prio);
  if (res || res1)
    return res;
  return 0;
}

int
NdbThread_SetThreadPrioNormal(struct NdbThread *pThread)
{
  int ret_code = NdbThread_SetThreadPrio(pThread, 5);
  if (ret_code == SET_THREAD_PRIO_NOT_SUPPORTED_ERROR)
  {
    return 0;
  }
  return ret_code;
}

#ifndef _WIN32
int
NdbThread_SetScheduler(struct NdbThread* pThread,
                       bool rt_prio,
                       bool high_prio)
{
  int error_no= 0;
#if defined HAVE_LINUX_SCHEDULING
  /* Linux */
  int ret, policy, prio;
  struct sched_param loc_sched_param;
  if (rt_prio)
  {
    policy = SCHED_RR;
    prio = get_prio(high_prio, policy);
  }
  else
  {
    policy = SCHED_OTHER;
    prio = 0;
  }
  bzero(&loc_sched_param, sizeof(loc_sched_param));
  loc_sched_param.sched_priority = prio;
  ret= sched_setscheduler(pThread->tid, policy, &loc_sched_param);
  if (ret)
    error_no= errno;
#elif defined HAVE_PTHREAD_SET_SCHEDPARAM
  /*
    This variant is POSIX compliant so should be useful on most
    Operating Systems supporting real-time scheduling.
  */
  int ret, policy, prio;
  struct sched_param loc_sched_param;
  if (rt_prio)
  {
    policy = SCHED_RR;
    prio = get_prio(high_prio, policy);
  }
  else
  {
    policy = SCHED_OTHER;
    prio = 0;
  }
  bzero(&loc_sched_param, sizeof(loc_sched_param));
  loc_sched_param.sched_priority = prio;
  ret= pthread_setschedparam(pThread->thread, policy, &loc_sched_param);
  if (ret)
    error_no= errno;
#else
  (void)rt_prio;
  (void)high_prio;
  (void)pThread;
  error_no = ENOSYS;
#endif

  return error_no;
}

/**
 * Set the priority of the scheduler, currently only available on Solaris
 * and Windows, on Windows we can set the general priority although it
 * still uses the time-sharing model. On Solaris we use the fixed priority
 * scheduler when setting the priority.
 *
 * We can also affect the thread priority by using NdbThread_SetScheduler.
 * This is mainly useful to switch back and forth between time-sharing
 * and real-time scheduling.
 *
 * On Linux you cannot affect the priority of the thread, but you can change
 * the nice value of the thread which indirectly influences the thread
 * priority.
 */
int
NdbThread_SetThreadPrio(struct NdbThread *pThread,
                        unsigned int prio)
{
  THREAD_ID_TYPE tid = pThread->tid;
#ifdef HAVE_PRIOCNTL
  /* Solaris */
  int ret;
  int error_no;
  int solaris_prio;
  if (prio < 10)
  {
    switch (prio)
    {
      case 0:
        solaris_prio = 15;
        break;
      case 1:
        solaris_prio = 20;
        break;
      case 2:
        solaris_prio = 25;
        break;
      case 3:
        solaris_prio = 30;
        break;
      case 4:
        solaris_prio = 35;
        break;
      case 5:
        solaris_prio = 40;
        break;
      case 6:
        solaris_prio = 45;
        break;
      case 7:
        solaris_prio = 50;
        break;
      case 8:
        solaris_prio = 55;
        break;
      case 9:
        solaris_prio = 59;
        break;
      default:
        /* Will never end up here */
        require(FALSE);
        break;
    }
  }
  else if (prio == 10)
  {
    solaris_prio = 60;
  }
  else
  {
    return SET_THREAD_PRIO_OUT_OF_RANGE_ERROR;
  }
  ret = priocntl(P_LWPID,
                 tid,
                 PC_SETXPARMS,
                 "FX",
                 FX_KY_UPRILIM,
                 (pri_t)solaris_prio,
                 FX_KY_UPRI,
                 (pri_t)solaris_prio,
                 FX_KY_TQNSECS,
                 FX_NOCHANGE,
                 FX_KY_TQSECS,
                 FX_TQDEF,
                 0);
  if (ret != 0)
  {
    error_no = errno;
  }
  return error_no;              
#elif defined HAVE_SETPRIORITY
  /* Linux */
  int ret;
  int error_no = 0;
  int nice_prio = -20;
  if (prio <= 10)
  {
    switch (prio)
    {
      case 0:
        nice_prio = 19;
        break;
      case 1:
        nice_prio = 16;
        break;
      case 2:
        nice_prio = 12;
        break;
      case 3:
        nice_prio = 8;
        break;
      case 4:
        nice_prio = 4;
        break;
      case 5:
        nice_prio = 0;
        break;
      case 6:
        nice_prio = -5;
        break;
      case 7:
        nice_prio = -10;
        break;
      case 8:
        nice_prio = -15;
        break;
      case 9:
        nice_prio = -20;
        break;
      case 10:
        nice_prio = -20;
        break;
    }
  }
  else
  {
    return SET_THREAD_PRIO_OUT_OF_RANGE_ERROR;
  }
  ret = setpriority(PRIO_PROCESS, tid, nice_prio);
  if (ret != 0)
  {
    error_no = errno;
  }
  return error_no;
#else
  /* Mac OS X */
  (void)pThread;
  (void)prio;
  return SET_THREAD_PRIO_NOT_SUPPORTED_ERROR;
#endif
}
#endif /* End Unix part of scheduling */

/**
 * Module to support CPU locking.
 * 
 * When a thread is locked to a set of CPUs or an individual we first
 * recall the previous CPU locking in some data structure (different for
 * OSs). So then when we call NdbThread_UnlockCPU we can restore the
 * old CPU locking.
 *
 * We support cpubind that locks a thread to 1 CPU non-exclusively
 * (meaning that other threads and other programs can still use this CPU).
 * This is supported on FreeBSD, Linux, Windows and Solaris on not too
 * old OS versions.
 *
 * We also support locking a thread to a set of CPUs non-exclusively.
 * This is supported on FreeBSD, Linux, Windows and Solaris since 11.2.
 *
 * Finally we also support locking to a set of CPUs exclusively. In this
 * case only Solaris supports this since no other OS has this functionality.
 *
 * The implementation of CPU sets on Windows is a bit weird since Windows
 * only supports locking a thread to a set of up to 64 CPUs which has to be
 * part of the same Processor Group. This means that if a cpuset is spanning
 * multiple ProcessorGroups then an individual thread will only be locked to
 * the set of CPUs in one of those Processor Groups.
 *
 * Another problem with this Windows weirdness is in how we identify the
 * processors in Windows. Actually in Windows the processor id is divided
 * in two parts, the first part is the id within the processor group which
 * is a number between 0 and 63. The second part is the processor group
 * number. Since we don't have access to configuration schemes with two-way
 * ids for processors we will use a mapping scheme from processor number
 * to processor group number and processor id within this group. So if your
 * CPU is located in Processor Group 2 and is processor 10 in this group, then
 * your NDB processor number will be 2 * 64 + 10 = 138. Thus the configuation
 * should always use these NDB processor numbers on Windows when using systems
 * with more than 64 logical processors which are becoming fairly commonplace
 * these days.
 */

#ifdef _WIN32
  /* Windows portability layer for CPU locking and thread priorities */

/**
 * These are defines that make up the check of supporting Windows CPU locking.
 * Each time we enter we will call the method
 * is_cpu_locking_supported_on_windows() that will tell us if we have proper
 * support to use CPU locking on Windows.
 *
 */

static unsigned int num_processor_groups = 0;
static unsigned int *num_processors_per_group = NULL;
static bool inited = FALSE;
static bool support_cpu_locking_on_windows = FALSE;

/**
 * On Windows the processors group have different sizes.
 * CPUs are numbered from 0 and upwards and the maximum
 * processor group size is 64. But in a system with 96
 * CPUs there will be 48 CPUs per group.
 *
 * This mapping can even change on different starts.
 *
 * So to avoid overwhelming complexity we will always
 * start a new processor group on a cpu id which is a
 * multiple of 64. This means that in a 96 CPU system
 * we will call the CPUs 0-47 and 64-111.
 *
 * To get the real numbers used by Windows we get
 * Processor Id by ANDing with 63 and to get the
 * ProcessorGroup we divide by 64.
 *
 * So in the above example if we have real CPU 53 it
 * will be CPU 69 in our numbering scheme from which
 * we will derive processor group 1 and processor id
 * equal to 5.
 */
#define GET_PROCESSOR_GROUP(a) ((a)/ 64)
#define GET_PROCESSOR_ID(a) ((a) & 63)
#define NOT_ASSIGNED_TO_PROCESSOR_GROUP 0xFFFF0000

static bool
is_cpu_locking_supported_on_windows()
{
  if (inited)
  {
    return support_cpu_locking_on_windows;
  }
  inited = TRUE;

  num_processor_groups = GetActiveProcessorGroupCount();
  if (num_processor_groups == 0)
  {
    return FALSE;
  }

  num_processors_per_group =
      (unsigned int*)malloc(num_processor_groups * sizeof(unsigned int));
  if (num_processors_per_group == NULL)
  {
    return FALSE;
  }

  for (Uint32 i = 0; i < num_processor_groups; i++)
  {
    num_processors_per_group[i] = GetActiveProcessorCount((WORD)i);
    if (num_processors_per_group[i] == 0)
    {
      return FALSE;
    }
  }

  // Initialization successful -> cpu locking supported!
  support_cpu_locking_on_windows = TRUE;
  return support_cpu_locking_on_windows;
}


static bool
is_cpu_available(unsigned int cpu_id)
{
  unsigned int processor_group = GET_PROCESSOR_GROUP(cpu_id);
  unsigned int processor_id = GET_PROCESSOR_ID(cpu_id);

  if (processor_group >= num_processor_groups)
  {
    return FALSE;
  }
  if (processor_id >= num_processors_per_group[processor_group])
  {
    return FALSE;
  }
  return TRUE;
}

static void
calculate_processor_mask(KAFFINITY *mask,
                         unsigned int processor_group,
                         unsigned int num_cpu_ids,
                         unsigned int *cpu_ids)
{
  *mask = 0;
  for (unsigned int i = 0; i < num_cpu_ids; i++)
  {
    unsigned int cpu_id = cpu_ids[i];
    if (GET_PROCESSOR_GROUP(cpu_id) == processor_group)
    {
      const KAFFINITY cpu0 = 1;
      (*mask) |= (cpu0 << GET_PROCESSOR_ID(cpu_id));
    }
  }
  assert((*mask) != 0);
}

int
NdbThread_SetScheduler(struct NdbThread* pThread,
                       bool rt_prio,
                       bool high_prio)
{
  int windows_prio;
  if (rt_prio && high_prio)
  {
    windows_prio = THREAD_PRIORITY_TIME_CRITICAL;
  }
  else if (rt_prio && !high_prio)
  {
    windows_prio = THREAD_PRIORITY_TIME_CRITICAL;
  }
  else if (!rt_prio && high_prio)
  {
    windows_prio = THREAD_PRIORITY_HIGHEST;
  }
  else
  {
    windows_prio = THREAD_PRIORITY_NORMAL;
  }
  const BOOL ret = SetThreadPriority(pThread->thread_handle, windows_prio);
  if (ret == 0)
  {
    // Failed to set thread priority
    const DWORD error_no = GetLastError();
    return (int)error_no;
  }

  return 0;
}

int
NdbThread_SetThreadPrio(struct NdbThread *pThread,
                        unsigned int prio)
{
  int windows_prio;
  switch (prio)
  {
    case 0:
    case 1:
      windows_prio = THREAD_PRIORITY_LOWEST;
      break;
    case 2:
    case 3:
      windows_prio = THREAD_PRIORITY_BELOW_NORMAL;
      break;
    case 4:
    case 5:
      windows_prio = THREAD_PRIORITY_NORMAL;
      break;
    case 6:
    case 7:
      windows_prio = THREAD_PRIORITY_ABOVE_NORMAL;
      break;
    case 8:
    case 9:
    case 10:
      windows_prio = THREAD_PRIORITY_HIGHEST;
      break;
      break;
    default:
      /* Impossible to reach */
      require(FALSE);
  }
  const BOOL ret = SetThreadPriority(pThread->thread_handle, windows_prio);
  if (ret == 0)
  {
    // Failed to set thread group affinity
    const DWORD error_no = GetLastError();
    return (int)error_no;
  }
  return 0;
}

void
NdbThread_UnassignFromCPUSet(struct NdbThread *pThread,
                                   struct NdbCpuSet *cpu_set)
{
  if (cpu_set == NULL)
  {
    assert(FALSE);
    return;
  }
  unsigned int *cpu_set_ptr = (unsigned int*)cpu_set;
  unsigned int processor_group = pThread->usedProcessorGroupNumber;
  assert(cpu_set_ptr[2 + processor_group] > 0);
  cpu_set_ptr[2 + processor_group]--;
  pThread->usedProcessorGroupNumber = NOT_ASSIGNED_TO_PROCESSOR_GROUP;
  return;
}

int
NdbThread_UnlockCPU(struct NdbThread* pThread)
{
  /* Windows */
  GROUP_AFFINITY new_affinity;

  bzero(&new_affinity, sizeof(GROUP_AFFINITY));

  new_affinity.Mask = pThread->oldProcessorMask;
  new_affinity.Group = pThread->oldProcessorGroupNumber;

  pThread->cpu_set_key = NULL;

  const BOOL ret = SetThreadGroupAffinity(pThread->thread_handle,
                                          &new_affinity,
                                          NULL);
  if (ret == 0)
  {
    // Failed to set thread group affinity
    const DWORD error_no = GetLastError();
    return (int)error_no;
  }

  return 0;
}

int
NdbThread_LockCPU(struct NdbThread* pThread,
                  Uint32 cpu_id,
                  const struct processor_set_handler *cpu_set_key)
{
  /* Windows 7 and later, Windows 2008 Server and later supported */
  (void)pThread;
  (void)cpu_id;
  (void)cpu_set_key;
  if (!is_cpu_locking_supported_on_windows())
  {
    return BIND_CPU_NOT_SUPPORTED_ERROR;
  }
  if (!is_cpu_available(cpu_id))
  {
    return CPU_ID_MISSING_ERROR;
  }
  GROUP_AFFINITY new_affinity;
  GROUP_AFFINITY old_affinity;

  bzero(&new_affinity, sizeof(GROUP_AFFINITY));
  bzero(&old_affinity, sizeof(GROUP_AFFINITY));

  const KAFFINITY cpu0 = 1;
  new_affinity.Mask = (cpu0 << GET_PROCESSOR_ID(cpu_id));
  new_affinity.Group = GET_PROCESSOR_GROUP(cpu_id);

  const BOOL ret = SetThreadGroupAffinity(pThread->thread_handle,
                                          &new_affinity,
                                          &old_affinity);
  if (ret == 0)
  {
    // Failed to set thread group affinity
    const DWORD error_no = GetLastError();
    return (int)error_no;
  }

  pThread->cpu_set_key = cpu_set_key;
  pThread->oldProcessorMask = old_affinity.Mask;
  pThread->oldProcessorGroupNumber = old_affinity.Group;

  return 0;
}

int
NdbThread_LockCreateCPUSet(const Uint32 *cpu_ids,
                           Uint32 num_cpu_ids,
                           struct NdbCpuSet **cpu_set)
{
  /* Supported if Windows 7 or newer or Windows Server 2008 or newer */

  /* Start by verifying CPU ids */
  for (Uint32 i = 0; i < num_cpu_ids; i++)
  {
    if (!is_cpu_available(cpu_ids[i]))
    {
      return CPU_ID_MISSING_ERROR;
    }
  }

  /**
   * The following is the layout of the cpu set information in Windows:
   *
   * n is number of processor groups
   * k is number of processors in CPU set
   *
   * Word 0: Total number of CPUs in the CPU set
   * Word 1: Total number of processor groups
   * Word 2 - (n + 1): Dynamic information about how many threads that
   *                   currently are connected to this processor group.
   * Word (n + 2) - (2*n + 1): Number of cpus in the cpuset that are connected
   *                           to this processor group.
   * Word (2*n + 2) - (2*n + 2 + (k - 1)): CPU ids used in CPU set
   *
   * Based on this static and dynamic information we can calculate where to
   * place the next thread (in which processor group).
   */
  unsigned int *cpu_set_ptr =
    (unsigned int*)malloc( (num_cpu_ids + 2 + (num_processor_groups * 2)) *
                            sizeof(unsigned int));
  if (!cpu_set_ptr)
  {
    int error_no = GetLastError();
    *cpu_set = NULL;
    return error_no;
  }
  cpu_set_ptr[0] = num_cpu_ids;
  cpu_set_ptr[1] = num_processor_groups;
  for (Uint32 i = 0; i < num_processor_groups; i++)
  {
    cpu_set_ptr[2 + i] = 0; /* No threads connected yet */
    cpu_set_ptr[2 + i + num_processor_groups] = 0; /* No CPUs connected yet */
  }
  /* Count the CPUs connected per processor group and assign CPU ids */
  for (Uint32 i = 0; i < num_cpu_ids; i++)
  {
    unsigned int group_id = GET_PROCESSOR_GROUP(cpu_ids[i]);
    cpu_set_ptr[2 + group_id + num_processor_groups]++;
    cpu_set_ptr[(2 + (2 * num_processor_groups)) + i] = cpu_ids[i];
  }
  *cpu_set = (struct NdbCpuSet*)cpu_set_ptr;
  return 0;
}

int
NdbThread_LockCPUSet(struct NdbThread* pThread,
                     struct NdbCpuSet *ndb_cpu_set,
                     const struct processor_set_handler *cpu_set_key)
{
  /* Windows 7 and later, Windows 2008 Server and later supported */
  if (!is_cpu_locking_supported_on_windows())
  {
    return NON_EXCLUSIVE_CPU_SET_NOT_SUPPORTED_ERROR;
  }
  unsigned int used_processor_group;
  unsigned int *cpu_set_ptr = (unsigned int*)ndb_cpu_set;
  unsigned int *dynamic_part = &cpu_set_ptr[2];
  unsigned long long min_so_far = (unsigned long long)-1;
  unsigned long long temp, stat, dynamic;
  unsigned long long num_cpu_ids = (unsigned long long)cpu_set_ptr[0];
  unsigned int *stat_part = &cpu_set_ptr[2 + num_processor_groups];
  int found = FALSE;
  for (unsigned int i = 0; i < num_processor_groups; i++)
  {
    stat = (unsigned long long)stat_part[i];
    if (stat != (unsigned long long)0)
    {
      dynamic = (unsigned long long)dynamic_part[i];
      temp = (num_cpu_ids * dynamic) / stat;
      if (temp < min_so_far)
      {
        used_processor_group = i;
        min_so_far = temp;
        found = TRUE;
      }
    }
  }
  assert(found == TRUE);
  KAFFINITY mask;
  calculate_processor_mask(&mask,
                           used_processor_group,
                           cpu_set_ptr[0],
                           &cpu_set_ptr[2 + (num_processor_groups*2)]);

  GROUP_AFFINITY new_affinity;
  GROUP_AFFINITY old_affinity;

  /**
   * A quirk in the Windows API is that the reserved words in GROUP_AFFINITY
   * has to be zeroed, otherwise Windows will return an error of invalid
   * parameter.
   */
  bzero(&new_affinity, sizeof(GROUP_AFFINITY));
  bzero(&old_affinity, sizeof(GROUP_AFFINITY));

  new_affinity.Mask = mask;
  new_affinity.Group = used_processor_group;

  const BOOL ret = SetThreadGroupAffinity(pThread->thread_handle,
                                          &new_affinity,
                                          &old_affinity);
  if (ret == 0)
  {
    // Failed to set thread group affinity
    const DWORD error_no = GetLastError();
    return (int)error_no;
  }

  cpu_set_ptr[2 + used_processor_group]++;
  pThread->cpu_set_key = cpu_set_key;
  pThread->usedProcessorGroupNumber = used_processor_group;
  pThread->oldProcessorMask = old_affinity.Mask;
  pThread->oldProcessorGroupNumber = old_affinity.Group;

  return 0;
}

int
NdbThread_LockCreateCPUSetExclusive(const Uint32 *cpu_ids,
                                    Uint32 num_cpu_ids,
                                    struct NdbCpuSet **cpu_set)
{
  /* Exclusive cpusets currently only supported on Solaris */
  (void)num_cpu_ids;
  (void)cpu_ids;
  *cpu_set = NULL;
  return EXCLUSIVE_CPU_SET_NOT_SUPPORTED_ERROR;
}

int
NdbThread_LockCPUSetExclusive(struct NdbThread* pThread,
                              struct NdbCpuSet *ndb_cpu_set,
                              const struct processor_set_handler *cpu_set_key)
{
  /* Non-supported OSs, currently only supported on Solaris */
  (void)pThread;
  (void)ndb_cpu_set;
  (void)cpu_set_key;
  return EXCLUSIVE_CPU_SET_NOT_SUPPORTED_ERROR;
}
#endif /* End of Windows part */

#ifndef _WIN32 /* Unix part */
int
NdbThread_UnlockCPU(struct NdbThread* pThread)
{
  /* Unix variants */
  int ret;
  int error_no = 0;

#if defined(HAVE_LINUX_SCHEDULING) || defined(HAVE_CPUSET_SETAFFINITY)
 /* Linux or FreeBSD */
 if (pThread->first_lock_call_non_exclusive)
 {
#if defined HAVE_LINUX_SCHEDULING
    /**
     * Linux variant
     * -------------
     * On Linux one clears CPU locking by assigning it to all available
     * CPUs. Linux will silently ignore any CPUs that isn't part of the
     * HW or VM or other entity that the Linux OS have assigned the
     * thread to stay within. So by assigning it to all potentially
     * available CPUs we are sure that it will be available for all
     * allowed CPUs.
     */
    cpu_set_t cpu_set;
    Uint32 i;
    Uint32 num_cpus = sysconf(_SC_NPROCESSORS_ONLN);

    CPU_ZERO(&cpu_set);
    for (i = 0; i < num_cpus; i++)
    {
      CPU_SET(i, &cpu_set);
    }
    ret= sched_setaffinity(pThread->tid, sizeof(cpu_set_t), &cpu_set);
#else
    /**
     * FreeBSD variant
     * ---------------
     * Start by retrieving root set which is the set of all
     * allowed CPUs for the process. Assign this to the
     * thread.
     */
    cpuset_t mask;
    CPU_ZERO(&mask);
    ret = cpuset_getaffinity(CPU_LEVEL_ROOT,
                             CPU_WHICH_TID,
                             pThread->tid,
                             sizeof(cpuset_t),
                             &mask);
    if (ret)
    {
      ret = cpuset_setaffinity(CPU_LEVEL_WHICH,
                               CPU_WHICH_TID,
                               pThread->tid,
                               sizeof(cpuset_t),
                               &mask);
    }
#endif
    if (ret)
    {
      error_no = errno;
    }
    else
    {
      pThread->first_lock_call_non_exclusive = FALSE;
    }
  }
#elif defined HAVE_SOLARIS_AFFINITY
  /* Solaris */
  if (pThread->first_lock_call_exclusive)
  {
    ret= pset_bind(PS_NONE,
                   P_LWPID,
                   pThread->tid,
                   NULL);
    if (ret)
    {
      error_no = errno;
    }
    else
    {
      pThread->first_lock_call_exclusive = FALSE;
    }
  }
  if (pThread->first_lock_call_non_exclusive)
  {
#if defined HAVE_PROCESSOR_AFFINITY
    /* Solaris 11.2 and newer */
    procset_t ps;
    uint_t flags = PA_CLEAR;
    setprocset(&ps,
               POP_AND,
               P_PID,
               P_MYID,
               P_LWPID,
               pThread->tid);

    ret = processor_affinity(&ps,
                             NULL,
                             NULL,
                             &flags);
#else
    /* Solaris older than 11.2 */
    ret= processor_bind(P_LWPID,
                        pThread->tid,
                        PBIND_NONE,
                        NULL);
#endif
    if (ret)
    {
      error_no = errno;
    }
    else
    {
      pThread->first_lock_call_non_exclusive = FALSE;
    }
  }
#else
  error_no = BIND_CPU_NOT_SUPPORTED_ERROR;
  (void)ret;
#endif
  if (!error_no)
  {
    pThread->cpu_set_key = NULL;
  }
  return error_no;
}

int
NdbThread_LockCPU(struct NdbThread* pThread,
                  Uint32 cpu_id,
                  const struct processor_set_handler *cpu_set_key)
{
#if defined(HAVE_LINUX_SCHEDULING) || defined(HAVE_CPUSET_SETAFFINITY) || \
    defined(HAVE_SOLARIS_AFFINITY)
  /* Unix supported OSs */
  int error_no = 0;
  int ret;
#if defined(HAVE_LINUX_SCHEDULING)
  /* Linux */
  cpu_set_t cpu_set;
#elif defined(HAVE_CPUSET_SETAFFINITY)
  /* FreeBSD */
  cpuset_t cpu_set;
#endif

#if defined(HAVE_LINUX_SCHEDULING) || defined(HAVE_CPUSET_SETAFFINITY)

  /*
    On recent Linux versions the ability to set processor
    affinity is available through the sched_setaffinity call.
    In Linux this is possible to do on thread level so we can
    lock execution thread to one CPU and the rest of the threads
    to another CPU.

    By combining Real-time Scheduling and Locking to CPU we can
    achieve more or less a realtime system for NDB Cluster.

    Linux and FreeBSD
  */

  CPU_ZERO(&cpu_set);
  CPU_SET(cpu_id, &cpu_set);
#if defined HAVE_LINUX_SCHEDULING
  /* Linux */
  ret= sched_setaffinity(pThread->tid, sizeof(cpu_set), &cpu_set);
#else
  /* FreeBSD */
  ret = cpuset_setaffinity(CPU_LEVEL_WHICH,
                           CPU_WHICH_TID,
                           pThread->tid,
                           sizeof(cpu_set),
                           &cpu_set);
#endif

#elif defined HAVE_SOLARIS_AFFINITY
  /*
    Solaris have a number of versions to lock threads to CPU's.
    We'll use the processor_bind interface since we only work
    with single threads here and bind those to CPU's.
  */
  /**
   * Before installing any new CPU locking scheme we have to remove the old
   * CPU locking scheme first. Once this is done we're ready to install the
   * new locking scheme. This is done to avoid that locking schemes are
   * put on top of the old. This can happen in Solaris with multiple levels
   * of CPU locking.
   */
  ret = NdbThread_UnlockCPU(pThread);
  if (ret)
  {
    return ret;
  }
  ret= processor_bind(P_LWPID, pThread->tid, cpu_id, NULL);
#endif
  if (ret)
  {
    error_no= errno;
  }
  if (!error_no)
  {
    pThread->cpu_set_key = cpu_set_key;
    pThread->first_lock_call_non_exclusive = TRUE;
  }
  return error_no;

#else
  /* Non-supported Unices, e.g. Mac OS X */
  (void)pThread;
  (void)cpu_id;
  (void)cpu_set_key;
  return BIND_CPU_NOT_SUPPORTED_ERROR;
#endif
}

int
NdbThread_LockCPUSet(struct NdbThread* pThread,
                     struct NdbCpuSet *ndb_cpu_set,
                     const struct processor_set_handler *cpu_set_key)
{
#if defined(HAVE_LINUX_SCHEDULING) || defined(HAVE_CPUSET_SETAFFINITY) || \
    (defined(HAVE_PROCESSOR_AFFINITY) && defined(HAVE_SOLARIS_AFFINITY))

  /* Unix OS with support */

  int error_no = 0;
  int ret;
#if defined(HAVE_LINUX_SCHEDULING)
  /* Linux */
  cpu_set_t *cpu_set_ptr;
#elif defined HAVE_CPUSET_SETAFFINITY
  /* FreeBSD */
  cpuset_t *cpu_set_ptr;
#elif defined(HAVE_PROCESSOR_AFFINITY) && defined(HAVE_SOLARIS_AFFINITY)
  /* Solaris version >= 11.2 */
  id_t *cpu_set_ptr;
  procset_t ps;
  uint32_t flags;
  uint_t number_of_cpus;
#endif

#if defined HAVE_LINUX_SCHEDULING
  /* Linux */
  cpu_set_ptr = (cpu_set_t*)ndb_cpu_set;

  /* Lock against the bitmask defined by CPUSet */
  ret= sched_setaffinity(pThread->tid,
                         sizeof(cpu_set_t),
                         cpu_set_ptr);
#elif defined HAVE_CPUSET_SETAFFINITY
  /* FreeBSD */
  cpu_set_ptr = (cpuset_t*)ndb_cpu_set;
  /* Lock against the bitmask defined by CPUSet */
  ret= cpuset_setaffinity(CPU_LEVEL_WHICH,
                          CPU_WHICH_TID,
                          pThread->tid,
                          sizeof(cpuset_t),
                          cpu_set_ptr);

#elif defined(HAVE_PROCESSOR_AFFINITY) && defined(HAVE_SOLARIS_AFFINITY)
  /* Solaris 11.2 and later */
  /**
   * Set the thread to only execute on the set of CPUs defined in
   * previous NdbThread_LockCreateCPUSet call. The information in
   * ndb_cpu_set holds number of cpus in the first array position.
   * The remainder of the positions are containing an array of
   * CPU ids.
   *
   * We must first remove the old CPU locking to ensure that this
   * locking isn't put on top of the old locking scheme.
   */
  ret = NdbThread_UnlockCPU(pThread);
  if (ret)
  {
    return ret;
  }
  cpu_set_ptr = (psetid_t*)ndb_cpu_set;
  flags = PA_TYPE_CPU | PA_AFF_STRONG | PA_NO_INH_THR;
  number_of_cpus = (uint_t)cpu_set_ptr[0];
  setprocset(&ps,
             POP_AND, 
             P_PID,
             P_MYID,
             P_LWPID,
             pThread->tid);

  ret = processor_affinity(&ps,
                           &number_of_cpus,
                           &cpu_set_ptr[1],
                           &flags);
#endif

  if (ret)
  {
    error_no = errno;
  }
  if (!error_no)
  {
    pThread->cpu_set_key = cpu_set_key;
    pThread->first_lock_call_non_exclusive = TRUE;
  }
  return error_no;

#else
  /* Unix without support (e.g. Solaris before 11.2 and Mac OS X) */
  (void)pThread;
  (void)ndb_cpu_set;
  (void)cpu_set_key;
  return NON_EXCLUSIVE_CPU_SET_NOT_SUPPORTED_ERROR;
#endif
}

int
NdbThread_LockCreateCPUSet(const Uint32 *cpu_ids,
                           Uint32 num_cpu_ids,
                           struct NdbCpuSet **cpu_set)
{
#if defined(HAVE_LINUX_SCHEDULING) || defined(HAVE_CPUSET_SETAFFINITY)
  /* Supported in newer Linux and FreeBSD versions */
  int error_no;
  Uint32 cpu_id;
  Uint32 i;
#if defined(HAVE_LINUX_SCHEDULING)
  /* Linux */
  cpu_set_t *cpu_set_ptr = (cpu_set_t *)malloc(sizeof(cpu_set_t));
#elif defined(HAVE_CPUSET_SETAFFINITY)
  /* FreeBSD */
  cpuset_t *cpu_set_ptr = (cpuset_t *)malloc(sizeof(cpuset_t));
#endif

  if (!cpu_set_ptr)
  {
    error_no = errno;
    *cpu_set = NULL;
    return error_no;
  }
  CPU_ZERO(cpu_set_ptr);
  for (i = 0; i < num_cpu_ids; i++)
  {
    cpu_id = cpu_ids[i];
    CPU_SET(cpu_id, cpu_set_ptr);
  }
  *cpu_set = (struct NdbCpuSet*)cpu_set_ptr;
  return 0;
#elif defined(HAVE_PROCESSOR_AFFINITY) && defined(HAVE_SOLARIS_AFFINITY)
  /* New interface added in Solaris 11.2 */
  int error_no;
  Uint32 i;
  id_t *cpu_set_ptr = (id_t*)malloc((num_cpu_ids + 1) * sizeof(id_t));
  if (!cpu_set_ptr)
  {
    error_no = errno;
    *cpu_set = NULL;
    return error_no;
  }
  cpu_set_ptr[0] = (id_t)num_cpu_ids;
  for (i = 0; i < num_cpu_ids; i++)
  {
    cpu_set_ptr[i + 1] = (id_t)cpu_ids[i];
  }
  *cpu_set = (struct NdbCpuSet*)cpu_set_ptr;
  return 0;
#else
  /* Non-supported OSs */
  (void)num_cpu_ids;
  (void)cpu_ids;
  *cpu_set = NULL;
  return NON_EXCLUSIVE_CPU_SET_NOT_SUPPORTED_ERROR;
#endif
}

int
NdbThread_LockCreateCPUSetExclusive(const Uint32 *cpu_ids,
                                    Uint32 num_cpu_ids,
                                    struct NdbCpuSet **cpu_set)
{
#if defined HAVE_SOLARIS_AFFINITY
  /* Solaris */
  int ret;
  int error_no;
  Uint32 i;
  psetid_t *cpu_set_ptr = (psetid_t*)malloc(sizeof(psetid_t));

  if (!cpu_set_ptr)
  {
    error_no = errno;
    goto end_error;
  }

  if ((ret = pset_create(cpu_set_ptr)))
  {
    error_no = errno;
    goto error;
  }

  for (i = 0; i < num_cpu_ids; i++)
  {
    if ((ret = pset_assign(*cpu_set_ptr, cpu_ids[i], NULL)))
    {
      error_no = errno;
      goto late_error;
    }
  }
  *cpu_set = (struct NdbCpuSet*)cpu_set_ptr;
  return 0;

late_error:
  pset_destroy(*cpu_set_ptr);
error:
  free(cpu_set_ptr);
end_error:
  *cpu_set = NULL;
  return error_no;

#else
  /* Exclusive cpusets currently only supported on Solaris */
  (void)num_cpu_ids;
  (void)cpu_ids;
  *cpu_set = NULL;
  return EXCLUSIVE_CPU_SET_NOT_SUPPORTED_ERROR;
#endif
}

void
NdbThread_UnassignFromCPUSet(struct NdbThread *pThread,
                                   struct NdbCpuSet *cpu_set)
{
  (void)pThread;
  (void)cpu_set;
  return;
}

int
NdbThread_LockCPUSetExclusive(struct NdbThread* pThread,
                              struct NdbCpuSet *ndb_cpu_set,
                              const struct processor_set_handler *cpu_set_key)
{
#if defined(HAVE_SOLARIS_AFFINITY)
  /* Solaris */
  int error_no = 0;
  int ret;
  psetid_t *cpu_set_ptr;

  ret = NdbThread_UnlockCPU(pThread);
  if (ret)
  {
    return ret;
  }
  cpu_set_ptr = (psetid_t*)ndb_cpu_set;

  /* Lock against Solaris processor set */
  ret= pset_bind(*cpu_set_ptr,
                 P_LWPID,
                 pThread->tid,
                 NULL);
  if (ret)
  {
    error_no = errno;
  }

  if (!error_no)
  {
    pThread->cpu_set_key = cpu_set_key;
    pThread->first_lock_call_exclusive = TRUE;
  }
  return error_no;

#else
  /* Non-supported OSs, currently only supported on Solaris */
  (void)pThread;
  (void)ndb_cpu_set;
  (void)cpu_set_key;
  return EXCLUSIVE_CPU_SET_NOT_SUPPORTED_ERROR;
#endif
}
#endif /* End Unix part */

void
NdbThread_LockDestroyCPUSet(struct NdbCpuSet *cpu_set)
{
  if (cpu_set != NULL)
  {
    free(cpu_set);
  }
}

void
NdbThread_LockDestroyCPUSetExclusive(struct NdbCpuSet *cpu_set)
{
  if (cpu_set != NULL)
  {
#if defined HAVE_SOLARIS_AFFINITY
    /* Solaris */
    pset_destroy(*((psetid_t*)cpu_set));
#endif
    free(cpu_set);
  }
}

const struct processor_set_handler*
NdbThread_LockGetCPUSetKey(struct NdbThread* pThread)
{
  return pThread->cpu_set_key;
}

struct EmulatedJamBuffer;
thread_local EmulatedJamBuffer* NDB_THREAD_TLS_JAM= nullptr;
struct thr_data;
thread_local thr_data* NDB_THREAD_TLS_THREAD= nullptr;
thread_local NdbThread* NDB_THREAD_TLS_NDB_THREAD= nullptr;

#ifdef NDB_DEBUG_RES_OWNERSHIP
thread_local Uint32 NDB_THREAD_TLS_RES_OWNER= 0;
#endif

struct NdbThread* NdbThread_GetNdbThread()
{
  return NDB_THREAD_TLS_NDB_THREAD;
}

extern "C" int
NdbThread_Init()
{
#ifdef _WIN32
  (void)is_cpu_locking_supported_on_windows();
#endif
  ndb_thread_mutex = NdbMutex_Create();
  ndb_thread_condition = NdbCondition_Create();
  NdbThread_CreateObject(0);
  return 0;
}

extern "C" void
NdbThread_End()
{
  if (ndb_thread_mutex)
  {
    NdbMutex_Destroy(ndb_thread_mutex);
  }

  if (ndb_thread_condition)
  {
    NdbCondition_Destroy(ndb_thread_condition);
  }

  if (g_main_thread)
  {
    free(g_main_thread);
    g_main_thread = 0;
  }
}

int
NdbThread_SetHighPrioProperties(const char * spec)
{
  char * copy = 0;
  char * prio = 0;
  int found = 0;

  if (spec == 0)
  {
    f_high_prio_set = 0;
    return 0;
  }

  /**
   * strip space/tab from beginning of string
   */
  while ((* spec == ' ') || (*spec == '\t'))
    spec++;

  copy = strdup(spec);
  if (copy == 0)
    return -1;

  /**
   * is there a "," in spec
   */
  prio = strchr(copy, ',');
  if (prio)
  {
    * prio = 0;
    prio++;
  }

  if (prio && strchr(prio, ','))
  {
    /**
     * extra prio??
     */
    free(copy);
    return -1;
  }

#ifdef HAVE_PTHREAD_SETSCHEDPARAM
  found = 0;
#ifdef SCHED_FIFO
  if (strcmp("fifo", copy) == 0)
  {
    found = 1;
    f_high_prio_policy = SCHED_FIFO;
  }
#endif
#ifdef SCHED_RR
  if (strcmp("rr", copy) == 0)
  {
    found = 1;
    f_high_prio_policy = SCHED_RR;
  }
#endif
  if (!found)
  {
    free(copy);
    return -1;
  }

  f_high_prio_prio = 50;
  if (prio)
  {
    char * endptr = 0;
    long p = strtol(prio, &endptr, 10);
    if (prio == endptr)
    {
      free(copy);
      return -1;
    }
    f_high_prio_prio = (int)p;
  }
  f_high_prio_set = 1;
  free(copy);
  return 0;
#else
  return 0;
#endif
}
