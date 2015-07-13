/*
   Copyright (c) 2003, 2015, Oracle and/or its affiliates. All rights reserved.

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
#include <my_thread.h>
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
#include <sys/pset.h>
#endif

#ifdef NDB_MUTEX_DEADLOCK_DETECTOR
#include "NdbMutex_DeadlockDetector.h"
#endif

#if defined(HAVE_LINUX_SCHEDULING) || defined(HAVE_PTHREAD_SET_SCHEDPARAM)
static int g_min_prio = 0;
static int g_max_prio = 0;
static my_bool get_prio_first = TRUE;
#endif

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
  my_thread_t thread;
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
#endif
#if defined HAVE_LINUX_SCHEDULING
  pid_t tid;
  struct NdbCpuSet *orig_cpu_set;
#elif defined HAVE_SOLARIS_AFFINITY
  /* Our thread id */
  id_t tid;
  /* Have we called any lock to CPU function yet for this thread */
  my_bool first_lock_call;
  /* Original processor set locked to */
  psetid_t orig_proc_set;
  /* Original processor locked to */
  processorid_t orig_processor_id;
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
#if defined HAVE_LINUX_SCHEDULING
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
#elif defined HAVE_SOLARIS_AFFINITY
  thr->tid = (id_t)_lwp_self();
#else
  (void)thr;
#endif
}

int
NdbThread_GetTid(struct NdbThread* thr)
{
#if defined HAVE_LINUX_SCHEDULING
  return (int)thr->tid;
#elif defined HAVE_SOLARIS_AFFINITY
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
        OpenThread(SYNCHRONIZE, FALSE, GetCurrentThreadId());
#endif

#ifdef NDB_MUTEX_DEADLOCK_DETECTOR
      ndb_mutex_thread_init(&ss->m_mutex_thr_state);
#endif
      NdbThread_SetTlsKey(NDB_THREAD_TLS_NDB_THREAD, ss);
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

  tmpThread = (struct NdbThread*)NdbMem_Allocate(sizeof(struct NdbThread));
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

  tmpThread = (struct NdbThread*)NdbMem_Allocate(sizeof(struct NdbThread));
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
#ifdef HAVE_LINUX_SCHEDULING
  tmpThread->orig_cpu_set = NULL;
#elif HAVE_SOLARIS_AFFINITY
  tmpThread->first_lock_call = TRUE;
#endif

  NdbMutex_Lock(g_ndb_thread_mutex);
  result = my_thread_create(&thread_handle,
			    &thread_attr,
  		            ndb_thread_wrapper,
  		            tmpThread);
  tmpThread->thread= thread_handle.thread;

  my_thread_attr_destroy(&thread_attr);

  if (result != 0)
  {
    NdbMem_Free(tmpThread);
    NdbMutex_Unlock(g_ndb_thread_mutex);
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
#ifdef _WIN32
    HANDLE thread_handle = (*p_thread)->thread_handle;
    if (thread_handle)
      CloseHandle(thread_handle);
#endif
    DBUG_PRINT("enter",("*p_thread: 0x%lx", (long) *p_thread));
#ifdef HAVE_LINUX_SCHEDULING
    if ((*p_thread)->orig_cpu_set)
    {
      free((*p_thread)->orig_cpu_set);
      (*p_thread)->orig_cpu_set = NULL;
    }
#endif
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
get_prio(my_bool high_prio, int policy)
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
NdbThread_yield_rt(struct NdbThread* pThread, my_bool high_prio)
{
  int res = NdbThread_SetScheduler(pThread, FALSE, high_prio);
  int res1 = NdbThread_SetScheduler(pThread, TRUE, high_prio);
  if (res || res1)
    return res;
  return 0;
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

int
NdbThread_UnlockCPU(struct NdbThread* pThread)
{
  int ret;
  int error_no = 0;

#if defined HAVE_LINUX_SCHEDULING
 if (pThread->orig_cpu_set != NULL)
 {
    cpu_set_t *cpu_set_ptr = (cpu_set_t *)pThread->orig_cpu_set;
    ret= sched_setaffinity(pThread->tid, sizeof(cpu_set_t), cpu_set_ptr);
    if (ret)
    {
      error_no = errno;
    }
    free(pThread->orig_cpu_set);
    pThread->orig_cpu_set = NULL;
  }
#elif defined HAVE_SOLARIS_AFFINITY
  if (!pThread->first_lock_call)
  {
    ret= pset_bind(pThread->orig_proc_set,
                   P_LWPID,
                   pThread->tid,
                   NULL);
    if (ret)
    {
      error_no = errno;
    }
    else
    {
      ret= processor_bind(P_LWPID,
                          pThread->tid,
                          pThread->orig_processor_id,
                          NULL);
      if (ret)
      {
        error_no = errno;
      }
      else
      {
        pThread->first_lock_call = TRUE;
      }
    }
  }
#else
  (void)ret;
#endif
  pThread->cpu_set_key = NULL;
  return error_no;
}

static int
set_old_cpu_locking(struct NdbThread* pThread)
{
  int error_no = 0;
  int ret;
#if defined HAVE_LINUX_SCHEDULING
  if (pThread->orig_cpu_set == NULL)
  {
    cpu_set_t *old_cpu_set_ptr = malloc(sizeof(cpu_set_t));
    if (!old_cpu_set_ptr)
    {
      error_no = errno;
      goto end;
    }
    ret = sched_getaffinity(pThread->tid, sizeof(cpu_set_t), old_cpu_set_ptr);
    if (ret)
    {
      error_no = errno;
      goto end;
    }
    pThread->orig_cpu_set = (struct NdbCpuSet*)old_cpu_set_ptr;
  }
#elif defined HAVE_SOLARIS_AFFINITY
  if (pThread->first_lock_call)
  {
    ret = pset_bind(PS_QUERY, P_LWPID, pThread->tid, &pThread->orig_proc_set);
    if (ret)
    {
      error_no= errno;
      goto end;
    }
    ret= processor_bind(P_LWPID,
                        pThread->tid,
                        PBIND_QUERY,
                        &pThread->orig_processor_id);
    if (ret)
    {
      error_no= errno;
      goto end;
    }
    pThread->first_lock_call = FALSE;
  }
#else
  (void)ret;
  (void)pThread;
  goto end;
#endif
end:
  return error_no;
}

int
NdbThread_LockCreateCPUSet(const Uint32 *cpu_ids,
                           Uint32 num_cpu_ids,
                           struct NdbCpuSet **cpu_set)
{
#if defined HAVE_LINUX_SCHEDULING
  int error_no;
  Uint32 cpu_id;
  Uint32 i;
  cpu_set_t *cpu_set_ptr = malloc(sizeof(cpu_set_t));

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
#elif defined HAVE_SOLARIS_AFFINITY
  int ret;
  int error_no;
  Uint32 i;
  psetid_t *cpu_set_ptr = malloc(sizeof(psetid_t));

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
  (void)num_cpu_ids;
  (void)cpu_ids;
  *cpu_set = NULL;
  return 0;
#endif
}

void
NdbThread_LockDestroyCPUSet(struct NdbCpuSet *cpu_set)
{
  if (cpu_set != NULL)
  {
#if defined HAVE_LINUX_SCHEDULING
  /* Empty */
#elif defined HAVE_SOLARIS_AFFINITY
    pset_destroy(*((psetid_t*)cpu_set));
#endif
    free(cpu_set);
  }
}

int
NdbThread_LockCPUSet(struct NdbThread* pThread,
                     struct NdbCpuSet *ndb_cpu_set,
                     const struct processor_set_handler *cpu_set_key)
{
  int error_no;
  int ret;
#if defined(HAVE_LINUX_SCHEDULING)
  cpu_set_t *cpu_set_ptr;
#elif defined(HAVE_SOLARIS_AFFINITY)
  psetid_t *cpu_set_ptr;
#endif

  if ((error_no = set_old_cpu_locking(pThread)))
    goto end;

  if (ndb_cpu_set == NULL)
  {
    return 0;
  }
#if defined HAVE_LINUX_SCHEDULING
  cpu_set_ptr = (cpu_set_t*)ndb_cpu_set;

  /* Lock against the bitmask defined by CPUSet */
  ret= sched_setaffinity(pThread->tid,
                         sizeof(cpu_set_t),
                         cpu_set_ptr);
  if (ret)
  {
    error_no = errno;
  }
  goto end;
#elif defined HAVE_SOLARIS_AFFINITY
  cpu_set_ptr = (psetid_t*)ndb_cpu_set;

  /* Lock against Solaris processor set */
  ret= pset_bind(*cpu_set_ptr,
                 P_LWPID,
                 pThread->tid,
                 NULL);
  if (ret)
  {
    error_no = errno;
    goto end;
  }
  return 0;
#else
  (void) ret;
  goto end;
#endif

end:
  if (!error_no)
    pThread->cpu_set_key = cpu_set_key;
  return error_no;
}

const struct processor_set_handler*
NdbThread_LockGetCPUSetKey(struct NdbThread* pThread)
{
  return pThread->cpu_set_key;
}

int
NdbThread_LockCPU(struct NdbThread* pThread,
                  Uint32 cpu_id,
                  const struct processor_set_handler *cpu_set_key)
{
  int error_no;
  int ret;
#if defined HAVE_LINUX_SCHEDULING
  cpu_set_t cpu_set;
#endif

  if ((error_no = set_old_cpu_locking(pThread)))
    goto end;

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

  CPU_ZERO(&cpu_set);
  CPU_SET(cpu_id, &cpu_set);
  ret= sched_setaffinity(pThread->tid, sizeof(cpu_set), &cpu_set);
  if (ret)
  {
    error_no = errno;
  }
#elif defined HAVE_SOLARIS_AFFINITY
  /*
    Solaris have a number of versions to lock threads to CPU's.
    We'll use the processor_bind interface since we only work
    with single threads and bind those to CPU's.
    A bit unclear as whether the id returned by pthread_self
    is the LWP id.
  */
  ret= processor_bind(P_LWPID, pThread->tid, cpu_id, NULL);
  if (ret)
  {
    error_no= errno;
  }
#else
  (void) ret;
  (void) cpu_id;
  error_no = ENOSYS;
  goto end;
#endif
end:
  if (!error_no)
    pThread->cpu_set_key = cpu_set_key;
  return error_no;
}

#ifndef NDB_MUTEX_DEADLOCK_DETECTOR
static thread_local_key_t tls_keys[NDB_THREAD_TLS_MAX];
#else
static thread_local_key_t tls_keys[NDB_THREAD_TLS_MAX + 1];
#endif

struct NdbThread* NdbThread_GetNdbThread()
{
  return (struct NdbThread*)NdbThread_GetTlsKey(NDB_THREAD_TLS_NDB_THREAD);
}

void *NdbThread_GetTlsKey(NDB_THREAD_TLS key)
{
  return my_get_thread_local(tls_keys[key]);
}

void NdbThread_SetTlsKey(NDB_THREAD_TLS key, void *value)
{
  my_set_thread_local(tls_keys[key], value);
}

int
NdbThread_Init()
{ 
  g_ndb_thread_mutex = NdbMutex_Create();
  g_ndb_thread_condition = NdbCondition_Create();
  my_create_thread_local_key(&(tls_keys[NDB_THREAD_TLS_JAM]), NULL);
  my_create_thread_local_key(&(tls_keys[NDB_THREAD_TLS_THREAD]), NULL);
  my_create_thread_local_key(&(tls_keys[NDB_THREAD_TLS_NDB_THREAD]), NULL);
#ifdef NDB_MUTEX_DEADLOCK_DETECTOR
  my_create_thread_local_key(&(tls_keys[NDB_THREAD_TLS_MAX]), NULL);
#endif
  NdbThread_CreateObject(0);
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

  if (g_main_thread)
  {
    NdbMem_Free(g_main_thread);
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
