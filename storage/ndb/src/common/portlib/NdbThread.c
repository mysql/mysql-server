/*
   Copyright (c) 2003, 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/


#include <ndb_global.h>
#include <NdbThread.h>
#include <my_pthread.h>
#include <NdbMem.h>
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
#elif defined HAVE_SOLARIS_AFFINITY
#include <sys/types.h>
#include <sys/lwp.h>
#include <sys/processor.h>
#include <sys/procset.h>
#endif

static int g_min_prio = 0;
static int g_max_prio = 0;
static int g_prio = 0;

static NdbMutex *g_ndb_thread_mutex = 0;
static struct NdbCondition * g_ndb_thread_condition = 0;

#ifdef NDB_SHM_TRANSPORTER
int g_ndb_shm_signum= 0;
#endif

static int f_high_prio_set = 0;
static int f_high_prio_policy;
static int f_high_prio_prio;

struct NdbThread 
{ 
  volatile int inited;
  pthread_t thread;
#if defined HAVE_SOLARIS_AFFINITY
  id_t tid;
#elif defined HAVE_LINUX_SCHEDULING
  pid_t tid;
#endif
  char thread_name[16];
  NDB_THREAD_FUNC * func;
  void * object;
};

#ifdef NDB_SHM_TRANSPORTER
void NdbThread_set_shm_sigmask(my_bool block)
{
  if (g_ndb_shm_signum)
  {
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, g_ndb_shm_signum);
    if (block)
      pthread_sigmask(SIG_BLOCK, &mask, 0);
    else
      pthread_sigmask(SIG_UNBLOCK, &mask, 0);
  }
  return;
}
#endif

static
void
settid(struct NdbThread * thr)
{
#if defined HAVE_SOLARIS_AFFINITY
  thr->tid = _lwp_self();
#elif defined HAVE_LINUX_SCHEDULING
  thr->tid = syscall(SYS_gettid);
  if (thr->tid == (pid_t)-1)
  {
    /*
      This extra check is from suggestion by Kristian Nielsen
      to handle cases when running binaries on LinuxThreads
      compiled with NPTL threads
    */
    thr->tid = getpid();
  }
#endif
}

int
NdbThread_GetTid(struct NdbThread* thr)
{
#if defined HAVE_SOLARIS_AFFINITY
  return (int)thr->tid;
#elif defined HAVE_LINUX_SCHEDULING
  return (int)thr->tid;
#endif
  return -1;
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
      pthread_sigmask(SIG_BLOCK, &mask, 0);
    }      
#endif

    {
      void *ret;
      struct NdbThread * ss = (struct NdbThread *)_ss;
      settid(ss);
      NdbMutex_Lock(g_ndb_thread_mutex);
      ss->inited = 1;
      NdbCondition_Signal(g_ndb_thread_condition);
      NdbMutex_Unlock(g_ndb_thread_mutex);
      ret= (* ss->func)(ss->object);
      DBUG_POP();
      NdbThread_Exit(ret);
    }
  /* will never be reached */
    DBUG_RETURN(0);
  }
}


struct NdbThread*
NdbThread_CreateObject(const char * name)
{
  struct NdbThread* tmpThread;
  DBUG_ENTER("NdbThread_Create");

  tmpThread = (struct NdbThread*)NdbMem_Allocate(sizeof(struct NdbThread));
  if (tmpThread == NULL)
    DBUG_RETURN(NULL);

  bzero(tmpThread, sizeof(* tmpThread));
  if (name)
  {
    strnmov(tmpThread->thread_name, name, sizeof(tmpThread->thread_name));
  }
  else
  {
    strnmov(tmpThread->thread_name, "main", sizeof(tmpThread->thread_name));
  }

#ifdef HAVE_PTHREAD_SELF
  tmpThread->thread = pthread_self();
#elif defined HAVE_GETPID
  tmpThread->thread = getpid();
#endif
  settid(tmpThread);
  tmpThread->inited = 1;

  return tmpThread;
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
  pthread_attr_t thread_attr;
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

  tmpThread = (struct NdbThread*)NdbMem_Allocate(sizeof(struct NdbThread));
  if (tmpThread == NULL)
    DBUG_RETURN(NULL);

  DBUG_PRINT("info",("thread_name: %s", p_thread_name));

  strnmov(tmpThread->thread_name,p_thread_name,sizeof(tmpThread->thread_name));

  pthread_attr_init(&thread_attr);
#ifdef PTHREAD_STACK_MIN
  if (thread_stack_size < PTHREAD_STACK_MIN)
    thread_stack_size = PTHREAD_STACK_MIN;
#endif
  DBUG_PRINT("info", ("stack_size: %llu", (ulonglong)thread_stack_size));
  pthread_attr_setstacksize(&thread_attr, thread_stack_size);
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

  NdbMutex_Lock(g_ndb_thread_mutex);
  result = pthread_create(&tmpThread->thread, 
			  &thread_attr,
  		          ndb_thread_wrapper,
  		          tmpThread);

  pthread_attr_destroy(&thread_attr);

  if (result != 0)
  {
    NdbMem_Free((char *)tmpThread);
    NdbMutex_Unlock(g_ndb_thread_mutex);
    return 0;
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
    NdbCondition_WaitTimeout(g_ndb_thread_condition, g_ndb_thread_mutex, 100);
  } while (tmpThread->inited == 0);

  NdbMutex_Unlock(g_ndb_thread_mutex);

  DBUG_PRINT("exit",("ret: 0x%lx", (long) tmpThread));
  DBUG_RETURN(tmpThread);
}


void NdbThread_Destroy(struct NdbThread** p_thread)
{
  DBUG_ENTER("NdbThread_Destroy");
  if (*p_thread != NULL){
    DBUG_PRINT("enter",("*p_thread: 0x%lx", (long) *p_thread));
    free(* p_thread); 
    * p_thread = 0;
  }
  DBUG_VOID_RETURN;
}


int NdbThread_WaitFor(struct NdbThread* p_wait_thread, void** status)
{
  int result;

  if (p_wait_thread == NULL)
    return 0;

  if (p_wait_thread->thread == 0)
    return 0;

  result = pthread_join(p_wait_thread->thread, status);
  
  return result;
}


void NdbThread_Exit(void *status)
{
  my_thread_end();
  pthread_exit(status);
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
get_prio(my_bool rt_prio, my_bool high_prio, int policy)
{
  if (!rt_prio)
    return 0;
  if (g_prio != 0)
    return g_prio;
  g_max_prio = get_max_prio(policy);
  g_min_prio = get_min_prio(policy);
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
    g_prio = g_min_prio + 3;
  else
    g_prio = g_min_prio + 1;
  if (g_prio < g_min_prio)
    g_prio = g_min_prio;
  return g_prio;
}

int
NdbThread_SetScheduler(struct NdbThread* pThread,
                       my_bool rt_prio,
                       my_bool high_prio)
{
  int error_no= 0;
#if defined HAVE_LINUX_SCHEDULING
  int ret, policy, prio;
  struct sched_param loc_sched_param;
  if (rt_prio)
  {
    policy = SCHED_RR;
    prio = get_prio(rt_prio, high_prio, policy);
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
    prio = get_prio(rt_prio, high_prio, policy);
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
  error_no = ENOSYS;
#endif

  return error_no;
}

int
NdbThread_LockCPU(struct NdbThread* pThread, Uint32 cpu_id)
{
  int error_no = 0;
#if defined HAVE_LINUX_SCHEDULING

  /*
    On recent Linux versions the ability to set processor
    affinity is available through the sched_setaffinity call.
    In Linux this is possible to do on thread level so we can
    lock execution thread to one CPU and the rest of the threads
    to another CPU.

    By combining Real-time Scheduling and Locking to CPU we can
    achieve more or less a realtime system for NDB Cluster.
  */
  int ret;
  cpu_set_t cpu_set;
  CPU_ZERO(&cpu_set);
  CPU_SET(cpu_id, &cpu_set);
  ret= sched_setaffinity(pThread->tid, sizeof(cpu_set), &cpu_set);
  if (ret)
    error_no = errno;
#elif defined HAVE_SOLARIS_AFFINITY
  /*
    Solaris have a number of versions to lock threads to CPU's.
    We'll use the processor_bind interface since we only work
    with single threads and bind those to CPU's.
    A bit unclear as whether the id returned by pthread_self
    is the LWP id.
  */
  int ret;
  ret= processor_bind(P_LWPID, pThread->tid, cpu_id, NULL);
  if (ret)
    error_no= errno;
#else
  error_no = ENOSYS;
#endif
  return error_no;
}

static pthread_key(void*, tls_keys[NDB_THREAD_TLS_MAX]);

void *NdbThread_GetTlsKey(NDB_THREAD_TLS key)
{
  return pthread_getspecific(tls_keys[key]);
}

void NdbThread_SetTlsKey(NDB_THREAD_TLS key, void *value)
{
  pthread_setspecific(tls_keys[key], value);
}

int
NdbThread_Init()
{ 
  g_ndb_thread_mutex = NdbMutex_Create();
  g_ndb_thread_condition = NdbCondition_Create();
  pthread_key_create(&(tls_keys[NDB_THREAD_TLS_JAM]), NULL);
  pthread_key_create(&(tls_keys[NDB_THREAD_TLS_THREAD]), NULL);
  return 0;
}

void
NdbThread_End()
{
  if (g_ndb_thread_mutex)
  {
    NdbMutex_Destroy(g_ndb_thread_mutex);
  }
  
  if (g_ndb_thread_condition)
  {
    NdbCondition_Destroy(g_ndb_thread_condition);
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
