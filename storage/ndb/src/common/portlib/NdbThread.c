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

#ifdef HAVE_LINUX_SCHEDULING
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <sys/types.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <sched.h>
#endif

#ifdef HAVE_SOLARIS_AFFINITY
#include <sys/types.h>
#include <sys/lwp.h>
#include <sys/processor.h>
#include <sys/procset.h>
#endif

#define MAX_THREAD_NAME 16

static int g_min_prio = 0;
static int g_max_prio = 0;
static int g_prio = 0;

/*#define USE_PTHREAD_EXTRAS*/

#ifdef NDB_SHM_TRANSPORTER
int g_ndb_shm_signum= 0;
#endif

static int f_high_prio_set = 0;
static int f_high_prio_policy;
static int f_high_prio_prio;

struct NdbThread 
{ 
  pthread_t thread;
  char thread_name[MAX_THREAD_NAME];
  NDB_THREAD_FUNC * func;
  void * object;
  NDB_THREAD_FUNC *start_func;
  NDB_THREAD_FUNC *end_func;
  my_bool same_start_end_object;
  char start_object[THREAD_CONTAINER_SIZE];
  char end_object[THREAD_CONTAINER_SIZE];
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
void*
ndb_thread_wrapper(void* _ss){
  my_thread_init();
  {
    DBUG_ENTER("ndb_thread_wrapper");
#ifdef NDB_SHM_TRANSPORTER
    NdbThread_set_shm_sigmask(TRUE);
#endif
    {
      /**
       * Block all signals to thread by default
       *   let them go to main process instead
       */
      sigset_t mask;
      sigfillset(&mask);
      pthread_sigmask(SIG_BLOCK, &mask, 0);
    }      
    
    {
      void *ret;
      struct NdbThread * ss = (struct NdbThread *)_ss;
      if (ss->start_func)
        (*ss->start_func)(ss->start_object);
      ret= (* ss->func)(ss->object);
      if (ss->end_func)
      {
        if (ss->same_start_end_object)
          (*ss->end_func)(ss->start_object);
        else
          (*ss->end_func)(ss->end_object);
      }
      DBUG_POP();
      NdbThread_Exit(ret);
    }
  /* will never be reached */
    DBUG_RETURN(0);
  }
}


struct NdbThread* NdbThread_Create(NDB_THREAD_FUNC *p_thread_func,
                      NDB_THREAD_ARG *p_thread_arg,
  		      const NDB_THREAD_STACKSIZE _thread_stack_size,
		      const char* p_thread_name,
                      NDB_THREAD_PRIO thread_prio)
{
  return NdbThread_CreateWithFunc(p_thread_func, p_thread_arg, _thread_stack_size,
                                  p_thread_name, thread_prio,
                                  NULL, NULL, 0, NULL, NULL, 0);
}
                      
struct NdbThread* NdbThread_CreateWithFunc(NDB_THREAD_FUNC *p_thread_func,
                      NDB_THREAD_ARG *p_thread_arg,
  		      const NDB_THREAD_STACKSIZE _thread_stack_size,
		      const char* p_thread_name,
                      NDB_THREAD_PRIO thread_prio,
                      NDB_THREAD_FUNC *start_func,
                      NDB_THREAD_ARG start_obj,
                      size_t start_obj_len,
                      NDB_THREAD_FUNC *end_func,
                      NDB_THREAD_ARG end_obj,
                      size_t end_obj_len)
{
  struct NdbThread* tmpThread;
  int result;
  pthread_attr_t thread_attr;
  NDB_THREAD_STACKSIZE thread_stack_size;

  DBUG_ENTER("NdbThread_Create");

  /* Use default stack size if 0 specified */
  if (_thread_stack_size == 0)
    thread_stack_size = 64 * 1024 * SIZEOF_CHARP/4;
  else
    thread_stack_size = _thread_stack_size * SIZEOF_CHARP/4;

  (void)thread_prio; /* remove warning for unused parameter */

  if (p_thread_func == NULL)
    DBUG_RETURN(NULL);

  tmpThread = (struct NdbThread*)NdbMem_Allocate(sizeof(struct NdbThread));
  if (tmpThread == NULL)
    DBUG_RETURN(NULL);

  DBUG_PRINT("info",("thread_name: %s", p_thread_name));

  strnmov(tmpThread->thread_name,p_thread_name,sizeof(tmpThread->thread_name));

  tmpThread->start_func = start_func;
  memcpy(tmpThread->start_object, start_obj, start_obj_len);
  tmpThread->end_func = end_func;
  memcpy(tmpThread->end_object, end_obj, end_obj_len);
  if (start_obj == end_obj)
    tmpThread->same_start_end_object = TRUE;
  else
    tmpThread->same_start_end_object = FALSE;

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
  tmpThread->func= p_thread_func;
  tmpThread->object= p_thread_arg;
  result = pthread_create(&tmpThread->thread, 
			  &thread_attr,
  		          ndb_thread_wrapper,
  		          tmpThread);
  if (result != 0)
  {
    NdbMem_Free((char *)tmpThread);
    tmpThread = 0;
  }

  if (result == 0 && thread_prio == NDB_THREAD_PRIO_HIGH && f_high_prio_set)
  {
#ifdef HAVE_PTHREAD_SETSCHEDPARAM
    struct sched_param param;
    bzero(&param, sizeof(param));
    param.sched_priority = f_high_prio_prio;
    if (pthread_setschedparam(tmpThread->thread, f_high_prio_policy, &param))
      perror("pthread_setschedparam failed");
#endif
  }

  pthread_attr_destroy(&thread_attr);
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

NDB_TID_TYPE
NdbThread_getThreadId()
{
#ifdef HAVE_LINUX_SCHEDULING
  pid_t tid = syscall(SYS_gettid);
  if (tid == (pid_t)-1)
  {
    /*
      This extra check is from suggestion by Kristian Nielsen
      to handle cases when running binaries on LinuxThreads
      compiled with NPTL threads
    */
    tid = getpid();
  }
  return tid;
#else
#ifdef HAVE_SOLARIS_AFFINITY
  id_t tid;
  tid = _lwp_self();
  return tid;
#else
  return 0;
#endif
#endif
}

NDB_THAND_TYPE
NdbThread_getThreadHandle()
{
#ifdef HAVE_LINUX_SCHEDULING
  pid_t tid = syscall(SYS_gettid);
  if (tid == (pid_t)-1)
    tid = getpid();
  return tid;
#else
#ifdef HAVE_PTHREAD_SELF
  return pthread_self();
#else
  return 0;
#endif
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
NdbThread_SetScheduler(NDB_THAND_TYPE threadHandle, my_bool rt_prio,
                       my_bool high_prio)
{
  int policy, prio, error_no= 0;
#ifdef HAVE_LINUX_SCHEDULING
  int ret;
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
  memset((char*)&loc_sched_param, 0, sizeof(loc_sched_param));
  loc_sched_param.sched_priority = prio;
  ret= sched_setscheduler(threadHandle, policy, &loc_sched_param);
  if (ret)
    error_no= errno;
#else
#ifdef HAVE_PTHREAD_SET_SCHEDPARAM
  /*
    This variant is POSIX compliant so should be useful on most
    Operating Systems supporting real-time scheduling.
  */
  int ret;
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
  memset((char*)&loc_sched_param, sizeof(loc_sched_param), 0);
  loc_sched_param.sched_priority = prio;
  ret= pthread_setschedparam(threadHandle, policy, &loc_sched_param);
  if (ret)
    error_no= errno;
#endif
#endif
  return error_no;
}

int
NdbThread_LockCPU(NDB_TID_TYPE threadId, Uint32 cpu_id)
{
  int error_no= 0;
#ifdef HAVE_LINUX_SCHEDULING
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
  ret= sched_setaffinity(threadId, sizeof(ulong),
                         (const cpu_set_t *)&cpu_set);
  if (ret)
    error_no= errno;
#else
#ifdef HAVE_SOLARIS_AFFINITY
  /*
    Solaris have a number of versions to lock threads to CPU's.
    We'll use the processor_bind interface since we only work
    with single threads and bind those to CPU's.
    A bit unclear as whether the id returned by pthread_self
    is the LWP id.
  */
  int ret;
  ret= processor_bind(P_LWPID, threadId, cpu_id, NULL); 
  if (ret)
    error_no= errno;
#else
#ifdef WIN32
  /*
    Windows can currently as far I found out only lock processes
    to CPU's, thus it cannot be used to support the desirable
    feture here. So we ignore the call on Windows for the moment.
  */
  return error_no;
#else
  return error_no;
#endif
#endif
#endif
  return error_no;
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
