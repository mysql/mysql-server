/* Copyright (c) 2008, 2020, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <ndb_global.h>

#define NDBD_MULTITHREADED

#include <VMSignal.hpp>
#include <kernel_types.h>
#include <Prio.hpp>
#include <SignalLoggerManager.hpp>
#include <SimulatedBlock.hpp>
#include <ErrorHandlingMacros.hpp>
#include <GlobalData.hpp>
#include <WatchDog.hpp>
#include <TransporterDefinitions.hpp>
#include <TransporterRegistry.hpp>
#include "FastScheduler.hpp"
#include "mt.hpp"
#include <DebuggerNames.hpp>
#include <signaldata/StopForCrash.hpp>
#include "TransporterCallbackKernel.hpp"
#include <NdbSleep.h>
#include <NdbGetRUsage.h>
#include <portlib/ndb_prefetch.h>
#include <blocks/pgman.hpp>
#include <blocks/thrman.hpp>
#include <Pool.hpp>
#include <NdbSpin.h>

#include "mt-asm.h"
#include "mt-lock.hpp"

#include "ThreadConfig.hpp"
#include <signaldata/StartOrd.hpp>

#include <NdbTick.h>
#include <NdbMutex.h>
#include <NdbCondition.h>
#include <ErrorReporter.hpp>
#include <EventLogger.hpp>

extern EventLogger * g_eventLogger;

#if (defined(VM_TRACE) || defined(ERROR_INSERT))
//#define DEBUG_MULTI_TRP 1
#endif

#ifdef DEBUG_MULTI_TRP
#define DEB_MULTI_TRP(arglist) do { g_eventLogger->info arglist ; } while (0)
#else
#define DEB_MULTI_TRP(arglist) do { } while (0)
#endif

/**
 * Two new manual(recompile) error-injections in mt.cpp :
 *
 *     NDB_BAD_SEND : Causes send buffer code to mess with a byte in a send buffer
 *     NDB_LUMPY_SEND : Causes transporters to be given small, oddly aligned and
 *                      sized IOVECs to send, testing ability of new and existing
 *                      code to handle this.
 *
 *   These are useful for testing the correctness of the new code, and
 *   the resulting behaviour / debugging output.
 */
//#define NDB_BAD_SEND
//#define NDB_LUMPY_SEND

/**
 * Number indicating that the trp has no current sender thread.
 *
 * trp is used for short form of transporter in quite a few places.
 * Originally there was a one to one mapping from node to transporter
 * and vice versa. Now there can be several transporters used to
 * connect to one node and thus we work with transporters and not with
 * nodes in most places used for communication.
 */
#define NO_OWNER_THREAD 0xFFFF

static void dumpJobQueues(void);

inline
SimulatedBlock*
GlobalData::mt_getBlock(BlockNumber blockNo, Uint32 instanceNo)
{
  SimulatedBlock* b = getBlock(blockNo);
  if (b != 0 && instanceNo != 0)
    b = b->getInstance(instanceNo);
  return b;
}

#ifdef __GNUC__
/* Provides a small (but noticeable) speedup in benchmarks. */
#define memcpy __builtin_memcpy
#endif

/* Constants found by benchmarks to be reasonable values. */

/*
 * Max. signals to execute from one job buffer before considering other
 * possible stuff to do.
 */
static const Uint32 MAX_SIGNALS_PER_JB = 75;

/**
 * Max signals written to other thread before calling flush_jbb_write_state
 */
static const Uint32 MAX_SIGNALS_BEFORE_FLUSH_RECEIVER = 2;
static const Uint32 MAX_SIGNALS_BEFORE_FLUSH_OTHER = 20;
static const Uint32 MAX_SIGNALS_BEFORE_WAKEUP = 128;

//#define NDB_MT_LOCK_TO_CPU

/* If this is too small it crashes before first signal. */
#define MAX_INSTANCES_PER_THREAD (16 + 8 * MAX_NDBMT_LQH_THREADS)

static Uint32 glob_num_threads = 0;
static Uint32 glob_num_tc_threads = 1;
static Uint32 first_receiver_thread_no = 0;
static Uint32 max_send_delay = 0;
static Uint32 glob_wakeup_latency = 25;

#define NO_SEND_THREAD (MAX_BLOCK_THREADS + MAX_NDBMT_SEND_THREADS + 1)

/* max signal is 32 words, 7 for signal header and 25 datawords */
#define MAX_SIGNAL_SIZE 32
#define MIN_SIGNALS_PER_PAGE (thr_job_buffer::SIZE / MAX_SIGNAL_SIZE) //255

#if defined(HAVE_LINUX_FUTEX) && defined(NDB_HAVE_XCNG)
#define USE_FUTEX
#endif

#ifdef USE_FUTEX
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>

#define FUTEX_WAIT              0
#define FUTEX_WAKE              1
#define FUTEX_FD                2
#define FUTEX_REQUEUE           3
#define FUTEX_CMP_REQUEUE       4
#define FUTEX_WAKE_OP           5

static inline
int
futex_wait(volatile unsigned * addr, int val, const struct timespec * timeout)
{
  return syscall(SYS_futex,
                 addr, FUTEX_WAIT, val, timeout, 0, 0) == 0 ? 0 : errno;
}

static inline
int
futex_wake(volatile unsigned * addr)
{
  return syscall(SYS_futex, addr, FUTEX_WAKE, 1, 0, 0, 0) == 0 ? 0 : errno;
}

struct alignas(NDB_CL) thr_wait
{
  volatile unsigned m_futex_state;
  enum {
    FS_RUNNING = 0,
    FS_SLEEPING = 1
  };
  thr_wait() {
    assert((sizeof(*this) % NDB_CL) == 0); //Maintain any CL-allignment
    xcng(&m_futex_state, FS_RUNNING);
  }
  void init () {}
};

/**
 * Sleep until woken up or timeout occurs.
 *
 * Will call check_callback(check_arg) after proper synchronisation, and only
 * if that returns true will it actually sleep, else it will return
 * immediately. This is needed to avoid races with wakeup.
 *
 * Returns 'true' if it actually did sleep.
 */
template<typename T>
static inline
bool
yield(struct thr_wait* wait, const Uint32 nsec,
      bool (*check_callback)(T*), T* check_arg)
{
  volatile unsigned * val = &wait->m_futex_state;
#ifndef NDEBUG
  int old = 
#endif
    xcng(val, thr_wait::FS_SLEEPING);
  assert(old == thr_wait::FS_RUNNING);

  /**
   * At this point, we need to re-check the condition that made us decide to
   * sleep, and skip sleeping if it changed..
   *
   * Otherwise, the condition may have not changed, and the thread making the
   * change have already decided not to wake us, as our state was FS_RUNNING
   * at the time.
   *
   * Also need a memory barrier to ensure this extra check is race-free.
   *   but that is already provided by xcng
   */
  const bool waited = (*check_callback)(check_arg);
  if (waited)
  {
    struct timespec timeout;
    timeout.tv_sec = 0;
    timeout.tv_nsec = nsec;
    futex_wait(val, thr_wait::FS_SLEEPING, &timeout);
    /**
     * Any spurious wakeups are handled by simply running the scheduler code.
     * The check_callback is needed to ensure that we don't miss wakeups. But
     * that a spurious wakeups causes one loop in the scheduler compared to
     * the cost of always checking through buffers to check condition.
     */
  }
  xcng(val, thr_wait::FS_RUNNING);
  return waited;
}

static inline
int
wakeup(struct thr_wait* wait)
{
  volatile unsigned * val = &wait->m_futex_state;
  /**
   * We must ensure that any state update (new data in buffers...) are visible
   * to the other thread before we can look at the sleep state of that other
   * thread.
   */
  if (xcng(val, thr_wait::FS_RUNNING) == thr_wait::FS_SLEEPING)
  {
    return futex_wake(val);
  }
  return 0;
}

static inline
int
try_wakeup(struct thr_wait* wait)
{
  return wakeup(wait);
}
#else

struct alignas(NDB_CL) thr_wait
{
  NdbMutex *m_mutex;
  NdbCondition *m_cond;
  bool m_need_wakeup;
  thr_wait() : m_mutex(0), m_cond(0), m_need_wakeup(false) {
    assert((sizeof(*this) % NDB_CL) == 0); //Maintain any CL-allignment
  }

  void init() {
    m_mutex = NdbMutex_Create();
    m_cond = NdbCondition_Create();
  }
};

template<typename T>
static inline
bool
yield(struct thr_wait* wait, const Uint32 nsec,
      bool (*check_callback)(T*), T* check_arg)
{
  struct timespec end;
  NdbCondition_ComputeAbsTime(&end, (nsec >= 1000000) ? nsec/1000000 : 1);
  NdbMutex_Lock(wait->m_mutex);

  /**
   * Any spurious wakeups are handled by simply running the scheduler code.
   * The check_callback is needed to ensure that we don't miss wakeups. But
   * that a spurious wakeups causes one loop in the scheduler compared to
   * the cost of always checking through buffers to check condition.
   */
  Uint32 waits = 0;
  if ((*check_callback)(check_arg))
  {
    wait->m_need_wakeup = true;
    waits++;
    if (NdbCondition_WaitTimeoutAbs(wait->m_cond,
                                    wait->m_mutex, &end) == ETIMEDOUT)
    {
      wait->m_need_wakeup = false;
    }
  }
  NdbMutex_Unlock(wait->m_mutex);
  return (waits > 0);
}


static inline
int
try_wakeup(struct thr_wait* wait)
{
  int success = NdbMutex_Trylock(wait->m_mutex);
  if (success != 0)
    return success;

  // We should avoid signaling when not waiting for wakeup
  if (wait->m_need_wakeup)
  {
    wait->m_need_wakeup = false;
    NdbCondition_Signal(wait->m_cond);
  }
  NdbMutex_Unlock(wait->m_mutex);
  return 0;
}

static inline
int
wakeup(struct thr_wait* wait)
{
  NdbMutex_Lock(wait->m_mutex);
  // We should avoid signaling when not waiting for wakeup
  if (wait->m_need_wakeup)
  {
    wait->m_need_wakeup = false;
    NdbCondition_Signal(wait->m_cond);
  }
  NdbMutex_Unlock(wait->m_mutex);
  return 0;
}

#endif

#define JAM_FILE_ID 236


/**
 * thr_safe_pool
 */
template<typename T>
struct alignas(NDB_CL) thr_safe_pool
{
  struct alignas(NDB_CL) thr_safe_pool_lock
  {
    struct thr_spin_lock m_lock;

    T* m_free_list;
    Uint32 m_cnt;
    bool m_used_all_reserved;
  };
  thr_safe_pool_lock m_safe_lock[MAX_NDBMT_SEND_THREADS];
  struct thr_spin_lock m_alloc_lock;
  Uint32 m_allocated;

  thr_safe_pool(const char * name)
  {
    m_allocated = 0;
    for (Uint32 i = 0; i < MAX_NDBMT_SEND_THREADS; i++)
    {
      char buf[100];
      m_safe_lock[i].m_free_list = 0;
      m_safe_lock[i].m_cnt = 0;
      m_safe_lock[i].m_used_all_reserved = false;
      BaseString::snprintf(buf, sizeof(buf), "Global_%s[%u]", name, i);
      register_lock(&m_safe_lock[i].m_lock, buf);
    }
    {
      char buf[100];
      BaseString::snprintf(buf, sizeof(buf), "Global_allocated%s", name);
      register_lock(&m_alloc_lock, buf);
    }
    assert((sizeof(*this) % NDB_CL) == 0); //Maintain any CL-alignment
  }

  T* seize(Ndbd_mem_manager *mm,
           Uint32 rg)
  {
    /* This function is used by job buffer allocation. */
    Uint32 instance_no = 0;
    thr_safe_pool_lock *lock_ptr = &m_safe_lock[instance_no];
    T* ret = 0;
    lock(&lock_ptr->m_lock);
    if (lock_ptr->m_free_list)
    {
      assert(lock_ptr->m_cnt);
      lock_ptr->m_cnt--;
      ret = lock_ptr->m_free_list;
      lock_ptr->m_free_list = ret->m_next;
      unlock(&lock_ptr->m_lock);
    }
    else
    {
      unlock(&lock_ptr->m_lock);
      Uint32 dummy;
      ret = reinterpret_cast<T*>
        (mm->alloc_page(rg, &dummy,
                        Ndbd_mem_manager::NDB_ZONE_LE_32));
      // ToDo: How to deal with failed allocation?!?
      // I think in this case we need to start grabbing buffers kept for signal
      // trace.
      if (ret != NULL)
      {
        lock(&m_alloc_lock);
        m_allocated++;
        unlock(&m_alloc_lock);
      }
    }
    return ret;
  }

#define RG_REQUIRED_PAGES 96
  bool found_instance(Uint32 instance,
                      Uint32 & max_found,
                      Uint32 & instance_no)
  {
    thr_safe_pool_lock *lock_ptr = &m_safe_lock[instance];
    Uint32 cnt = lock_ptr->m_cnt;
    if (cnt > RG_REQUIRED_PAGES)
    {
      return true;
    }
    if (cnt > max_found)
    {
      instance_no = instance;
      max_found = cnt;
    }
    return false;
  }

  Uint32 get_least_empty_instance(Uint32 skip_instance)
  {
    /**
     * Read without mutex protection since it is ok to not get a perfect
     * result.
     */
    Uint32 instance_no_found = 0;
    Uint32 cnt_found = 0;
    for (Uint32 i = skip_instance + 1;
                i < globalData.ndbMtSendThreads;
                i++)
    {
      if (found_instance(i,
                         cnt_found,
                         instance_no_found))
        return i;
    }
    for (Uint32 i = 0; i < skip_instance; i++)
    {
      if (found_instance(i,
                         cnt_found,
                         instance_no_found))
        return i;
    }
    return instance_no_found;
  }

  Uint32 seize_list(Ndbd_mem_manager *mm,
                    Uint32 rg,
                    Uint32 requested,
                    T** head,
                    T** tail,
                    Uint32 instance_no,
                    bool first_call)
  {
    /* This function is used by send buffer allocation. */
    assert(instance_no < MAX_NDBMT_SEND_THREADS);
    thr_safe_pool_lock *lock_ptr = &m_safe_lock[instance_no];
    lock(&lock_ptr->m_lock);
    if (unlikely(lock_ptr->m_cnt == 0))
    {
      unlock(&lock_ptr->m_lock);
      if (likely(first_call))
      {
        /**
         * No free pages in this instance. We will use the following order
         * of allocation.
         *
         * Case 1: Either no send thread or only one send thread
         * => Call alloc_page and set use_max_part to true.
         * If this fails we fail the call.
         *
         * Case 2: At least 2 send threads
         * In this case we will first try to allocate from the memory
         * manager. But this first call only retrieves from the reserved
         * part. If we already allocated all from the reserved part we
         * will skip this call.
         * Next we will check which instance is the least empty of the
         * instances. We will try allocating from this instance. The
         * purpose of this is to avoid allocating beyond the reserved
         * part as long as possible.
         * If this call fails as well we will make another call to
         * alloc_page. This time we will also allow allocations beyond
         * the reserved part.
         * If even this fails we will go through the other instances to
         * see if we can get pages from any instance. Only when this
         * fails as well will we return no pages found.
         */
        Uint32 filled_instance_no = 0;
        for (Uint32 step = 0; step < 2; step++)
        {
          Uint32 dummy;
          bool locked = false;
          bool use_max_part = (globalData.ndbMtSendThreads < 2 ||
                               step == 1);
          if (use_max_part || !lock_ptr->m_used_all_reserved)
          {
            T* ret = reinterpret_cast<T*>
              (mm->alloc_page(rg,
                              &dummy,
                              Ndbd_mem_manager::NDB_ZONE_LE_32,
                              locked,
                              use_max_part));
            if (ret != 0)
            {
              ret->m_next = 0;
              * head = * tail = ret;
              if (ret != NULL)
              {
                lock(&m_alloc_lock);
                m_allocated++;
                unlock(&m_alloc_lock);
              }
              return 1;
            }
            /**
             * This will only transition from false to true, so no need
             * to protect it with mutex.
             */
            lock_ptr->m_used_all_reserved = true;
          }
          /**
           * No more memory available from global memory, let's see if we
           * can steal some memory from a neighbour instance.
           *
           * This is the call from the local pool, we want to avoid
           * failing this call since it means we are announcing that we
           * are out of memory. Try all the other instances before we
           * move on to requesting memory from the global pool of memory.
           * We first attempt with the most filled instance, we find this
           * without acquiring any mutex.
           */
          if (globalData.ndbMtSendThreads < 2)
          {
            return 0;
          }
          if (step == 0)
          {
            filled_instance_no = get_least_empty_instance(instance_no);
            Uint32 returned = seize_list(mm,
                                         rg,
                                         requested,
                                         head,
                                         tail,
                                         filled_instance_no,
                                         false);
            if (likely(returned > 0))
            {
              return returned;
            }
          }
          else
          {
            for (Uint32 i = 0; i < globalData.ndbMtSendThreads; i++)
            {
              if (i != instance_no &&
                  i != filled_instance_no)
              {
                Uint32 returned = seize_list(mm,
                                             rg,
                                             requested,
                                             head,
                                             tail,
                                             i,
                                             false);
                if (returned != 0)
                {
                  ndbout_c("seize_list: returns %u from instance %u",
                           returned,
                           i);
                  return returned;
                }
              }
            }
          }
        }
        return 0;
      }
      else
      {
        return 0;
      }
    }
    else
    {
      if (lock_ptr->m_cnt < requested )
        requested = lock_ptr->m_cnt;

      T* first = lock_ptr->m_free_list;
      T* last = first;
      for (Uint32 i = 1; i < requested; i++)
      {
        last = last->m_next;
      }
      lock_ptr->m_cnt -= requested;
      lock_ptr->m_free_list = last->m_next;
      unlock(&lock_ptr->m_lock);
      last->m_next = 0;
      * head = first;
      * tail = last;
      return requested;
    }
  }

  void release(Ndbd_mem_manager *mm,
               Uint32 rg,
               T* t)
  {
    /* This function is used by job buffer release. */
    Uint32 instance_no = 0;
    thr_safe_pool_lock *lock_ptr = &m_safe_lock[instance_no];
    lock(&lock_ptr->m_lock);
    t->m_next = lock_ptr->m_free_list;
    lock_ptr->m_free_list = t;
    lock_ptr->m_cnt++;
    unlock(&lock_ptr->m_lock);
  }

  void release_list(Ndbd_mem_manager *mm,
                    Uint32 rg, 
                    T* head,
                    T* tail,
                    Uint32 cnt,
                    Uint32 instance_no)
  {
    /* This function is used by send buffer release. */
    assert(instance_no < MAX_NDBMT_SEND_THREADS);
    Uint32 used_instance_no = instance_no;
    thr_safe_pool_lock *lock_ptr = &m_safe_lock[used_instance_no];
    lock(&lock_ptr->m_lock);
    tail->m_next = lock_ptr->m_free_list;
    lock_ptr->m_free_list = head;
    lock_ptr->m_cnt += cnt;
    unlock(&lock_ptr->m_lock);
  }
};

/**
 * thread_local_pool
 */
template<typename T>
class thread_local_pool
{
public:
  thread_local_pool(thr_safe_pool<T> *global_pool,
                    unsigned max_free, unsigned alloc_size = 1) :
    m_max_free(max_free),
    m_alloc_size(alloc_size),
    m_free(0),
    m_freelist(0),
    m_global_pool(global_pool)
  {
  }

  T *seize(Ndbd_mem_manager *mm,
           Uint32 rg,
           Uint32 instance_no)
  {
    T *tmp = m_freelist;
    if (tmp == 0)
    {
      T * tail;
      m_free = m_global_pool->seize_list(mm,
                                         rg,
                                         m_alloc_size,
                                         &tmp,
                                         &tail,
                                         instance_no,
                                         true);
    }
    if (tmp)
    {
      m_freelist = tmp->m_next;
      assert(m_free > 0);
      m_free--;
    }

    validate();
    return tmp;
  }

  /**
   * Release to local pool even if it get's "too" full
   *   (wrt to m_max_free)
   */
  void release_local(T *t)
  {
    m_free++;
    t->m_next = m_freelist;
    m_freelist = t;

    validate();
  }

  void validate() const
  {
#ifdef VM_TRACE
    Uint32 cnt = 0;
    T* t = m_freelist;
    while (t)
    {
      cnt++;
      t = t->m_next;
    }
    assert(cnt == m_free);
#endif
  }

  /**
   * Release entries so that m_max_free is honored
   *   (likely used together with release_local)
   */
  void release_global(Ndbd_mem_manager *mm,
                      Uint32 rg,
                      Uint32 instance_no)
  {
    validate();
    unsigned free = m_free;
    Uint32 maxfree = m_max_free;
    assert(maxfree > 0);

    if (unlikely(free > maxfree))
    {
      T* head = m_freelist;
      T* tail = m_freelist;
      unsigned cnt = 1;
      free--;

      while (free > maxfree)
      {
        cnt++;
        free--;
        tail = tail->m_next;
      } 

      assert(free == maxfree);

      m_free = free;
      m_freelist = tail->m_next;
      m_global_pool->release_list(mm,
                                  rg,
                                  head,
                                  tail,
                                  cnt,
                                  instance_no);
    }
    validate();
  }

  void release_all(Ndbd_mem_manager *mm,
                   Uint32 rg,
                   Uint32 instance_no)
  {
    validate();
    T* head = m_freelist;
    T* tail = m_freelist;
    if (tail)
    {
      unsigned cnt = 1;
      while (tail->m_next != 0)
      {
        cnt++;
        tail = tail->m_next;
      }
      m_global_pool->release_list(mm,
                                  rg,
                                  head,
                                  tail,
                                  cnt,
                                  instance_no);
      m_free = 0;
      m_freelist = 0;
    }
    validate();
  }

  /**
   * release everything if more than m_max_free
   *   else do nothing
   */
  void release_chunk(Ndbd_mem_manager *mm,
                     Uint32 rg,
                     Uint32 instance_no)
  {
    if (m_free > m_max_free)
    {
      release_all(mm, rg, instance_no);
    }
  }

  /**
   * prealloc up to <em>cnt</em> pages into this pool
   */
  bool fill(Ndbd_mem_manager *mm,
            Uint32 rg,
            Uint32 cnt,
            Uint32 instance_no)
  {
    if (m_free >= cnt)
    {
      return true;
    }

    T *head, *tail;
    Uint32 allocated = m_global_pool->seize_list(mm,
                                                 rg,
                                                 m_alloc_size,
                                                 &head,
                                                 &tail,
                                                 instance_no,
                                                 true);
    if (allocated)
    {
      tail->m_next = m_freelist;
      m_freelist = head;
      m_free += allocated;
      return m_free >= cnt;
    }

    return false;
  }

  void set_pool(thr_safe_pool<T> * pool) { m_global_pool = pool; }

private:
  const unsigned m_max_free;
  const unsigned m_alloc_size;
  unsigned m_free;
  T *m_freelist;
  thr_safe_pool<T> *m_global_pool;
};

/**
 * Signal buffers.
 *
 * Each thread job queue contains a list of these buffers with signals.
 *
 * There is an underlying assumption that the size of this structure is the
 * same as the global memory manager page size.
 */
struct thr_job_buffer // 32k
{
  static const unsigned SIZE = 8190;

  /*
   * Amount of signal data currently in m_data buffer.
   * Read/written by producer, read by consumer.
   */
  Uint32 m_len;
  /*
   * Whether this buffer contained prio A or prio B signals, used when dumping
   * signals from released buffers.
   */
  Uint32 m_prioa;
  union {
    Uint32 m_data[SIZE];

    thr_job_buffer * m_next; // For free-list
  };
};  

static
inline
Uint32
calc_fifo_used(Uint32 ri, Uint32 wi, Uint32 sz)
{
  return (wi >= ri) ? wi - ri : (sz - ri) + wi;
}

/**
 * thr_job_queue is shared between consumer / producer. 
 *
 * The hot-spot of the thr_job_queue are the read/write indexes.
 * As they are updated and read frequently they have been placed
 * in its own thr_job_queue_head[] in order to make them fit inside a
 * single/few cache lines and thereby avoid complete L1-cache replacement
 * every time the job_queue is scanned.
 */
struct thr_job_queue_head
{
  unsigned m_read_index;  // Read/written by consumer, read by producer
  unsigned m_write_index; // Read/written by producer, read by consumer

  /**
   * Waiter object: In case job queue is full, the produced thread
   * will 'yield' on this waiter object until the consumer thread
   * has consumed (at least) a job buffer.
   */
  thr_wait m_waiter;

  Uint32 used() const;
};

struct thr_job_queue
{
  static const unsigned SIZE = 32;
  
  /**
   * There is a SAFETY limit on free buffers we never allocate,
   * but may allow these to be implicitly used as a last resort
   * when job scheduler is really stuck. ('sleeploop 10')
   */
  static const unsigned SAFETY = 2;

  /**
   * Some more free buffers are RESERVED to be used to avoid
   * or resolve circular wait-locks between threads waiting
   * for buffers to become available.
   */
  static const unsigned RESERVED = 4;

  /**
   * When free buffer count drops below ALMOST_FULL, we
   * are allowed to start using RESERVED buffers to prevent
   * circular wait-locks.
   */
  static const unsigned ALMOST_FULL = RESERVED + 2;

  struct thr_job_buffer* m_buffers[SIZE];
};

inline
Uint32
thr_job_queue_head::used() const
{
  return calc_fifo_used(m_read_index, m_write_index, thr_job_queue::SIZE);
}

/*
 * Two structures tightly associated with thr_job_queue.
 *
 * There will generally be exactly one thr_jb_read_state and one
 * thr_jb_write_state associated with each thr_job_queue.
 *
 * The reason they are kept separate is to avoid unnecessary inter-CPU
 * cache line pollution. All fields shared among producer and consumer
 * threads are in thr_job_queue, thr_jb_write_state fields are only
 * accessed by the producer thread(s), and thr_jb_read_state fields are
 * only accessed by the consumer thread.
 *
 * For example, on Intel core 2 quad processors, there is a ~33%
 * penalty for two cores accessing the same 64-byte cacheline.
 */
struct thr_jb_write_state
{
  /*
   * The position to insert the next signal into the queue.
   *
   * m_write_index is the index into thr_job_queue::m_buffers[] of the buffer
   * to insert into, and m_write_pos is the index into thr_job_buffer::m_data[]
   * at which to store the next signal.
   */
  Uint32 m_write_index;
  Uint32 m_write_pos;

  /* Thread-local copy of thr_job_queue::m_buffers[m_write_index]. */
  thr_job_buffer *m_write_buffer;

  /**
    Number of signals inserted since last flush to thr_job_queue.
    This variable stores the number of pending signals not yet flushed
    in the lower 16 bits and the number of pending signals before a
    wakeup is called of the other side in the upper 16 bits. To
    simplify the code we implement the bit manipulations in the
    methods below.

    The reason for this optimisation is to minimise use of memory for
    these variables as they are likely to consume CPU cache memory.
    It also speeds up some pending signal checks.
  */
  Uint32 m_pending_signals;

  bool has_any_pending_signals() const
  {
    return m_pending_signals;
  }
  Uint32 get_pending_signals() const
  {
    return (m_pending_signals & 0xFFFF);
  }
  Uint32 get_pending_signals_wakeup() const
  {
    return (m_pending_signals >> 16);
  }
  void clear_pending_signals_and_set_wakeup(Uint32 wakeups)
  {
    m_pending_signals = (wakeups << 16);
  }
  void increment_pending_signals()
  {
    m_pending_signals++;
  }
  void init_pending_signals()
  {
    m_pending_signals = 0;
  }

  /*
   * Is this job buffer open for communication at all?
   * Several threads are not expected to communicate, and thus does
   * not allocate thr_job_buffer for exchange of signals.
   * Don't access any job_buffers without ensuring 'is_open()==true'.
   */
  bool is_open() const
  {
    return (m_write_buffer != NULL);
  }
};

/**
 * Identify type of thread.
 * Based on assumption that threads are allocated in the order:
 *  main, ldm, tc, recv, send
 */
static bool
is_main_thread(unsigned thr_no)
{
  return thr_no < NUM_MAIN_THREADS;
}

static bool
is_ldm_thread(unsigned thr_no)
{
  return thr_no >= NUM_MAIN_THREADS && 
         thr_no <  NUM_MAIN_THREADS+globalData.ndbMtLqhThreads;
}

/**
 * All LDM threads are not created equal: 
 * First LDMs BACKUP-thread act as client during BACKUP
 * (See usage of Backup::UserBackupInstanceKey)
 */
static bool
is_first_ldm_thread(unsigned thr_no)
{
  return thr_no == NUM_MAIN_THREADS;
}

static bool
is_tc_thread(unsigned thr_no)
{
  unsigned tc_base = NUM_MAIN_THREADS+globalData.ndbMtLqhThreads;
  return thr_no >= tc_base && 
         thr_no <  tc_base+globalData.ndbMtTcThreads;
}

static bool
is_recv_thread(unsigned thr_no)
{
  unsigned recv_base = NUM_MAIN_THREADS +
                       globalData.ndbMtLqhThreads +
                       globalData.ndbMtTcThreads;
  return thr_no >= recv_base &&
         thr_no <  recv_base+globalData.ndbMtReceiveThreads;
}

/*
 * This structure is also used when dumping signal traces, to dump executed
 * signals from the buffer(s) currently being processed.
 */
struct thr_jb_read_state
{
  /*
   * Index into thr_job_queue::m_buffers[] of the buffer that we are currently
   * executing signals from.
   */
  Uint32 m_read_index;
  /*
   * Index into m_read_buffer->m_data[] of the next signal to execute from the
   * current buffer.
   */
  Uint32 m_read_pos;
  /*
   * Thread local copy of thr_job_queue::m_buffers[m_read_index].
   */
  thr_job_buffer *m_read_buffer;
  /*
   * These are thread-local copies of thr_job_queue::m_write_index and
   * thr_job_buffer::m_len. They are read once at the start of the signal
   * execution loop and used to determine when the end of available signals is
   * reached.
   */
  Uint32 m_read_end;    // End within current thr_job_buffer. (*m_read_buffer)

  Uint32 m_write_index; // Last available thr_job_buffer.

  /*
   * Is this job buffer open for communication at all?
   * Several threads are not expected to communicate, and thus does
   * not allocate thr_job_buffer for exchange of signals.
   * Don't access any job_buffers without ensuring 'is_open()==true'.
   */
  bool is_open() const
  {
    return (m_read_buffer != NULL);
  }

  bool is_empty() const
  {
    assert(m_read_index != m_write_index  ||  m_read_pos <= m_read_end);
    return (m_read_index == m_write_index) && (m_read_pos >= m_read_end);
  }
};

/**
 * time-queue
 */
struct thr_tq
{
  static const unsigned ZQ_SIZE = 256;
  static const unsigned SQ_SIZE = 512;
  static const unsigned LQ_SIZE = 512;
  static const unsigned PAGES = (MAX_SIGNAL_SIZE *
                                (ZQ_SIZE + SQ_SIZE + LQ_SIZE)) / 8192;
  
  Uint32 * m_delayed_signals[PAGES];
  Uint32 m_next_free;
  Uint32 m_next_timer;
  Uint32 m_current_time;
  Uint32 m_cnt[3];
  Uint32 m_zero_queue[ZQ_SIZE];
  Uint32 m_short_queue[SQ_SIZE];
  Uint32 m_long_queue[LQ_SIZE];
};

/**
 * THR_SEND_BUFFER_ALLOC_SIZE is the amount of 32k pages allocated
 * when we allocate pages from the global pool of send buffers to
 * the thread_local_pool (which is local to a thread).
 *
 * We allocate a bunch to decrease contention on send-buffer-pool-mutex
 */
#define THR_SEND_BUFFER_ALLOC_SIZE 32

/**
 * THR_SEND_BUFFER_PRE_ALLOC is the amout of 32k pages that are
 *   allocated before we start to run signals
 */
#define THR_SEND_BUFFER_PRE_ALLOC 32

/**
 * Amount of pages that is allowed to linger in a
 * thread-local send-buffer pool
 */
#define THR_SEND_BUFFER_MAX_FREE \
  (THR_SEND_BUFFER_ALLOC_SIZE + THR_SEND_BUFFER_PRE_ALLOC - 1)

/*
 * Max number of thread-local job buffers to keep before releasing to
 * global pool.
 */
#define THR_FREE_BUF_MAX 32
/* Minimum number of buffers (to ensure useful trace dumps). */
#define THR_FREE_BUF_MIN 12
/*
 * 1/THR_FREE_BUF_BATCH is the fraction of job buffers to allocate/free
 * at a time from/to global pool.
 */
#define THR_FREE_BUF_BATCH 6

/**
 * a page with send data
 */
struct thr_send_page
{
  static const Uint32 PGSIZE = 32768;
#if SIZEOF_CHARP == 4
  static const Uint32 HEADER_SIZE = 8;
#else
  static const Uint32 HEADER_SIZE = 12;
#endif

  static Uint32 max_bytes() {
    return PGSIZE - offsetof(thr_send_page, m_data);
  }

  /* Next page */
  thr_send_page* m_next;

  /* Bytes of send data available in this page. */
  Uint16 m_bytes;

  /* Start of unsent data */
  Uint16 m_start;

  /* Data; real size is to the end of one page. */
  char m_data[2];
};

/**
 * a linked list with thr_send_page
 */
struct thr_send_buffer
{
  thr_send_page* m_first_page;
  thr_send_page* m_last_page;
};

/**
 * a ring buffer with linked list of thr_send_page
 */
struct thr_send_queue
{
  unsigned m_write_index;
#if SIZEOF_CHARP == 8
  unsigned m_unused;
  thr_send_page* m_buffers[7];
  static const unsigned SIZE = 7;
#else
  thr_send_page* m_buffers[15];
  static const unsigned SIZE = 15;
#endif
};

struct thr_send_thread_instance;

struct alignas(NDB_CL) thr_data
{
  thr_data() : m_jba_write_lock("jbalock"),
               m_signal_id_counter(0),
               m_send_buffer_pool(0,
                                  THR_SEND_BUFFER_MAX_FREE,
                                  THR_SEND_BUFFER_ALLOC_SIZE)
#if defined(USE_INIT_GLOBAL_VARIABLES)
               ,m_global_variables_ptr_instances(0)
               ,m_global_variables_uint32_ptr_instances(0)
               ,m_global_variables_uint32_instances(0)
               ,m_global_variables_enabled(true)
#endif
  {

    // Check cacheline allignment
    assert((((UintPtr)this) % NDB_CL) == 0);
    assert((((UintPtr)&m_waiter) % NDB_CL) == 0);
    assert((((UintPtr)&m_jba_write_lock) % NDB_CL) == 0);
    assert((((UintPtr)&m_jba) % NDB_CL) == 0);
    assert((((UintPtr)m_in_queue_head) % NDB_CL) == 0);
    assert((((UintPtr)m_in_queue) % NDB_CL) == 0);
  }

  /**
   * We start with the data structures that are shared globally to
   * ensure that they get the proper cache line alignment
   */
  thr_wait m_waiter; /* Cacheline aligned*/

  /*
   * Prio A signal incoming queue. This area is used from many threads
   * protected by the spin lock. Thus it is also important to protect
   * surrounding thread-local variables from CPU cache line sharing
   * with this part.
   */
  alignas(NDB_CL) struct thr_spin_lock m_jba_write_lock;
  alignas(NDB_CL) struct thr_job_queue m_jba;
  struct thr_job_queue_head m_jba_head;

  /*
   * These are the thread input queues, where other threads deliver signals
   * into.
   * These cache lines are going to be updated by many different CPU's 
   * all the time whereas other neighbour variables are thread-local variables.
   * Avoid false cacheline sharing by require an alignment.
   */
  alignas(NDB_CL) struct thr_job_queue_head m_in_queue_head[MAX_BLOCK_THREADS];
  alignas(NDB_CL) struct thr_job_queue m_in_queue[MAX_BLOCK_THREADS];

  /**
   * The remainder of the variables in thr_data are thread-local,
   * meaning that they are always updated by the thread that owns those
   * data structures and thus those variables aren't shared with other
   * CPUs.
   */

  unsigned m_thr_no;

  /**
   * Thread 0 doesn't necessarily handle all threads in a loop.
   * This variable keeps track of which to handle next.
   */
  unsigned m_next_jbb_no;

  /**
   * Spin time of thread after completing all its work (in microseconds).
   * We won't go to sleep until we have spun for sufficient time, the aim
   * is to increase readiness in systems with much CPU resources
   */
  unsigned m_spintime;
  unsigned m_conf_spintime;

  /**
   * nosend option on a thread means that it will never assist with sending.
   */
  unsigned m_nosend;

  /**
   * Realtime scheduler activated for this thread. This means this
   * thread will run at a very high priority even beyond the priority
   * of the OS.
   */
  unsigned m_realtime;

  /**
   * Index of thread locally in Configuration.cpp
   */
  unsigned m_thr_index;

  /**
   * max signals to execute per JBB buffer
   */
  unsigned m_max_signals_per_jb;

  /**
   * This state show how much assistance we are to provide to the
   * send threads in sending. At OVERLOAD we provide no assistance
   * and at MEDIUM we take care of our own generated sends and
   * at LIGHT we provide some assistance to other threads.
   */
  OverloadStatus m_overload_status;

  /**
   * This is the wakeup instance that we currently use, if 0 it
   * means that we don't wake any other block thread up to
   * assist in sending. This is a simple way of using idle
   * block threads to act as send threads instead of simply
   * being idle. In particular this is often used for the main
   * thread and the rep thread.
   */
  Uint32 m_wakeup_instance;

  /**
   * This variable keeps track of when we last woke up another thread
   * to assist the send thread. We use other timeout calls for this.
   */
  NDB_TICKS m_last_wakeup_idle_thread;

  /**
   * We also keep track of node state, this is in overload state
   * if any thread is in OVERLOAD state. In this state we will
   * sleep shorter times and be more active in waking up to
   * assist the send threads.
   */
  OverloadStatus m_node_overload_status;

  /**
   * Extra JBB signal execute quota allowed to be used to
   * drain (almost) full in-buffers. Reserved for usage where
   * we are about to end up in a circular wait-lock between 
   * threads where none if them will be able to proceed.
   */
  unsigned m_max_extra_signals;

  /**
   * max signals to execute before recomputing m_max_signals_per_jb
   */
  unsigned m_max_exec_signals;

  /**
   * Flag indicating that we have sent a local Prio A signal. Used to know
   * if to scan for more prio A signals after executing those signals.
   * This is used to ensure that if we execute at prio A level and send a
   * prio A signal it will be immediately executed (or at least before any
   * prio B signal).
   */
  bool m_sent_local_prioa_signal;

  /* Last read of current ticks */
  NDB_TICKS m_curr_ticks;

  NDB_TICKS m_ticks;
  struct thr_tq m_tq;

  /**
   * If thread overslept it is interesting to see how much time was actually
   * spent on executing and how much time was idle time. This will help to
   * see if overslept is due to long-running signals or OS not scheduling the
   * thread.
   *
   * We keep the real time last we made scan of time queues to ensure we can
   * report proper things in warning messages.
   */
  NDB_TICKS m_scan_real_ticks;
  struct ndb_rusage m_scan_time_queue_rusage;

  /*
   * In m_next_buffer we keep a free buffer at all times, so that when
   * we hold the lock and find we need a new buffer, we can use this and this
   * way defer allocation to after releasing the lock.
   */
  struct thr_job_buffer* m_next_buffer;

  /*
   * We keep a small number of buffers in a thread-local cyclic FIFO, so that
   * we can avoid going to the global pool in most cases, and so that we have
   * recent buffers available for dumping in trace files.
   */
  struct thr_job_buffer *m_free_fifo[THR_FREE_BUF_MAX];
  /* m_first_free is the index of the entry to return next from seize(). */
  Uint32 m_first_free;
  /* m_first_unused is the first unused entry in m_free_fifo. */
  Uint32 m_first_unused;


  /* Thread-local read state of prio A buffer. */
  struct thr_jb_read_state m_jba_read_state;

  /*
   * There is no m_jba_write_state, as we have multiple writers to the prio A
   * queue, so local state becomes invalid as soon as we release the lock.
   */

  /* These are the write states of m_in_queue[self] in each thread. */
  struct thr_jb_write_state m_write_states[MAX_BLOCK_THREADS];
  /* These are the read states of all of our own m_in_queue[]. */
  struct thr_jb_read_state m_read_states[MAX_BLOCK_THREADS];

  /* Jam buffers for making trace files at crashes. */
  EmulatedJamBuffer m_jam;
  /* Watchdog counter for this thread. */
  Uint32 m_watchdog_counter;
  /* Latest executed signal id assigned in this thread */
  Uint32 m_signal_id_counter;

  struct thr_send_thread_instance *m_send_instance;
  Uint32 m_send_instance_no;

  /* Signal delivery statistics. */
  struct
  {
    Uint64 m_loop_cnt;
    Uint64 m_exec_cnt;
    Uint64 m_wait_cnt;
    Uint64 m_prioa_count;
    Uint64 m_prioa_size;
    Uint64 m_priob_count;
    Uint64 m_priob_size;
  } m_stat;

  struct
  {
    Uint32 m_sleep_longer_spin_time;
    Uint32 m_sleep_shorter_spin_time;
    Uint32 m_num_waits;
    Uint32 m_micros_sleep_times[NUM_SPIN_INTERVALS];
    Uint32 m_spin_interval[NUM_SPIN_INTERVALS];
  } m_spin_stat;

  Uint64 m_micros_send;
  Uint64 m_micros_sleep;
  Uint64 m_buffer_full_micros_sleep;
  Uint64 m_measured_spintime;

  /* Array of trp ids with pending remote send data. */
  TrpId m_pending_send_trps[MAX_NTRANSPORTERS];
  /* Number of trp ids in m_pending_send_trps. */
  Uint32 m_pending_send_count;

  /**
   * Bitmap of pending ids with send data.
   * Used to quickly check if a trp id is already in m_pending_send_trps.
   */
  Bitmask<(MAX_NTRANSPORTERS+31)/32> m_pending_send_mask;

  /* pool for send buffers */
  class thread_local_pool<thr_send_page> m_send_buffer_pool;

  /* Send buffer for this thread, these are not touched by any other thread */
  struct thr_send_buffer m_send_buffers[MAX_NTRANSPORTERS];

  /* Block instances (main and worker) handled by this thread. */
  /* Used for sendpacked (send-at-job-buffer-end). */
  Uint32 m_instance_count;
  BlockNumber m_instance_list[MAX_INSTANCES_PER_THREAD];

  SectionSegmentPool::Cache m_sectionPoolCache;

  Uint32 m_cpu;
  my_thread_t m_thr_id;
  NdbThread* m_thread;
  Signal *m_signal;
  Uint32 m_sched_responsiveness;
  Uint32 m_max_signals_before_send;
  Uint32 m_max_signals_before_send_flush;

#ifdef ERROR_INSERT
  bool m_delayed_prepare;
#endif

#if defined (USE_INIT_GLOBAL_VARIABLES)
  Uint32 m_global_variables_ptr_instances;
  Uint32 m_global_variables_uint32_ptr_instances;
  Uint32 m_global_variables_uint32_instances;
  bool m_global_variables_enabled;
  void* m_global_variables_ptrs[1024];
  void* m_global_variables_uint32_ptrs[1024];
  void* m_global_variables_uint32[1024];
#endif
};

struct mt_send_handle  : public TransporterSendBufferHandle
{
  struct thr_data * m_selfptr;
  mt_send_handle(thr_data* ptr) : m_selfptr(ptr) {}
  ~mt_send_handle() override {}

  Uint32 *getWritePtr(NodeId nodeId,
                      TrpId trp_id,
                      Uint32 len,
                      Uint32 prio,
                      Uint32 max,
                      SendStatus *error) override;
  Uint32 updateWritePtr(NodeId nodeId,
                        TrpId trp_id,
                        Uint32 lenBytes,
                        Uint32 prio) override;
  void getSendBufferLevel(NodeId node_id, SB_LevelType &level) override;
  bool forceSend(NodeId, TrpId) override;
};

struct trp_callback : public TransporterCallback
{
  trp_callback() {}

  /* Callback interface. */
  void enable_send_buffer(NodeId, TrpId) override;
  void disable_send_buffer(NodeId, TrpId) override;

  void reportSendLen(NodeId nodeId, Uint32 count, Uint64 bytes) override;
  void lock_transporter(NodeId, TrpId) override;
  void unlock_transporter(NodeId, TrpId) override;
  void lock_send_transporter(NodeId, TrpId) override;
  void unlock_send_transporter(NodeId, TrpId) override;
  Uint32 get_bytes_to_send_iovec(NodeId nodeId,
                                 TrpId trp_id,
                                 struct iovec *dst,
                                 Uint32 max) override;
  Uint32 bytes_sent(NodeId, TrpId, Uint32 bytes) override;
};

static char *g_thr_repository_mem = NULL;
static struct thr_repository *g_thr_repository = NULL;

struct thr_repository
{
  thr_repository() :
      m_section_lock("sectionlock"),
      m_mem_manager_lock("memmanagerlock"),
      m_jb_pool("jobbufferpool"),
      m_sb_pool("sendbufferpool")
  {
    // Verify assumed cacheline allignment
    assert((((UintPtr)this) % NDB_CL) == 0);
    assert((((UintPtr)&m_receive_lock) % NDB_CL) == 0);
    assert((((UintPtr)&m_section_lock) % NDB_CL) == 0);
    assert((((UintPtr)&m_mem_manager_lock) % NDB_CL) == 0);
    assert((((UintPtr)&m_jb_pool) % NDB_CL) == 0);
    assert((((UintPtr)&m_sb_pool) % NDB_CL) == 0);
    assert((((UintPtr)m_thread) % NDB_CL) == 0);
    assert((sizeof(m_receive_lock[0]) % NDB_CL) == 0);
  }

  /**
   * m_receive_lock, m_section_lock, m_mem_manager_lock, m_jb_pool
   * and m_sb_pool are all variables globally shared among the threads
   * and also heavily updated.
   * Requiring alignments avoid false cache line sharing.
   */
  thr_aligned_spin_lock m_receive_lock[MAX_NDBMT_RECEIVE_THREADS];

  alignas(NDB_CL) struct thr_spin_lock m_section_lock;
  alignas(NDB_CL) struct thr_spin_lock m_mem_manager_lock;
  alignas(NDB_CL) struct thr_safe_pool<thr_job_buffer> m_jb_pool;
  alignas(NDB_CL) struct thr_safe_pool<thr_send_page> m_sb_pool;

  /* m_mm and m_thread_count are globally shared and read only variables */
  Ndbd_mem_manager * m_mm;
  unsigned m_thread_count;

  /**
   * Protect m_mm and m_thread_count from CPU cache misses, first
   * part of m_thread (struct thr_data) is globally shared variables.
   * So sharing cache line with these for these read only variables
   * isn't a good idea
   */
  alignas(NDB_CL) struct thr_data m_thread[MAX_BLOCK_THREADS];

  /* The buffers that are to be sent */
  struct send_buffer
  {
    /**
     * In order to reduce lock contention while 
     * adding job buffer pages to the send buffers,
     * and sending these with the help of the send
     * transporters, there are two different
     * thr_send_buffer's. Each protected by its own lock:
     *
     * - m_buffer / m_buffer_lock:
     *   Send buffer pages from all threads are linked into
     *   the m_buffer when collected by link_thread_send_buffers().
     *
     * - m_sending / m_send_lock:
     *   Before send buffers are given to the send-transporter,
     *   they are moved from m_buffer -> m_sending by
     *   get_bytes_to_send_iovec(). (Req. both locks.)
     *   When transporter has consumed some/all of m_sending
     *   buffers, ::bytes_sent() will update m_sending accordingly.
     *
     * If both locks are required, grab the m_send_lock first.
     * Release m_buffer_lock before releasing m_send_lock.
     */
    struct thr_spin_lock m_buffer_lock; //Protect m_buffer
    struct thr_send_buffer m_buffer;

    struct thr_spin_lock m_send_lock;   //Protect m_sending + transporter
    struct thr_send_buffer m_sending;

    /* Size of resp. 'm_buffer' and 'm_sending' buffered data */
    Uint64 m_buffered_size;             //Protected by m_buffer_lock
    Uint64 m_sending_size;              //Protected by m_send_lock

    bool m_enabled;                     //Protected by m_send_lock

    /**
     * Flag used to coordinate sending to same remote trp from different
     * threads when there are contention on m_send_lock.
     *
     * If two threads need to send to the same trp at the same time, the
     * second thread, rather than wait for the first to finish, will just
     * set this flag. The first thread will will then take responsibility 
     * for sending to this trp when done with its own sending.
     */
    Uint32 m_force_send;   //Check after release of m_send_lock

    /**
     * Which thread is currently holding the m_send_lock
     * This is the thr_no of the thread sending, this can be both a
     * send thread and a block thread. Send thread start their
     * thr_no at glob_num_threads. So it is easy to check this
     * thr_no to see if it is a block thread or a send thread.
     * This variable is used to find the proper place to return
     * the send buffer pages after completing the send.
     */
    Uint32 m_send_thread;  //Protected by m_send_lock

    /**
     * Bytes sent in last performSend().
     */
    Uint32 m_bytes_sent;

    /* read index(es) in thr_send_queue */
    Uint32 m_read_index[MAX_BLOCK_THREADS];
  } m_send_buffers[MAX_NTRANSPORTERS];

  /* The buffers published by threads */
  thr_send_queue m_thread_send_buffers[MAX_NTRANSPORTERS][MAX_BLOCK_THREADS];

  /*
   * These are used to synchronize during crash / trace dumps.
   *
   */
  NdbMutex stop_for_crash_mutex;
  NdbCondition stop_for_crash_cond;
  Uint32 stopped_threads;
};

/**
 *  Class to handle send threads
 *  ----------------------------
 *  We can have up to 8 send threads.
 *
 *  This class will handle when a block thread needs to send, it will
 *  handle the running of the send thread and will also start the
 *  send thread.
 */
#define is_send_thread(thr_no) (thr_no >= glob_num_threads)

struct thr_send_thread_instance
{
  thr_send_thread_instance() :
               m_instance_no(0),
               m_watchdog_counter(0),
               m_thr_index(0),
               m_thread(NULL),
               m_waiter_struct(),
               m_send_buffer_pool(0,
                                  THR_SEND_BUFFER_MAX_FREE,
                                  THR_SEND_BUFFER_ALLOC_SIZE),
               m_exec_time(0),
               m_sleep_time(0),
               m_user_time_os(0),
               m_kernel_time_os(0),
               m_elapsed_time_os(0),
               m_measured_spintime(0),
               m_awake(FALSE),
               m_first_trp(0),
               m_last_trp(0),
               m_next_is_high_prio_trp(false),
               m_more_trps(false),
               m_num_neighbour_trps(0),
               m_neighbour_trp_index(0)
  {}

  /**
   * Instance number of send thread, this is set at creation of
   * send thread and after that not changed, so no need to protect
   * it when reading it.
   */
  Uint32 m_instance_no;

  /**
   * This variable is registered in the watchdog, it is set by the
   * send thread and reset every now and then by watchdog thread.
   * No sepecial protection is required in setting it.
   */
  Uint32 m_watchdog_counter;

  /**
   * Thread index of send thread in data node, this variable is
   * currently not used.
   */
  Uint32 m_thr_index;
  NdbThread *m_thread;

  /**
   * Variable controlling send thread sleep and awakeness, this is
   * used in call to wakeup a thread.
   */
  thr_wait m_waiter_struct;

  class thread_local_pool<thr_send_page> m_send_buffer_pool;

  /**
   * The below variables are protected by the send_thread_mutex.
   * Each send thread is taking care of a subset of the transporters
   * in the data node. The function to decide which send thread
   * instance is responsible is simply the transporter id modulo the
   * number of send thread instances, possibly extended with a simple
   * hash function to make it less likely that some simple regularity
   * in node ids create unnecessary bottlenecks.
   *
   * Each send thread only has neighbour transporters it is responsible
   * for in the list below.
   */

  /**
   * Statistical variables that track send thread CPU usage that is
   * reported in call getSendPerformanceTimers that is used by
   * THRMAN block to track CPU usage in send threads and is also
   * used by THRMAN to report data on send threads in ndbinfo
   * tables. The data is used in adaptive send thread control by
   * THRMAN.
   */
  Uint64 m_exec_time;
  Uint64 m_sleep_time;
  Uint64 m_user_time_os;
  Uint64 m_kernel_time_os;
  Uint64 m_elapsed_time_os;
  Uint64 m_measured_spintime;

  /**
   * Boolean indicating if send thread is awake or not.
   */
  Uint32 m_awake;

  /* First trp that has data to be sent */
  Uint32 m_first_trp;

  /* Last trp in list of trps with data available for sending */
  Uint32 m_last_trp;

  /* Which list should I get trp from next time. */
  bool m_next_is_high_prio_trp;

  /* 'true': More trps became available -> Need recheck ::get_trp() */
  bool m_more_trps;

#define MAX_NEIGHBOURS (3 * MAX_NODE_GROUP_TRANSPORTERS)
  Uint32 m_num_neighbour_trps;
  Uint32 m_neighbour_trp_index;
  Uint32 m_neighbour_trps[MAX_NEIGHBOURS];

  /**
   * Mutex protecting the linked list of trps awaiting sending
   * and also the m_awake variable of the send thread. This
   * includes the neighbour transporters listed above.
   *
   * In addition the statistical variables listed above.
   *
   * Finally it also protects the data for transporters handled by this
   * send thread in the m_trp_state array (the thr_send_trps struct).
   */
  NdbMutex *send_thread_mutex;

  /**
   * Check if a trp possibly is having data ready to be sent.
   * Upon 'true', callee should grab send_thread_mutex and 
   * try to get_trp() while holding lock.
   */
  bool data_available() const
  {
    rmb();
    return (m_more_trps == TRUE);
  }

  bool check_pending_data()
  {
    return m_more_trps;
  }
};

struct thr_send_trps
{
  /**
   * 'm_next' implements a list of 'send_trps' with PENDING'
   * data, not yet assigned to a send thread. 0 means NULL.
   */
  Uint16 m_next;

  /**
   * m_data_available are incremented/decremented by each 
   * party having data to be sent to this specific trp.
   * It work in conjunction with a queue of get'able trps
   * (insert_trp(), get_trp()) waiting to be served by 
   * the send threads, such that:
   *
   * 1) IDLE-state (m_data_available==0, not in list)
   *    There are no data available for sending, and
   *    no send threads are assigned to this trp.
   *
   * 2) PENDING-state (m_data_available>0, in list)
   *    There are data available for sending, possibly
   *    supplied by multiple parties. No send threads
   *    are currently serving this request.
   *
   * 3) ACTIVE-state (m_data_available==1, not in list)
   *    There are data available for sending, possibly
   *    supplied by multiple parties, which are currently
   *    being served by a send thread. All known
   *    data available at the time when we became 'ACTIVE'
   *    will be served now ( -> '==1')
   *
   * 3b ACTIVE-WITH-PENDING-state (m_data_available>1, not in list)
   *    Variant of above state, send thread is serving requests,
   *    and even more data became available since we started.
   *
   * Allowed state transitions are:
   *
   * IDLE     -> PENDING  (alert_send_thread w/ insert_trp)
   * PENDING  -> ACTIVE   (get_trp)
   * ACTIVE   -> IDLE     (run_send_thread if check_done_trp)
   * ACTIVE   -> PENDING  (run_send_thread if 'more'
   * ACTIVE   -> ACTIVE-P (alert_send_thread while ACTIVE)
   * ACTIVE-P -> PENDING  (run_send_thread while not check_done_trp)
   * ACTIVE-P -> ACTIVE-P (alert_send_thread while ACTIVE-P)
   *
   * A consequence of this, is that only a (single-) ACTIVE
   * send thread will serve send request to a specific trp.
   * Thus, there will be no contention on the m_send_lock
   * caused by the send threads.
   */
  Uint16 m_data_available;

  /**
   * This variable shows which trp is actually sending for the moment.
   * This will be reset again immediately after sending is completed.
   * It is used to ensure that neighbour trps aren't taken out for
   * sending by more than one thread. The neighbour list is simply
   * an array of the neighbours and we will send if data is avaiable
   * to send AND no one else is sending which is checked by looking at
   * this variable.
   */
  Uint16 m_thr_no_sender;

  /* Send to this trp has caused a Transporter overload */
  Uint16 m_send_overload;

  /**
   * This is neighbour trp in the same node group as ourselves. This means
   * that we are likely to communicate with this trp more heavily than
   * other trps. Also delays in this communication will make the updates
   * take much longer since updates has to traverse this link and the
   * corresponding link back 6 times as part of an updating transaction.
   *
   * Thus for good performance of updates it is essential to prioritise this
   * link a bit.
   */
  bool m_neighbour_trp;

  /**
   * Further sending to this trp should be delayed until
   * 'm_micros_delayed' has passed since 'm_inserted_time'.
   */
  Uint32 m_micros_delayed;
  NDB_TICKS m_inserted_time;

  /**
   * Counter of how many overload situations we experienced towards this
   * trp. We keep track of this to get an idea if the config setup is
   * incorrect somehow, one should consider increasing TCP_SND_BUF_SIZE
   * if this counter is incremented often. It is an indication that a
   * bigger buffer is needed to handle bandwith-delay product of the
   * node communication.
   */
  Uint64 m_overload_counter;
};

class thr_send_threads
{
public:
  /* Create send thread environment */
  thr_send_threads();

  /* Destroy send thread environment and ensure threads are stopped */
  ~thr_send_threads();

  struct thr_send_thread_instance* get_send_thread_instance_by_num(Uint32);
  /**
   * A block thread provides assistance to send thread by executing send
   * to one of the trps.
   */
  bool assist_send_thread(Uint32 max_num_trps,
                          Uint32 thr_no,
                          NDB_TICKS now,
                          Uint32 &watchdog_counter,
               struct thr_send_thread_instance *send_instance,
               class thread_local_pool<thr_send_page>  & send_buffer_pool);

  /* Send thread method to send to a transporter picked by get_trp */
  bool handle_send_trp(TrpId id,
                       Uint32 & num_trp_sent,
                       Uint32 thr_no,
                       NDB_TICKS & now,
                       Uint32 & watchdog_counter,
                       struct thr_send_thread_instance *send_instance);

  /* A block thread has flushed data for a trp and wants it sent */
  Uint32 alert_send_thread(TrpId trp_id,
                           NDB_TICKS now,
                           struct thr_send_thread_instance* send_instance);

  /* Method used to run the send thread */
  void run_send_thread(Uint32 instance_no);

  /* Method to assign the base transporter to send threads */
  void assign_trps_to_send_threads();

  /* Method to assign the multi transporter to send threads */
  void assign_multi_trps_to_send_threads();

  /* Method to assign the block threads to assist send threads */
  void assign_threads_to_assist_send_threads();

  /* Method to start the send threads */
  void start_send_threads();

  /* Get send buffer pool for send thread */
  thread_local_pool<thr_send_page>* get_send_buffer_pool(Uint32 thr_no)
  {
    return &m_send_threads[thr_no - glob_num_threads].m_send_buffer_pool;
  }

  void wake_my_send_thread_if_needed(TrpId *trp_id_array,
                                     Uint32 count,
                   struct thr_send_thread_instance *my_send_instance);
  Uint32 get_send_instance(TrpId trp_id);
private:
  struct thr_send_thread_instance* get_send_thread_instance_by_trp(TrpId);

  /* Insert a trp in list of trps that has data available to send */
  void insert_trp(TrpId trp_id, struct thr_send_thread_instance*);

  /* Get a trp id in order to send to it */
  TrpId get_trp(Uint32 instance_no,
                 NDB_TICKS now,
                 struct thr_send_thread_instance* send_instance);

  /* Update rusage parameters for send thread. */
  void update_rusage(struct thr_send_thread_instance *this_send_thread,
                     Uint64 elapsed_time);

  /**
   * Set of utility methods to aid in scheduling of send work:
   *
   * Further sending to trp can be delayed
   * until 'now+delay'. Used either to wait for more packets
   * to be available for bigger chunks, or to wait for an overload
   * situation to clear.
   */
  void set_max_delay(TrpId trp_id, NDB_TICKS now, Uint32 delay_usec);
  void set_overload_delay(TrpId trp_id, NDB_TICKS now, Uint32 delay_usec);
  Uint32 check_delay_expired(TrpId trp_id, NDB_TICKS now);

  /* Completed sending data to this trp, check if more work pending. */ 
  bool check_done_trp(TrpId trp_id);

  /* Get a send thread which isn't awake currently */
  struct thr_send_thread_instance* get_not_awake_send_thread(
                 TrpId trp_id,
                 struct thr_send_thread_instance *send_instance);

  /* Try to lock send_buffer for this trp. */
  static
  int trylock_send_trp(TrpId trp_id);

  /* Perform the actual send to the trp, release send_buffer lock.
   * Return 'true' if there are still more to be sent to this trp.
   */
  static
  bool perform_send(TrpId trp_id, Uint32 thr_no, Uint32& bytes_sent);

  /* Have threads been started */
  Uint32 m_started_threads;

  OverloadStatus m_node_overload_status;

  /* Is data available and next reference for each trp in cluster */
  struct thr_send_trps m_trp_state[MAX_NTRANSPORTERS];

  /**
   * Very few compiler (gcc) allow zero length arrays
   */
#if MAX_NDBMT_SEND_THREADS == 0
#define _MAX_SEND_THREADS 1
#else
#define _MAX_SEND_THREADS MAX_NDBMT_SEND_THREADS
#endif

  /* Data and state for the send threads */
  Uint32 m_num_trps;
  Uint32 m_next_send_thread_instance_by_trp;
  struct thr_send_thread_instance m_send_threads[_MAX_SEND_THREADS];
  Uint16 m_send_thread_instance_by_trp[MAX_NTRANSPORTERS];

public:

  void getSendPerformanceTimers(Uint32 send_instance,
                                Uint64 & exec_time,
                                Uint64 & sleep_time,
                                Uint64 & spin_time,
                                Uint64 & user_time_os,
                                Uint64 & kernel_time_os,
                                Uint64 & elapsed_time_os)
  {
    require(send_instance < globalData.ndbMtSendThreads);
    NdbMutex_Lock(m_send_threads[send_instance].send_thread_mutex);
    exec_time = m_send_threads[send_instance].m_exec_time;
    sleep_time = m_send_threads[send_instance].m_sleep_time;
    spin_time = m_send_threads[send_instance].m_measured_spintime;
    user_time_os= m_send_threads[send_instance].m_user_time_os;
    kernel_time_os = m_send_threads[send_instance].m_kernel_time_os;
    elapsed_time_os = m_send_threads[send_instance].m_elapsed_time_os;
    NdbMutex_Unlock(m_send_threads[send_instance].send_thread_mutex);
  }
  void startChangeNeighbourNode()
  {
    for (Uint32 i = 0; i < globalData.ndbMtSendThreads; i++)
    {
      NdbMutex_Lock(m_send_threads[i].send_thread_mutex);
      for (Uint32 j = 0; j < MAX_NEIGHBOURS; j++)
      {
        m_send_threads[i].m_neighbour_trps[j] = 0;
      }
      m_send_threads[i].m_num_neighbour_trps = 0;
    }
    for (Uint32 i = 0; i < MAX_NTRANSPORTERS; i++)
    {
      m_trp_state[i].m_neighbour_trp = FALSE;
    }
  }
  void setNeighbourNode(NodeId nodeId)
  {
    NodeId id[MAX_NODE_GROUP_TRANSPORTERS];
    Uint32 num_ids;
    if (globalData.ndbMtSendThreads == 0)
    {
      return;
    }
    globalTransporterRegistry.get_trps_for_node(nodeId,
                                                &id[0],
                                                num_ids,
                                                MAX_NODE_GROUP_TRANSPORTERS);
    for (Uint32 index = 0; index < num_ids; index++)
    {
      Uint32 this_id = id[index];
      Uint32 send_instance = get_send_instance(this_id);
      m_trp_state[this_id].m_neighbour_trp = TRUE;
      for (Uint32 i = 0; i < MAX_NEIGHBOURS; i++)
      {
        require(m_send_threads[send_instance].m_neighbour_trps[i] != this_id);
        if (m_send_threads[send_instance].m_neighbour_trps[i] == 0)
        {
          DEB_MULTI_TRP(("Neighbour(%u) of node %u is trp %u",
                         i,
                         nodeId,
                         this_id));
          assert(m_send_threads[send_instance].m_num_neighbour_trps == i);
          m_send_threads[send_instance].m_neighbour_trps[i] = this_id;
          m_send_threads[send_instance].m_num_neighbour_trps++;
          assert(m_send_threads[send_instance].m_num_neighbour_trps <=
                 MAX_NEIGHBOURS);

          break;
        }
      }
    }
  }
  void endChangeNeighbourNode()
  {
    /**
     * If a transporter was in the transporter list before (don't think it
     * should be possible) it doesn't represent an issue since it will simply
     * be handled twice, first from neighbour list and second from list of
     * transporters.
     *
     * The opposite behaviour that a transporter goes from neighbour to not
     * a neighbour transporter any more should only happen in node failures
     * and in that case the transporter should not have any data to send
     * and the transporter will be cleared before the node is allowed to
     * restart again.
     */
    for (Uint32 i = 0; i < globalData.ndbMtSendThreads; i++)
    {
      m_send_threads[i].m_neighbour_trp_index = 0;
      NdbMutex_Unlock(m_send_threads[i].send_thread_mutex);
    }
  }
  void setNodeOverloadStatus(OverloadStatus new_status)
  {
    /**
     * The read of this variable is unsafe, but has no dire consequences
     * if it is shortly inconsistent. We use a memory barrier to at least
     * speed up the spreading of the variable to all CPUs.
     */
    m_node_overload_status = new_status;
    mb();
  }
};


/*
 * The single instance of the thr_send_threads class, if this variable
 * is non-NULL, then we're using send threads, otherwise if NULL, there
 * are no send threads.
 */
static char* g_send_threads_mem = NULL;
static thr_send_threads *g_send_threads = NULL;

extern "C"
void *
mt_send_thread_main(void *thr_arg)
{
  struct thr_send_thread_instance *this_send_thread =
    (thr_send_thread_instance*)thr_arg;

  Uint32 instance_no = this_send_thread->m_instance_no;
  g_send_threads->run_send_thread(instance_no);
  return NULL;
}

thr_send_threads::thr_send_threads()
  : m_started_threads(FALSE),
    m_node_overload_status((OverloadStatus)LIGHT_LOAD_CONST)
{
  struct thr_repository *rep = g_thr_repository;

  for (Uint32 i = 0; i < NDB_ARRAY_SIZE(m_trp_state); i++)
  {
    m_trp_state[i].m_next = 0;
    m_trp_state[i].m_data_available = 0;
    m_trp_state[i].m_thr_no_sender = Uint16(NO_OWNER_THREAD);
    m_trp_state[i].m_send_overload = FALSE;
    m_trp_state[i].m_micros_delayed = 0;
    m_trp_state[i].m_neighbour_trp = FALSE;
    m_trp_state[i].m_overload_counter = 0;
    NdbTick_Invalidate(&m_trp_state[i].m_inserted_time);
  }
  for (Uint32 i = 0; i < NDB_ARRAY_SIZE(m_send_threads); i++)
  {
    m_send_threads[i].m_more_trps = false;
    m_send_threads[i].m_first_trp = 0;
    m_send_threads[i].m_last_trp = 0;
    m_send_threads[i].m_next_is_high_prio_trp = false;
    m_send_threads[i].m_num_neighbour_trps = 0;
    m_send_threads[i].m_neighbour_trp_index = 0;
    for (Uint32 j = 0; j < MAX_NEIGHBOURS; j++)
    {
      m_send_threads[i].m_neighbour_trps[j] = 0;
    }
    m_send_threads[i].m_waiter_struct.init();
    m_send_threads[i].m_instance_no = i;
    m_send_threads[i].m_send_buffer_pool.set_pool(&rep->m_sb_pool);
    m_send_threads[i].send_thread_mutex = NdbMutex_Create();
  }
  memset(&m_send_thread_instance_by_trp[0],
         0xFF,
         sizeof(m_send_thread_instance_by_trp));
  m_next_send_thread_instance_by_trp = 0;
  m_num_trps = 0;
}

thr_send_threads::~thr_send_threads()
{
  if (!m_started_threads)
    return;

  for (Uint32 i = 0; i < globalData.ndbMtSendThreads; i++)
  {
    void *dummy_return_status;

    /* Ensure thread is woken up to die */
    wakeup(&(m_send_threads[i].m_waiter_struct));
    NdbThread_WaitFor(m_send_threads[i].m_thread, &dummy_return_status);
    globalEmulatorData.theConfiguration->removeThread(
      m_send_threads[i].m_thread);
    NdbThread_Destroy(&(m_send_threads[i].m_thread));
  }
}

/**
 * Base transporters are spread equally among the send threads.
 * There is no special connection between a thread and a transporter
 * to another node. Thus round-robin scheduling is good enough.
 */
void
thr_send_threads::assign_trps_to_send_threads()
{
  Uint32 num_trps = globalTransporterRegistry.get_num_trps();
  m_num_trps = num_trps;
  /* Transporter instance 0 isn't used */
  m_send_thread_instance_by_trp[0] = Uint16(~0);
  Uint32 send_instance = 0;
  for (Uint32 i = 1; i <= num_trps; i++)
  {
    m_send_thread_instance_by_trp[i] = send_instance;
    send_instance++;
    if (send_instance == globalData.ndbMtSendThreads)
    {
      send_instance = 0;
    }
  }
  m_next_send_thread_instance_by_trp = 0;
}

void
mt_assign_multi_trps_to_send_threads()
{
  DEB_MULTI_TRP(("mt_assign_multi_trps_to_send_threads()"));
  if (g_send_threads)
  {
    g_send_threads->assign_multi_trps_to_send_threads();
  }
}

/**
 * Multi transporters are assigned to send thread instances to mimic
 * the assignment of LDM instances to send thread instances. This
 * ensures that if an LDM thread sends a message to another LDM
 * thread in the same node group the LDM thread will assist with
 * the sending of this message. The LDM thread will send to another
 * LDM thread mostly in case it is within the same node group and it
 * will then send to the same LDM instance in that node.
 *
 * Ideally the number of LDM threads should be a multiple of the number
 * of send threads to get the best assignment of transporters to send
 * threads.
 */
void
thr_send_threads::assign_multi_trps_to_send_threads()
{
  DEB_MULTI_TRP(("assign_multi_trps_to_send_threads()"));
  Uint32 new_num_trps = globalTransporterRegistry.get_num_trps();
  Uint32 send_instance = m_next_send_thread_instance_by_trp;
  DEB_MULTI_TRP(("assign_multi_trps_to_send_threads(): new_num_trps = %u",
                 new_num_trps));
  for (Uint32 i = m_num_trps + 1; i <= new_num_trps; i++)
  {
    m_send_thread_instance_by_trp[i] = send_instance;
    send_instance++;
    if (send_instance == globalData.ndbMtSendThreads)
    {
      send_instance = 0;
    }
  }
  m_num_trps = new_num_trps;
  m_next_send_thread_instance_by_trp = send_instance;
}

void
thr_send_threads::assign_threads_to_assist_send_threads()
{
  /**
   * Assign the block thread (ldm, tc, rep and main) to assist a certain send
   * thread instance. This means that assistance will only be provided to a
   * subset of the transporters from this block thread. The actual send
   * threads can also assist other send threads to avoid having to wake up
   * all send threads all the time.
   *
   * If we have configured the block thread to not provide any send thread
   * assistance we will not assign any send thread to it, similarly receive
   * threads don't provide send thread assistance and if no send threads
   * are around we use the old method of sending without send threads and
   * in this case the sending is done by all block threads and there are
   * no send threads around at all.
   *
   * We perform round robin of LDM threads first and then round robin on the
   * non-LDM threads. This ensures that the first LDM thread starts at send
   * instance 0 to ensure that we support the transporters used for
   * communication to the same LDM in the same node group. This is not
   * guaranteed for all configurations, but we strive for this configuration
   * to ensure that the LDM thread will quickly send its own messages within
   * the node group. Messages to other nodes will be picked up by another
   * send thread. With only one send thread the LDM threads will support all
   * transporters. Multiple send threads is mainly intended for larger
   * configurations.
   */
  THRConfigApplier & conf = globalEmulatorData.theConfiguration->m_thr_config;
  struct thr_repository* rep = g_thr_repository;
  unsigned int thr_no;
  unsigned next_send_instance = 0;
  for (thr_no = 0; thr_no < glob_num_threads; thr_no++)
  {
    thr_data *selfptr = &rep->m_thread[thr_no];
    selfptr->m_nosend = conf.do_get_nosend(selfptr->m_instance_list,
                                           selfptr->m_instance_count);
    if (is_recv_thread(thr_no) || selfptr->m_nosend == 1)
    {
      selfptr->m_send_instance_no = 0;
      selfptr->m_send_instance = NULL;
      selfptr->m_nosend = 1;
    }
    else if (is_ldm_thread(thr_no))
    {
      selfptr->m_send_instance_no = next_send_instance;
      selfptr->m_send_instance =
        get_send_thread_instance_by_num(next_send_instance);
      next_send_instance++;
      if (next_send_instance == globalData.ndbMtSendThreads)
      {
        next_send_instance = 0;
      }
    }
    else
    {
    }
  }
  for (thr_no = 0; thr_no < glob_num_threads; thr_no++)
  {
    thr_data *selfptr = &rep->m_thread[thr_no];
    if (is_recv_thread(thr_no) ||
        selfptr->m_nosend == 1 ||
        is_ldm_thread(thr_no))
    {
      continue;
    }
    else
    {
      selfptr->m_send_instance_no = next_send_instance;
      selfptr->m_send_instance =
        get_send_thread_instance_by_num(next_send_instance);
      next_send_instance++;
      if (next_send_instance == globalData.ndbMtSendThreads)
      {
        next_send_instance = 0;
      }
    }
  }
}

void
thr_send_threads::start_send_threads()
{
  for (Uint32 i = 0; i < globalData.ndbMtSendThreads; i++)
  {
    m_send_threads[i].m_thread =
      NdbThread_Create(mt_send_thread_main,
                       (void **)&m_send_threads[i],
                       1024*1024,
                       "send thread", //ToDo add number
                       NDB_THREAD_PRIO_MEAN);
    m_send_threads[i].m_thr_index =
      globalEmulatorData.theConfiguration->addThread(
        m_send_threads[i].m_thread,
        SendThread);
  }
  m_started_threads = TRUE;
}

struct thr_send_thread_instance*
thr_send_threads::get_send_thread_instance_by_num(Uint32 instance_no)
{
  return &m_send_threads[instance_no];
}

Uint32
thr_send_threads::get_send_instance(TrpId trp_id)
{
  require(trp_id < MAX_NTRANSPORTERS);
  Uint32 send_thread_instance = m_send_thread_instance_by_trp[trp_id];
  require(send_thread_instance < globalData.ndbMtSendThreads);
  return send_thread_instance;
}

struct thr_send_thread_instance*
thr_send_threads::get_send_thread_instance_by_trp(TrpId trp_id)
{
  require(trp_id < MAX_NTRANSPORTERS);
  Uint32 send_thread_instance = m_send_thread_instance_by_trp[trp_id];
  require(send_thread_instance < globalData.ndbMtSendThreads);
  return &m_send_threads[send_thread_instance];
}

/**
 * Called under mutex protection of send_thread_mutex
 */
void
thr_send_threads::insert_trp(TrpId trp_id,
                             struct thr_send_thread_instance *send_instance)
{
  struct thr_send_trps &trp_state = m_trp_state[trp_id];

  send_instance->m_more_trps = true;
  /* Ensure the lock free ::data_available see 'm_more_trps == TRUE' */
  wmb();

  if (trp_state.m_neighbour_trp)
    return;

  Uint32 first_trp = send_instance->m_first_trp;
  struct thr_send_trps &last_trp_state =
    m_trp_state[send_instance->m_last_trp];
  trp_state.m_next = 0;
  send_instance->m_last_trp = trp_id;
  assert(trp_state.m_data_available > 0);

  if (first_trp == 0)
  {
    send_instance->m_first_trp = trp_id;
  }
  else
  {
    last_trp_state.m_next = trp_id;
  }
}

/**
 * Called under mutex protection of send_thread_mutex
 * The timer is taken before grabbing the mutex and can thus be a
 * bit older than now when compared to other times.
 */
void 
thr_send_threads::set_max_delay(TrpId trp_id, NDB_TICKS now, Uint32 delay_usec)
{
  struct thr_send_trps &trp_state = m_trp_state[trp_id];
  assert(trp_state.m_data_available > 0);
  assert(!trp_state.m_send_overload);

  trp_state.m_micros_delayed = delay_usec;
  trp_state.m_inserted_time = now;
  trp_state.m_overload_counter++;
}

/**
 * Called under mutex protection of send_thread_mutex
 * The time is taken before grabbing the mutex, so this timer
 * could be older time than now in rare cases.
 */
void 
thr_send_threads::set_overload_delay(TrpId trp_id,
                                     NDB_TICKS now,
                                     Uint32 delay_usec)
{
  struct thr_send_trps &trp_state = m_trp_state[trp_id];
  assert(trp_state.m_data_available > 0);
  trp_state.m_send_overload = TRUE;
  trp_state.m_micros_delayed = delay_usec;
  trp_state.m_inserted_time = now;
  trp_state.m_overload_counter++;
}

/**
 * Called under mutex protection of send_thread_mutex
 * The now can be older than what is set in m_inserted_time since
 * now is not taken holding the mutex, thus we can take the time,
 * be scheduled away for a while and return, in the meantime
 * another thread could insert a new event with a newer insert
 * time.
 *
 * We ensure in code below that if this type of event happens that
 * we set the timer to be expired and we use the more recent time
 * as now.
 */
Uint32 
thr_send_threads::check_delay_expired(TrpId trp_id, NDB_TICKS now)
{
  struct thr_send_trps &trp_state = m_trp_state[trp_id];
  assert(trp_state.m_data_available > 0);
  Uint64 micros_delayed = Uint64(trp_state.m_micros_delayed);

  if (micros_delayed == 0)
    return 0;

  Uint64 micros_passed;
  if (now.getUint64() > trp_state.m_inserted_time.getUint64())
  {
    micros_passed = NdbTick_Elapsed(trp_state.m_inserted_time,
                                    now).microSec();
  }
  else
  {
    now = trp_state.m_inserted_time;
    micros_passed = micros_delayed;
  }
  if (micros_passed >= micros_delayed) //Expired
  {
    trp_state.m_inserted_time = now;
    trp_state.m_micros_delayed = 0;
    trp_state.m_send_overload = FALSE;
    return 0;
  }

  // Update and return remaining wait time
  Uint64 remaining_micros = micros_delayed - micros_passed;
  return Uint32(remaining_micros);
}

/**
 * TODO RONM:
 * Add some more NDBINFO table to make it easier to analyse the behaviour
 * of the workings of the MaxSendDelay parameter.
 */

static Uint64 mt_get_send_buffer_bytes(NodeId id);

/**
 * MAX_SEND_BUFFER_SIZE_TO_DELAY is a heauristic constant that specifies
 * a send buffer size that will always be sent. The size of this is based
 * on experience that maximum performance of the send part is achieved at
 * around 64 kBytes of send buffer size and that the difference between
 * 20 kB and 64 kByte is small. So thus avoiding unnecessary delays that
 * gain no significant performance gain.
 */
static const Uint64 MAX_SEND_BUFFER_SIZE_TO_DELAY = (20 * 1024);


/**
 * Get a trp having data to be sent to a trp (returned).
 *
 * Sending could have been delayed, in such cases the trp
 * to expire its delay first will be returned. It is then upto 
 * the callee to either accept this trp, or reinsert it
 * such that it can be returned and retried later.
 *
 * Called under mutex protection of send_thread_mutex
 */
#define DELAYED_PREV_NODE_IS_NEIGHBOUR UINT_MAX32
TrpId
thr_send_threads::get_trp(Uint32 instance_no,
                          NDB_TICKS now,
                          struct thr_send_thread_instance *send_instance)
{
  Uint32 next;
  TrpId trp_id;
  bool retry = false;
  Uint32 prev = 0;
  Uint32 delayed_trp = 0;
  Uint32 delayed_prev_trp = 0;
  Uint32 min_wait_usec = UINT_MAX32;
  do
  {
    if (send_instance->m_next_is_high_prio_trp)
    {
      Uint32 num_neighbour_trps = send_instance->m_num_neighbour_trps;
      Uint32 neighbour_trp_index = send_instance->m_neighbour_trp_index;
      for (Uint32 i = 0; i < num_neighbour_trps; i++)
      {
        trp_id = send_instance->m_neighbour_trps[neighbour_trp_index];
        neighbour_trp_index++;
        if (neighbour_trp_index == num_neighbour_trps)
          neighbour_trp_index = 0;
        send_instance->m_neighbour_trp_index = neighbour_trp_index;
        if (m_trp_state[trp_id].m_data_available > 0 &&
            m_trp_state[trp_id].m_thr_no_sender == NO_OWNER_THREAD)
        {
          const Uint32 send_delay = check_delay_expired(trp_id, now);
          if (likely(send_delay == 0))
          {
            /**
             * Found a neighbour trp to return. Handle this and ensure that
             * next call to get_trp will start looking for non-neighbour
             * trps.
             */
            send_instance->m_next_is_high_prio_trp = false;
            goto found_neighbour;
          }

          /**
           * Found a neighbour trp with delay, record the delay
           * and the trp and set indicator that delayed trp is
           * a neighbour.
           */
          if (send_delay < min_wait_usec)
          {
            min_wait_usec = send_delay;
            delayed_trp = trp_id;
            delayed_prev_trp = DELAYED_PREV_NODE_IS_NEIGHBOUR;
          }
        }
      }
      if (retry)
      {
        /**
         * We have already searched the non-neighbour trps and we
         * have now searched the neighbour trps and found no trps
         * ready to start sending to, we might still have a delayed
         * trp, this will be checked before exiting.
         */
        goto found_no_ready_trps;
      }

      /**
       * We found no ready trps amongst the neighbour trps, we will
       * also search the non-neighbours, we will do this simply by
       * falling through into this part and setting retry to true to
       * indicate that we already searched the neighbour trps.
       */
      retry = true;
    }
    else
    {
      /**
       * We might loop one more time and then we need to ensure that
       * we don't just come back here. If we report a trp from this
       * function this variable will be set again. If we find no trp 
       * then it really doesn't matter what this variable is set to.
       * When trps are available we will always try to be fair and
       * return high prio trps as often as non-high prio trps.
       */
      send_instance->m_next_is_high_prio_trp = true;
    }

    trp_id = send_instance->m_first_trp;
    if (!trp_id)
    {
      if (!retry)
      {
        /**
         * We need to check the neighbour trps before we decide that
         * there is no trps to send to.
         */
        retry = true;
        continue;
      }
      /**
       * Found no trps ready to be sent to, will still need check of
       * delayed trps before exiting.
       */
      goto found_no_ready_trps;
    }

    /**
     * Search for a trp ready to be sent to among the non-neighbour trps.
     * If none found, remember the one with the smallest delay.
     */
    prev = 0;
    while (trp_id)
    {
      next = m_trp_state[trp_id].m_next;
  
      const Uint32 send_delay = check_delay_expired(trp_id, now);
      if (likely(send_delay == 0))
      {
        /**
         * We found a non-neighbour trp to return, handle this
         * and set the next get_trp to start looking for
         * neighbour trps.
         */
        send_instance->m_next_is_high_prio_trp = true;
        goto found_non_neighbour;
      }

      /* Find remaining minimum wait: */
      if (min_wait_usec > send_delay)
      {
        min_wait_usec = send_delay;
        delayed_trp = trp_id;
        delayed_prev_trp = prev;
      }

      prev = trp_id;
      trp_id = next;
    }

    // As 'first_trp != 0', there has to be a 'delayed_trp'
    assert(delayed_trp != 0); 

    if (!retry)
    {
      /**
       * Before we decide to send to a delayed non-neighbour trp
       * we should check if there is a neighbour ready to be sent
       * to, or if there is a neighbour with a lower delay that
       * can be sent to.
       */
      retry = true;
      continue;
    }
    /**
     * No trps ready to send to, but we only get here when we know
     * there is at least a delayed trp, so jump directly to handling
     * of returning delayed trps.
     */
    goto found_delayed_trp;
  } while (1);

found_no_ready_trps:
  /**
   * We have found no trps ready to be sent to yet, we can still
   * have a delayed trp and we don't know from where it comes.
   */
  if (delayed_trp == 0)
  {
    /**
     * We have found no trps to send to, neither non-delayed nor
     * delayed trps. Mark m_more_trps as false to indicate that
     * we have no trps to send to for the moment to give the
     * send threads a possibility to go to sleep.
     */
    send_instance->m_more_trps = false;
    return 0;
  }

  /**
   * We have ensured that delayed_trp exists although we have no
   * trps ready to be sent to yet. We will fall through to handling
   * of finding a delayed trp.
   */

found_delayed_trp:
  /**
   * We found no trp ready to send to but we did find a delayed trp.
   * We don't know if the delayed trp is a neighbour trp or not, we
   * check this using delayed_prev_trp which is set to ~0 for
   * neighbour trps.
   */
  assert(delayed_trp != 0); 
  trp_id = delayed_trp;
  if (delayed_prev_trp == DELAYED_PREV_NODE_IS_NEIGHBOUR)
  {
    /**
     * Go to handling of found neighbour as we have decided to return
     * this delayed neighbour trp.
     */
    send_instance->m_next_is_high_prio_trp = false;
    goto found_neighbour;
  }
  else
  {
    send_instance->m_next_is_high_prio_trp = true;
  }

  prev = delayed_prev_trp;
  next = m_trp_state[trp_id].m_next;

  /**
   * Fall through to found_non_neighbour since we have decided that this
   * delayed trp will be returned.
   */

found_non_neighbour:
  /**
   * We are going to return a non-neighbour trp, either delayed
   * or not. We need to remove it from the list of non-neighbour
   * trps to send to.
   */

  if (likely(trp_id == send_instance->m_first_trp))
  {
    send_instance->m_first_trp = next;
    assert(prev == 0);
  }
  else
  {
    assert(prev != 0);
    m_trp_state[prev].m_next = next;
  }

  if (trp_id == send_instance->m_last_trp)
    send_instance->m_last_trp = prev;

  /**
   * Fall through for non-neighbour trps to same return handling as
   * neighbour trps.
   */

found_neighbour:
  /**
   * We found a trp to return, we will update the data available,
   * we also need to set m_thr_no_sender to indicate which thread
   * is owning the right to send to this trp for the moment.
   *
   * Neighbour trps can go directly here since they are not
   * organised in any lists, but we come here also for
   * non-neighbour trps.
   */
  struct thr_send_trps &trp_state = m_trp_state[trp_id];

  assert(trp_state.m_data_available > 0);
  assert(trp_state.m_thr_no_sender == NO_OWNER_THREAD);
  trp_state.m_next = 0;
  trp_state.m_data_available = 1;
  return (TrpId)trp_id;
}

/* Called under mutex protection of send_thread_mutex */
bool
thr_send_threads::check_done_trp(TrpId trp_id)
{
  struct thr_send_trps &trp_state = m_trp_state[trp_id];
  assert(trp_state.m_data_available > 0);
  trp_state.m_data_available--;
  return (trp_state.m_data_available == 0);
}

/* Called under mutex protection of send_thread_mutex */
struct thr_send_thread_instance*
thr_send_threads::get_not_awake_send_thread(TrpId trp_id,
                         struct thr_send_thread_instance *send_instance)
{
  struct thr_send_thread_instance *used_send_thread;
  if (trp_id != 0)
  {
    Uint32 send_thread = get_send_instance(trp_id);
    if (!m_send_threads[send_thread].m_awake)
    {
      used_send_thread= &m_send_threads[send_thread];
      assert(used_send_thread == send_instance);
      return used_send_thread;
    }
  }
  if (!send_instance->m_awake)
    return send_instance;
  return NULL;
}

/**
 * We have assisted our send thread instance, check if it still
 * need to be woken up.
 */
void
thr_send_threads::wake_my_send_thread_if_needed(TrpId *trp_id_array,
                                                Uint32 count,
                   struct thr_send_thread_instance *my_send_instance)
{
  bool mutex_locked = false;
  struct thr_send_thread_instance *wake_send_instance = NULL;
  for (Uint32 i = 0; i < count; i++)
  {
    TrpId trp_id = trp_id_array[i];
    struct thr_send_thread_instance *send_instance =
      get_send_thread_instance_by_trp(trp_id);
    if (send_instance != my_send_instance)
      continue;
    if (!mutex_locked)
    {
      mutex_locked = true;
      NdbMutex_Lock(my_send_instance->send_thread_mutex);
    }
    struct thr_send_trps& trp_state = m_trp_state[trp_id];
    if (trp_state.m_data_available > 0)
    {
      wake_send_instance = my_send_instance;
      break;
    }
  }
  if (mutex_locked)
  {
    NdbMutex_Unlock(my_send_instance->send_thread_mutex);
  }
  if (wake_send_instance != NULL)
  {
    wakeup(&(wake_send_instance->m_waiter_struct));
  }
}

/**
 * Insert transporter into send thread instance data structures.
 * Wake send thread unless it is the one which we handle ourselves.
 * If we handle it ourselves we will check after assisting the
 * send thread if the thread is still required to wake up. This
 * ensures that running with 1 send thread will avoid waking up
 * send thread when not required to do so. With many send threads
 * we will avoid a small portion of wakeup calls through this
 * handling.
 *
 * If we don't do any send thread assistance the instance is simply
 * NULL here and we will wake all required send threads.
 */
Uint32
thr_send_threads::alert_send_thread(TrpId trp_id,
                                    NDB_TICKS now,
                   struct thr_send_thread_instance *my_send_instance)
{
  struct thr_send_thread_instance *send_instance =
    get_send_thread_instance_by_trp(trp_id);
  struct thr_send_trps& trp_state = m_trp_state[trp_id];

  NdbMutex_Lock(send_instance->send_thread_mutex);
  trp_state.m_data_available++;  // There is more to send
  if (trp_state.m_data_available > 1)
  {
    /**
     * ACTIVE(_P) -> ACTIVE_P
     *
     * The trp is already flagged that it has data needing to be sent.
     * There is no need to wake even more threads up in this case
     * since we piggyback on someone else's request.
     *
     * Waking another thread for sending to this trp, had only
     * resulted in contention and blockage on the send_lock.
     *
     * We are safe that the buffers we have flushed will be read by a send
     * thread: They will either be piggybacked when the send thread
     * 'get_trp()' for sending, or data will be available when
     * send thread 'check_done_trp()', finds that more data has
     * become available. In the later case, the send thread will schedule
     * the trp for another round with insert_trp()
     */
    NdbMutex_Unlock(send_instance->send_thread_mutex);
    return 0;
  }
  assert(!trp_state.m_send_overload);      // Caught above as ACTIVE
  assert(m_trp_state[trp_id].m_thr_no_sender == NO_OWNER_THREAD);
  insert_trp(trp_id, send_instance);       // IDLE -> PENDING

  /**
   * We need to delay sending the data, as set in config.
   * This is the first send to this trp, so we start the
   * delay timer now.
   */
  if (max_send_delay > 0)                   // Wait for more payload?
  {
    set_max_delay(trp_id, now, max_send_delay);
  }

  if (send_instance == my_send_instance)
  {
    NdbMutex_Unlock(send_instance->send_thread_mutex);
    return 1;
  }

  /*
   * Check if the send thread especially responsible for this transporter
   * is awake, if not wake it up.
   */
  struct thr_send_thread_instance *avail_send_thread
    = get_not_awake_send_thread(trp_id, send_instance);

  NdbMutex_Unlock(send_instance->send_thread_mutex);

  if (avail_send_thread)
  {
    /*
     * Wake the assigned sleeping send thread, potentially a spurious wakeup,
     * but this is not a problem, important is to ensure that at least one
     * send thread is awoken to handle our request. If someone is already
     * awake and takes care of our request before we get to wake someone up
     * it's not a problem.
     */
    wakeup(&(avail_send_thread->m_waiter_struct));
  }
  return 1;
}

static bool
check_available_send_data(struct thr_send_thread_instance *send_instance)
{
  return !send_instance->data_available();
}

//static
int
thr_send_threads::trylock_send_trp(TrpId trp_id)
{
  thr_repository::send_buffer *sb = g_thr_repository->m_send_buffers+trp_id;
  return trylock(&sb->m_send_lock);
}

//static
bool
thr_send_threads::perform_send(TrpId trp_id, Uint32 thr_no, Uint32& bytes_sent)
{
  thr_repository::send_buffer * sb = g_thr_repository->m_send_buffers+trp_id;

  /**
   * Set m_send_thread so that our transporter callback can know which thread
   * holds the send lock for this remote trp. This is the thr_no of a block
   * thread or the thr_no of a send thread.
   */
  sb->m_send_thread = thr_no;
  const bool more = globalTransporterRegistry.performSend(trp_id);
  bytes_sent = sb->m_bytes_sent;
  sb->m_send_thread = NO_SEND_THREAD;
  unlock(&sb->m_send_lock);
  return more;
}

static void
update_send_sched_config(THRConfigApplier & conf,
                         unsigned instance_no,
                         bool & real_time)
{
  real_time = conf.do_get_realtime_send(instance_no);
}

static void
yield_rt_break(NdbThread *thread,
               enum ThreadTypes type,
               bool real_time)
{
  Configuration * conf = globalEmulatorData.theConfiguration;
  conf->setRealtimeScheduler(thread,
                             type,
                             FALSE,
                             FALSE);
  conf->setRealtimeScheduler(thread,
                             type,
                             real_time,
                             FALSE);
}

static void
check_real_time_break(NDB_TICKS now,
                      NDB_TICKS *yield_time,
                      NdbThread *thread,
                      enum ThreadTypes type)
{
  if (unlikely(NdbTick_Compare(now, *yield_time) < 0))
  {
    /**
     * Timer was adjusted backwards, or the monotonic timer implementation
     * on this platform is unstable. Best we can do is to restart
     * RT-yield timers from new current time.
     */
    *yield_time = now;
  }

  const Uint64 micros_passed =
    NdbTick_Elapsed(*yield_time, now).microSec();

  if (micros_passed > 50000)
  {
    /**
     * Lower scheduling prio to time-sharing mode to ensure that
     * other threads and processes gets a chance to be scheduled
     * if we run for an extended time.
     */
    yield_rt_break(thread, type, TRUE);
    *yield_time = now;
  }
}

#define NUM_WAITS_TO_CHECK_SPINTIME 6
static void
wait_time_tracking(thr_data *selfptr, Uint64 wait_time_in_us)
{
  for (Uint32 i = 0; i < NUM_SPIN_INTERVALS; i++)
  {
    if (wait_time_in_us <= selfptr->m_spin_stat.m_spin_interval[i])
    {
      selfptr->m_spin_stat.m_micros_sleep_times[i]++;
      selfptr->m_spin_stat.m_num_waits++;
      if (unlikely(selfptr->m_spintime == 0 &&
            selfptr->m_conf_spintime != 0 &&
            selfptr->m_spin_stat.m_num_waits == NUM_WAITS_TO_CHECK_SPINTIME))
      {
        /**
         * React quickly to changes in environment, if we don't have
         * spinning activated and have already seen 15 wait times, it means
         * that there is a good chance that spinning is a good idea now.
         * So invoke a check if we should activate spinning now.
         */
        SimulatedBlock *b = globalData.getBlock(THRMAN, selfptr->m_thr_no + 1);
        ((Thrman*)b)->check_spintime(false);
      }
      return;
    }
  }
  require(false);
}

static bool check_queues_empty(thr_data *selfptr);
static Uint32 scan_time_queues(struct thr_data* selfptr, NDB_TICKS now);
static bool do_send(struct thr_data* selfptr,
                    bool must_send,
                    bool assist_send);
/**
 * We call this function only after executing no jobs and thus it is
 * safe to spin for a short time.
 */
static bool
check_yield(thr_data *selfptr,
            Uint64 min_spin_timer, //microseconds
            Uint32 *spin_time_in_us,
            NDB_TICKS start_spin_ticks)
{
  NDB_TICKS now;
  bool cont_flag = true;
  do
  {
    for (Uint32 i = 0; i < 50; i++)
    {
      /**
       * During around 50 us we only check for JBA and JBB
       * queues to not be empty. This happens when another thread or
       * the receive thread sends a signal to the thread.
       */
      NdbSpin();
      if (!check_queues_empty(selfptr))
      {
        /* Found jobs to execute, successful spin */
        cont_flag = false;
        now = NdbTick_getCurrentTicks();
        break;
      }
      /* Check if we have done enough spinning once per 3 us */
      if ((i & 3) == 3)
        continue;
      now = NdbTick_getCurrentTicks();
      Uint64 spin_micros = NdbTick_Elapsed(start_spin_ticks, now).microSec();
      if (spin_micros > min_spin_timer)
      {
        /**
         * We have spun for the required time, but to no avail, there was no
         * work to do, so it is now time to yield and go to sleep.
         */
        *spin_time_in_us = spin_micros;
        selfptr->m_curr_ticks = now;
        selfptr->m_spin_stat.m_sleep_longer_spin_time++;
        selfptr->m_measured_spintime += spin_micros;
        return true;
      }
    }
    if (!cont_flag)
      break;
    /**
     * Every 50 us we also scan time queues to see if any delayed signals
     * need to be delivered. After checking if this generates any new
     * messages we also check if we have completed spinning for this
     * time.
     */
    const Uint32 lagging_timers = scan_time_queues(selfptr, now);
    if (lagging_timers != 0 ||
        !check_queues_empty(selfptr))
    {
      /* Found jobs to execute, successful spin */
      cont_flag = false;
      break;
    }
  } while (cont_flag);
  /**
   * Successful spinning, we will record spinning time. We will also record
   * the number of micros that this has saved. This is a static number based
   * on experience. We use measurements from virtual machines where we gain
   * the time it would take to go to sleep and wakeup again. This is roughly
   * 25 microseconds.
   *
   * This is the positive part of spinning where we gained something through
   * spinning.
   */
  Uint64 spin_micros = NdbTick_Elapsed(start_spin_ticks, now).microSec();
  selfptr->m_curr_ticks = now;
  selfptr->m_measured_spintime += spin_micros;
  selfptr->m_spin_stat.m_sleep_shorter_spin_time++;
  selfptr->m_micros_sleep += spin_micros;
  wait_time_tracking(selfptr, spin_micros);
  return false;
}

/**
 * We call this function only after executing no jobs and thus it is
 * safe to spin for a short time.
 */
static bool
check_recv_yield(thr_data *selfptr,
                 TransporterReceiveHandle & recvdata,
                 Uint64 min_spin_timer, //microseconds
                 Uint32 & num_events,
                 Uint32 *spin_time_in_us,
                 NDB_TICKS start_spin_ticks)
{
  NDB_TICKS now;
  bool cont_flag = true;
  do
  {
    for (Uint32 i = 0; i < 60; i++)
    {
      /**
       * During around 50 us we only check for JBA and JBB
       * queues to not be empty. This happens when another thread or
       * the receive thread sends a signal to the thread.
       */
      NdbSpin();
      if ((!check_queues_empty(selfptr)) ||
          ((num_events =
            globalTransporterRegistry.pollReceive(0, recvdata)) > 0))
      {
        /* Found jobs to execute, successful spin */
        cont_flag = false;
        now = NdbTick_getCurrentTicks();
        break;
      }
      /* Check if we have done enough spinning once per 3 us */
      if ((i & 3) == 3)
        continue;
      /* Check if we have done enough spinning */
      now = NdbTick_getCurrentTicks();
      Uint64 spin_micros = NdbTick_Elapsed(start_spin_ticks, now).microSec();
      if (spin_micros > min_spin_timer)
      {
        /**
         * We have spun for the required time, but to no avail, there was no
         * work to do, so it is now time to yield and go to sleep.
         */
        selfptr->m_measured_spintime += spin_micros;
        selfptr->m_spin_stat.m_sleep_longer_spin_time++;
        return true;
      }
    }
    if (!cont_flag)
      break;
    /**
     * Every 50 us we also scan time queues to see if any delayed signals
     * need to be delivered. After checking if this generates any new
     * messages we also check if we have completed spinning for this
     * time.
     */
    const Uint32 lagging_timers = scan_time_queues(selfptr, now);
    if (lagging_timers != 0 ||
        !check_queues_empty(selfptr))
    {
      /* Found jobs to execute, successful spin */
      cont_flag = false;
      break;
    }
  } while (cont_flag);
  /**
   * Successful spinning, we will record spinning time. We will also record
   * the number of micros that this has saved. This is a static number based
   * on experience. We use measurements from virtual machines where we gain
   * the time it would take to go to sleep and wakeup again. This is roughly
   * 25 microseconds.
   *
   * This is the positive part of spinning where we gained something through
   * spinning.
   */
  Uint64 spin_micros = NdbTick_Elapsed(start_spin_ticks, now).microSec();
  selfptr->m_measured_spintime += spin_micros;
  selfptr->m_spin_stat.m_sleep_shorter_spin_time++;
  selfptr->m_micros_sleep += spin_micros;
  wait_time_tracking(selfptr, spin_micros);
  return false;
}

/**
 * We enter this function holding the send_thread_mutex if lock is
 * false and we leave no longer holding the mutex.
 */
bool
thr_send_threads::assist_send_thread(Uint32 max_num_trps,
                                     Uint32 thr_no,
                                     NDB_TICKS now,
                                     Uint32 &watchdog_counter,
                   struct thr_send_thread_instance *send_instance,
                   class thread_local_pool<thr_send_page>  & send_buffer_pool)
{
  Uint32 num_trps_sent = 0;
  Uint32 loop = 0;
  NDB_TICKS spin_ticks_dummy;
  TrpId trp_id = 0;

  NdbMutex_Lock(send_instance->send_thread_mutex);

  while (globalData.theRestartFlag != perform_stop &&
         loop < max_num_trps &&
         (trp_id = get_trp(NO_SEND_THREAD, now, send_instance)) != 0)
         // PENDING -> ACTIVE
  {
    if (!handle_send_trp(trp_id,
                         num_trps_sent,
                         thr_no,
                         now,
                         watchdog_counter,
                         send_instance))
    {
      /**
       * Neighbour trps are locked through setting
       * m_trp_state[id].m_thr_no_sender to thr_no while holding
       * the mutex. This flag is set between start of send and end
       * of send. In this case there was no send so the flag isn't
       * set now, since we insert it back immediately it will simply
       * remain unset. We assert on this just in case.
       *
       * Only transporters waiting for delay to expire was waiting to send,
       * we will skip sending in this case and leave it for the send
       * thread to handle it. No reason to set pending_send to true since
       * there is no hurry to send (through setting id = 0 below).
       */
      assert(m_trp_state[trp_id].m_thr_no_sender == NO_OWNER_THREAD);
      insert_trp(trp_id, send_instance);
      trp_id = 0;
      break;
    }

    watchdog_counter = 3;
    send_buffer_pool.release_global(g_thr_repository->m_mm,
                                    RG_TRANSPORTER_BUFFERS,
                                    send_instance->m_instance_no);

    loop++;
  }
  if (trp_id == 0)
  {
    NdbMutex_Unlock(send_instance->send_thread_mutex);
    return false;
  }
  /**
   * There is more work to do, keep pending_send flag to true such
   * that we will quickly work off the queue of send tasks available.
   */
  bool pending_send = send_instance->check_pending_data();
  NdbMutex_Unlock(send_instance->send_thread_mutex);
  return pending_send;
}

/**
 * We hold the send_thread_mutex of the send_instance when we
 * enter this function.
 */
bool
thr_send_threads::handle_send_trp(TrpId trp_id,
                                  Uint32 & num_trps_sent,
                                  Uint32 thr_no,
                                  NDB_TICKS & now,
                                  Uint32 & watchdog_counter,
                         struct thr_send_thread_instance *send_instance)
{
  assert(send_instance == get_send_thread_instance_by_trp(trp_id));
  assert(m_trp_state[trp_id].m_thr_no_sender == NO_OWNER_THREAD);
  if (m_trp_state[trp_id].m_micros_delayed > 0)     // Trp send is delayed
  {
    /**
     * The only transporter ready for send was a transporter that still
     * required waiting. We will only send if we have enough data to
     * send without delay.
     */
    if (m_trp_state[trp_id].m_send_overload)        // Pause overloaded trp
    {
      return false;
    }

    if (mt_get_send_buffer_bytes(trp_id) >= MAX_SEND_BUFFER_SIZE_TO_DELAY)
      set_max_delay(trp_id, now, 0);              // Large packet -> Send now
    else                                          // Sleep, let last awake send
    {
      if (thr_no >= glob_num_threads)
      {
        /**
         * When encountering max_send_delay from send thread we
         * will let the send thread go to sleep for as long as
         * this trp has to wait (it is the shortest sleep we
         * we have. For non-send threads the trp will simply
         * be reinserted and someone will pick up later to handle
         * things.
         *
         * At this point in time there are no transporters ready to
         * send, they all are waiting for the delay to expire.
         */
        send_instance->m_more_trps = false;
      }
      return false;
    }
  }

  /**
   * Multiple send threads can not 'get' the same
   * trp simultaneously. Thus, we does not need
   * to keep the global send thread mutex any longer.
   * Also avoids worker threads blocking on us in 
   * ::alert_send_thread
   */
#ifdef VM_TRACE
  my_thread_yield();
#endif
  assert(m_trp_state[trp_id].m_thr_no_sender == NO_OWNER_THREAD);
  m_trp_state[trp_id].m_thr_no_sender = thr_no;
  NdbMutex_Unlock(send_instance->send_thread_mutex);

  watchdog_counter = 6;

  /**
   * Need a lock on the send buffers to protect against 
   * worker thread doing ::forceSend, possibly
   * disable_send_buffers() and/or lock_/unlock_transporter().
   * To avoid a livelock with ::forceSend() on an overloaded 
   * systems, we 'try-lock', and reinsert the trp for 
   * later retry if failed.
   *
   * To ensure that the combination of more == true &&
   * bytes_sent == 0 can be used to signal that the
   * transporter is overloaded, we initialise bytes_sent to 1 to avoid
   * interpreting a try_lock failure as if it was an overloaded
   * transporter. This is a fix for BUG#22393612.
   */
  bool more = true;
  Uint32 bytes_sent = 1;
#ifdef VM_TRACE
  my_thread_yield();
#endif
  if (likely(trylock_send_trp(trp_id) == 0))
  {
    more = perform_send(trp_id, thr_no, bytes_sent);
    /* We return with no locks or mutexes held */
  }

  /**
   * Note that we do not yet return any send_buffers to the
   * global pool: handle_send_trp() may be called from either
   * a send-thread, or a worker-thread doing 'assist send'.
   * These has different policies for releasing send_buffers,
   * which should be handled by the respective callers.
   * (release_chunk() or release_global())
   *
   * Either own perform_send() processing, or external 'alert'
   * could have signaled that there are more sends pending.
   * If we had no progress in perform_send, we conclude that
   * trp is overloaded, and takes a break doing further send
   * attempts to that trp. Also failure of trylock_send_trp
   * will result on the 'overload' to be concluded.
   * (Quite reasonable as the worker thread is likely forceSend'ing)
   */
  now = NdbTick_getCurrentTicks();

  NdbMutex_Lock(send_instance->send_thread_mutex);
#ifdef VM_TRACE
  my_thread_yield();
#endif
  assert(m_trp_state[trp_id].m_thr_no_sender == thr_no);
  m_trp_state[trp_id].m_thr_no_sender = NO_OWNER_THREAD;
  if (more ||                   // ACTIVE   -> PENDING
      !check_done_trp(trp_id))  // ACTIVE-P -> PENDING
  {
    insert_trp(trp_id, send_instance);

    if (unlikely(more && bytes_sent == 0)) //Trp is overloaded
    {
      set_overload_delay(trp_id, now, 200);//Delay send-retry by 200 us
    }
  }                            // ACTIVE   -> IDLE
  else
  {
    num_trps_sent++;
  }
  return true;
}

void
thr_send_threads::update_rusage(
  struct thr_send_thread_instance *this_send_thread,
  Uint64 elapsed_time)
{
  struct ndb_rusage rusage;

  int res = Ndb_GetRUsage(&rusage, false);
  if (res != 0)
  {
    this_send_thread->m_user_time_os = 0;
    this_send_thread->m_kernel_time_os = 0;
    this_send_thread->m_elapsed_time_os = 0;
    return;
  }
  this_send_thread->m_user_time_os = rusage.ru_utime;
  this_send_thread->m_kernel_time_os = rusage.ru_stime;
  this_send_thread->m_elapsed_time_os = elapsed_time;
}

/**
 * There are some send scheduling algorithms build into the send thread.
 * Mainly implemented as part of ::run_send_thread, thus commented here:
 *
 * We have the possibility to set a 'send delay' for each trp. This
 * is used both for handling send overload where we should wait
 * before retrying, and as an aid for collecting smaller packets into
 * larger, and thus fewer packets. Thus decreasing the send overhead
 * on a highly loaded system.
 *
 * A delay due to overload is always waited for. As there are already
 * queued up send work in the buffers, sending will be possible
 * without the send thread actively busy-retrying. However, delays
 * in order to increase the packed size can be ignored.
 *
 * The basic idea if the later is the following:
 * By introducing a delay we ensure that all block threads have
 * gotten a chance to execute messages that will generate data
 * to be sent to trps. This is particularly helpful in e.g.
 * queries that are scanning a table. Here a SCAN_TABREQ is
 * received in a TC and this generates a number of SCAN_FRAGREQ
 * signals to each LDM, each of those LDMs will in turn generate
 * a number of new signals that are all destined to the same
 * trp. So this delay here increases the chance that those
 * signals can be sent in the same TCP/IP packet over the wire.
 *
 * Another use case is applications using the asynchronous API
 * and thus sending many PK lookups that traverse a trp in
 * parallel from the same destination trp. These can benefit
 * greatly from this extra delay increasing the packet sizes.
 *
 * There is also a case when sending many updates that need to
 * be sent to the other trp in the same node group. By delaying
 * the send of this data we ensure that the receiver thread on
 * the other end is getting larger packet sizes and thus we
 * improve the throughput of the system in all sorts of ways.
 *
 * However we also try to ensure that we don't delay signals in
 * an idle system where response time is more important than
 * the throughput. This is achieved by the fact that we will
 * send after looping through the trps ready to send to. In
 * an idle system this will be a quick operation. In a loaded
 * system this delay can be fairly substantial on the other
 * hand.
 *
 * Finally we attempt to limit the use of more than one send
 * thread to cases of very high load. So if there are only 
 * delayed trp sends remaining, we deduce that the
 * system is lightly loaded and we will go to sleep if there
 * are other send threads also awake.
 */
void
thr_send_threads::run_send_thread(Uint32 instance_no)
{
  struct thr_send_thread_instance *this_send_thread =
    &m_send_threads[instance_no];
  const Uint32 thr_no = glob_num_threads + instance_no;

  {
    /**
     * Wait for thread object to be visible
     */
    while(this_send_thread->m_thread == 0)
      NdbSleep_MilliSleep(30);
  }

  {
    /**
     * Print out information about starting thread
     *   (number, tid, name, the CPU it's locked into (if locked at all))
     * Also perform the locking to CPU.
     */
    BaseString tmp;
    bool fail = false;
    THRConfigApplier & conf = globalEmulatorData.theConfiguration->m_thr_config;
    tmp.appfmt("thr: %u ", thr_no);
    int tid = NdbThread_GetTid(this_send_thread->m_thread);
    if (tid != -1)
    {
      tmp.appfmt("tid: %u ", tid);
    }
    conf.appendInfoSendThread(tmp, instance_no);
    int res = conf.do_bind_send(this_send_thread->m_thread,
                                instance_no);
    if (res < 0)
    {
      fail = true;
      tmp.appfmt("err: %d ", -res);
    }
    else if (res > 0)
    {
      tmp.appfmt("OK ");
    }

    unsigned thread_prio;
    res = conf.do_thread_prio_send(this_send_thread->m_thread,
                                   instance_no,
                                   thread_prio);
    if (res < 0)
    {
      fail = true;
      res = -res;
      tmp.appfmt("Failed to set thread prio to %u, ", thread_prio);
      if (res == SET_THREAD_PRIO_NOT_SUPPORTED_ERROR)
      {
        tmp.appfmt("not supported on this OS");
      }
      else
      {
        tmp.appfmt("error: %d", res);
      }
    }
    else if (res > 0)
    {
      tmp.appfmt("Successfully set thread prio to %u ", thread_prio);
    }

    printf("%s\n", tmp.c_str());
    fflush(stdout);
    if (fail)
    {
      abort();
    }
  }

  /**
   * register watchdog
   */
  globalEmulatorData.theWatchDog->
    registerWatchedThread(&this_send_thread->m_watchdog_counter, thr_no);

  NdbMutex_Lock(this_send_thread->send_thread_mutex);
  this_send_thread->m_awake = FALSE;
  NdbMutex_Unlock(this_send_thread->send_thread_mutex);

  NDB_TICKS yield_ticks;
  bool real_time = false;

  yield_ticks = NdbTick_getCurrentTicks();
  THRConfigApplier & conf = globalEmulatorData.theConfiguration->m_thr_config;
  update_send_sched_config(conf, instance_no, real_time);

  TrpId trp_id = 0;
  Uint64 micros_sleep = 0;
  NDB_TICKS last_now = NdbTick_getCurrentTicks();
  NDB_TICKS last_rusage = last_now;
  NDB_TICKS first_now = last_now;

  while (globalData.theRestartFlag != perform_stop)
  {
    this_send_thread->m_watchdog_counter = 19;

    NDB_TICKS now = NdbTick_getCurrentTicks();
    Uint64 sleep_time = micros_sleep;
    Uint64 exec_time = NdbTick_Elapsed(last_now, now).microSec();
    Uint64 time_since_update_rusage =
      NdbTick_Elapsed(last_rusage, now).microSec();
    /**
     * At this moment exec_time is elapsed time since last time
     * we were here. Now remove the time we spent sleeping to
     * get exec_time, thus exec_time + sleep_time will always
     * be elapsed time.
     */
    exec_time -= sleep_time;
    last_now = now;
    micros_sleep = 0;
    if (time_since_update_rusage > Uint64(50 * 1000))
    {
      Uint64 elapsed_time = NdbTick_Elapsed(first_now, now).microSec();
      last_rusage = last_now;
      NdbMutex_Lock(this_send_thread->send_thread_mutex);
      update_rusage(this_send_thread, elapsed_time);
    }
    else
    {
      NdbMutex_Lock(this_send_thread->send_thread_mutex);
    }
    this_send_thread->m_exec_time += exec_time;
    this_send_thread->m_sleep_time += sleep_time;
    this_send_thread->m_awake = TRUE;

    /**
     * If waited for a specific transporter, reinsert it such that
     * it can be re-evaluated for send by get_trp().
     *
     * This happens when handle_send_trp returns false due to that the
     * only transporter ready for execute was a transporter that still
     * waited for expiration of delay and no other condition allowed it
     * to be sent.
     */
    if (trp_id != 0)
    {
      /**
       * The trp was locked during our sleep. We now release the
       * lock again such that we can acquire the lock again after
       * a short sleep. For non-neighbour trps the insert_trp is
       * sufficient. For neighbour trps we need to ensure that
       * m_trp_state[trp_id].m_thr_no_sender is set to NO_OWNER_THREAD
       * since this is the manner in releasing the lock on those
       * trps.
       */
      assert(m_trp_state[trp_id].m_thr_no_sender == thr_no);
      m_trp_state[trp_id].m_thr_no_sender = NO_OWNER_THREAD;
      insert_trp(trp_id, this_send_thread);
      trp_id = 0;
    }
    while (globalData.theRestartFlag != perform_stop &&
           (trp_id = get_trp(instance_no, now, this_send_thread)) != 0)
           // PENDING -> ACTIVE
    {
      Uint32 num_trps_sent_dummy;
      if (!handle_send_trp(trp_id,
                           num_trps_sent_dummy,
                           thr_no,
                           now,
                           this_send_thread->m_watchdog_counter,
                           this_send_thread))
      {
        /**
         * Neighbour trps are not locked by get_trp and insert_trp.
         * They are locked by setting
         * m_trp_state[trp_id].m_thr_no_sender to thr_no.
         * Here we returned false from handle_send_trp since we were
         * not allowed to send to trp at this time. We want to keep
         * lock on trp as get_trp does for non-neighbour trps, so
         * we set this flag to retain lock even after we release mutex.
         * We also use asserts to ensure the state transitions are ok.
         *
         * The transporter is reinserted into the list of transporters
         * ready to transmit above in the code since id != 0 when we
         * return after sleep.
         */
        assert(m_trp_state[trp_id].m_thr_no_sender == NO_OWNER_THREAD);
        m_trp_state[trp_id].m_thr_no_sender = thr_no;
        break;
      }
      
      /* Release chunk-wise to decrease pressure on lock */
      this_send_thread->m_watchdog_counter = 3;
      this_send_thread->m_send_buffer_pool.release_chunk(
                                     g_thr_repository->m_mm,
                                     RG_TRANSPORTER_BUFFERS,
                                     instance_no);

      /**
       * We set trp_id = 0 for the very rare case where theRestartFlag is set
       * to perform_stop, we should never need this, but add it in just in
       * case.
       */
      trp_id = 0;
    } // while (get_trp()...)

    /* No more trps having data to send right now, prepare to sleep */
    this_send_thread->m_awake = FALSE;
    const Uint32 trp_wait = (trp_id != 0) ?
      m_trp_state[trp_id].m_micros_delayed : 0;
    NdbMutex_Unlock(this_send_thread->send_thread_mutex);


    if (real_time)
    {
      check_real_time_break(now,
                            &yield_ticks,
                            this_send_thread->m_thread,
                            SendThread);
    }


    /**
     * Send thread is by definition a throughput supportive thread.
     * Thus in situations when the latency is at risk the sending
     * is performed by the block threads. Thus there is no reason
     * to perform any spinning in the send thread, we will ignore
     * spin timer for send threads.
     */
    {
      Uint32 max_wait_nsec;
      /**
       * We sleep a max time, possibly waiting for a specific trp
       * with delayed send (overloaded, or waiting for more payload).
       * (Will be alerted to start working when more send work arrives)
       */
      if (trp_wait == 0)
      {
        //50ms, has to wakeup before 100ms watchdog alert.
        max_wait_nsec = 50*1000*1000;
      }
      else
      {
        max_wait_nsec = trp_wait * 1000;
      }
      NDB_TICKS before = NdbTick_getCurrentTicks();
      bool waited = yield(&this_send_thread->m_waiter_struct,
                          max_wait_nsec,
                          check_available_send_data,
                          this_send_thread);
      if (waited)
      {
        NDB_TICKS after = NdbTick_getCurrentTicks();
        micros_sleep += NdbTick_Elapsed(before, after).microSec();
      }
    }
  }

  globalEmulatorData.theWatchDog->unregisterWatchedThread(thr_no);
}

#if 0
static
Uint32
fifo_used_pages(struct thr_data* selfptr)
{
  return calc_fifo_used(selfptr->m_first_unused,
                        selfptr->m_first_free,
                        THR_FREE_BUF_MAX);
}
#endif

ATTRIBUTE_NOINLINE
static
void
job_buffer_full(struct thr_data* selfptr)
{
  ndbout_c("job buffer full");
  dumpJobQueues();
  abort();
}

ATTRIBUTE_NOINLINE
static
void
out_of_job_buffer(struct thr_data* selfptr)
{
  ndbout_c("out of job buffer");
  dumpJobQueues();
  abort();
}

static
thr_job_buffer*
seize_buffer(struct thr_repository* rep, int thr_no, bool prioa)
{
  thr_job_buffer* jb;
  struct thr_data* selfptr = &rep->m_thread[thr_no];
  Uint32 first_free = selfptr->m_first_free;
  Uint32 first_unused = selfptr->m_first_unused;

  /*
   * An empty FIFO is denoted by m_first_free == m_first_unused.
   * So we will never have a completely full FIFO array, at least one entry will
   * always be unused. But the code is simpler as a result.
   */

  /*
   * We never allow the fifo to become completely empty, as we want to have
   * a good number of signals available for trace files in case of a forced
   * shutdown.
   */
  Uint32 buffers = (first_free > first_unused ?
                    first_unused + THR_FREE_BUF_MAX - first_free :
                    first_unused - first_free);
  if (unlikely(buffers <= THR_FREE_BUF_MIN))
  {
    /*
     * All used, allocate another batch from global pool.
     *
     * Put the new buffers at the head of the fifo, so as not to needlessly
     * push out any existing buffers from the fifo (that would loose useful
     * data for signal dumps in trace files).
     */
    Uint32 cnt = 0;
    Uint32 batch = THR_FREE_BUF_MAX / THR_FREE_BUF_BATCH;
    assert(batch > 0);
    assert(batch + THR_FREE_BUF_MIN < THR_FREE_BUF_MAX);
    do {
      jb = rep->m_jb_pool.seize(rep->m_mm,
                                RG_JOBBUFFER);
      if (unlikely(jb == 0))
      {
        if (unlikely(cnt == 0))
        {
          out_of_job_buffer(selfptr);
        }
        break;
      }
      jb->m_len = 0;
      jb->m_prioa = false;
      first_free = (first_free ? first_free : THR_FREE_BUF_MAX) - 1;
      selfptr->m_free_fifo[first_free] = jb;
      batch--;
    } while (cnt < batch);
    selfptr->m_first_free = first_free;
  }

  jb= selfptr->m_free_fifo[first_free];
  selfptr->m_first_free = (first_free + 1) % THR_FREE_BUF_MAX;
  /* Init here rather than in release_buffer() so signal dump will work. */
  jb->m_len = 0;
  jb->m_prioa = prioa;
  return jb;
}

static
void
release_buffer(struct thr_repository* rep, int thr_no, thr_job_buffer* jb)
{
  struct thr_data* selfptr = &rep->m_thread[thr_no];
  Uint32 first_free = selfptr->m_first_free;
  Uint32 first_unused = selfptr->m_first_unused;

  /*
   * Pack near-empty signals, to get more info in the signal traces.
   *
   * This is not currently used, as we only release full job buffers, hence
   * the #if 0.
   */
#if 0
  Uint32 last_free = (first_unused ? first_unused : THR_FREE_BUF_MAX) - 1;
  thr_job_buffer *last_jb = selfptr->m_free_fifo[last_free];
  Uint32 len1, len2;

  if (!jb->m_prioa &&
      first_free != first_unused &&
      !last_jb->m_prioa &&
      (len2 = jb->m_len) <= (thr_job_buffer::SIZE / 4) &&
      (len1 = last_jb->m_len) + len2 <= thr_job_buffer::SIZE)
  {
    /*
     * The buffer being release is fairly empty, and what data it contains fit
     * in the previously released buffer.
     *
     * We want to avoid too many almost-empty buffers in the free fifo, as that
     * makes signal traces less useful due to too little data available. So in
     * this case we move the data from the buffer to be released into the
     * previous buffer, and place the to-be-released buffer at the head of the
     * fifo (to be immediately reused).
     *
     * This is only done for prio B buffers, as we must not merge prio A and B
     * data (or dumps would be incorrect), and prio A buffers are in any case
     * full when released.
     */
    memcpy(last_jb->m_data + len1, jb->m_data, len2*sizeof(jb->m_data[0]));
    last_jb->m_len = len1 + len2;
    jb->m_len = 0;
    first_free = (first_free ? first_free : THR_FREE_BUF_MAX) - 1;
    selfptr->m_free_fifo[first_free] = jb;
    selfptr->m_first_free = first_free;
  }
  else
#endif
  {
    /* Just insert at the end of the fifo. */
    selfptr->m_free_fifo[first_unused] = jb;
    first_unused = (first_unused + 1) % THR_FREE_BUF_MAX;
    selfptr->m_first_unused = first_unused;
  }

  if (unlikely(first_unused == first_free))
  {
    /* FIFO full, need to release to global pool. */
    Uint32 batch = THR_FREE_BUF_MAX / THR_FREE_BUF_BATCH;
    assert(batch > 0);
    assert(batch < THR_FREE_BUF_MAX);
    do {
      rep->m_jb_pool.release(rep->m_mm,
                             RG_JOBBUFFER,
                             selfptr->m_free_fifo[first_free]);
      first_free = (first_free + 1) % THR_FREE_BUF_MAX;
      batch--;
    } while (batch > 0);
    selfptr->m_first_free = first_free;
  }
}

static
inline
Uint32
scan_queue(struct thr_data* selfptr, Uint32 cnt, Uint32 end, Uint32* ptr)
{
  Uint32 thr_no = selfptr->m_thr_no;
  Uint32 **pages = selfptr->m_tq.m_delayed_signals;
  Uint32 free = selfptr->m_tq.m_next_free;
  Uint32* save = ptr;
  for (Uint32 i = 0; i < cnt; i++, ptr++)
  {
    Uint32 val = * ptr;
    if ((val & 0xFFFF) <= end)
    {
      Uint32 idx = val >> 16;
      Uint32 buf = idx >> 8;
      Uint32 pos = MAX_SIGNAL_SIZE * (idx & 0xFF);

      Uint32* page = * (pages + buf);

      const SignalHeader *s = reinterpret_cast<SignalHeader*>(page + pos);
      const Uint32 *data = page + pos + (sizeof(*s)>>2);
      if (0)
	ndbout_c("found %p val: %d end: %d", s, val & 0xFFFF, end);
      /*
       * ToDo: Do measurements of the frequency of these prio A timed signals.
       *
       * If they are frequent, we may want to optimize, as sending one prio A
       * signal is somewhat expensive compared to sending one prio B.
       */
      sendprioa(thr_no, s, data,
                data + s->theLength);
      * (page + pos) = free;
      free = idx;
    }
    else if (i > 0)
    {
      selfptr->m_tq.m_next_free = free;
      memmove(save, ptr, 4 * (cnt - i));
      return i;
    }
    else
    {
      return 0;
    }
  }
  selfptr->m_tq.m_next_free = free;
  return cnt;
}

static
void
handle_time_wrap(struct thr_data* selfptr)
{
  Uint32 i;
  struct thr_tq * tq = &selfptr->m_tq;
  Uint32 cnt0 = tq->m_cnt[0];
  Uint32 cnt1 = tq->m_cnt[1];
  Uint32 tmp0 = scan_queue(selfptr, cnt0, 32767, tq->m_short_queue);
  Uint32 tmp1 = scan_queue(selfptr, cnt1, 32767, tq->m_long_queue);
  cnt0 -= tmp0;
  cnt1 -= tmp1;
  tq->m_cnt[0] = cnt0;
  tq->m_cnt[1] = cnt1;
  for (i = 0; i<cnt0; i++)
  {
    assert((tq->m_short_queue[i] & 0xFFFF) > 32767);
    tq->m_short_queue[i] -= 32767;
  }
  for (i = 0; i<cnt1; i++)
  {
    assert((tq->m_long_queue[i] & 0xFFFF) > 32767);
    tq->m_long_queue[i] -= 32767;
  }
}

/**
 * FUNCTION: scan_time_queues(), scan_time_queues_impl(),
 *           scan_time_queues_backtick()
 *
 * scan_time_queues() Implements the part we want to be inlined
 * into the scheduler loops, while *_impl() & *_backtick() is
 * the more unlikely part we don't call unless the timer has 
 * ticked backward or forward more than 1ms since last 'scan_time.
 * 
 * Check if any delayed signals has expired and should be sent now.
 * The time_queues will be checked every time we detect a change
 * in current time of >= 1ms. If idle we will sleep for max 10ms
 * before rechecking the time_queue.
 *
 * However, some situations need special attention:
 * - Even if we prefer monotonic timers, they are not available, or
 *   implemented in our abstraction layer, for all platforms.
 *   A non-monotonic timer may leap when adjusted by the user, both
 *   forward or backwards.
 * - Early implementation of monotonic timers had bugs where time 
 *   could jump. Similar problems has been reported for several VMs.
 * - There might be CPU contention or system swapping where we might 
 *   sleep for significantly longer that 10ms, causing long forward 
 *   leaps in perceived time.
 *
 * In order to adapt to this non-perfect clock behaviour, the
 * scheduler has its own 'm_ticks' which is the current time
 * as perceived by the scheduler. On entering this function, 'now'
 * is the 'real' current time fetched from NdbTick_getCurrentTime().
 * 'selfptr->m_ticks' is the previous tick seen by the scheduler,
 * and as such is the timestamp which reflects the current time
 * as seen by the timer queues.
 *
 * Normally only a few milliseconds will elapse between each ticks
 * as seen by the diff between 'now' and 'selfthr->m_ticks'.
 * However, if there are larger leaps in the current time,
 * we breaks this up in several small(20ms) steps
 * by gradually increasing schedulers 'm_ticks' time. This ensure
 * that delayed signals will arrive in correct relative order, 
 * and repeated signals (pace signals) are received with
 * the expected frequence. However, each individual signal may
 * be delayed or arriving to fast. Where excact timing is critical,
 * these signals should do their own time calculation by reading 
 * the clock, instead of trusting that the signal is delivered as
 * specified by the 'delay' argument
 *
 * If there are leaps larger than 1500ms, we try a hybrid
 * solution by moving the 'm_ticks' forward, close to the
 * actuall current time, then continue as above from that
 * point in time. A 'time leap Warning' will also be printed
 * in the logs.
 */
static
Uint32
scan_time_queues_impl(struct thr_data* selfptr,
                      Uint32 diff,
                      NDB_TICKS now)
{
  NDB_TICKS last = selfptr->m_ticks;
  Uint32 step = diff;

  if (unlikely(diff > 20))     // Break up into max 20ms steps
  {
    if (unlikely(diff > 1500)) // Time leaped more than 1500ms
    {
      /**
       * There was a long leap in the time since last checking
       * of the time_queues. The clock could have been adjusted, or we
       * are CPU starved. Anyway, we can never make up for the lost 
       * CPU cycles, so we forget about them and start fresh from 
       * a point in time 1000ms behind our current time.
       */
      struct ndb_rusage curr_rusage;
      Ndb_GetRUsage(&curr_rusage, false);
      if ((curr_rusage.ru_utime == 0 &&
           curr_rusage.ru_stime == 0) ||
          (selfptr->m_scan_time_queue_rusage.ru_utime == 0 &&
           selfptr->m_scan_time_queue_rusage.ru_stime == 0))
      {
        /**
         * get_rusage failed for some reason, print old variant of warning
         * message.
         */
        g_eventLogger->warning("thr: %u: Overslept %u ms, expected ~10ms",
                               selfptr->m_thr_no, diff);
      }
      else
      {
        Uint32 diff_real =
          NdbTick_Elapsed(selfptr->m_scan_real_ticks, now).milliSec();
        Uint64 exec_time = curr_rusage.ru_utime -
                           selfptr->m_scan_time_queue_rusage.ru_utime;
        Uint64 sys_time = curr_rusage.ru_stime -
                          selfptr->m_scan_time_queue_rusage.ru_stime;
        g_eventLogger->warning("thr: %u Overslept %u ms, expected ~10ms"
                               ", user time: %llu us, sys_time: %llu us",
                               selfptr->m_thr_no,
                               diff_real,
                               exec_time,
                               sys_time);
      }
      last = NdbTick_AddMilliseconds(last, diff-1000);
    }
    step = 20;  // Max expire intervall handled is 20ms 
  }

  struct thr_tq * tq = &selfptr->m_tq;
  Uint32 curr = tq->m_current_time;
  Uint32 cnt0 = tq->m_cnt[0];
  Uint32 cnt1 = tq->m_cnt[1];
  Uint32 end = (curr + step);
  if (end >= 32767)
  {
    handle_time_wrap(selfptr);
    cnt0 = tq->m_cnt[0];
    cnt1 = tq->m_cnt[1];
    end -= 32767;
  }

  Uint32 tmp0 = scan_queue(selfptr, cnt0, end, tq->m_short_queue);
  Uint32 tmp1 = scan_queue(selfptr, cnt1, end, tq->m_long_queue);

  tq->m_current_time = end;
  tq->m_cnt[0] = cnt0 - tmp0;
  tq->m_cnt[1] = cnt1 - tmp1;
  selfptr->m_ticks = NdbTick_AddMilliseconds(last, step);
  selfptr->m_scan_real_ticks = now;
  Ndb_GetRUsage(&selfptr->m_scan_time_queue_rusage, false);
  return (diff - step);
}

/**
 * Clock has ticked backwards. We try to handle this
 * as best we can.
 */
static
void
scan_time_queues_backtick(struct thr_data* selfptr, NDB_TICKS now)
{
  const NDB_TICKS last = selfptr->m_ticks;
  assert(NdbTick_Compare(now, last) < 0);

  const Uint64 backward = NdbTick_Elapsed(now, last).milliSec();

  /**
   * Silently ignore sub millisecond backticks.
   * Such 'noise' is unfortunately common, even for monotonic timers.
   */
  if (backward > 0)
  {
    g_eventLogger->warning("thr: %u Time ticked backwards %llu ms.",
		           selfptr->m_thr_no, backward);

    /* Long backticks should never happen for monotonic timers */
    assert(backward < 100 || !NdbTick_IsMonotonic()); 

    /* Accept new time as current */
    selfptr->m_ticks = now;
  }
}

/**
 * If someone sends a signal with delay it means that the signal
 * should be executed as soon as we come to the scan_time_queues
 * independent of the amount of time spent since it was sent. We
 * use a special time queue for bounded delay signals to avoid having
 * to scan through all short time queue signals in every loop of
 * the run job buffers.
 */
static inline
void
scan_zero_queue(struct thr_data* selfptr)
{
  struct thr_tq * tq = &selfptr->m_tq;
  Uint32 cnt = tq->m_cnt[2];
  if (cnt)
  {
    Uint32 num_found = scan_queue(selfptr,
                                  cnt,
                                  tq->m_current_time,
                                  tq->m_zero_queue);
    require(num_found == cnt);
  }
  tq->m_cnt[2] = 0;
}

static inline
Uint32
scan_time_queues(struct thr_data* selfptr, NDB_TICKS now)
{
  scan_zero_queue(selfptr);
  const NDB_TICKS last = selfptr->m_ticks;
  if (unlikely(NdbTick_Compare(now, last) < 0))
  {
    scan_time_queues_backtick(selfptr, now);
    return 0;
  }

  const Uint32 diff = (Uint32)NdbTick_Elapsed(last, now).milliSec();
  if (unlikely(diff > 0))
  {
    return scan_time_queues_impl(selfptr, diff, now);
  }
  return 0;
}

static
inline
Uint32*
get_free_slot(struct thr_repository* rep,
	      struct thr_data* selfptr,
	      Uint32* idxptr)
{
  struct thr_tq * tq = &selfptr->m_tq;
  Uint32 idx = tq->m_next_free;
retry:

  if (idx != RNIL)
  {
    Uint32 buf = idx >> 8;
    Uint32 pos = idx & 0xFF;
    Uint32* page = * (tq->m_delayed_signals + buf);
    Uint32* ptr = page + (MAX_SIGNAL_SIZE * pos);
    tq->m_next_free = * ptr;
    * idxptr = idx;
    return ptr;
  }

  Uint32 thr_no = selfptr->m_thr_no;
  for (Uint32 i = 0; i<thr_tq::PAGES; i++)
  {
    if (tq->m_delayed_signals[i] == 0)
    {
      struct thr_job_buffer *jb = seize_buffer(rep, thr_no, false);
      Uint32 * page = reinterpret_cast<Uint32*>(jb);
      tq->m_delayed_signals[i] = page;
      /**
       * Init page
       */
      for (Uint32 j = 0; j < MIN_SIGNALS_PER_PAGE; j ++)
      {
	page[j * MAX_SIGNAL_SIZE] = (i << 8) + (j + 1);
      }
      page[MIN_SIGNALS_PER_PAGE*MAX_SIGNAL_SIZE] = RNIL;
      idx = (i << 8);
      goto retry;
    }
  }
  abort();
  return NULL;
}

void
senddelay(Uint32 thr_no, const SignalHeader* s, Uint32 delay)
{
  struct thr_repository* rep = g_thr_repository;
  struct thr_data* selfptr = &rep->m_thread[thr_no];
  assert(my_thread_equal(selfptr->m_thr_id, my_thread_self()));
  unsigned siglen = (sizeof(*s) >> 2) + s->theLength + s->m_noOfSections;

  Uint32 max;
  Uint32 * cntptr;
  Uint32 * queueptr;

  Uint32 alarm;
  Uint32 nexttimer = selfptr->m_tq.m_next_timer;
  if (delay == SimulatedBlock::BOUNDED_DELAY)
  {
    alarm = selfptr->m_tq.m_current_time;
    cntptr = selfptr->m_tq.m_cnt + 2;
    queueptr = selfptr->m_tq.m_zero_queue;
    max = thr_tq::ZQ_SIZE;
  }
  else
  {
    alarm = selfptr->m_tq.m_current_time + delay;
    if (delay < 100)
    {
      cntptr = selfptr->m_tq.m_cnt + 0;
      queueptr = selfptr->m_tq.m_short_queue;
      max = thr_tq::SQ_SIZE;
    }
    else
    {
      cntptr = selfptr->m_tq.m_cnt + 1;
      queueptr = selfptr->m_tq.m_long_queue;
      max = thr_tq::LQ_SIZE;
    }
  }

  Uint32 idx;
  Uint32* ptr = get_free_slot(rep, selfptr, &idx);
  memcpy(ptr, s, 4*siglen);

  if (0)
    ndbout_c("now: %d alarm: %d send %s from %s to %s delay: %d idx: %x %p",
	     selfptr->m_tq.m_current_time,
	     alarm,
	     getSignalName(s->theVerId_signalNumber),
	     getBlockName(refToBlock(s->theSendersBlockRef)),
	     getBlockName(s->theReceiversBlockNumber),
	     delay,
	     idx, ptr);

  Uint32 i;
  Uint32 cnt = *cntptr;
  Uint32 newentry = (idx << 16) | (alarm & 0xFFFF);

  * cntptr = cnt + 1;
  selfptr->m_tq.m_next_timer = alarm < nexttimer ? alarm : nexttimer;

  if (cnt == 0 || delay == SimulatedBlock::BOUNDED_DELAY)
  {
    /* First delayed signal needs no order and bounded delay is FIFO */
    queueptr[cnt] = newentry;
    return;
  }
  else if (cnt < max)
  {
    for (i = 0; i<cnt; i++)
    {
      Uint32 save = queueptr[i];
      if ((save & 0xFFFF) > alarm)
      {
	memmove(queueptr+i+1, queueptr+i, 4*(cnt - i));
	queueptr[i] = newentry;
	return;
      }
    }
    assert(i == cnt);
    queueptr[i] = newentry;
    return;
  }
  else
  {
    /* Out of entries in time queue, issue proper error */
    if (cntptr == (selfptr->m_tq.m_cnt + 0))
    {
      /* Error in short time queue */
      ERROR_SET(ecError, NDBD_EXIT_TIME_QUEUE_SHORT,
                "Too many in Short Time Queue", "mt.cpp" );
    }
    else if (cntptr == (selfptr->m_tq.m_cnt + 1))
    {
      /* Error in long time queue */
      ERROR_SET(ecError, NDBD_EXIT_TIME_QUEUE_LONG,
                "Too many in Long Time Queue", "mt.cpp" );
    }
    else
    {
      /* Error in zero time queue */
      ERROR_SET(ecError, NDBD_EXIT_TIME_QUEUE_ZERO,
                "Too many in Zero Time Queue", "mt.cpp" );
    }
  }
}

/*
 * Flush the write state to the job queue, making any new signals available to
 * receiving threads.
 *
 * Two versions:
 *    - The general version flush_write_state_other() which may flush to
 *      any thread, and possibly signal any waiters.
 *    - The special version flush_write_state_self() which should only be used
 *      to flush messages to itself.
 *
 * Call to these functions are encapsulated through flush_write_state
 * which decides which of these functions to call.
 */
static inline
void
flush_write_state_self(thr_job_queue_head *q_head, thr_jb_write_state *w)
{
  /* 
   * Can simplify the flush_write_state when writing to myself:
   * Simply update write references wo/ mutex, memory barrier and signaling
   */
  w->m_write_buffer->m_len = w->m_write_pos;
  q_head->m_write_index = w->m_write_index;
  w->init_pending_signals();
}

static inline
void
flush_write_state_other(thr_data *dstptr,
                        thr_job_queue_head *q_head,
                        thr_jb_write_state *w,
                        bool prioa_flag)
{
  Uint32 pending_signals_saved;
  /*
   * Two write memory barriers here, as assigning m_len may make signal data
   * available to other threads, and assigning m_write_index may make new
   * buffers available.
   *
   * We could optimize this by only doing it as needed, and only doing it
   * once before setting all m_len, and once before setting all m_write_index.
   *
   * But wmb() is a no-op anyway in x86 ...
   */
  wmb();
  w->m_write_buffer->m_len = w->m_write_pos;
  wmb();
  q_head->m_write_index = w->m_write_index;

  pending_signals_saved = w->get_pending_signals_wakeup();
  pending_signals_saved += w->get_pending_signals();

  if (pending_signals_saved >= MAX_SIGNALS_BEFORE_WAKEUP &&
      (!prioa_flag))
  {
    w->init_pending_signals();
    wakeup(&(dstptr->m_waiter));
  }
  else
  {
    w->clear_pending_signals_and_set_wakeup(pending_signals_saved);
  }
}

/**
  This function is used when we need to send signal immediately
  due to the flush limit being reached. We don't know whether
  signal is to ourselves in this case and we act dependent on who
  is the receiver of the signal.
*/
static inline
void
flush_write_state(const thr_data *selfptr,
                  thr_data *dstptr,
                  thr_job_queue_head *q_head,
                  thr_jb_write_state *w,
                  bool prioa_flag)
{
  if (dstptr == selfptr)
  {
    flush_write_state_self(q_head, w);
  }
  else
  {
    flush_write_state_other(dstptr, q_head, w, prioa_flag);
  }
}

/**
  This function is used when we are called from flush_jbb_write_state
  where we know that the receiver should wakeup to receive the signals
  we're sending.
*/
static inline
void
flush_write_state_other_wakeup(thr_data *dstptr,
                               thr_job_queue_head *q_head,
                               thr_jb_write_state *w)
{
  /*
   * We already did a memory barrier before the loop calling this
   * function to ensure the buffer is properly seen by receiving
   * thread.
   */
  w->m_write_buffer->m_len = w->m_write_pos;
  wmb();
  q_head->m_write_index = w->m_write_index;

  w->init_pending_signals();
  wakeup(&(dstptr->m_waiter));
}

static
void
flush_jbb_write_state(thr_data *selfptr)
{
  Uint32 thr_count = g_thr_repository->m_thread_count;
  Uint32 self = selfptr->m_thr_no;

  thr_jb_write_state *w = selfptr->m_write_states + self;
  thr_data *thrptr = g_thr_repository->m_thread;

  /**
    We start by flushing to ourselves, this requires no extra memory
    barriers and ensures that we can proceed in the loop knowing that
    we will only send to remote threads.

    After this we will insert a memory barrier before we start updating
    the m_len variable that makes other threads see our signals that
    we're sending to them. We need the memory barrier to ensure that the
    buffers are seen properly updated by the remote thread when they see
    the pointer to them.
  */
  if (w->has_any_pending_signals())
  {
    flush_write_state_self(selfptr->m_in_queue_head + self, w);
  }
  wmb();
  w = selfptr->m_write_states;
  thr_jb_write_state *w_end = selfptr->m_write_states + thr_count;
  for (; w < w_end; thrptr++, w++)
  {
    if (w->has_any_pending_signals())
    {
      thr_job_queue_head *q_head = thrptr->m_in_queue_head + self;
      flush_write_state_other_wakeup(thrptr, q_head, w);
    }
  }
}

/**
 * Receive thread will unpack 1024 signals (MAX_RECEIVED_SIGNALS)
 * from Transporters before running another check_recv_queue
 *
 * This function returns true if there is not space to unpack
 * this amount of signals, else false.
 *
 * Also used as callback function from yield() to recheck
 * 'full' condition before going to sleep.
 */
static bool
check_recv_queue(thr_job_queue_head *q_head)
{
  const Uint32 minfree = (1024 + MIN_SIGNALS_PER_PAGE - 1)/MIN_SIGNALS_PER_PAGE;
  /**
   * NOTE: m_read_index is read wo/ lock (and updated by different thread)
   *       but since the different thread can only consume
   *       signals this means that the value returned from this
   *       function is always conservative (i.e it can be better than
   *       returned value, if read-index has moved but we didnt see it)
   */
  const unsigned ri = q_head->m_read_index;
  const unsigned wi = q_head->m_write_index;
  const unsigned busy = (wi >= ri) ? wi - ri : (thr_job_queue::SIZE - ri) + wi;
  return (1 + minfree + busy >= thr_job_queue::SIZE);
}

/**
 * Check if any of the receive queues for the threads being served
 * by this receive thread, are full.
 * If full: Return 'Thr_data*' for (one of) the thread(s)
 *          which we have to wait for. (to consume from queue)
 */
static struct thr_data*
get_congested_recv_queue(struct thr_repository* rep, Uint32 recv_thread_id)
{
  const unsigned thr_no = first_receiver_thread_no + recv_thread_id;
  thr_data *thrptr = rep->m_thread;

  for (unsigned i = 0; i<glob_num_threads; i++, thrptr++)
  {
    thr_job_queue_head *q_head = thrptr->m_in_queue_head + thr_no;
    if (check_recv_queue(q_head))
    {
      return thrptr;
    }
  }
  return NULL;
}

/**
 * Compute free buffers in specified queue.
 * The SAFETY margin is subtracted from the available
 * 'free'. which is returned.
 */
static
Uint32
compute_free_buffers_in_queue(const thr_job_queue_head *q_head)
{
  /**
   * NOTE: m_read_index is read wo/ lock (and updated by different thread)
   *       but since the different thread can only consume
   *       signals this means that the value returned from this
   *       function is always conservative (i.e it can be better than
   *       returned value, if read-index has moved but we didnt see it)
   */
  unsigned ri = q_head->m_read_index;
  unsigned wi = q_head->m_write_index;
  unsigned free = (wi < ri) ? ri - wi : (thr_job_queue::SIZE + ri) - wi;

  assert(free <= thr_job_queue::SIZE);

  if (free <= (1 + thr_job_queue::SAFETY))
    return 0;
  else 
    return free - (1 + thr_job_queue::SAFETY);
}

static
Uint32
compute_min_free_out_buffers(Uint32 thr_no)
{
  Uint32 minfree = thr_job_queue::SIZE;
  const struct thr_repository* rep = g_thr_repository;
  const struct thr_data *thrptr = rep->m_thread;

  for (unsigned i = 0; i<glob_num_threads; i++, thrptr++)
  {
    const thr_job_queue_head *q_head = thrptr->m_in_queue_head + thr_no;
    unsigned free = compute_free_buffers_in_queue(q_head);

    if (free < minfree)
      minfree = free;
  }
  return minfree;
}

/**
 * Compute max signals that thr_no can execute wo/ risking
 *   job-buffer-full
 *
 *  see-also update_sched_config
 *
 *
 * 1) compute free-slots in ring-buffer from self to each thread in system
 * 2) pick smallest value
 * 3) compute how many signals this corresponds to
 * 4) compute how many signals self can execute if all were to be to
 *    the thread with the fullest ring-buffer (i.e the worst case)
 *
 *   Assumption: each signal may send *at most* 4 signals
 *     - this assumption is made the same in ndbd and ndbmtd and is
 *       mostly followed by block-code, although not it all places :-(
 */
static
Uint32
compute_max_signals_to_execute(Uint32 min_free_buffers)
{
  return ((min_free_buffers * MIN_SIGNALS_PER_PAGE) + 3) / 4;
}

static
void
dumpJobQueues(void)
{
  BaseString tmp;
  const struct thr_repository* rep = g_thr_repository;
  for (unsigned from = 0; from<glob_num_threads; from++)
  {
    for (unsigned to = 0; to<glob_num_threads; to++)
    {
      const thr_data *thrptr = rep->m_thread + to;
      const thr_job_queue_head *q_head = thrptr->m_in_queue_head + from;

      const unsigned used = q_head->used();
      if (used > 0)
      {
        tmp.appfmt(" job buffer %d --> %d, used %d",
                   from, to, used);
        unsigned free = compute_free_buffers_in_queue(q_head);
        if (free <= 0)
        {
          tmp.appfmt(" FULL!");
        }
        else if (free <= thr_job_queue::RESERVED)
        {
          tmp.appfmt(" HIGH LOAD (free:%d)", free);
        }
        tmp.appfmt("\n");
      }
    }
  }
  if (!tmp.empty())
  {
    ndbout_c("Dumping non-empty job queues:\n%s", tmp.c_str());
  }
}

void
trp_callback::reportSendLen(NodeId nodeId, Uint32 count, Uint64 bytes)
{
  SignalT<3> signalT;
  Signal &signal = * new (&signalT) Signal(0);
  memset(&signal.header, 0, sizeof(signal.header));

  if (g_send_threads)
  {
    /**
     * TODO: Implement this also when using send threads!!
     * To handle this we need to be able to send from send
     * threads since the m_send_thread below can be a send
     * thread. One manner to handle is to keep it in send
     * thread data structure and have some block thread
     * gather the data every now and then.
     */
    return;
  }

#ifdef RONM_TODO
  signal.header.theLength = 3;
  signal.header.theSendersSignalId = 0;
  signal.header.theSendersBlockRef = numberToRef(0, globalData.ownId);
  signal.theData[0] = NDB_LE_SendBytesStatistic;
  signal.theData[1] = nodeId;
  signal.theData[2] = (Uint32)(bytes/count);
  signal.header.theVerId_signalNumber = GSN_EVENT_REP;
  signal.header.theReceiversBlockNumber = CMVMI;
  sendlocal(g_thr_repository->m_send_buffers[nodeId].m_send_thread,
            &signalT.header, signalT.theData, NULL);
#endif
}

/**
 * To lock during connect/disconnect, we take both the send lock for the trp
 * (to protect performSend(), and the global receive lock (to protect
 * performReceive()). By having two locks, we avoid contention between the
 * common send and receive operations.
 *
 * We can have contention between connect/disconnect of one transporter and
 * receive for the others. But the transporter code should try to keep this
 * lock only briefly, ie. only to set state to DISCONNECTING / socket fd to
 * NDB_INVALID_SOCKET, not for the actual close() syscall.
 */
void
trp_callback::lock_transporter(NodeId node, TrpId trp_id)
{
  (void)node;
  Uint32 recv_thread_idx = mt_get_recv_thread_idx(trp_id);
  struct thr_repository* rep = g_thr_repository;
  /**
   * Note: take the send lock _first_, so that we will not hold the receive
   * lock while blocking on the send lock.
   *
   * The reverse case, blocking send lock for one transporter while waiting
   * for receive lock, is not a problem, as the transporter being blocked is
   * in any case disconnecting/connecting at this point in time, and sends are
   * non-waiting (so we will not block sending on other transporters).
   */
  lock(&rep->m_send_buffers[trp_id].m_send_lock);
  lock(&rep->m_receive_lock[recv_thread_idx]);
}

void
trp_callback::unlock_transporter(NodeId node, TrpId trp_id)
{
  (void)node;
  Uint32 recv_thread_idx = mt_get_recv_thread_idx(trp_id);
  struct thr_repository* rep = g_thr_repository;
  unlock(&rep->m_receive_lock[recv_thread_idx]);
  unlock(&rep->m_send_buffers[trp_id].m_send_lock);
}

void
trp_callback::lock_send_transporter(NodeId node, TrpId trp_id)
{
  (void)node;
  struct thr_repository* rep = g_thr_repository;
  lock(&rep->m_send_buffers[trp_id].m_send_lock);
}

void
trp_callback::unlock_send_transporter(NodeId node, TrpId trp_id)
{
  (void)node;
  struct thr_repository* rep = g_thr_repository;
  unlock(&rep->m_send_buffers[trp_id].m_send_lock);
}

int
mt_checkDoJob(Uint32 recv_thread_idx)
{
  struct thr_repository* rep = g_thr_repository;

  /**
   * Return '1' if we are not allowed to receive more signals
   * into the job buffers from this 'recv_thread_idx'.
   *
   * NOTE:
   *   We should not loop-wait for buffers to become available
   *   here as we currently hold the receiver-lock. Furthermore
   *   waiting too long here could cause the receiver thread to be
   *   less responsive wrt. moving incoming (TCP) data from the 
   *   TCPTransporters into the (local) receiveBuffers.
   *   The thread could also oversleep on its other tasks as 
   *   handling open/close of connections, and catching 
   *   its own shutdown events
   */
  return (get_congested_recv_queue(rep, recv_thread_idx) != NULL);
}

/**
 * Collect all send-buffer-pages to be delivered to trp
 * from each thread. Link them together and append them to
 * the single send_buffer list 'sb->m_buffer'.
 *
 * The 'sb->m_buffer_lock' has to be held prior to calling
 * this function.
 *
 * Return: Number of bytes in the collected send-buffers.
 *
 * TODO: This is not completely fair,
 *       it would be better to get one entry from each thr_send_queue
 *       per thread instead (until empty)
 */
static
Uint32
link_thread_send_buffers(thr_repository::send_buffer * sb, Uint32 id)
{
  Uint32 ri[MAX_BLOCK_THREADS];
  Uint32 wi[MAX_BLOCK_THREADS];
  thr_send_queue *src = g_thr_repository->m_thread_send_buffers[id];
  for (unsigned thr = 0; thr < glob_num_threads; thr++)
  {
    ri[thr] = sb->m_read_index[thr];
    wi[thr] = src[thr].m_write_index;
  }

  Uint64 sentinel[thr_send_page::HEADER_SIZE >> 1];
  thr_send_page* sentinel_page = new (&sentinel[0]) thr_send_page;
  sentinel_page->m_next = 0;

  struct thr_send_buffer tmp;
  tmp.m_first_page = sentinel_page;
  tmp.m_last_page = sentinel_page;

  Uint32 bytes = 0;

#ifdef ERROR_INSERT

#define MIXOLOGY_MIX_MT_SEND 2

  if (unlikely(globalEmulatorData.theConfiguration->getMixologyLevel() &
               MIXOLOGY_MIX_MT_SEND))
  {
    /**
     * DEBUGGING only
     * Interleave at the page level from all threads with
     * pages to send - intended to help expose signal
     * order dependency bugs
     * TODO : Avoid having a whole separate implementation
     * like this.
     */
    bool more_pages;
    
    do
    {
      src = g_thr_repository->m_thread_send_buffers[id];
      more_pages = false;
      for (unsigned thr = 0; thr < glob_num_threads; thr++, src++)
      {
        Uint32 r = ri[thr];
        Uint32 w = wi[thr];
        if (r != w)
        {
          rmb();
          /* Take one page from this thread's send buffer for this trp */
          thr_send_page * p = src->m_buffers[r];
          assert(p->m_start == 0);
          bytes += p->m_bytes;
          tmp.m_last_page->m_next = p;
          tmp.m_last_page = p;
          
          /* Take page out of read_index slot list */
          thr_send_page * next = p->m_next;
          p->m_next = NULL;
          src->m_buffers[r] = next;
          
          if (next == NULL)
          {
            /**
             * Used up read slot, any more slots available to read
             * from this thread?
             */
            r = (r+1) % thr_send_queue::SIZE;
            more_pages |= (r != w);
            
            /* Update global and local per thread read indices */
            sb->m_read_index[thr] = r;
            ri[thr] = r;
          }
          else
          {
            more_pages |= true;
          }        
        }
      }
    } while (more_pages);
  }
  else

#endif 

  {
    for (unsigned thr = 0; thr < glob_num_threads; thr++, src++)
    {
      Uint32 r = ri[thr];
      Uint32 w = wi[thr];
      if (r != w)
      {
        rmb();
        while (r != w)
        {
          thr_send_page * p = src->m_buffers[r];
          assert(p->m_start == 0);
          bytes += p->m_bytes;
          tmp.m_last_page->m_next = p;
          while (p->m_next != 0)
          {
            p = p->m_next;
            assert(p->m_start == 0);
            bytes += p->m_bytes;
          }
          tmp.m_last_page = p;
          assert(tmp.m_last_page != 0); /* Impossible */
          r = (r + 1) % thr_send_queue::SIZE;
        }
        sb->m_read_index[thr] = r;
      }
    }
  }
  if (bytes > 0)
  {
    const Uint64 buffered_size = sb->m_buffered_size;
    /**
     * Append send buffers collected from threads
     * to end of existing m_buffers.
     */
    if (sb->m_buffer.m_first_page)
    {
      assert(sb->m_buffer.m_first_page != NULL);
      assert(sb->m_buffer.m_last_page != NULL);
      sb->m_buffer.m_last_page->m_next = tmp.m_first_page->m_next;
      sb->m_buffer.m_last_page = tmp.m_last_page;
    }
    else
    {
      assert(sb->m_buffer.m_first_page == NULL);
      assert(sb->m_buffer.m_last_page == NULL);
      sb->m_buffer.m_first_page = tmp.m_first_page->m_next;
      sb->m_buffer.m_last_page = tmp.m_last_page;
    }
    sb->m_buffered_size = buffered_size + bytes;
  }
  return bytes;
}

/**
 * pack thr_send_pages for a particular send-buffer <em>db</em>
 * release pages (local) to <em>pool</em>
 *
 * We're using a very simple algorithm that packs two neighbour
 * pages into one page if possible, if not possible we simply
 * move on. This guarantees that pages will at least be full to
 * 50% fill level which should be sufficient for our needs here.
 *
 * We call pack_sb_pages() when we fail to send all data to one
 * specific trp immediately. This ensures that we won't keep
 * pages allocated with lots of free spaces.
 *
 * We may also pack_sb_pages() from get_bytes_to_send_iovec()
 * if all send buffers can't be filled into the iovec[]. Thus 
 * possibly saving extra send roundtrips.
 *
 * The send threads will use the pack_sb_pages()
 * from the bytes_sent function which is a callback from
 * the transporter.
 *
 * Can only be called with relevant lock held on 'buffer'.
 * Return remaining unsent bytes in 'buffer'.
 */
static
Uint32
pack_sb_pages(thread_local_pool<thr_send_page>* pool,
              struct thr_send_buffer* buffer)
{
  assert(buffer->m_first_page != NULL);
  assert(buffer->m_last_page != NULL);
  assert(buffer->m_last_page->m_next == NULL);

  thr_send_page* curr = buffer->m_first_page;
  Uint32 curr_free = curr->max_bytes() - (curr->m_bytes + curr->m_start);
  Uint32 bytes = curr->m_bytes;
  while (curr->m_next != 0)
  {
    thr_send_page* next = curr->m_next;
    bytes += next->m_bytes;
    assert(next->m_start == 0); // only first page should have half sent bytes
    if (next->m_bytes <= curr_free)
    {
      /**
       * There is free space in the current page and it is sufficient to
       * store the entire next-page. Copy from next page to current page
       * and update current page and release next page to local pool.
       */
      thr_send_page * save = next;
      memcpy(curr->m_data + (curr->m_bytes + curr->m_start),
             next->m_data,
             next->m_bytes);

      curr_free -= next->m_bytes;

      curr->m_bytes += next->m_bytes;
      curr->m_next = next->m_next;

      pool->release_local(save);

#ifdef NDB_BAD_SEND
      if ((curr->m_bytes % 40) == 24)
      {
        /* Oops */
        curr->m_data[curr->m_start + 21] = 'F';
      }
#endif
    }
    else
    {
      /* Not enough free space in current, move to next page */
      curr = next;
      curr_free = curr->max_bytes() - (curr->m_bytes + curr->m_start);
    }
  }

  buffer->m_last_page = curr;
  assert(bytes > 0);
  return bytes;
}

static
void
release_list(thread_local_pool<thr_send_page>* pool,
             thr_send_page* head, thr_send_page * tail)
{
  while (head != tail)
  {
    thr_send_page * tmp = head;
    head = head->m_next;
    pool->release_local(tmp);
  }
  pool->release_local(tail);
}

/**
 * Get buffered pages ready to be sent by the transporter.
 * All pages returned from this function will refer to
 * pages in the m_sending buffers
 * 
 * The 'sb->m_send_lock' has to be held prior to calling
 * this function.
 *
 * Any available 'm_buffer's will be appended to the
 * 'm_sending' buffers with apropriate locks taken.
 *
 * If sending to trp is not enabled, the buffered pages
 * are released instead of being returned from this method.
 */
Uint32
trp_callback::get_bytes_to_send_iovec(NodeId node,
                                      TrpId trp_id,
                                      struct iovec *dst,
                                      Uint32 max)
{
  (void)node;
  thr_repository::send_buffer *sb = g_thr_repository->m_send_buffers + trp_id;
  sb->m_bytes_sent = 0;

  /**
   * Collect any available send pages from the thread queues
   * and 'm_buffers'. Append them to the end of m_sending buffers
   */
  {
    lock(&sb->m_buffer_lock);
    link_thread_send_buffers(sb, trp_id);

    if (sb->m_buffer.m_first_page != NULL)
    {
      // If first page is not NULL, the last page also can't be NULL
      require(sb->m_buffer.m_last_page != NULL);
      if (sb->m_sending.m_first_page == NULL)
      {
        sb->m_sending = sb->m_buffer;
      }
      else
      {
        assert(sb->m_sending.m_last_page != NULL);
        sb->m_sending.m_last_page->m_next = sb->m_buffer.m_first_page;
        sb->m_sending.m_last_page = sb->m_buffer.m_last_page;
      }
      sb->m_buffer.m_first_page = NULL;
      sb->m_buffer.m_last_page  = NULL;

      sb->m_sending_size += sb->m_buffered_size;
      sb->m_buffered_size = 0;
    }
    unlock(&sb->m_buffer_lock);

    if (sb->m_sending.m_first_page == NULL)
      return 0;
  }

  /**
   * If sending to trp is not enabled; discard the send buffers.
   */
  if (unlikely(!sb->m_enabled))
  {
    thread_local_pool<thr_send_page> pool(&g_thr_repository->m_sb_pool, 0);
    release_list(&pool, sb->m_sending.m_first_page, sb->m_sending.m_last_page);
    pool.release_all(g_thr_repository->m_mm,
                     RG_TRANSPORTER_BUFFERS,
                     g_send_threads == NULL ?
                       0 :
                       g_send_threads->get_send_instance(trp_id));

    sb->m_sending.m_first_page = NULL;
    sb->m_sending.m_last_page = NULL;
    sb->m_sending_size = 0;
    return 0;
  }

  /**
   * Process linked-list and put into iovecs
   */
fill_iovec:
  Uint32 tot = 0;
  Uint32 pos = 0;
  thr_send_page * p = sb->m_sending.m_first_page;

#ifdef NDB_LUMPY_SEND
  /* Drip feed transporter a few bytes at a time to send */
  do
  {
    Uint32 offset = 0;
    while ((offset < p->m_bytes) && (pos < max))
    {
      /* 0 -+1-> 1 -+6-> (7)3 -+11-> (18)2 -+10-> 0 */
      Uint32 lumpSz = 1;
      switch (offset % 4)
      {
      case 0 : lumpSz = 1; break;
      case 1 : lumpSz = 6; break;
      case 2 : lumpSz = 10; break;
      case 3 : lumpSz = 11; break;
      }
      const Uint32 remain = p->m_bytes - offset;
      lumpSz = (remain < lumpSz)?
        remain :
        lumpSz;

      dst[pos].iov_base = p->m_data + p->m_start + offset;
      dst[pos].iov_len = lumpSz;
      pos ++;
      offset+= lumpSz;
    }
    if (pos == max)
    {
      return pos;
    }
    assert(offset == p->m_bytes);
    p = p->m_next;
  } while (p != NULL);

  return pos;
#endif

  do {
    dst[pos].iov_len = p->m_bytes;
    dst[pos].iov_base = p->m_data + p->m_start;
    assert(p->m_start + p->m_bytes <= p->max_bytes());
    tot += p->m_bytes;
    pos++;
    p = p->m_next;
    if (p == NULL)
      return pos;
  } while (pos < max);

  /**
   * Possibly pack send-buffers to get better utilization:
   * If we were unable to fill all sendbuffers into iovec[],
   * we pack the sendbuffers now if they have a low fill degree.
   * This could save us another OS-send for sending the remaining.
   */
  if (pos == max && max > 1 &&                    // Exhausted iovec[]
      tot < (pos * thr_send_page::max_bytes())/4) // < 25% filled
  {
    const Uint32 thr_no = sb->m_send_thread;
    assert(thr_no != NO_SEND_THREAD);

    if (!is_send_thread(thr_no))
    {
      thr_data * thrptr = &g_thr_repository->m_thread[thr_no];
      pack_sb_pages(&thrptr->m_send_buffer_pool, &sb->m_sending);
    }
    else
    {
      pack_sb_pages(g_send_threads->get_send_buffer_pool(thr_no), &sb->m_sending);
    }

    /**
     * Retry filling iovec[]. As 'pack' will ensure at least 50% fill degree,
     * we will not do another 'pack' after the retry.
     */
    goto fill_iovec;
  }
  return pos;
}

static
Uint32
bytes_sent(thread_local_pool<thr_send_page>* pool,
           thr_repository::send_buffer* sb, Uint32 bytes)
{
  const Uint64 sending_size = sb->m_sending_size;
  assert(bytes && bytes <= sending_size);

  sb->m_bytes_sent = bytes;
  sb->m_sending_size = sending_size - bytes;

  Uint32 remain = bytes;
  thr_send_page * prev = NULL;
  thr_send_page * curr = sb->m_sending.m_first_page;

  /* Some, or all, in 'm_sending' was sent, find endpoint. */
  while (remain && remain >= curr->m_bytes)
  {
    /**
     * Calculate new current page such that we can release the
     * pages that have been completed and update the state of
     * the new current page
     */
    remain -= curr->m_bytes;
    prev = curr;
    curr = curr->m_next;
  }

  if (remain)
  {
    /**
     * Not all pages was fully sent and we stopped in the middle of
     * a page
     *
     * Update state of new current page and release any pages
     * that have already been sent
     */
    curr->m_start += remain;
    assert(curr->m_bytes > remain);
    curr->m_bytes -= remain;
    if (prev)
    {
      release_list(pool, sb->m_sending.m_first_page, prev);
    }
  }
  else
  {
    /**
     * We sent a couple of full pages and the sending stopped at a
     * page boundary, so we only need to release the sent pages
     * and update the new current page.
     */
    if (prev)
    {
      release_list(pool, sb->m_sending.m_first_page, prev);

      if (prev == sb->m_sending.m_last_page)
      {
        /**
         * Every thing was released, release the pages in the local pool
         */
        sb->m_sending.m_first_page = NULL;
        sb->m_sending.m_last_page = NULL;
        return 0;
      }
    }
    else
    {
      assert(sb->m_sending.m_first_page != NULL);
      pool->release_local(sb->m_sending.m_first_page);
    }
  }

  sb->m_sending.m_first_page = curr;

  /**
   * Since not all bytes were sent...
   * spend the time to try to pack the m_sending pages
   * possibly releasing send-buffer
   */
  return pack_sb_pages(pool, &sb->m_sending);
}

/**
 * Register the specified amount of 'bytes' as sent, starting
 * from the first avail byte in the m_sending buffer.
 *
 * The 'm_send_lock' has to be held prior to calling
 * this function.
 */
Uint32
trp_callback::bytes_sent(NodeId node, TrpId trp_id, Uint32 bytes)
{
  (void)node;
  thr_repository::send_buffer *sb = g_thr_repository->m_send_buffers+trp_id;
  Uint32 thr_no = sb->m_send_thread;
  assert(thr_no != NO_SEND_THREAD);
  if (!is_send_thread(thr_no))
  {
    thr_data * thrptr = &g_thr_repository->m_thread[thr_no];
    return ::bytes_sent(&thrptr->m_send_buffer_pool,
                        sb,
                        bytes);
  }
  else
  {
    return ::bytes_sent(g_send_threads->get_send_buffer_pool(thr_no),
                        sb,
                        bytes);
  }
}

void
trp_callback::enable_send_buffer(NodeId node, TrpId trp_id)
{
  (void)node;
  thr_repository::send_buffer *sb = g_thr_repository->m_send_buffers+trp_id;
  lock(&sb->m_send_lock);
  assert(sb->m_sending_size == 0);
  {
    /**
     * Collect and discard any sent buffered signals while
     * send buffers were disabled.
     */ 
    lock(&sb->m_buffer_lock);
    link_thread_send_buffers(sb, trp_id);

    if (sb->m_buffer.m_first_page != NULL)
    {
      thread_local_pool<thr_send_page> pool(&g_thr_repository->m_sb_pool, 0);
      release_list(&pool, sb->m_buffer.m_first_page, sb->m_buffer.m_last_page);
      pool.release_all(g_thr_repository->m_mm,
                       RG_TRANSPORTER_BUFFERS,
                       g_send_threads == NULL ?
                         0 :
                         g_send_threads->get_send_instance(trp_id));
      sb->m_buffer.m_first_page = NULL;
      sb->m_buffer.m_last_page = NULL;
      sb->m_buffered_size = 0;
    }
    unlock(&sb->m_buffer_lock);
  }
  assert(sb->m_enabled == false);
  sb->m_enabled = true;
  unlock(&sb->m_send_lock);
}

void
trp_callback::disable_send_buffer(NodeId node, TrpId trp_id)
{
  (void)node;
  thr_repository::send_buffer *sb = g_thr_repository->m_send_buffers+trp_id;
  lock(&sb->m_send_lock);
  sb->m_enabled = false;

  /**
   * Discard buffered signals not yet sent:
   * Note that other threads may still continue send-buffering into
   * their thread local send buffers until they discover that the 
   * transporter has disconnect. However, these sent signals will
   * either be discarded when collected by ::get_bytes_to_send_iovec(),
   * or any leftovers discarded by ::enable_send_buffer()
   */
  if (sb->m_sending.m_first_page != NULL)
  {
    thread_local_pool<thr_send_page> pool(&g_thr_repository->m_sb_pool, 0);
    release_list(&pool, sb->m_sending.m_first_page, sb->m_sending.m_last_page);
    pool.release_all(g_thr_repository->m_mm,
                     RG_TRANSPORTER_BUFFERS,
                     g_send_threads == NULL ?
                       0 :
                       g_send_threads->get_send_instance(trp_id));
    sb->m_sending.m_first_page = NULL;
    sb->m_sending.m_last_page = NULL;
    sb->m_sending_size = 0;
  }

  unlock(&sb->m_send_lock);
}

static inline
void
register_pending_send(thr_data *selfptr, Uint32 trp_id)
{
  /* Mark that this trp has pending send data. */
  if (!selfptr->m_pending_send_mask.get(trp_id))
  {
    selfptr->m_pending_send_mask.set(trp_id, 1);
    Uint32 i = selfptr->m_pending_send_count;
    selfptr->m_pending_send_trps[i] = trp_id;
    selfptr->m_pending_send_count = i + 1;
  }
}

/**
  Pack send buffers to make memory available to other threads. The signals
  sent uses often one page per signal which means that most pages are very
  unpacked. In some situations this means that we can run out of send buffers
  and still have massive amounts of free space. 

  We call this from the main loop in the block threads when we fail to
  allocate enough send buffers. In addition we call the node local
  pack_sb_pages() several places - See header-comment for that function.
*/
static
void
try_pack_send_buffers(thr_data* selfptr)
{
  thr_repository* rep = g_thr_repository;
  thread_local_pool<thr_send_page>* pool = &selfptr->m_send_buffer_pool;

  for (Uint32 i = 1; i < NDB_ARRAY_SIZE(selfptr->m_send_buffers); i++)
  {
    if (globalTransporterRegistry.get_transporter(i))
    {
      thr_repository::send_buffer* sb = rep->m_send_buffers+i;
      if (trylock(&sb->m_buffer_lock) != 0)
      {
        continue; // Continue with next if busy
      }

      link_thread_send_buffers(sb, i);
      if (sb->m_buffer.m_first_page != NULL)
      {
        pack_sb_pages(pool, &sb->m_buffer);
      }
      unlock(&sb->m_buffer_lock);
    }
  }
  /* Release surplus buffers from local pool to global pool */
  pool->release_global(g_thr_repository->m_mm,
                       RG_TRANSPORTER_BUFFERS,
                       selfptr->m_send_instance_no);
}


/**
 * publish thread-locally prepared send-buffer
 */
static
void
flush_send_buffer(thr_data* selfptr, Uint32 trp_id)
{
  Uint32 thr_no = selfptr->m_thr_no;
  thr_send_buffer * src = selfptr->m_send_buffers + trp_id;
  thr_repository* rep = g_thr_repository;

  if (src->m_first_page == 0)
  {
    return;
  }
  assert(src->m_last_page != 0);

  thr_send_queue * dst = rep->m_thread_send_buffers[trp_id]+thr_no;
  thr_repository::send_buffer* sb = rep->m_send_buffers+trp_id;

  Uint32 wi = dst->m_write_index;
  Uint32 next = (wi + 1) % thr_send_queue::SIZE;
  Uint32 ri = sb->m_read_index[thr_no];

  /**
   * If thread local ring buffer of send-buffers is full:
   * Empty it by transfering them to the global send_buffer list.
   */
  if (unlikely(next == ri))
  {
    lock(&sb->m_buffer_lock);
    link_thread_send_buffers(sb, trp_id);
    unlock(&sb->m_buffer_lock);
  }

  dst->m_buffers[wi] = src->m_first_page;
  wmb();
  dst->m_write_index = next;

  src->m_first_page = 0;
  src->m_last_page = 0;
}

/**
 * This is used in case send buffer gets full, to force an emergency send,
 * hopefully freeing up some buffer space for the next signal.
 */
bool
mt_send_handle::forceSend(NodeId node, TrpId trp_id)
{
  (void)node;
  struct thr_repository *rep = g_thr_repository;
  struct thr_data *selfptr = m_selfptr;
  struct thr_repository::send_buffer * sb = rep->m_send_buffers + trp_id;

  {
    /**
     * NOTE: we don't need a memory barrier after clearing
     *       m_force_send here as we unconditionally lock m_send_lock
     *       hence there is no way that our data can be "unsent"
     */
    sb->m_force_send = 0;

    lock(&sb->m_send_lock);
    sb->m_send_thread = selfptr->m_thr_no;
    bool more = globalTransporterRegistry.performSend(trp_id, false);
    sb->m_send_thread = NO_SEND_THREAD;
    unlock(&sb->m_send_lock);

    /**
     * release buffers prior to maybe looping on sb->m_force_send
     */
    selfptr->m_send_buffer_pool.release_global(rep->m_mm,
                                               RG_TRANSPORTER_BUFFERS,
                                               selfptr->m_send_instance_no);
    /**
     * We need a memory barrier here to prevent race between clearing lock
     *   and reading of m_force_send.
     *   CPU can reorder the load to before the clear of the lock
     */
    mb();
    if (unlikely(sb->m_force_send) || more)
    {
      register_pending_send(selfptr, trp_id);
    } 
  }

  return true;
}

/**
 * try sending data
 */
static
void
try_send(thr_data * selfptr, Uint32 trp_id)
{
  struct thr_repository *rep = g_thr_repository;
  struct thr_repository::send_buffer * sb = rep->m_send_buffers + trp_id;

  if (trylock(&sb->m_send_lock) == 0)
  {
    /**
     * Now clear the flag, and start sending all data available to this trp.
     *
     * Put a memory barrier here, so that if another thread tries to grab
     * the send lock but fails due to us holding it here, we either
     * 1) Will see m_force_send[id] set to 1 at the end of the loop, or
     * 2) We clear here the flag just set by the other thread, but then we
     * will (thanks to mb()) be able to see and send all of the data already
     * in the first send iteration.
     */
    sb->m_force_send = 0;
    mb();

    sb->m_send_thread = selfptr->m_thr_no;
    globalTransporterRegistry.performSend(trp_id);
    sb->m_send_thread = NO_SEND_THREAD;
    unlock(&sb->m_send_lock);

    /**
     * release buffers prior to maybe looping on sb->m_force_send
     */
    selfptr->m_send_buffer_pool.release_global(rep->m_mm,
                                               RG_TRANSPORTER_BUFFERS,
                                               selfptr->m_send_instance_no);

    /**
     * We need a memory barrier here to prevent race between clearing lock
     *   and reading of m_force_send.
     *   CPU can reorder the load to before the clear of the lock
     */
    mb();
    if (unlikely(sb->m_force_send))
    {
      register_pending_send(selfptr, trp_id);
    } 
  }
}

/**
 * Flush send buffers and append them to dst. trps send queue
 *
 * Flushed buffer contents are piggybacked when another thread
 * do_send() to the same dst. trp. This makes it possible to have
 * more data included in each message, and thereby reduces total
 * #messages handled by the OS which really impacts performance!
 */
static
void
do_flush(struct thr_data* selfptr)
{
  Uint32 i;
  Uint32 count = selfptr->m_pending_send_count;
  NodeId *trps = selfptr->m_pending_send_trps;

  for (i = 0; i < count; i++)
  {
    flush_send_buffer(selfptr, trps[i]);
  }
}

/**
 * Use the THRMAN block to send the WAKEUP_THREAD_ORD signal
 * to the block thread that we want to wakeup.
 */
#define MICROS_BETWEEN_WAKEUP_IDLE_THREAD 100
static
inline
void
send_wakeup_thread_ord(struct thr_data* selfptr,
                       NDB_TICKS now)
{
  if (selfptr->m_wakeup_instance > 0)
  {
    Uint64 since_last =
      NdbTick_Elapsed(selfptr->m_last_wakeup_idle_thread, now).microSec();
    if (since_last > MICROS_BETWEEN_WAKEUP_IDLE_THREAD)
    {
      selfptr->m_signal->theData[0] = selfptr->m_wakeup_instance;
      SimulatedBlock *b = globalData.getBlock(THRMAN, selfptr->m_thr_no+1);
      b->executeFunction_async(GSN_SEND_WAKEUP_THREAD_ORD, selfptr->m_signal);
      selfptr->m_last_wakeup_idle_thread = now;
    }
  }
}

/**
 * Send any pending data to remote trps.
 *
 * If MUST_SEND is false, will only try to lock the send lock, but if it would
 * block, that trp is skipped, to be tried again next time round.
 *
 * If MUST_SEND is true, we still only try to lock, but if it would block,
 * we will force the thread holding the lock, to do the sending on our behalf.
 *
 * The list of pending trps to send to is thread-local, but the per-trp send
 * buffer is shared by all threads. Thus we might skip a trp for which
 * another thread has pending send data, and we might send pending data also
 * for another thread without clearing the trp from the pending list of that
 * other thread (but we will never loose signals due to this).
 *
 * Return number of trps which still has pending data to be sent.
 * These will be retried again in the next round. 'Pending' is 
 * returned as a negative number if nothing was sent in this round.
 *
 * (Likely due to receivers consuming too slow, and receive and send buffers
 *  already being filled up)
 *
 * Sending data to other trps is a task that we perform using an algorithm
 * that depends on the state of block threads. The block threads can be in
 * 3 different states:
 *
 * LIGHT_LOAD:
 * -----------
 * In this state we will send to all trps we generate data for. In addition
 * we will also send to one trp if we are going to sleep, we will stay awake
 * until no more trps to send to. However between each send we will also
 * ensure that we execute any signals destined for us.
 *
 * LIGHT_LOAD threads can also be provided to other threads as wakeup targets.
 * This means that these threads will be woken up regularly under load to
 * assist with sending.
 *
 * MEDIUM_LOAD:
 * ------------
 * At this load level we will also assist send threads before going to sleep
 * and continue so until we have work ourselves to do or until there are no
 * more trps to send to. We will additionally send partially our own data.
 * We will also wake up a send thread during send to ensure that sends are
 * performed ASAP.
 *
 * OVERLOAD:
 * ---------
 * At this level we will simply inform the send threads about the trps we
 * sent some data to, the actual sending will be handled by send threads
 * and other block threads assisting the send threads.
 *
 * In addition if any thread is at overload level we will sleep for a shorter
 * time.
 *
 * The decision about which idle threads to wake up, which overload level to
 * use and when to sleep for shorter time is all taken by the local THRMAN
 * block. Some decisions is also taken by the THRMAN instance in the main
 * thread.
 *
 * Send threads are woken up in a round robin fashion, each time they are
 * awoken they will continue executing until no more work is around.
 */
static
bool
do_send(struct thr_data* selfptr, bool must_send, bool assist_send)
{
  Uint32 count = selfptr->m_pending_send_count;
  NodeId *trps = selfptr->m_pending_send_trps;

  const NDB_TICKS now = NdbTick_getCurrentTicks();
  selfptr->m_curr_ticks = now;
  bool pending_send = false;
  selfptr->m_watchdog_counter = 6;

  if (count == 0)
  {
    if (must_send && assist_send && g_send_threads &&
        selfptr->m_overload_status <= (OverloadStatus)MEDIUM_LOAD_CONST &&
        (selfptr->m_nosend == 0))
    {
      /**
       * For some overload states we will here provide some
       * send assistance even though we had nothing to send
       * ourselves. We will however not need to offload any
       * sends ourselves.
       * 
       * The idea is that when we get here the thread is usually not so
       * active with other things as it has nothing to send, it must
       * send which means that it is preparing to go to sleep and
       * we have excluded the receive threads through assist_send.
       *
       * We will avoid this extra send when we are in overload mode since
       * it is likely that we will find work to do before going to sleep
       * anyways. In all other modes it makes sense to spend some time
       * sending before going to sleep. In particular TC threads will be
       * doing major send assistance here.
       *
       * In case there is more work to do and our thread is mostly idle,
       * we will soon enough be back here and assist the send thread
       * again. We make this happen by setting pending_send flag in
       * return from this mode. We come back here after checking that
       * we have no signals to process, so at most we will delay the
       * signal execution here by the time it takes to send to one
       * trp.
       *
       * The receive threads won't assist the send thread to ensure
       * that we can respond to incoming messages ASAP. We want to
       * to optimise for response time here since this is needed to
       * ensure that the block threads have sufficient work to do.
       *
       * If we come here and have had nothing to send, then we're able to
       * do some more sending if there are pending send still in send queue.
       * So we return pending_send != 0 in this case to ensure that this
       * thread doesn't go to sleep, but rather come back here to assist the
       * send thread a bit more. We'll continue spinning here until we get
       * some work to do or until the send queue is empty.
       */
      Uint32 num_trps_to_send_to = 1;
      pending_send = g_send_threads->assist_send_thread(
                                         num_trps_to_send_to,
                                         selfptr->m_thr_no,
                                         now,
                                         selfptr->m_watchdog_counter,
                                         selfptr->m_send_instance,
                                         selfptr->m_send_buffer_pool);
      NDB_TICKS after = NdbTick_getCurrentTicks();
      selfptr->m_micros_send += NdbTick_Elapsed(now, after).microSec();
    }
    return pending_send; // send-buffers empty
  }

  /* Clear the pending list. */
  selfptr->m_pending_send_mask.clear();
  selfptr->m_pending_send_count = 0;
  selfptr->m_watchdog_counter = 6;
  for (Uint32 i = 0; i < count; i++)
  {
    /**
     * Make the data available for sending immediately so that
     * any other trp sending will grab this data without having
     * wait for us to handling the other trps.
     */
    Uint32 id = trps[i];
    flush_send_buffer(selfptr, id);
  }
  selfptr->m_watchdog_counter = 6;
  if (g_send_threads)
  {
    /**
     * Each send thread is only responsible for a subset of the transporters
     * to send to and we will only assist a subset of the transporters
     * for sending. This means that it is very hard to predict whether send
     * thread needs to be woken up. This means that we will awake the send
     * threads required for sending, even if no send assistance was really
     * required. This will create some extra load on the send threads, but
     * will make NDB data nodes more scalable to handle extremely high loads.
     *
     * When we are in an overloaded state, we move the trps to send to
     * into the send thread global lists. Since we already woken up the
     * send threads to handle sends we do no more in overloaded state.
     *
     * We don't record any send time here since it would be
     * an unnecessary extra load, we only grab a mutex and
     * ensure that someone else takes over our send work.
     *
     * When the user have set nosend=1 on this thread we will
     * never assist with the sending.
     */
    if (selfptr->m_overload_status == (OverloadStatus)OVERLOAD_CONST ||
        selfptr->m_nosend != 0)
    {
      for (Uint32 i = 0; i < count; i++)
      {
        g_send_threads->alert_send_thread(trps[i], now, NULL);
      }
    }
    else
    {
      /**
       * While we are in an light load state we will always try to
       * send to as many trps that we inserted ourselves. In this case
       * we don't need to wake any send threads. If the trps still need
       * sending to after we're done we will ensure that a send thread
       * is woken up. assist_send_thread will ensure that send threads
       * are woken up if needed.
       *
       * At medium load levels we keep track of how much trps we have
       * wanted to send to and ensure that we at least do a part of that
       * work if need be. However we try as much as possible to avoid
       * sending at medium load at this point since we still have more
       * work to do. So we offload the sending to other threads and
       * wait with providing send assistance until we're out of work
       * or we have accumulated sufficiently to provide a bit of
       * assistance to the send threads.
       *
       * At medium load we set num_trps_inserted to 0 since we
       * have already woken up a send thread and thus there is no
       * need to wake up another thread in assist_send_thread, so we
       * indicate that we call this function only to assist and need
       * no wakeup service.
       *
       * We will check here also if we should wake an idle thread to
       * do some send assistance. We check so that we don't perform
       * this wakeup function too often.
       */

      Uint32 num_trps_inserted = 0;
      for (Uint32 i = 0; i < count; i++)
      {
        num_trps_inserted += g_send_threads->alert_send_thread(trps[i],
                                                               now,
                                             selfptr->m_send_instance);
      }
      Uint32 num_trps_to_send_to = num_trps_inserted;
      if (selfptr->m_overload_status != (OverloadStatus)MEDIUM_LOAD_CONST)
      {
        num_trps_to_send_to++;
      }
      send_wakeup_thread_ord(selfptr, now);
      if (num_trps_to_send_to > 0)
      {
        pending_send = g_send_threads->assist_send_thread(
                                           num_trps_to_send_to,
                                           selfptr->m_thr_no,
                                           now,
                                           selfptr->m_watchdog_counter,
                                           selfptr->m_send_instance,
                                           selfptr->m_send_buffer_pool);
      }
      NDB_TICKS after = NdbTick_getCurrentTicks();
      selfptr->m_micros_send += NdbTick_Elapsed(now, after).microSec();
      g_send_threads->wake_my_send_thread_if_needed(&trps[0],
                                    count,
                                    selfptr->m_send_instance);
    }
    return pending_send;
  }

  /**
   * We're not using send threads, we keep this code around for now
   * to ensure that we can support the same behaviour also in newer
   * versions for a while. Eventually this code will be deprecated.
   */
  Uint32 made_progress = 0;
  struct thr_repository* rep = g_thr_repository;

  for (Uint32 i = 0; i < count; i++)
  {
    Uint32 id = trps[i];
    thr_repository::send_buffer * sb = rep->m_send_buffers + id;

    selfptr->m_watchdog_counter = 6;

    /**
     * If we must send now, set the force_send flag.
     *
     * This will ensure that if we do not get the send lock, the thread
     * holding the lock will try sending again for us when it has released
     * the lock.
     *
     * The lock/unlock pair works as a memory barrier to ensure that the
     * flag update is flushed to the other thread.
     */
    if (must_send)
    {
      sb->m_force_send = 1;
    }

    if (trylock(&sb->m_send_lock) != 0)
    {
      if (!must_send)
      {
        /**
         * Not doing this trp now, re-add to pending list.
         *
         * As we only add from the start of an empty list, we are safe from
         * overwriting the list while we are iterating over it.
         */
        register_pending_send(selfptr, id);
      }
      else
      {
        /* Other thread will send for us as we set m_force_send. */
      }
    }
    else  //Got send_lock
    {
      /**
       * Now clear the flag, and start sending all data available to this trp.
       *
       * Put a memory barrier here, so that if another thread tries to grab
       * the send lock but fails due to us holding it here, we either
       * 1) Will see m_force_send[id] set to 1 at the end of the loop, or
       * 2) We clear here the flag just set by the other thread, but then we
       * will (thanks to mb()) be able to see and send all of the data already
       * in the first send iteration.
       */
      sb->m_force_send = 0;
      mb();

      /**
       * Set m_send_thread so that our transporter callback can know which
       * thread holds the send lock for this remote trp.
       */
      sb->m_send_thread = selfptr->m_thr_no;
      const bool more = globalTransporterRegistry.performSend(id);
      made_progress += sb->m_bytes_sent;
      sb->m_send_thread = NO_SEND_THREAD;
      unlock(&sb->m_send_lock);

      if (more)   //Didn't complete all my send work 
      {
        register_pending_send(selfptr, id);
      }
      else
      {
        /**
         * We need a memory barrier here to prevent race between clearing lock
         *   and reading of m_force_send.
         *   CPU can reorder the load to before the clear of the lock
         */
        mb();
        if (sb->m_force_send) //Other thread forced us to do more send
        {
          made_progress++;    //Avoid false 'no progress' handling
          register_pending_send(selfptr, id);
        }
      }
    }
  } //for all trps

  selfptr->m_send_buffer_pool.release_global(rep->m_mm,
                                             RG_TRANSPORTER_BUFFERS,
                                             selfptr->m_send_instance_no);

  return (made_progress)         // Had some progress?
     ?  (selfptr->m_pending_send_count > 0)   // More do_send is required
    : false;                     // All busy, or didn't find any work (-> -0)
}

#ifdef ERROR_INSERT
void
mt_set_delayed_prepare(Uint32 self)
{
  thr_repository *rep = g_thr_repository;
  struct thr_data *selfptr = &rep->m_thread[self];
  
  selfptr->m_delayed_prepare = true;
}
#endif


/**
 * These are the implementations of the TransporterSendBufferHandle methods
 * in ndbmtd.
 */
Uint32 *
mt_send_handle::getWritePtr(NodeId nodeId,
                            TrpId trp_id,
                            Uint32 len,
                            Uint32 prio,
                            Uint32 max,
                            SendStatus *error)
{
  (void)nodeId;
#ifdef ERROR_INSERT
  if (m_selfptr->m_delayed_prepare)
  {
    g_eventLogger->info("MT thread %u delaying in prepare",
                        m_selfptr->m_thr_no);
    NdbSleep_MilliSleep(500);
    g_eventLogger->info("MT thread %u finished delay, clearing",
                        m_selfptr->m_thr_no);
    m_selfptr->m_delayed_prepare = false;
  }
#endif

  struct thr_send_buffer * b = m_selfptr->m_send_buffers+trp_id;
  thr_send_page * p = b->m_last_page;
  if (likely(p != NULL))
  {
    assert(p->m_start == 0); //Nothing sent until flushed
    
    if (likely(p->m_bytes + len <= thr_send_page::max_bytes()))
    {
      return (Uint32*)(p->m_data + p->m_bytes);
    }
    // TODO: maybe dont always flush on page-boundary ???
    flush_send_buffer(m_selfptr, trp_id);
    if (!g_send_threads)
      try_send(m_selfptr, trp_id);
  }
  if(unlikely(len > thr_send_page::max_bytes()))
  {
    *error = SEND_MESSAGE_TOO_BIG;
    return 0;
  }

  bool first = true;
  while (first)
  {
    if (likely((p = m_selfptr->m_send_buffer_pool.seize(g_thr_repository->m_mm,
                                      RG_TRANSPORTER_BUFFERS,
                                      m_selfptr->m_send_instance_no)) != 0))
    {
      p->m_bytes = 0;
      p->m_start = 0;
      p->m_next = 0;
      b->m_first_page = b->m_last_page = p;
      return (Uint32*)p->m_data;
    }
    try_pack_send_buffers(m_selfptr);
    first = false;
  }
  *error = SEND_BUFFER_FULL;
  return 0;
}

/**
 * Acquire total send buffer size without locking and without gathering 
 *
 * OJA: The usability of this function is rather questionable.
 *      m_buffered_size and m_sending_size is updated by
 *      link_thread_send_buffers(), get_bytes_to_send_iovec() and
 *      bytes_sent() - All part of performSend(). Thus, it is
 *      valid *after* a send.
 *
 *      However, checking it *before* a send in order to 
 *      determine if the payload is yet too small doesn't 
 *      really provide correct information of the current state.
 *      Most likely '0 will be returned if previous send succeeded.
 *
 *      A better alternative could be to add a 'min_send' argument
 *      to perform_send(), and skip sending if not '>='.
 *      (After real size is recalculated)
 */
static Uint64
mt_get_send_buffer_bytes(TrpId trp_id)
{
  thr_repository *rep = g_thr_repository;
  thr_repository::send_buffer *sb = &rep->m_send_buffers[trp_id];
  const Uint64 total_send_buffer_size =
    sb->m_buffered_size + sb->m_sending_size;
  return total_send_buffer_size;
}

void
mt_getSendBufferLevel(Uint32 self, NodeId id, SB_LevelType &level)
{
  Resource_limit rl;
  const Uint32 page_size = thr_send_page::PGSIZE;
  thr_repository *rep = g_thr_repository;
  thr_repository::send_buffer *sb = &rep->m_send_buffers[id];
  const Uint64 current_trp_send_buffer_size =
    sb->m_buffered_size + sb->m_sending_size;
  
  /* Memory barrier to get a fresher value for rl.m_curr */
  mb();
  rep->m_mm->get_resource_limit_nolock(RG_TRANSPORTER_BUFFERS, rl);
  Uint64 current_send_buffer_size = rl.m_min * page_size;
  Uint64 current_used_send_buffer_size = rl.m_curr * page_size;
  Uint64 current_percentage =
    (100 * current_used_send_buffer_size) / current_send_buffer_size;

  if (current_percentage >= 90)
  {
    const Uint32 avail_shared = rep->m_mm->get_free_shared_nolock();
    if (rl.m_min + avail_shared > rl.m_max)
    {
      current_send_buffer_size = rl.m_max * page_size;
    }
    else
    {
      current_send_buffer_size = (rl.m_min + avail_shared) * page_size;
    }
  }
  calculate_send_buffer_level(current_trp_send_buffer_size,
                              current_send_buffer_size,
                              current_used_send_buffer_size,
                              glob_num_threads,
                              level);
  return;
}

void
mt_send_handle::getSendBufferLevel(NodeId id, SB_LevelType &level)
{
  (void)id;
  (void)level;
  return;
}

Uint32
mt_send_handle::updateWritePtr(NodeId nodeId,
                               TrpId trp_id,
                               Uint32 lenBytes,
                               Uint32 prio)
{
  (void)nodeId;
  struct thr_send_buffer * b = m_selfptr->m_send_buffers+trp_id;
  thr_send_page * p = b->m_last_page;
  p->m_bytes += lenBytes;
  return p->m_bytes;
}

/*
 * Insert a signal in a job queue.
 *
 * The signal is not visible to consumers yet after return from this function,
 * only recorded in the thr_jb_write_state. It is necessary to first call
 * flush_write_state() for this.
 *
 * The new_buffer is a job buffer to use if the current one gets full. If used,
 * we return true, indicating that the caller should allocate a new one for
 * the next call. (This is done to allow to insert under lock, but do the
 * allocation outside the lock).
 */
static inline
bool
insert_signal(thr_job_queue *q, thr_job_queue_head *h,
              thr_jb_write_state *w, Uint32 prioa,
              const SignalHeader* sh, const Uint32 *data,
              const Uint32 secPtr[3], thr_job_buffer *new_buffer)
{
  Uint32 write_pos = w->m_write_pos;
  Uint32 datalen = sh->theLength;
  assert(w->is_open());
  assert(w->m_write_buffer == q->m_buffers[w->m_write_index]);
  memcpy(w->m_write_buffer->m_data + write_pos, sh, sizeof(*sh));
  write_pos += (sizeof(*sh) >> 2);
  memcpy(w->m_write_buffer->m_data + write_pos, data, 4*datalen);
  write_pos += datalen;
  const Uint32 *p= secPtr;
  for (Uint32 i = 0; i < sh->m_noOfSections; i++)
    w->m_write_buffer->m_data[write_pos++] = *p++;
  w->increment_pending_signals();

#if SIZEOF_CHARP == 8
  /* Align to 8-byte boundary, to ensure aligned copies. */
  write_pos= (write_pos+1) & ~((Uint32)1);
#endif

  /*
   * We make sure that there is always room for at least one signal in the
   * current buffer in the queue, so one insert is always possible without
   * adding a new buffer.
   */
  if (likely(write_pos + MAX_SIGNAL_SIZE <= thr_job_buffer::SIZE))
  {
    w->m_write_pos = write_pos;
    return false;
  }
  else
  {
    /*
     * Need a write memory barrier here, as this might make signal data visible
     * to other threads.
     *
     * ToDo: We actually only need the wmb() here if we already make this
     * buffer visible to the other thread. So we might optimize it a bit. But
     * wmb() is a no-op on x86 anyway...
     */
    wmb();
    w->m_write_buffer->m_len = write_pos;
    Uint32 write_index = (w->m_write_index + 1) % thr_job_queue::SIZE;

    /**
     * Full job buffer is fatal.
     *
     * ToDo: should we wait for it to become non-full? There is no guarantee
     * that this will actually happen...
     *
     * Or alternatively, ndbrequire() ?
     */
    if (unlikely(write_index == h->m_read_index))
    {
      job_buffer_full(0);
    }
    new_buffer->m_len = 0;
    new_buffer->m_prioa = prioa;
    q->m_buffers[write_index] = new_buffer;
    w->m_write_index = write_index;
    w->m_write_pos = 0;
    w->m_write_buffer = new_buffer;
    return true;                // Buffer new_buffer used
  }

  return false;                 // Buffer new_buffer not used
}

static
void
read_jbb_state(thr_data *selfptr, Uint32 count)
{
  thr_jb_read_state *r = selfptr->m_read_states;
  const thr_job_queue *q = selfptr->m_in_queue;
  const thr_job_queue_head *h = selfptr->m_in_queue_head;
  for (Uint32 i = 0; i < count; i++,r++)
  {
    if (r->is_open())
    {
      Uint32 read_index = r->m_read_index;

      /**
       * Optimization: Only reload when possibly empty.
       * Avoid cache reload of shared thr_job_queue_head
       * Read head directly to avoid unnecessary cache
       * load of first cache line of m_in_queue entry.
       */
      if (r->m_write_index == read_index)
      {
        r->m_write_index = h[i].m_write_index;
        read_barrier_depends();
        r->m_read_end = q[i].m_buffers[read_index]->m_len;
      }
    }
  }
}

static
bool
read_jba_state(thr_data *selfptr)
{
  thr_jb_read_state *r = &(selfptr->m_jba_read_state);
  r->m_write_index = selfptr->m_jba_head.m_write_index;
  read_barrier_depends();
  r->m_read_end = selfptr->m_jba.m_buffers[r->m_read_index]->m_len;
  return r->is_empty();
}

static
inline
bool
check_for_input_from_ndbfs(struct thr_data* thr_ptr, Signal* signal)
{
  /**
   * The manner to check for input from NDBFS file threads misuses
   * the SEND_PACKED signal. For ndbmtd this is intended to be
   * replaced by using signals directly from NDBFS file threads to
   * the issuer of the file request. This is WL#8890.
   */
  Uint32 i;
  for (i = 0; i < thr_ptr->m_instance_count; i++)
  {
    BlockReference block = thr_ptr->m_instance_list[i];
    Uint32 main = blockToMain(block);
    if (main == NDBFS)
    {
      Uint32 instance = blockToInstance(block);
      SimulatedBlock* b = globalData.getBlock(main, instance);
      b->executeFunction_async(GSN_SEND_PACKED, signal);
      if (signal->theData[0] == 1)
        return true;
      return false;
    }
  }
  return false;
}

/* Check all job queues, return true only if all are empty. */
static bool
check_queues_empty(thr_data *selfptr)
{
  Uint32 thr_count = g_thr_repository->m_thread_count;
  if (selfptr->m_thr_no == 0)
  {
    if (check_for_input_from_ndbfs(selfptr, selfptr->m_signal))
      return false;
  }
  bool empty = read_jba_state(selfptr);
  if (!empty)
    return false;

  read_jbb_state(selfptr, thr_count);
  const thr_jb_read_state *r = selfptr->m_read_states;
  for (Uint32 i = 0; i < thr_count; i++,r++)
  {
    if (!r->is_empty())
      return false;
  }
  return true;
}

static
inline
void
sendpacked(struct thr_data* thr_ptr, Signal* signal)
{
  Uint32 i;
  signal->header.m_noOfSections = 0; /* valgrind */
  thr_ptr->m_watchdog_counter = 15;
  for (i = 0; i < thr_ptr->m_instance_count; i++)
  {
    BlockReference block = thr_ptr->m_instance_list[i];
    Uint32 main = blockToMain(block);
    if (main == DBLQH || main == DBTC || main == DBTUP || main == NDBFS)
    {
      Uint32 instance = blockToInstance(block);
      SimulatedBlock* b = globalData.getBlock(main, instance);
      // wl4391_todo remove useless assert
      assert(b != 0 && b->getThreadId() == thr_ptr->m_thr_no);
      /* b->send_at_job_buffer_end(); */
      b->executeFunction_async(GSN_SEND_PACKED, signal);
    }
  }
}

/**
 * We check whether it is time to call do_send or do_flush. These are
 * central decisions to the data node scheduler in a multithreaded data
 * node. If we wait for too long to make this decision it will severely
 * impact our response times since messages will be waiting in the send
 * buffer without being sent for up to several milliseconds.
 *
 * Since we call this function now after executing jobs from one thread,
 * we will never call this function with more than 75 signals executed.
 * The decision to send/flush is determined by config parameters that
 * control the responsiveness of MySQL Cluster. Setting it to a be highly
 * responsive means that we will send very often at the expense of
 * throughput. Setting it to a high throughput means that we will send
 * seldom at the expense of response time to gain higher throughput.
 *
 * It is possible to change this variable through a DUMP command and can
 * thus be changed as the environment changes.
 */
static
void handle_scheduling_decisions(thr_data *selfptr,
                                 Signal *signal,
                                 Uint32 & send_sum,
                                 Uint32 & flush_sum,
                                 bool & pending_send)
{
  if (send_sum >= selfptr->m_max_signals_before_send)
  {
    /* Try to send, but skip for now in case of lock contention. */
    sendpacked(selfptr, signal);
    selfptr->m_watchdog_counter = 6;
    flush_jbb_write_state(selfptr);
    pending_send = do_send(selfptr, FALSE, FALSE);
    selfptr->m_watchdog_counter = 20;
    send_sum = 0;
    flush_sum = 0;
  }
  else if (flush_sum >= selfptr->m_max_signals_before_send_flush)
  {
    /* Send buffers append to send queues to dst. trps. */
    sendpacked(selfptr, signal);
    selfptr->m_watchdog_counter = 6;
    flush_jbb_write_state(selfptr);
    do_flush(selfptr);
    selfptr->m_watchdog_counter = 20;
    flush_sum = 0;
  }
}

#if defined(USE_INIT_GLOBAL_VARIABLES)
  void mt_clear_global_variables(thr_data*);
#endif
/*
 * Execute at most MAX_SIGNALS signals from one job queue, updating local read
 * state as appropriate.
 *
 * Returns number of signals actually executed.
 */
static
Uint32
execute_signals(thr_data *selfptr,
                thr_job_queue *q, thr_job_queue_head *h,
                thr_jb_read_state *r,
                Signal *sig, Uint32 max_signals)
{
  Uint32 num_signals;
  Uint32 extra_signals = 0;
  Uint32 read_index = r->m_read_index;
  Uint32 write_index = r->m_write_index;
  Uint32 read_pos = r->m_read_pos;
  Uint32 read_end = r->m_read_end;
  Uint32 *watchDogCounter = &selfptr->m_watchdog_counter;

  if (read_index == write_index && read_pos >= read_end)
    return 0;          // empty read_state

  thr_job_buffer *read_buffer = r->m_read_buffer;

  for (num_signals = 0; num_signals < max_signals; num_signals++)
  {
    *watchDogCounter = 12;
    while (read_pos >= read_end)
    {
      if (read_index == write_index)
      {
        /* No more available now. */
        selfptr->m_stat.m_exec_cnt += num_signals;
        return num_signals;
      }
      else
      {
        /* Move to next buffer. */
        read_index = (read_index + 1) % thr_job_queue::SIZE;
        release_buffer(g_thr_repository, selfptr->m_thr_no, read_buffer);
        read_buffer = q->m_buffers[read_index];
        read_pos = 0;
        read_end = read_buffer->m_len;
        /* Update thread-local read state. */
        r->m_read_index = h->m_read_index = read_index;
        r->m_read_buffer = read_buffer;
        r->m_read_pos = read_pos;
        r->m_read_end = read_end;
        /* Wakeup threads waiting for job buffers to become free */
        wakeup(&h->m_waiter);
      }
    }

    /*
     * These pre-fetching were found using OProfile to reduce cache misses.
     * (Though on Intel Core 2, they do not give much speedup, as apparently
     * the hardware prefetcher is already doing a fairly good job).
     */
    NDB_PREFETCH_READ (read_buffer->m_data + read_pos + 16);
    NDB_PREFETCH_WRITE ((Uint32 *)&sig->header + 16);

#ifdef VM_TRACE
    /* Find reading / propagation of junk */
    sig->garbage_register();
#endif
    /* Now execute the signal. */
    SignalHeader* s =
      reinterpret_cast<SignalHeader*>(read_buffer->m_data + read_pos);
    Uint32 seccnt = s->m_noOfSections;
    Uint32 siglen = (sizeof(*s)>>2) + s->theLength;
    if(siglen>16)
    {
      NDB_PREFETCH_READ (read_buffer->m_data + read_pos + 32);
    }
    Uint32 bno = blockToMain(s->theReceiversBlockNumber);
    Uint32 ino = blockToInstance(s->theReceiversBlockNumber);
    SimulatedBlock* block = globalData.mt_getBlock(bno, ino);
    assert(block != 0);

    Uint32 gsn = s->theVerId_signalNumber;
    *watchDogCounter = 1 +
      (bno << 8) +
      (gsn << 20);

    /* Must update original buffer so signal dump will see it. */
    s->theSignalId = selfptr->m_signal_id_counter++;
    memcpy(&sig->header, s, 4*siglen);
    for(Uint32 i = 0; i < seccnt; i++)
    {
      sig->m_sectionPtrI[i] = read_buffer->m_data[read_pos + siglen + i];
    }

    read_pos += siglen + seccnt;
#if SIZEOF_CHARP == 8
    /* Handle 8-byte alignment. */
    read_pos = (read_pos + 1) & ~((Uint32)1);
#endif

    /* Update just before execute so signal dump can know how far we are. */
    r->m_read_pos = read_pos;

#ifdef VM_TRACE
    if (globalData.testOn)
    { //wl4391_todo segments
      SegmentedSectionPtr ptr[3];
      ptr[0].i = sig->m_sectionPtrI[0];
      ptr[1].i = sig->m_sectionPtrI[1];
      ptr[2].i = sig->m_sectionPtrI[2];
      ::getSections(seccnt, ptr);
      globalSignalLoggers.executeSignal(*s,
                                        0,
                                        &sig->theData[0], 
                                        globalData.ownId,
                                        ptr, seccnt);
    }
#endif

    /**
     * In 7.4 we introduced the ability for scans in LDM threads to scan
     * several rows in the same signal execution without issuing a
     * CONTINUEB signal. This means that we effectively changed the
     * real-time characteristics of the scheduler. This change ensures
     * that we behave the same way as in 7.3 and earlier with respect to
     * how many signals are executed. So the m_extra_signals variable can
     * be used in the future for other cases where we combine several
     * signal executions into one signal and thus ensure that we don't
     * change the scheduler algorithms.
     *
     * This variable is incremented every time we decide to execute more
     * signals without real-time breaks in scans in DBLQH.
     */
    block->jamBuffer()->markEndOfSigExec();
    sig->m_extra_signals = 0;
#if defined(USE_INIT_GLOBAL_VARIABLES)
    mt_clear_global_variables(selfptr);
#endif
    block->executeFunction_async(gsn, sig);
    extra_signals += sig->m_extra_signals;
  }
  /**
   * Only count signals causing real-time break and not the one used to
   * balance the scheduler.
   */
  selfptr->m_stat.m_exec_cnt += num_signals;

  return num_signals + extra_signals;
}

static
Uint32
run_job_buffers(thr_data *selfptr,
                Signal *sig,
                Uint32 & send_sum,
                Uint32 & flush_sum,
                bool & pending_send)
{
  Uint32 thr_count = g_thr_repository->m_thread_count;
  Uint32 signal_count = 0;
  Uint32 signal_count_since_last_zero_time_queue = 0;
  Uint32 perjb = selfptr->m_max_signals_per_jb;

  read_jbb_state(selfptr, thr_count);
  /*
   * A load memory barrier to ensure that we see any prio A signal sent later
   * than loaded prio B signals.
   */
  rmb();

  /**
   * For the main thread we can stop at any job buffer, so we proceed from
   * where we stopped to make different job buffers be equal in importance.
   *
   * For all other threads m_next_jbb_no should always be 0 when we reach here.
   */
  Uint32 first_jbb_no = selfptr->m_next_jbb_no;
  thr_job_queue *queue = selfptr->m_in_queue + first_jbb_no;
  thr_job_queue_head *head = selfptr->m_in_queue_head + first_jbb_no;
  thr_jb_read_state *read_state = selfptr->m_read_states + first_jbb_no;
  selfptr->m_watchdog_counter = 13;
  for (Uint32 jbb_no = first_jbb_no;
       jbb_no < thr_count;
       jbb_no++,queue++,read_state++,head++)
  {
    /* Read the prio A state often, to avoid starvation of prio A. */
    while (!read_jba_state(selfptr))
    {
      selfptr->m_sent_local_prioa_signal = false;
      static Uint32 max_prioA = thr_job_queue::SIZE * thr_job_buffer::SIZE;
      Uint32 num_signals = execute_signals(selfptr,
                                           &(selfptr->m_jba),
                                           &(selfptr->m_jba_head),
                                           &(selfptr->m_jba_read_state), sig,
                                           max_prioA);
      signal_count += num_signals;
      send_sum += num_signals;
      flush_sum += num_signals;
      if (!selfptr->m_sent_local_prioa_signal)
      {
        /**
         * Break out of loop if there was no prio A signals generated
         * from the local execution.
         */
        break;
      }
    }

    /**
     * Contended queues get an extra execute quota:
     *
     * If we didn't get a max 'perjb' quota, our out buffers
     * are about to fill up. This thread is thus effectively
     * slowed down in order to let other threads consume from
     * our out buffers. Eventually, when 'perjb==0', we will
     * have to wait/sleep for buffers to become available.
     * 
     * This can bring is into a circular wait-lock, where
     * threads are stalled due to full out buffers. The same
     * thread may also have full in buffers, thus blocking other
     * threads from progressing. This could bring us into a 
     * circular wait-lock, where no threads are able to progress.
     * The entire scheduler will then be stuck.
     *
     * We try to avoid this situation by reserving some
     * 'm_max_extra_signals' which are only used to consume
     * from 'almost full' in-buffers. We will then reduce the
     * risk of ending up in the above wait-lock.
     *
     * Exclude receiver threads, as there can't be a
     * circular wait between recv-thread and workers.
     */
    Uint32 extra = 0;

    if (perjb < MAX_SIGNALS_PER_JB)  //Job buffer contention
    {
      const Uint32 free = compute_free_buffers_in_queue(head);
      if (free <= thr_job_queue::ALMOST_FULL)
      {
        if (selfptr->m_max_extra_signals > MAX_SIGNALS_PER_JB - perjb)
	{
          extra = MAX_SIGNALS_PER_JB - perjb;
        }
        else
	{
          extra = selfptr->m_max_extra_signals;
          selfptr->m_max_exec_signals = 0; //Force recalc
        }
        selfptr->m_max_extra_signals -= extra;
      }
    }

#ifdef ERROR_INSERT

#define MIXOLOGY_MIX_MT_JBB 1

    if (unlikely(globalEmulatorData.theConfiguration->getMixologyLevel() &
                 MIXOLOGY_MIX_MT_JBB))
    {
      /**
       * Let's maximise interleaving to find inter-thread
       * signal order dependency bugs
       */
      perjb = 1;
      extra = 0;
    }
#endif

    /* Now execute prio B signals from one thread. */
    Uint32 num_signals = execute_signals(selfptr, queue, head, read_state,
                                         sig, perjb+extra);

    if (num_signals > 0)
    {
      signal_count += num_signals;
      send_sum += num_signals;
      flush_sum += num_signals;
      handle_scheduling_decisions(selfptr,
                                  sig,
                                  send_sum,
                                  flush_sum,
                                  pending_send);

      if (signal_count - signal_count_since_last_zero_time_queue >
          (MAX_SIGNALS_EXECUTED_BEFORE_ZERO_TIME_QUEUE_SCAN -
           MAX_SIGNALS_PER_JB))
      {
        /**
         * Each execution of execute_signals can at most execute 75 signals
         * from one job buffer. We want to ensure that we execute no more than
         * 100 signals before we arrive here to get the signals from the
         * zero time queue. This implements the bounded delay signal
         * concept which is required for rate controlled activities.
         *
         * We scan the zero time queue if more than 25 signals were executed.
         * This means that at most 100 signals will be executed before we arrive
         * here again to check the bounded delay signals.
         */
        signal_count_since_last_zero_time_queue = signal_count;
        selfptr->m_watchdog_counter = 14;
        scan_zero_queue(selfptr);
        selfptr->m_watchdog_counter = 13;
      }
      if (selfptr->m_thr_no == 0)
      {
        /**
         * Execution in main thread can sometimes be a bit more lengthy,
         * so we ensure that we don't miss out on heartbeats and other
         * important things by returning to checking scan_time_queues
         * more often.
         */
        jbb_no++;
        if (jbb_no >= thr_count)
        {
          jbb_no = 0;
        }
        selfptr->m_next_jbb_no = jbb_no;
        return signal_count;
      }
    }
  }
  selfptr->m_next_jbb_no = 0;
  return signal_count;
}

struct thr_map_entry {
  enum { NULL_THR_NO = 0xFF };
  Uint8 thr_no;
  thr_map_entry() : thr_no(NULL_THR_NO) {}
};

static struct thr_map_entry thr_map[NO_OF_BLOCKS][NDBMT_MAX_BLOCK_INSTANCES];
static Uint32 block_instance_count[NO_OF_BLOCKS];

static inline Uint32
block2ThreadId(Uint32 block, Uint32 instance)
{
  assert(block >= MIN_BLOCK_NO && block <= MAX_BLOCK_NO);
  Uint32 index = block - MIN_BLOCK_NO;
  assert(instance < NDB_ARRAY_SIZE(thr_map[index]));
  const thr_map_entry& entry = thr_map[index][instance];
  assert(entry.thr_no < glob_num_threads);
  return entry.thr_no;
}

void
add_thr_map(Uint32 main, Uint32 instance, Uint32 thr_no)
{
  assert(main == blockToMain(main));
  Uint32 index = main - MIN_BLOCK_NO;
  assert(index < NO_OF_BLOCKS);
  assert(instance < NDB_ARRAY_SIZE(thr_map[index]));

  SimulatedBlock* b = globalData.getBlock(main, instance);
  require(b != 0);

  /* Block number including instance. */
  Uint32 block = numberToBlock(main, instance);

  require(thr_no < glob_num_threads);
  struct thr_repository* rep = g_thr_repository;
  struct thr_data* thr_ptr = &rep->m_thread[thr_no];

  /* Add to list. */
  {
    Uint32 i;
    for (i = 0; i < thr_ptr->m_instance_count; i++)
      require(thr_ptr->m_instance_list[i] != block);
  }
  require(thr_ptr->m_instance_count < MAX_INSTANCES_PER_THREAD);
  thr_ptr->m_instance_list[thr_ptr->m_instance_count++] = block;

  SimulatedBlock::ThreadContext ctx;
  ctx.threadId = thr_no;
  ctx.jamBuffer = &thr_ptr->m_jam;
  ctx.watchDogCounter = &thr_ptr->m_watchdog_counter;
  ctx.sectionPoolCache = &thr_ptr->m_sectionPoolCache;
  ctx.pHighResTimer = &thr_ptr->m_curr_ticks;
  b->assignToThread(ctx);

  /* Create entry mapping block to thread. */
  thr_map_entry& entry = thr_map[index][instance];
  require(entry.thr_no == thr_map_entry::NULL_THR_NO);
  entry.thr_no = thr_no;
}

/* Static assignment of main instances (before first signal). */
void
mt_init_thr_map()
{
  /* Keep mt-classic assignments in MT LQH. */
  const Uint32 thr_GLOBAL = 0;
  const Uint32 thr_LOCAL = 1;

  add_thr_map(BACKUP, 0, thr_LOCAL);
  add_thr_map(DBTC, 0, thr_GLOBAL);
  add_thr_map(DBDIH, 0, thr_GLOBAL);
  add_thr_map(DBLQH, 0, thr_LOCAL);
  add_thr_map(DBACC, 0, thr_LOCAL);
  add_thr_map(DBTUP, 0, thr_LOCAL);
  add_thr_map(DBDICT, 0, thr_GLOBAL);
  add_thr_map(NDBCNTR, 0, thr_GLOBAL);
  add_thr_map(QMGR, 0, thr_GLOBAL);
  add_thr_map(NDBFS, 0, thr_GLOBAL);
  add_thr_map(CMVMI, 0, thr_GLOBAL);
  add_thr_map(TRIX, 0, thr_GLOBAL);
  add_thr_map(DBUTIL, 0, thr_GLOBAL);
  add_thr_map(SUMA, 0, thr_LOCAL);
  add_thr_map(DBTUX, 0, thr_LOCAL);
  add_thr_map(TSMAN, 0, thr_LOCAL);
  add_thr_map(LGMAN, 0, thr_LOCAL);
  add_thr_map(PGMAN, 0, thr_LOCAL);
  add_thr_map(RESTORE, 0, thr_LOCAL);
  add_thr_map(DBINFO, 0, thr_LOCAL);
  add_thr_map(DBSPJ, 0, thr_GLOBAL);
  add_thr_map(THRMAN, 0, thr_GLOBAL);
  add_thr_map(TRPMAN, 0, thr_GLOBAL);
}

Uint32
mt_get_instance_count(Uint32 block)
{
  switch(block){
  case DBLQH:
  case DBACC:
  case DBTUP:
  case DBTUX:
  case BACKUP:
  case RESTORE:
    return globalData.ndbMtLqhWorkers;
    break;
  case PGMAN:
    return globalData.ndbMtLqhWorkers + 1;
    break;
  case DBTC:
  case DBSPJ:
    return globalData.ndbMtTcThreads;
    break;
  case TRPMAN:
    return globalData.ndbMtReceiveThreads;
  case THRMAN:
    return glob_num_threads;
  default:
    require(false);
  }
  return 0;
}

void
mt_add_thr_map(Uint32 block, Uint32 instance)
{
  Uint32 num_lqh_threads = globalData.ndbMtLqhThreads;
  Uint32 num_tc_threads = globalData.ndbMtTcThreads;

  require(instance != 0);
  Uint32 thr_no = NUM_MAIN_THREADS;
  switch(block){
  case DBLQH:
  case DBACC:
  case DBTUP:
  case DBTUX:
  case BACKUP:
  case RESTORE:
    thr_no += (instance - 1) % num_lqh_threads;
    break;
  case PGMAN:
    if (instance == num_lqh_threads + 1)
    {
      // Put extra PGMAN together with it's Proxy
      thr_no = block2ThreadId(block, 0);
    }
    else
    {
      thr_no += (instance - 1) % num_lqh_threads;
    }
    break;
  case DBTC:
  case DBSPJ:
    thr_no += num_lqh_threads + (instance - 1);
    break;
  case THRMAN:
    thr_no = instance - 1;
    break;
  case TRPMAN:
    thr_no += num_lqh_threads + num_tc_threads + (instance - 1);
    break;
  default:
    require(false);
  }

  add_thr_map(block, instance, thr_no);
}

/**
 * create the duplicate entries needed so that
 *   sender doesnt need to know how many instances there
 *   actually are in this node...
 *
 * if only 1 instance...then duplicate that for all slots
 * else assume instance 0 is proxy...and duplicate workers (modulo)
 *
 * NOTE: extra pgman worker is instance 5
 */
void
mt_finalize_thr_map()
{
  for (Uint32 b = 0; b < NO_OF_BLOCKS; b++)
  {
    Uint32 bno = b + MIN_BLOCK_NO;
    Uint32 cnt = 0;
    while (cnt < NDB_ARRAY_SIZE(thr_map[b]) &&
           thr_map[b][cnt].thr_no != thr_map_entry::NULL_THR_NO)
    {
      cnt++;
    }
    block_instance_count[b] = cnt;
    if (cnt != NDB_ARRAY_SIZE(thr_map[b]))
    {
      SimulatedBlock * main = globalData.getBlock(bno, 0);
      for (Uint32 i = cnt; i < NDB_ARRAY_SIZE(thr_map[b]); i++)
      {
        Uint32 dup = (cnt == 1) ? 0 : 1 + ((i - 1) % (cnt - 1));
        if (thr_map[b][i].thr_no == thr_map_entry::NULL_THR_NO)
        {
          thr_map[b][i] = thr_map[b][dup];
          main->addInstance(globalData.getBlock(bno, dup), i);
        }
        else
        {
          /**
           * extra pgman instance
           */
          require(bno == PGMAN);
          require(false);
        }
      }
    }
  }
}

static
void
calculate_max_signals_parameters(thr_data *selfptr)
{
  switch (selfptr->m_sched_responsiveness)
  {
    case 0:
      selfptr->m_max_signals_before_send = 1000;
      selfptr->m_max_signals_before_send_flush = 340;
      break;
    case 1:
      selfptr->m_max_signals_before_send = 800;
      selfptr->m_max_signals_before_send_flush = 270;
      break;
    case 2:
      selfptr->m_max_signals_before_send = 600;
      selfptr->m_max_signals_before_send_flush = 200;
      break;
    case 3:
      selfptr->m_max_signals_before_send = 450;
      selfptr->m_max_signals_before_send_flush = 155;
      break;
    case 4:
      selfptr->m_max_signals_before_send = 350;
      selfptr->m_max_signals_before_send_flush = 130;
      break;
    case 5:
      selfptr->m_max_signals_before_send = 300;
      selfptr->m_max_signals_before_send_flush = 110;
      break;
    case 6:
      selfptr->m_max_signals_before_send = 250;
      selfptr->m_max_signals_before_send_flush = 90;
      break;
    case 7:
      selfptr->m_max_signals_before_send = 200;
      selfptr->m_max_signals_before_send_flush = 70;
      break;
    case 8:
      selfptr->m_max_signals_before_send = 170;
      selfptr->m_max_signals_before_send_flush = 50;
      break;
    case 9:
      selfptr->m_max_signals_before_send = 135;
      selfptr->m_max_signals_before_send_flush = 30;
      break;
    case 10:
      selfptr->m_max_signals_before_send = 70;
      selfptr->m_max_signals_before_send_flush = 10;
      break;
    default:
      assert(FALSE);
  }
  return;
}

static void
init_thread(thr_data *selfptr)
{
  selfptr->m_waiter.init();
  selfptr->m_jam.theEmulatedJamIndex = 0;

  selfptr->m_overload_status = (OverloadStatus)LIGHT_LOAD_CONST;
  selfptr->m_node_overload_status = (OverloadStatus)LIGHT_LOAD_CONST;
  selfptr->m_wakeup_instance = 0;
  selfptr->m_last_wakeup_idle_thread = NdbTick_getCurrentTicks();
  selfptr->m_micros_send = 0;
  selfptr->m_micros_sleep = 0;
  selfptr->m_buffer_full_micros_sleep = 0;
  selfptr->m_measured_spintime = 0;

  NDB_THREAD_TLS_JAM = &selfptr->m_jam;
  NDB_THREAD_TLS_THREAD= selfptr;

  unsigned thr_no = selfptr->m_thr_no;
  globalEmulatorData.theWatchDog->
    registerWatchedThread(&selfptr->m_watchdog_counter, thr_no);
  {
    while(selfptr->m_thread == 0)
      NdbSleep_MilliSleep(30);
  }

  THRConfigApplier & conf = globalEmulatorData.theConfiguration->m_thr_config;
  BaseString tmp;
  tmp.appfmt("thr: %u ", thr_no);

  bool fail = false;
  int tid = NdbThread_GetTid(selfptr->m_thread);
  if (tid != -1)
  {
    tmp.appfmt("tid: %u ", tid);
  }

  conf.appendInfo(tmp,
                  selfptr->m_instance_list,
                  selfptr->m_instance_count);
  int res = conf.do_bind(selfptr->m_thread,
                         selfptr->m_instance_list,
                         selfptr->m_instance_count);
  if (res < 0)
  {
    fail = true;
    tmp.appfmt("err: %d ", -res);
  }
  else if (res > 0)
  {
    tmp.appfmt("OK ");
  }

  unsigned thread_prio;
  res = conf.do_thread_prio(selfptr->m_thread,
                            selfptr->m_instance_list,
                            selfptr->m_instance_count,
                            thread_prio);
  if (res < 0)
  {
    fail = true;
    res = -res;
    tmp.appfmt("Failed to set thread prio to %u, ", thread_prio);
    if (res == SET_THREAD_PRIO_NOT_SUPPORTED_ERROR)
    {
      tmp.appfmt("not supported on this OS");
    }
    else
    {
      tmp.appfmt("error: %d", res);
    }
  }
  else if (res > 0)
  {
    tmp.appfmt("Successfully set thread prio to %u ", thread_prio);
  }

  selfptr->m_realtime = conf.do_get_realtime(selfptr->m_instance_list,
                                             selfptr->m_instance_count);
  selfptr->m_conf_spintime = conf.do_get_spintime(selfptr->m_instance_list,
                                                  selfptr->m_instance_count);

  /* spintime always 0 on platforms not supporting spin */
  if (!NdbSpin_is_supported())
  {
    selfptr->m_conf_spintime = 0;
  }
  selfptr->m_spintime = 0;
  memset(&selfptr->m_spin_stat, 0, sizeof(selfptr->m_spin_stat));
  selfptr->m_spin_stat.m_spin_interval[NUM_SPIN_INTERVALS - 1] = 0xFFFFFFFF;

  selfptr->m_sched_responsiveness =
    globalEmulatorData.theConfiguration->schedulerResponsiveness();
  calculate_max_signals_parameters(selfptr);

  selfptr->m_thr_id = my_thread_self();

  for (Uint32 i = 0; i < selfptr->m_instance_count; i++) 
  {
    BlockReference block = selfptr->m_instance_list[i];
    Uint32 main = blockToMain(block);
    Uint32 instance = blockToInstance(block);
    tmp.appfmt("%s(%u) ", getBlockName(main), instance);
  }
  /* Report parameters used by thread to node log */
  tmp.appfmt("realtime=%u, spintime=%u, max_signals_before_send=%u"
             ", max_signals_before_send_flush=%u",
             selfptr->m_realtime,
             selfptr->m_conf_spintime,
             selfptr->m_max_signals_before_send,
             selfptr->m_max_signals_before_send_flush);

  printf("%s\n", tmp.c_str());
  fflush(stdout);
  if (fail)
  {
#ifndef HAVE_MAC_OS_X_THREAD_INFO
    abort();
#endif
  }
}

/**
 * Align signal buffer for better cache performance.
 * Also skew it a litte for each thread to avoid cache pollution.
 */
#define SIGBUF_SIZE (sizeof(Signal) + 63 + 256 * MAX_BLOCK_THREADS)
static Signal *
aligned_signal(unsigned char signal_buf[SIGBUF_SIZE], unsigned thr_no)
{
  UintPtr sigtmp= (UintPtr)signal_buf;
  sigtmp= (sigtmp+63) & (~(UintPtr)63);
  sigtmp+= thr_no*256;
  return (Signal *)sigtmp;
}

/*
 * We only do receive in receiver thread(s), no other threads do receive.
 *
 * As part of the receive loop, we also periodically call update_connections()
 * (this way we are similar to single-threaded ndbd).
 *
 * The TRPMAN block (and no other blocks) run in the same thread as this
 * receive loop; this way we avoid races between update_connections() and
 * TRPMAN calls into the transporters.
 */

/**
 * Array of pointers to TransporterReceiveHandleKernel
 *   these are not used "in traffic"
 */
static TransporterReceiveHandleKernel *
  g_trp_receive_handle_ptr[MAX_NDBMT_RECEIVE_THREADS];

/**
 * Array for mapping trps to receiver threads and function to access it.
 */
static Uint32 g_trp_to_recv_thr_map[MAX_NTRANSPORTERS];

/**
 * We use this method both to initialise the realtime variable
 * and also for updating it. Currently there is no method to
 * update it, but it's likely that we will soon invent one and
 * thus the code is prepared for this case.
 */
static void
update_rt_config(struct thr_data *selfptr,
                 bool & real_time,
                 enum ThreadTypes type)
{
  bool old_real_time = real_time;
  real_time = selfptr->m_realtime;
  if (old_real_time == true && real_time == false)
  {
    yield_rt_break(selfptr->m_thread,
                   type,
                   false);
  }
}

/**
 * We use this method both to initialise the spintime variable
 * and also for updating it. Currently there is no method to
 * update it, but it's likely that we will soon invent one and
 * thus the code is prepared for this case.
 */
static void
update_spin_config(struct thr_data *selfptr,
                   Uint64 & min_spin_timer)
{
  min_spin_timer = selfptr->m_spintime;
}

extern "C"
void *
mt_receiver_thread_main(void *thr_arg)
{
  unsigned char signal_buf[SIGBUF_SIZE];
  Signal *signal;
  struct thr_repository* rep = g_thr_repository;
  struct thr_data* selfptr = (struct thr_data *)thr_arg;
  unsigned thr_no = selfptr->m_thr_no;
  Uint32& watchDogCounter = selfptr->m_watchdog_counter;
  const Uint32 recv_thread_idx = thr_no - first_receiver_thread_no;
  bool has_received = false;
  int cnt = 0;
  bool real_time = false;
  Uint64 min_spin_timer;
  NDB_TICKS yield_ticks;
  NDB_TICKS before;

  init_thread(selfptr);
  signal = aligned_signal(signal_buf, thr_no);
  update_rt_config(selfptr, real_time, ReceiveThread);
  update_spin_config(selfptr, min_spin_timer);

  /**
   * Object that keeps track of our pollReceive-state
   */
  TransporterReceiveHandleKernel recvdata(thr_no, recv_thread_idx);
  recvdata.assign_trps(g_trp_to_recv_thr_map);
  globalTransporterRegistry.init(recvdata);

  /**
   * Save pointer to this for management/error-insert
   */
  g_trp_receive_handle_ptr[recv_thread_idx] = &recvdata;

  NDB_TICKS now = NdbTick_getCurrentTicks();
  before = now;
  selfptr->m_curr_ticks = now;
  selfptr->m_signal = signal;
  selfptr->m_ticks = selfptr->m_scan_real_ticks = yield_ticks = now;
  Ndb_GetRUsage(&selfptr->m_scan_time_queue_rusage, false);

  while (globalData.theRestartFlag != perform_stop)
  {
    if (cnt == 0)
    {
      watchDogCounter = 5;
      update_spin_config(selfptr, min_spin_timer);
      Uint32 max_spintime = 0;
      /**
       * The settings of spinning on transporter is only aimed at
       * the NDB API part. We have an elaborate scheme for handling
       * spinning in ndbmtd, so we shut down any spinning inside
       * the transporter here. The principle is to only spin in one
       * location and spinning in recv thread overrides any spinning
       * desired on transporter level.
       */
      max_spintime = 0;
      globalTransporterRegistry.update_connections(recvdata,
                                                   max_spintime);
    }
    cnt = (cnt + 1) & 15;

    watchDogCounter = 2;

    now = NdbTick_getCurrentTicks();
    selfptr->m_curr_ticks = now;
    const Uint32 lagging_timers = scan_time_queues(selfptr, now);
    Uint32 dummy1 = 0;
    Uint32 dummy2 = 0;
    bool dummy3 = false;

    Uint32 sum = run_job_buffers(selfptr, signal, dummy1, dummy2, dummy3);

    if (sum || has_received)
    {
      sendpacked(selfptr, signal);
      watchDogCounter = 6;
      flush_jbb_write_state(selfptr);
    }

    const bool pending_send = do_send(selfptr, TRUE, FALSE);

    watchDogCounter = 7;

    if (real_time)
    {
      check_real_time_break(now,
                            &yield_ticks,
                            selfptr->m_thread,
                            ReceiveThread);
    }

    /**
     * Only allow to sleep in pollReceive when:
     * 1) We are not lagging behind in handling timer events.
     * 2) No more pending sends, or no send progress.
     * 3) There are no 'min_spin' configured or min_spin has elapsed
     * We will not check spin timer until we have checked the
     * transporters at least one loop and discovered no data. We also
     * ensure that we have not executed any signals before we start
     * the actual spin timer.
     */
    Uint32 delay = 0;
    Uint32 num_events = 0;
    Uint32 spin_micros = 0;
    update_spin_config(selfptr, min_spin_timer);
    before = NdbTick_getCurrentTicks();

    if (lagging_timers == 0 &&       // 1)
        pending_send  == false &&    // 2)
        (min_spin_timer == 0 ||      // 3)
         (sum == 0 &&
          !has_received &&
          check_recv_yield(selfptr,
                           recvdata,
                           min_spin_timer,
                           num_events,
                           &spin_micros,
                           before))))
    {
      delay = 10; // 10 ms
    }

    has_received = false;
    if (num_events == 0)
    {
      /* Need to call pollReceive if not already done in check_recv_yield */
      num_events = globalTransporterRegistry.pollReceive(delay, recvdata);
    }
    if (delay > 0)
    {
      NDB_TICKS after = NdbTick_getCurrentTicks();
      Uint64 micros_sleep = NdbTick_Elapsed(before, after).microSec();
      selfptr->m_micros_sleep += micros_sleep;
      wait_time_tracking(selfptr, micros_sleep);
    }
    if (num_events)
    {
      watchDogCounter = 8;
      lock(&rep->m_receive_lock[recv_thread_idx]);
      const bool buffersFull = 
        (globalTransporterRegistry.performReceive(recvdata,
                                                  recv_thread_idx) != 0);
      unlock(&rep->m_receive_lock[recv_thread_idx]);
      has_received = true;

      if (buffersFull)       /* Receive queues(s) are full */
      {
        thr_data* waitthr = get_congested_recv_queue(rep, recv_thread_idx);
        if (waitthr != NULL) /* Will wait for buffers to be freed */
        {
          /**
           * Wait for thread 'waitthr' to consume some of the
           * pending signals in m_in_queue previously received 
           * from this receive thread, 'thr_no'.
           * Will recheck queue status with 'check_recv_queue' after latch
           * has been set, and *before* going to sleep.
           */
          const Uint32 nano_wait = 1000*1000;    /* -> 1 ms */
          thr_job_queue_head *wait_queue = waitthr->m_in_queue_head + thr_no;
          NDB_TICKS before = NdbTick_getCurrentTicks();
          const bool waited = yield(&wait_queue->m_waiter,
                                    nano_wait,
                                    check_recv_queue,
                                    wait_queue);
          if (waited)
          {
            NDB_TICKS after = NdbTick_getCurrentTicks();
            selfptr->m_buffer_full_micros_sleep +=
              NdbTick_Elapsed(before, after).microSec();
          }
        }
      }
    }
    selfptr->m_stat.m_loop_cnt++;
  }

  globalEmulatorData.theWatchDog->unregisterWatchedThread(thr_no);
  return NULL;                  // Return value not currently used
}

/**
 * Callback function used by yield() to recheck 
 * 'job queue full' condition before going to sleep.
 *
 * Check if the specified 'thr_job_queue_head' (arg)
 * is still full, return true if so.
 */
static bool
check_congested_job_queue(thr_job_queue_head *waitfor)
{
  return (compute_free_buffers_in_queue(waitfor) <= thr_job_queue::RESERVED);
}

/**
 * Check if any out-queues of selfptr is full.
 * If full: Return 'Thr_data*' for (one of) the thread(s)
 *          which we have to wait for. (to consume from queue)
 */
static struct thr_data*
get_congested_job_queue(const thr_data *selfptr)
{
  const Uint32 thr_no = selfptr->m_thr_no;
  struct thr_repository* rep = g_thr_repository;
  struct thr_data *thrptr = rep->m_thread;
  struct thr_data *waitfor = NULL;

  for (unsigned i = 0; i<glob_num_threads; i++, thrptr++)
  {
    thr_job_queue_head *q_head = thrptr->m_in_queue_head + thr_no;

    if (compute_free_buffers_in_queue(q_head) <= thr_job_queue::RESERVED)
    {
      if (thrptr != selfptr)  // Don't wait on myself (yet)
        return thrptr;
      else
        waitfor = thrptr;
    }
  }
  return waitfor;             // Possibly 'thrptr == selfptr'
}

/**
 * has_full_in_queues()
 *
 * Avoid circular waits between block-threads:
 * A thread is not allowed to sleep due to full
 * 'out' job-buffers if there are other threads
 * already having full 'in' job buffers sent to
 * this thread.
 *
 * run_job_buffers() has reserved a 'm_max_extra_signals'
 * quota which will be used to drain these 'full_in_queues'.
 * So we should allow it to be.
 *
 * Returns 'true' if any in-queues to this thread are full
 */
static
bool
has_full_in_queues(struct thr_data* selfptr)
{
  thr_job_queue_head *head = selfptr->m_in_queue_head;

  for (Uint32 thr_no = 0; thr_no < glob_num_threads; thr_no++, head++)
  {
    if (compute_free_buffers_in_queue(head) <= thr_job_queue::RESERVED)
    {
      return true;
    }
  }
  return false;
}

/**
 * update_sched_config
 *
 *   In order to prevent "job-buffer-full", i.e
 *     that one thread(T1) produces so much signals to another thread(T2)
 *     so that the ring-buffer from T1 to T2 gets full
 *     the main loop have 2 "config" variables
 *   - m_max_exec_signals
 *     This is the *total* no of signals T1 can execute before calling
 *     this method again
 *   - m_max_signals_per_jb
 *     This is the max no of signals T1 can execute from each other thread
 *     in system
 *
 *   Assumption: each signal may send *at most* 4 signals
 *     - this assumption is made the same in ndbd and ndbmtd and is
 *       mostly followed by block-code, although not in all places :-(
 *
 *   This function return true, if it it slept
 *     (i.e that it concluded that it could not execute *any* signals, wo/
 *      risking job-buffer-full)
 */
static
bool
update_sched_config(struct thr_data* selfptr,
                    bool pending_send,
                    Uint32 & send_sum,
                    Uint32 & flush_sum)
{
  Uint32 sleeploop = 0;
  Uint32 thr_no = selfptr->m_thr_no;
  selfptr->m_watchdog_counter = 16;
loop:
  Uint32 minfree = compute_min_free_out_buffers(thr_no);
  Uint32 reserved = (minfree > thr_job_queue::RESERVED)
                   ? thr_job_queue::RESERVED
                   : minfree;

  Uint32 avail = compute_max_signals_to_execute(minfree - reserved);
  Uint32 perjb = (avail + g_thr_repository->m_thread_count - 1) /
                  g_thr_repository->m_thread_count;

  if (selfptr->m_thr_no == 0)
  {
    /**
     * The main thread has some signals that execute for a bit longer than
     * other threads. We only allow the main thread thus to execute at most
     * 5 signals per round of signal execution. We handle this here and
     * also only handle signals from one queue at a time with the main
     * thread.
     *
     * LCP_FRAG_REP is one such signal that can execute now for about
     * 1 millisecond, so 5 signals can become 5 milliseconds which should
     * fairly safe to ensure we always come back for the 10ms TIME_SIGNAL
     * that is handled by the main thread.
     */
    perjb = MAX(perjb, 5);
  }
  if (perjb > MAX_SIGNALS_PER_JB)
    perjb = MAX_SIGNALS_PER_JB;

  selfptr->m_max_exec_signals = avail;
  selfptr->m_max_signals_per_jb = perjb;
  selfptr->m_max_extra_signals = compute_max_signals_to_execute(reserved);

  if (unlikely(perjb == 0))
  {
    if (sleeploop == 10)
    {
      /**
       * we've slept for 10ms...try running anyway
       */
      selfptr->m_max_signals_per_jb = 1;
      ndbout_c("thr_no:%u - sleeploop 10!! "
               "(Worker thread blocked (>= 10ms) by slow consumer threads)",
               selfptr->m_thr_no);
      return true;
    }

    struct thr_data* waitthr = get_congested_job_queue(selfptr);
    if (waitthr == NULL)                 // Waiters resolved
    {
      goto loop;
    }
    else if (has_full_in_queues(selfptr) &&
             selfptr->m_max_extra_signals > 0)
    {
      /* 'extra_signals' used to drain 'full_in_queues'. */
      return sleeploop > 0;
    }

    if (pending_send)
    {
      /* About to sleep, _must_ send now. */
      pending_send = do_send(selfptr, TRUE, TRUE);
      send_sum = 0;
      flush_sum = 0;
    }

    /**
     * Wait for thread 'waitthr' to consume some of the
     * pending signals in m_in_queue[].
     * Will recheck queue status with 'check_recv_queue'
     * after latch has been set, and *before* going to sleep.
     */
    const Uint32 nano_wait = 1000*1000;    /* -> 1 ms */
    thr_job_queue_head *wait_queue = waitthr->m_in_queue_head + thr_no;
    
    NDB_TICKS before = NdbTick_getCurrentTicks();
    const bool waited = yield(&wait_queue->m_waiter,
                              nano_wait,
                              check_congested_job_queue,
                              wait_queue);
    if (waited)
    {
      NDB_TICKS after = NdbTick_getCurrentTicks();
      selfptr->m_buffer_full_micros_sleep +=
        NdbTick_Elapsed(before, after).microSec();
      sleeploop++;
    }
    goto loop;
  }

  return sleeploop > 0;
}

extern "C"
void *
mt_job_thread_main(void *thr_arg)
{
  unsigned char signal_buf[SIGBUF_SIZE];
  Signal *signal;

  struct thr_data* selfptr = (struct thr_data *)thr_arg;
  init_thread(selfptr);
  Uint32& watchDogCounter = selfptr->m_watchdog_counter;

  unsigned thr_no = selfptr->m_thr_no;
  signal = aligned_signal(signal_buf, thr_no);

  /* Avoid false watchdog alarms caused by race condition. */
  watchDogCounter = 21;

  bool pending_send = false;
  Uint32 send_sum = 0;
  Uint32 flush_sum = 0;
  Uint32 loops = 0;
  Uint32 maxloops = 10;/* Loops before reading clock, fuzzy adapted to 1ms freq. */
  Uint32 waits = 0;

  NDB_TICKS yield_ticks;

  Uint64 min_spin_timer;
  bool real_time = false;

  update_rt_config(selfptr, real_time, BlockThread);
  update_spin_config(selfptr, min_spin_timer);

  NDB_TICKS now = NdbTick_getCurrentTicks();
  selfptr->m_ticks = yield_ticks = now;
  selfptr->m_scan_real_ticks = now;
  selfptr->m_signal = signal;
  selfptr->m_curr_ticks = now;
  Ndb_GetRUsage(&selfptr->m_scan_time_queue_rusage, false);

  while (globalData.theRestartFlag != perform_stop)
  {
    loops++;

    /**
     * prefill our thread local send buffers
     *   up to THR_SEND_BUFFER_PRE_ALLOC (1Mb)
     *
     * and if this doesnt work pack buffers before start to execute signals
     */
    watchDogCounter = 11;
    if (!selfptr->m_send_buffer_pool.fill(g_thr_repository->m_mm,
                                          RG_TRANSPORTER_BUFFERS,
                                          THR_SEND_BUFFER_PRE_ALLOC,
                                          selfptr->m_send_instance_no))
    {
      try_pack_send_buffers(selfptr);
    }

    watchDogCounter = 2;
    const Uint32 lagging_timers = scan_time_queues(selfptr, now);

    Uint32 sum = run_job_buffers(selfptr,
                                 signal,
                                 send_sum,
                                 flush_sum,
                                 pending_send);
    

    if (sum)
    {
      /**
       * It is imperative that we flush signals within our node after
       * each round of execution. This makes sure that the receiver
       * thread are woken up to do their work which often means that
       * they will send some signals back to us (e.g. the commit
       * protocol for updates). Quite often we continue executing one
       * more loop and while so doing the other threads can return
       * new signals to us and thus we avoid going back and forth to
       * sleep too often which otherwise would happen.
       *
       * Many of the optimisations of having TC and LDM colocated
       * for transactions would go away unless we use this principle.
       *
       * No need to flush however if no signals have been executed since
       * last flush.
       *
       * No need to check for send packed signals if we didn't send
       * any signals, packed signals are sent as a result of an
       * executed signal.
       */
      sendpacked(selfptr, signal);
      watchDogCounter = 6;
      if (flush_sum > 0)
      {
        flush_jbb_write_state(selfptr);
        do_flush(selfptr);
        flush_sum = 0;
      }
    }
    /**
     * Scheduler is not allowed to yield until its internal
     * time has caught up on real time.
     */
    else if (lagging_timers == 0)
    {
      /* No signals processed, prepare to sleep to wait for more */
      if (send_sum > 0 || pending_send == true)
      {
        /* About to sleep, _must_ send now. */
        flush_jbb_write_state(selfptr);
        pending_send = do_send(selfptr, TRUE, TRUE);
        send_sum = 0;
        flush_sum = 0;
      }

      /**
       * No more incoming signals to process yet, and we have 
       * either completed all pending sends, or had no progress
       * due to full transporters in last do_send(). Wait for
       * more signals, use a shorter timeout if pending_send.
       */
      if (pending_send == false) /* Nothing pending, or no progress made */
      {
        /**
         * When min_spin_timer > 0 it means we are spinning, if we executed
         * jobs this time there is no reason to check spin timer and since
         * we executed at least one signal we are per definition not yet
         * spinning. Thus we can immediately move to the next loop.
         * Spinning is performed for a while when sum == 0 AND
         * min_spin_timer > 0. In this case we need to go into check_yield
         * and initialise spin timer (on first round) and check spin timer
         * on subsequent loops.
         */
        Uint32 spin_time_in_us = 0;
        update_spin_config(selfptr, min_spin_timer);
        NDB_TICKS before = NdbTick_getCurrentTicks();
        bool has_spun = (min_spin_timer != 0);
        if (min_spin_timer == 0 ||
            check_yield(selfptr,
                        min_spin_timer,
                        &spin_time_in_us,
                        before))
        {
          /**
           * Sleep, either a short nap if send failed due to send overload,
           * or a longer sleep if there are no more work waiting.
           */
          Uint32 maxwait_in_us =
            (selfptr->m_node_overload_status >=
             (OverloadStatus)MEDIUM_LOAD_CONST) ?
            1 * 1000 :
            10 * 1000;
          if (maxwait_in_us < spin_time_in_us)
          {
            maxwait_in_us = 0;
          }
          else
          {
            maxwait_in_us -= spin_time_in_us;
          }
          selfptr->m_watchdog_counter = 18;
          const Uint32 used_maxwait_in_ns = maxwait_in_us * 1000;
          bool waited = yield(&selfptr->m_waiter,
                              used_maxwait_in_ns,
                              check_queues_empty,
                              selfptr);
          if (waited)
          {
            waits++;
            /* Update current time after sleeping */
            now = NdbTick_getCurrentTicks();
            selfptr->m_curr_ticks = now;
            yield_ticks = now;
            Uint64 micros_sleep = NdbTick_Elapsed(before, now).microSec();
            selfptr->m_micros_sleep += micros_sleep;
            wait_time_tracking(selfptr, micros_sleep);
            selfptr->m_stat.m_wait_cnt += waits;
            selfptr->m_stat.m_loop_cnt += loops;
            if (selfptr->m_overload_status <=
                (OverloadStatus)MEDIUM_LOAD_CONST)
            {
              /**
               * To ensure that we at least check for trps to send to
               * before we yield we set pending_send to true. We will
               * quickly discover if nothing is pending.
               */
              pending_send = true;
            }         
            waits = loops = 0;
            if (selfptr->m_thr_no == 0)
            {
              /**
               * NDBFS is using thread 0, here we need to call SEND_PACKED
               * to scan the memory channel for messages from NDBFS threads.
               * We want to do this here to avoid an extra loop in scheduler
               * before we discover those messages from NDBFS.
               */
              selfptr->m_watchdog_counter = 17;
              check_for_input_from_ndbfs(selfptr, signal);
            }
          }
          else if (has_spun)
          {
            selfptr->m_micros_sleep += spin_time_in_us;
            wait_time_tracking(selfptr, spin_time_in_us);
          }
        }
      }
    }

    /**
     * Check if we executed enough signals,
     *   and if so recompute how many signals to execute
     */
    now = NdbTick_getCurrentTicks();
    if (sum >= selfptr->m_max_exec_signals)
    {
      if (update_sched_config(selfptr,
                              send_sum + Uint32(pending_send),
                              send_sum,
                              flush_sum))
      {
        /* Update current time after sleeping */
        selfptr->m_curr_ticks = now;
        selfptr->m_stat.m_wait_cnt += waits;
        selfptr->m_stat.m_loop_cnt += loops;
        waits = loops = 0;
        update_rt_config(selfptr, real_time, BlockThread);
        calculate_max_signals_parameters(selfptr);
      }
    }
    else
    {
      selfptr->m_max_exec_signals -= sum;
    }

    /**
     * Adaptive reading freq. of system time every time 1ms
     * is likely to have passed
     */
    now = NdbTick_getCurrentTicks();
    selfptr->m_curr_ticks = now;
    if (loops > maxloops)
    {
      if (real_time)
      {
        check_real_time_break(now,
                              &yield_ticks,
                              selfptr->m_thread,
                              BlockThread);
      }
      const Uint64 diff = NdbTick_Elapsed(selfptr->m_ticks, now).milliSec();

      /* Adjust 'maxloop' to achieve clock reading frequency of 1ms */
      if (diff < 1)
        maxloops += ((maxloops/10) + 1); /* No change: less frequent reading */
      else if (diff > 1 && maxloops > 1)
        maxloops -= ((maxloops/10) + 1); /* Overslept: Need more frequent read*/

      selfptr->m_stat.m_wait_cnt += waits;
      selfptr->m_stat.m_loop_cnt += loops;
      waits = loops = 0;
    }
  }

  globalEmulatorData.theWatchDog->unregisterWatchedThread(thr_no);
  return NULL;                  // Return value not currently used
}

/**
 * Get number of pending signals at B-level in our own thread. Used
 * to make some decisions in rate-critical parts of the data node.
 */
Uint32
mt_getSignalsInJBB(Uint32 self)
{
  Uint32 pending_signals = 0;
  struct thr_repository* rep = g_thr_repository;
  struct thr_data *selfptr = &rep->m_thread[self];
  for (Uint32 thr_no = 0; thr_no < glob_num_threads; thr_no++)
  {
    thr_jb_write_state *w = selfptr->m_write_states + thr_no;
    pending_signals += w->get_pending_signals();
  }
  return pending_signals;
}

NDB_TICKS
mt_getHighResTimer(Uint32 self)
{
  struct thr_repository* rep = g_thr_repository;
  struct thr_data *selfptr = &rep->m_thread[self];
  return selfptr->m_curr_ticks;
}

void
mt_setNoSend(Uint32 self)
{
  struct thr_repository* rep = g_thr_repository;
  struct thr_data *selfptr = &rep->m_thread[self];
  selfptr->m_nosend = 1;
}

void
mt_startChangeNeighbourNode()
{
  if (g_send_threads)
  {
    g_send_threads->startChangeNeighbourNode();
  }
}

void
mt_setNeighbourNode(NodeId node)
{
  if (g_send_threads)
  {
    g_send_threads->setNeighbourNode(node);
  }
}

void
mt_endChangeNeighbourNode()
{
  if (g_send_threads)
  {
    g_send_threads->endChangeNeighbourNode();
  }
}

void
mt_setOverloadStatus(Uint32 self,
                     OverloadStatus new_status)
{
  struct thr_repository* rep = g_thr_repository;
  struct thr_data *selfptr = &rep->m_thread[self];
  selfptr->m_overload_status = new_status;
}

void
mt_setWakeupThread(Uint32 self,
                   Uint32 wakeup_instance)
{
  struct thr_repository* rep = g_thr_repository;
  struct thr_data *selfptr = &rep->m_thread[self];
  selfptr->m_wakeup_instance = wakeup_instance;
}

void
mt_setNodeOverloadStatus(Uint32 self,
                         OverloadStatus new_status)
{
  struct thr_repository* rep = g_thr_repository;
  struct thr_data *selfptr = &rep->m_thread[self];
  selfptr->m_node_overload_status = new_status;
}

void
mt_setSendNodeOverloadStatus(OverloadStatus new_status)
{
  if (g_send_threads)
  {
    g_send_threads->setNodeOverloadStatus(new_status);
  }
}

void
mt_setSpintime(Uint32 self, Uint32 new_spintime)
{
  struct thr_repository* rep = g_thr_repository;
  struct thr_data *selfptr = &rep->m_thread[self];
  /* spintime always 0 on platforms not supporting spin */
  if (!NdbSpin_is_supported())
  {
    new_spintime = 0;
  }
  selfptr->m_spintime = new_spintime;
}

Uint32
mt_getConfiguredSpintime(Uint32 self)
{
  struct thr_repository* rep = g_thr_repository;
  struct thr_data *selfptr = &rep->m_thread[self];

  return selfptr->m_conf_spintime;
}

Uint32
mt_getWakeupLatency(void)
{
  return glob_wakeup_latency;
}

void
mt_setWakeupLatency(Uint32 latency)
{
  /**
   * Round up to next 5 micros (+4) AND
   * add 2 microseconds for time to execute going to sleep (+2).
   * Rounding up is an attempt to decrease variance by selecting the
   * latency more coarsely.
   * 
   */
  latency = (latency + 4 + 2) / 5;
  latency *= 5;
  glob_wakeup_latency = latency;
}

void
mt_flush_send_buffers(Uint32 self)
{
  struct thr_repository* rep = g_thr_repository;
  struct thr_data *selfptr = &rep->m_thread[self];
  do_flush(selfptr);
}

void
mt_set_watchdog_counter(Uint32 self)
{
  struct thr_repository* rep = g_thr_repository;
  struct thr_data *selfptr = &rep->m_thread[self];
  selfptr->m_watchdog_counter = 12;
}

void
mt_getPerformanceTimers(Uint32 self,
                        Uint64 & micros_sleep,
                        Uint64 & spin_time,
                        Uint64 & buffer_full_micros_sleep,
                        Uint64 & micros_send)
{
  struct thr_repository* rep = g_thr_repository;
  struct thr_data *selfptr = &rep->m_thread[self];

  /**
   * Internally in mt.cpp sleep time now includes spin time. However
   * to ensure backwards compatibility we report them separate to
   * any block users of this information.
   */
  micros_sleep = selfptr->m_micros_sleep;
  spin_time = selfptr->m_measured_spintime;
  if (micros_sleep >= spin_time)
  {
    micros_sleep -= spin_time;
  }
  else
  {
    micros_sleep = 0;
  }
  buffer_full_micros_sleep = selfptr->m_buffer_full_micros_sleep;
  micros_send = selfptr->m_micros_send;
}

const char *
mt_getThreadDescription(Uint32 self)
{
  if (is_main_thread(self))
  {
    if (self == 0)
      return "main thread, schema and distribution handling";
    else if (self == 1)
      return "rep thread, asynch replication and proxy block handling";
    require(false);
  }
  else if (is_ldm_thread(self))
  {
    return "ldm thread, handling a set of data partitions";
  }
  else if (is_tc_thread(self))
  {
    return "tc thread, transaction handling, unique index and pushdown join"
           " handling";
  }
  else if (is_recv_thread(self))
  {
    return "receive thread, performing receieve and polling for new receives";
  }
  else
  {
    require(false);
  }
  return NULL;
}

const char *
mt_getThreadName(Uint32 self)
{
  if (is_main_thread(self))
  {
    if (self == 0)
      return "main";
    else if (self == 1)
      return "rep";
    require(false);
  }
  else if (is_ldm_thread(self))
  {
    return "ldm";
  }
  else if (is_tc_thread(self))
  {
    return "tc";
  }
  else if (is_recv_thread(self))
  {
    return "recv";
  }
  else
  {
    require(false);
  }
  return NULL;
}

void
mt_getSendPerformanceTimers(Uint32 send_instance,
                            Uint64 & exec_time,
                            Uint64 & sleep_time,
                            Uint64 & spin_time,
                            Uint64 & user_time_os,
                            Uint64 & kernel_time_os,
                            Uint64 & elapsed_time_os)
{
  assert(g_send_threads != NULL);
  if (g_send_threads != NULL)
  {
    g_send_threads->getSendPerformanceTimers(send_instance,
                                             exec_time,
                                             sleep_time,
                                             spin_time,
                                             user_time_os,
                                             kernel_time_os,
                                             elapsed_time_os);
  }
}

Uint32
mt_getNumSendThreads()
{
  return globalData.ndbMtSendThreads;
}

Uint32
mt_getNumThreads()
{
  return glob_num_threads;
}

void
sendlocal(Uint32 self, const SignalHeader *s, const Uint32 *data,
          const Uint32 secPtr[3])
{
  Uint32 block = blockToMain(s->theReceiversBlockNumber);
  Uint32 instance = blockToInstance(s->theReceiversBlockNumber);

  /*
   * Max number of signals to put into job buffer before flushing the buffer
   * to the other thread.
   * This parameter found to be reasonable by benchmarking.
   */
  Uint32 MAX_SIGNALS_BEFORE_FLUSH = (self >= first_receiver_thread_no) ? 
    MAX_SIGNALS_BEFORE_FLUSH_RECEIVER : 
    MAX_SIGNALS_BEFORE_FLUSH_OTHER;

  Uint32 dst = block2ThreadId(block, instance);
  struct thr_repository* rep = g_thr_repository;
  struct thr_data *selfptr = &rep->m_thread[self];
  assert(my_thread_equal(selfptr->m_thr_id, my_thread_self()));
  struct thr_data *dstptr = &rep->m_thread[dst];

  selfptr->m_stat.m_priob_count++;
  Uint32 siglen = (sizeof(*s) >> 2) + s->theLength + s->m_noOfSections;
  selfptr->m_stat.m_priob_size += siglen;

  assert(s->theLength + s->m_noOfSections <= 25);
  thr_job_queue *q = dstptr->m_in_queue + self;
  thr_job_queue_head *h = dstptr->m_in_queue_head + self;
  thr_jb_write_state *w = selfptr->m_write_states + dst;
  if (insert_signal(q, h, w, false, s, data, secPtr, selfptr->m_next_buffer))
  {
    selfptr->m_next_buffer = seize_buffer(rep, self, false);
  }
  if (w->get_pending_signals() >= MAX_SIGNALS_BEFORE_FLUSH)
  {
    flush_write_state(selfptr, dstptr, h, w, false);
  }
}

void
sendprioa(Uint32 self, const SignalHeader *s, const uint32 *data,
          const Uint32 secPtr[3])
{
  Uint32 block = blockToMain(s->theReceiversBlockNumber);
  Uint32 instance = blockToInstance(s->theReceiversBlockNumber);

  Uint32 dst = block2ThreadId(block, instance);
  struct thr_repository* rep = g_thr_repository;
  struct thr_data *selfptr = &rep->m_thread[self];
  assert(s->theVerId_signalNumber == GSN_START_ORD ||
         my_thread_equal(selfptr->m_thr_id, my_thread_self()));
  struct thr_data *dstptr = &rep->m_thread[dst];

  selfptr->m_stat.m_prioa_count++;
  Uint32 siglen = (sizeof(*s) >> 2) + s->theLength + s->m_noOfSections;
  selfptr->m_stat.m_prioa_size += siglen;

  thr_job_queue *q = &(dstptr->m_jba);
  thr_job_queue_head *h = &(dstptr->m_jba_head);
  thr_jb_write_state w;

  if (selfptr == dstptr)
  {
    /**
     * Indicate that we sent Prio A signal to ourself.
     */
    selfptr->m_sent_local_prioa_signal = true;
  }

  w.init_pending_signals();
  lock(&dstptr->m_jba_write_lock);

  Uint32 index = h->m_write_index;
  w.m_write_index = index;
  thr_job_buffer *buffer = q->m_buffers[index];
  w.m_write_buffer = buffer;
  w.m_write_pos = buffer->m_len;
  bool buf_used = insert_signal(q, h, &w, true, s, data, secPtr,
                                selfptr->m_next_buffer);
  flush_write_state(selfptr, dstptr, h, &w, true);

  unlock(&dstptr->m_jba_write_lock);
  if (w.has_any_pending_signals())
  {
    wakeup(&(dstptr->m_waiter));
  }
  if (buf_used)
    selfptr->m_next_buffer = seize_buffer(rep, self, true);
}

/**
 * Send a signal to a remote node.
 *
 * (The signal is only queued here, and actually sent later in do_send()).
 */
SendStatus
mt_send_remote(Uint32 self, const SignalHeader *sh, Uint8 prio,
               const Uint32 * data, NodeId nodeId,
               const LinearSectionPtr ptr[3])
{
  thr_repository *rep = g_thr_repository;
  struct thr_data *selfptr = &rep->m_thread[self];
  SendStatus ss;

  mt_send_handle handle(selfptr);
  /* prepareSend() is lock-free, as we have per-thread send buffers. */
  TrpId trp_id = 0;
  ss = globalTransporterRegistry.prepareSend(&handle,
                                             sh,
                                             prio,
                                             data,
                                             nodeId,
                                             trp_id,
                                             ptr);
  if (likely(ss == SEND_OK))
  {
    register_pending_send(selfptr, trp_id);
  }
  return ss;
}

SendStatus
mt_send_remote(Uint32 self, const SignalHeader *sh, Uint8 prio,
               const Uint32 *data, NodeId nodeId,
               class SectionSegmentPool *thePool,
               const SegmentedSectionPtr ptr[3])
{
  thr_repository *rep = g_thr_repository;
  struct thr_data *selfptr = &rep->m_thread[self];
  SendStatus ss;

  mt_send_handle handle(selfptr);
  TrpId trp_id = 0;
  ss = globalTransporterRegistry.prepareSend(&handle,
                                             sh,
                                             prio,
                                             data,
                                             nodeId,
                                             trp_id,
                                             *thePool, ptr);
  if (likely(ss == SEND_OK))
  {
    register_pending_send(selfptr, trp_id);
  }
  return ss;
}

/*
 * This functions sends a prio A STOP_FOR_CRASH signal to a thread.
 *
 * It works when called from any other thread, not just from job processing
 * threads. But note that this signal will be the last signal to be executed by
 * the other thread, as it will exit immediately.
 */
static
void
sendprioa_STOP_FOR_CRASH(const struct thr_data *selfptr, Uint32 dst)
{
  SignalT<StopForCrash::SignalLength> signalT;
  struct thr_repository* rep = g_thr_repository;
  /* As this signal will be the last one executed by the other thread, it does
     not matter which buffer we use in case the current buffer is filled up by
     the STOP_FOR_CRASH signal; the data in it will never be read.
  */
  static Uint32 MAX_WAIT = 3000;
  static thr_job_buffer dummy_buffer;

  /**
   * Pick any instance running in this thread
   */
  struct thr_data *dstptr = &rep->m_thread[dst];
  Uint32 bno = dstptr->m_instance_list[0];

  memset(&signalT.header, 0, sizeof(SignalHeader));
  signalT.header.theVerId_signalNumber   = GSN_STOP_FOR_CRASH;
  signalT.header.theReceiversBlockNumber = bno;
  signalT.header.theSendersBlockRef      = 0;
  signalT.header.theTrace                = 0;
  signalT.header.theSendersSignalId      = 0;
  signalT.header.theSignalId             = 0;
  signalT.header.theLength               = StopForCrash::SignalLength;
  StopForCrash * stopForCrash = CAST_PTR(StopForCrash, &signalT.theData[0]);
  stopForCrash->flags = 0;

  thr_job_queue *q = &(dstptr->m_jba);
  thr_job_queue_head *h = &(dstptr->m_jba_head);
  thr_jb_write_state w;

  /**
   * Ensure that a crash while holding m_jba_write_lock won't block
   * dump process forever.
   */
  Uint64 loop_count = 0;
  const NDB_TICKS start_try_lock = NdbTick_getCurrentTicks();
  while (trylock(&dstptr->m_jba_write_lock) != 0)
  {
    if (++loop_count >= 10000)
    {
      const NDB_TICKS now = NdbTick_getCurrentTicks();
      if (NdbTick_Elapsed(start_try_lock, now).milliSec() > MAX_WAIT)
      {
        return;
      }
      NdbSleep_MilliSleep(1);
      loop_count = 0;
    }
  }

  w.init_pending_signals();
  Uint32 index = h->m_write_index;
  w.m_write_index = index;
  thr_job_buffer *buffer = q->m_buffers[index];
  w.m_write_buffer = buffer;
  w.m_write_pos = buffer->m_len;
  insert_signal(q, h, &w, true, &signalT.header, signalT.theData, NULL,
                &dummy_buffer);
  flush_write_state(selfptr, dstptr, h, &w, true);

  unlock(&dstptr->m_jba_write_lock);
  if (w.has_any_pending_signals())
  {
    loop_count = 0;
    /**
     * Ensure that a crash while holding wakeup lock won't block
     * dump process forever. We will wait at most 3 seconds.
     */
    const NDB_TICKS start_try_wakeup = NdbTick_getCurrentTicks();
    while (try_wakeup(&(dstptr->m_waiter)) != 0)
    {
      if (++loop_count >= 10000)
      {
        const NDB_TICKS now = NdbTick_getCurrentTicks();
        if (NdbTick_Elapsed(start_try_wakeup, now).milliSec() > MAX_WAIT)
        {
          return;
        }
        NdbSleep_MilliSleep(1);
        loop_count = 0;
      }
    }
  }
}

/**
 * Implements the rules for which threads are allowed to have
 * communication with each other.
 * Also see compute_jb_pages() which has similar logic.
 */
static bool
may_communicate(unsigned from, unsigned to)
{
  if (is_main_thread(from))
  {
    // Main threads communicates with all other threads
    return true;
  }
  else if (is_ldm_thread(from))
  {
    // First LDM is special as it may act as internal client
    // during backup, and thus communicate with other LDMs:
    if (is_first_ldm_thread(from) && is_ldm_thread(to))
      return true;

    // All LDM threads can communicates with TC-, main-
    // itself, and the BACKUP client (above) 
    return is_main_thread(to) ||
           is_tc_thread(to)   ||
           is_first_ldm_thread(to) ||
           (to == from);
  }
  else if (is_tc_thread(from))
  {
    // TC threads can communicate with SPJ-, LQH-, main- and itself
    return is_main_thread(to) ||
           is_ldm_thread(to)  ||
           is_tc_thread(to);      // Cover both SPJs and itself 
  }
  else
  {
    assert(is_recv_thread(from));
    // Receive treads communicate with all, except other receivers
    return !is_recv_thread(to);
  }
}

/**
 * init functions
 */
static
void
queue_init(struct thr_tq* tq)
{
  tq->m_next_timer = 0;
  tq->m_current_time = 0;
  tq->m_next_free = RNIL;
  tq->m_cnt[0] = tq->m_cnt[1] = tq->m_cnt[2] = 0;
  bzero(tq->m_delayed_signals, sizeof(tq->m_delayed_signals));
}

static
void
thr_init(struct thr_repository* rep, struct thr_data *selfptr, unsigned int cnt,
         unsigned thr_no)
{
  Uint32 i;

  selfptr->m_thr_no = thr_no;
  selfptr->m_next_jbb_no = 0;
  selfptr->m_max_signals_per_jb = MAX_SIGNALS_PER_JB;
  selfptr->m_max_exec_signals = 0;
  selfptr->m_max_extra_signals = 0;
  selfptr->m_first_free = 0;
  selfptr->m_first_unused = 0;
  selfptr->m_send_instance_no = 0;
  selfptr->m_send_instance = NULL;
  selfptr->m_nosend = 1;
  
  {
    char buf[100];
    BaseString::snprintf(buf, sizeof(buf), "jbalock thr: %u", thr_no);
    register_lock(&selfptr->m_jba_write_lock, buf);
  }
  selfptr->m_jba_head.m_read_index = 0;
  selfptr->m_jba_head.m_write_index = 0;
  thr_job_buffer *buffer = seize_buffer(rep, thr_no, true);
  selfptr->m_jba.m_buffers[0] = buffer;
  selfptr->m_jba_read_state.m_read_index = 0;
  selfptr->m_jba_read_state.m_read_buffer = buffer;
  selfptr->m_jba_read_state.m_read_pos = 0;
  selfptr->m_jba_read_state.m_read_end = 0;
  selfptr->m_jba_read_state.m_write_index = 0;
  selfptr->m_next_buffer = seize_buffer(rep, thr_no, false);
  selfptr->m_send_buffer_pool.set_pool(&rep->m_sb_pool);
  
  for (i = 0; i<cnt; i++)
  {
    selfptr->m_in_queue_head[i].m_waiter.init();
    selfptr->m_in_queue_head[i].m_read_index = 0;
    selfptr->m_in_queue_head[i].m_write_index = 0;
    buffer = may_communicate(i,thr_no) 
              ? seize_buffer(rep, thr_no, false) : NULL;
    selfptr->m_in_queue[i].m_buffers[0] = buffer;
    selfptr->m_read_states[i].m_read_index = 0;
    selfptr->m_read_states[i].m_read_buffer = buffer;
    selfptr->m_read_states[i].m_read_pos = 0;
    selfptr->m_read_states[i].m_read_end = 0;
    selfptr->m_read_states[i].m_write_index = 0;
  }
  queue_init(&selfptr->m_tq);

  bzero(&selfptr->m_stat, sizeof(selfptr->m_stat));

  selfptr->m_pending_send_count = 0;
  selfptr->m_pending_send_mask.clear();

  selfptr->m_instance_count = 0;
  for (i = 0; i < MAX_INSTANCES_PER_THREAD; i++)
    selfptr->m_instance_list[i] = 0;

  bzero(&selfptr->m_send_buffers, sizeof(selfptr->m_send_buffers));

  selfptr->m_thread = 0;
  selfptr->m_cpu = NO_LOCK_CPU;
#ifdef ERROR_INSERT
  selfptr->m_delayed_prepare = false;
#endif
}

/* Have to do this after init of all m_in_queues is done. */
static
void
thr_init2(struct thr_repository* rep, struct thr_data *selfptr,
          unsigned int cnt, unsigned thr_no)
{
  for (Uint32 i = 0; i<cnt; i++)
  {
    selfptr->m_write_states[i].m_write_index = 0;
    selfptr->m_write_states[i].m_write_pos = 0;
    selfptr->m_write_states[i].m_write_buffer =
      rep->m_thread[i].m_in_queue[thr_no].m_buffers[0];
    selfptr->m_write_states[i].init_pending_signals();
  }    
}

static
void
receive_lock_init(Uint32 recv_thread_id, thr_repository *rep)
{
  char buf[100];
  BaseString::snprintf(buf, sizeof(buf), "receive lock thread id %d",
                       recv_thread_id);
  register_lock(&rep->m_receive_lock[recv_thread_id], buf);
}

static
void
send_buffer_init(Uint32 id, thr_repository::send_buffer * sb)
{
  char buf[100];
  BaseString::snprintf(buf, sizeof(buf), "send lock trp %d", id);
  register_lock(&sb->m_send_lock, buf);
  BaseString::snprintf(buf, sizeof(buf), "send_buffer lock trp %d", id);
  register_lock(&sb->m_buffer_lock, buf);
  sb->m_buffered_size = 0;
  sb->m_sending_size = 0;
  sb->m_force_send = 0;
  sb->m_bytes_sent = 0;
  sb->m_send_thread = NO_SEND_THREAD;
  sb->m_enabled = false;
  bzero(&sb->m_buffer, sizeof(sb->m_buffer));
  bzero(&sb->m_sending, sizeof(sb->m_sending));
  bzero(sb->m_read_index, sizeof(sb->m_read_index));
}

static
void
rep_init(struct thr_repository* rep, unsigned int cnt, Ndbd_mem_manager *mm)
{
  rep->m_mm = mm;

  rep->m_thread_count = cnt;
  for (unsigned int i = 0; i<cnt; i++)
  {
    thr_init(rep, &rep->m_thread[i], cnt, i);
  }
  for (unsigned int i = 0; i<cnt; i++)
  {
    thr_init2(rep, &rep->m_thread[i], cnt, i);
  }

  rep->stopped_threads = 0;
  NdbMutex_Init(&rep->stop_for_crash_mutex);
  NdbCondition_Init(&rep->stop_for_crash_cond);

  for (Uint32 i = 0; i < NDB_ARRAY_SIZE(rep->m_receive_lock); i++)
  {
    receive_lock_init(i, rep);
  }
  for (int i = 0 ; i < MAX_NTRANSPORTERS; i++)
  {
    send_buffer_init(i, rep->m_send_buffers+i);
  }

  bzero(rep->m_thread_send_buffers, sizeof(rep->m_thread_send_buffers));
}


/**
 * Thread Config
 */

static Uint32
get_total_number_of_block_threads(void)
{
  return (NUM_MAIN_THREADS +
          globalData.ndbMtLqhThreads + 
          globalData.ndbMtTcThreads +
          globalData.ndbMtReceiveThreads);
}

static Uint32
get_num_trps()
{
  Uint32 count = 0;
  for (Uint32 id = 1; id < MAX_NTRANSPORTERS; id++)
  {
    if (globalTransporterRegistry.get_transporter(id))
    {
      count++;
    }
  }
  return count;
}

/**
 * This function returns the amount of extra send buffer pages
 * that we should allocate in addition to the amount allocated
 * for each trp send buffer.
 */
#define MIN_SEND_BUFFER_GENERAL (512) //16M
#define MIN_SEND_BUFFER_PER_NODE (8) //256k
#define MIN_SEND_BUFFER_PER_THREAD (64) //2M

Uint32
mt_get_extra_send_buffer_pages(Uint32 curr_num_pages,
                               Uint32 extra_mem_pages)
{
  Uint32 loc_num_threads = get_total_number_of_block_threads();
  Uint32 num_trps = get_num_trps();

  Uint32 extra_pages = extra_mem_pages;

  /**
   * Add 2M for each thread since we allocate 1M every
   * time we allocate and also we ensure there is also a minimum
   * of 1M of send buffer in each thread. Thus we can easily have
   * 2M of send buffer just to keep the contention around the
   * send buffer page spinlock small. This memory we add independent
   * of the configuration settings since the user cannot be
   * expected to handle this and also since we could change this
   * behaviour at any time.
   */
  extra_pages += loc_num_threads * THR_SEND_BUFFER_MAX_FREE;

  if (extra_mem_pages == 0)
  {
    /**
     * The user have set extra send buffer memory to 0 and left for us
     * to decide on our own how much extra memory is needed.
     *
     * We'll make sure that we have at least a minimum of 16M +
     * 2M per thread + 256k per trp. If we have this based on
     * curr_num_pages and our local additions we don't add
     * anything more, if we don't come up to this level we add to
     * reach this minimum level.
     */
    Uint32 min_pages = MIN_SEND_BUFFER_GENERAL +
      (MIN_SEND_BUFFER_PER_NODE * num_trps) +
      (MIN_SEND_BUFFER_PER_THREAD * loc_num_threads);

    if ((curr_num_pages + extra_pages) < min_pages)
    {
      extra_pages = min_pages - curr_num_pages;
    }
  }
  return extra_pages;
}

Uint32
compute_jb_pages(struct EmulatorData * ed)
{
  Uint32 cnt = get_total_number_of_block_threads();
  Uint32 num_receive_threads = globalData.ndbMtReceiveThreads;
  Uint32 num_lqh_threads = globalData.ndbMtLqhThreads;
  Uint32 num_tc_threads = globalData.ndbMtTcThreads;
  Uint32 num_main_threads = NUM_MAIN_THREADS;

  /**
   * Number of pages each thread needs to communicate with another
   * thread.
   */
  Uint32 job_queue_pages_per_thread = thr_job_queue::SIZE;

  /**
   * In 'perthread' we calculate number of pages required by
   * all 'block threads' (excludes 'send-threads'). 'perthread'
   * usage is independent of whether this thread will communicate 
   * with other 'block threads' or not.
   */
  Uint32 perthread = 0;

  /**
   * Each threads has its own job_queue for 'prio A' signals
   */
  perthread += job_queue_pages_per_thread;

  /**
   * Each thread keeps a available free page in 'm_next_buffer'
   * in case it is required by insert_signal() into JBA or JBB.
   */
  perthread += 1;

  /**
   * Each thread keeps time-queued signals in 'struct thr_tq'
   * thr_tq::PAGES are used to store these. 
   */
  perthread += thr_tq::PAGES;

  /**
   * Each thread has its own 'm_free_fifo[THR_FREE_BUF_MAX]' cache.
   * As it is filled to MAX *before* a page is allocated, which consumes a page,
   * it will never cache more than MAX-1 pages. Pages are also returned to global
   * allocator as soon as MAX is reached.
   */
  perthread += THR_FREE_BUF_MAX-1;

  /**
   * Start by calculating the basic number of pages required for
   * our 'cnt' block threads.
   * (no inter-thread communication assumed so far)
   */
  Uint32 tot = cnt * perthread;

  /**
   * We then start adding pages required for inter-thread communications:
   * 
   * Receiver threads will be able to communicate with all other
   * threads except other receive threads.
   */
  tot += num_receive_threads *
         (cnt - num_receive_threads) *
         job_queue_pages_per_thread;

  /**
   * LQH threads can communicate with TC threads and main threads.
   * Cannot communicate with receive threads and other LQH threads,
   * but it can communicate with itself.
   */
  tot += num_lqh_threads *
         (num_tc_threads + num_main_threads + 1) *
         job_queue_pages_per_thread;

  /**
   * First LDM thread is special as it will act as client
   * during backup. It will send to, and receive from (2x) 
   * the 'num_lqh_threads - 1' other LQH threads.
   */
  tot += 2 * (num_lqh_threads-1) *
         job_queue_pages_per_thread;

  /**
   * TC threads can communicate with SPJ-, LQH- and main threads.
   * Cannot communicate with receive threads and other TC threads,
   * but as SPJ is located together with TC, it is counted as it
   * communicate with all TC threads.
   */
  tot += num_tc_threads *
         (num_lqh_threads + num_main_threads + num_tc_threads) *
         job_queue_pages_per_thread;

  /**
   * Main threads can communicate with all other threads
   */
  tot += num_main_threads *
         cnt *
         job_queue_pages_per_thread;

  return tot;
}

ThreadConfig::ThreadConfig()
{
  /**
   * We take great care within struct thr_repository to optimize
   * cache line placement of the different members. This all 
   * depends on that the base address of thr_repository itself
   * is cache line alligned.
   *
   * So we allocate a char[] sufficient large to hold the 
   * thr_repository object, with added bytes for placing
   * g_thr_repository on a CL-alligned offset withing it.
   */
  g_thr_repository_mem = new char[sizeof(thr_repository)+NDB_CL];
  const int alligned_offs = NDB_CL_PADSZ((UintPtr)g_thr_repository_mem);
  char* cache_alligned_mem = &g_thr_repository_mem[alligned_offs];
  require((((UintPtr)cache_alligned_mem) % NDB_CL) == 0);
  g_thr_repository = new(cache_alligned_mem) thr_repository();
}

ThreadConfig::~ThreadConfig()
{
  g_thr_repository->~thr_repository();
  g_thr_repository = NULL;
  delete[] g_thr_repository_mem;
  g_thr_repository_mem = NULL;
}

/*
 * We must do the init here rather than in the constructor, since at
 * constructor time the global memory manager is not available.
 */
void
ThreadConfig::init()
{
  Uint32 num_lqh_threads = globalData.ndbMtLqhThreads;
  Uint32 num_tc_threads = globalData.ndbMtTcThreads;
  Uint32 num_recv_threads = globalData.ndbMtReceiveThreads;
  first_receiver_thread_no =
    NUM_MAIN_THREADS + num_tc_threads + num_lqh_threads;
  glob_num_threads = first_receiver_thread_no + num_recv_threads;
  require(glob_num_threads <= MAX_BLOCK_THREADS);

  glob_num_tc_threads = num_tc_threads;
  if (glob_num_tc_threads == 0)
    glob_num_tc_threads = 1;

  ndbout << "NDBMT: number of block threads=" << glob_num_threads << endl;

  ::rep_init(g_thr_repository, glob_num_threads,
             globalEmulatorData.m_mem_manager);
}

/**
 * return receiver thread handling a particular trp
 *   returned number is indexed from 0 and upwards to #receiver threads
 *   (or MAX_NODES is none)
 */
Uint32
mt_get_recv_thread_idx(TrpId trp_id)
{
  assert(trp_id < NDB_ARRAY_SIZE(g_trp_to_recv_thr_map));
  return g_trp_to_recv_thr_map[trp_id];
}

static
void
assign_receiver_threads(void)
{
  Uint32 num_recv_threads = globalData.ndbMtReceiveThreads;
  Uint32 recv_thread_idx = 0;
  Uint32 recv_thread_idx_shm = 0;
  for (Uint32 trp_id = 1; trp_id < MAX_NTRANSPORTERS; trp_id++)
  {
    Transporter *trp =
      globalTransporterRegistry.get_transporter(trp_id);

    /**
     * Ensure that shared memory transporters are well distributed
     * over all receive threads, so distribute those independent of
     * rest of transporters.
     */
    if (trp)
    {
      if (globalTransporterRegistry.is_shm_transporter(trp_id))
      {
        g_trp_to_recv_thr_map[trp_id] = recv_thread_idx_shm;
        globalTransporterRegistry.set_recv_thread_idx(trp,recv_thread_idx_shm);
        DEB_MULTI_TRP(("SHM trp %u uses recv_thread_idx: %u",
                       trp_id, recv_thread_idx_shm));
        recv_thread_idx_shm++;
        if (recv_thread_idx_shm == num_recv_threads)
          recv_thread_idx_shm = 0;
      }
      else
      {
        g_trp_to_recv_thr_map[trp_id] = recv_thread_idx;
        DEB_MULTI_TRP(("TCP trp %u uses recv_thread_idx: %u",
                       trp_id, recv_thread_idx));
        globalTransporterRegistry.set_recv_thread_idx(trp,recv_thread_idx);
        recv_thread_idx++;
        if (recv_thread_idx == num_recv_threads)
          recv_thread_idx = 0;
      }
    }
    else
    {
      /* Flag for no transporter */
      g_trp_to_recv_thr_map[trp_id] = MAX_NTRANSPORTERS;
    }
  }
  return;
}

void
mt_assign_recv_thread_new_trp(Uint32 trp_id)
{
  if (g_trp_to_recv_thr_map[trp_id] != MAX_NTRANSPORTERS)
  {
    /* Already assigned in the past, keep assignment */
    return;
  }
  Uint32 num_recv_threads = globalData.ndbMtReceiveThreads;
  Uint32 next_recv_thread_tcp = 0;
  Uint32 next_recv_thread_shm = 0;
  for (Uint32 id = 1; id < MAX_NTRANSPORTERS; id++)
  {
    if (id == trp_id)
      continue;
    Transporter *trp =
      globalTransporterRegistry.get_transporter(id);
    if (trp)
    {
      if (globalTransporterRegistry.is_shm_transporter(id))
      {
        next_recv_thread_shm = g_trp_to_recv_thr_map[id];
      }
      else
      {
        next_recv_thread_tcp = g_trp_to_recv_thr_map[id];
      }
    }
  }
  Transporter *trp =
    globalTransporterRegistry.get_transporter(trp_id);
  require(trp);
  Uint32 choosen_recv_thread;
  if (globalTransporterRegistry.is_shm_transporter(trp_id))
  {
    next_recv_thread_shm++;
    if (next_recv_thread_shm == num_recv_threads)
      next_recv_thread_shm = 0;
    g_trp_to_recv_thr_map[trp_id] = next_recv_thread_shm;
    choosen_recv_thread = next_recv_thread_shm;
    globalTransporterRegistry.set_recv_thread_idx(trp, next_recv_thread_shm);
    DEB_MULTI_TRP(("SHM multi trp %u uses recv_thread_idx: %u",
                   trp_id, next_recv_thread_shm));
  }
  else
  {
    next_recv_thread_tcp++;
    if (next_recv_thread_tcp == num_recv_threads)
      next_recv_thread_tcp = 0;
    g_trp_to_recv_thr_map[trp_id] = next_recv_thread_tcp;
    choosen_recv_thread = next_recv_thread_tcp;
    globalTransporterRegistry.set_recv_thread_idx(trp, next_recv_thread_tcp);
    DEB_MULTI_TRP(("TCP multi trp %u uses recv_thread_idx: %u",
                   trp_id, next_recv_thread_tcp));
  }
  TransporterReceiveHandleKernel *recvdata =
    g_trp_receive_handle_ptr[choosen_recv_thread];
  recvdata->m_transporters.set(trp_id);
}

bool
mt_epoll_add_trp(Uint32 self, NodeId node_id, TrpId trp_id)
{
  (void)node_id;
  struct thr_repository* rep = g_thr_repository;
  struct thr_data *selfptr = &rep->m_thread[self];
  Uint32 thr_no = selfptr->m_thr_no;
  require(thr_no >= first_receiver_thread_no);
  Uint32 recv_thread_idx = thr_no - first_receiver_thread_no;
  TransporterReceiveHandleKernel *recvdata =
    g_trp_receive_handle_ptr[recv_thread_idx];
  if (recv_thread_idx != g_trp_to_recv_thr_map[trp_id])
  {
    return false;
  }
  Transporter *t = globalTransporterRegistry.get_transporter(trp_id);
  lock(&rep->m_send_buffers[trp_id].m_send_lock);
  lock(&rep->m_receive_lock[recv_thread_idx]);
  require(recvdata->epoll_add(t));
  unlock(&rep->m_receive_lock[recv_thread_idx]);
  unlock(&rep->m_send_buffers[trp_id].m_send_lock);
  return true;
}

bool
mt_is_recv_thread_for_new_trp(Uint32 self,
                              NodeId node_id,
                              TrpId trp_id)
{
  (void)node_id;
  struct thr_repository* rep = g_thr_repository;
  struct thr_data *selfptr = &rep->m_thread[self];
  Uint32 thr_no = selfptr->m_thr_no;
  require(thr_no >= first_receiver_thread_no);
  Uint32 recv_thread_idx = thr_no - first_receiver_thread_no;
  if (recv_thread_idx != g_trp_to_recv_thr_map[trp_id])
  {
    return false;
  }
  return true;
}

void
ThreadConfig::ipControlLoop(NdbThread* pThis)
{
  unsigned int thr_no;
  struct thr_repository* rep = g_thr_repository;

  rep->m_thread[first_receiver_thread_no].m_thr_index =
    globalEmulatorData.theConfiguration->addThread(pThis, ReceiveThread);

  max_send_delay = globalEmulatorData.theConfiguration->maxSendDelay();

  /**
   * Set the configured time we will spend in spinloop before coming
   * back to check conditions.
   */
  Uint32 spin_nanos = globalEmulatorData.theConfiguration->spinTimePerCall();
  NdbSpin_Change(Uint64(spin_nanos));
  g_eventLogger->info("Number of spin loops is %llu to pause %llu nanoseconds",
                      NdbSpin_get_num_spin_loops(),
                      NdbSpin_get_current_spin_nanos());

  if (globalData.ndbMtSendThreads)
  {
    /**
     * new operator do not ensure alignment for overaligned data types.
     * As for g_thr_repository, overallocate memory and construct the
     * thr_send_threads object within at aligned address.
     */
    g_send_threads_mem = new char[sizeof(thr_send_threads) + NDB_CL];
    const int aligned_offs = NDB_CL_PADSZ((UintPtr)g_send_threads_mem);
    char* cache_aligned_mem = &g_send_threads_mem[aligned_offs];
    require((((UintPtr)cache_aligned_mem) % NDB_CL) == 0);
    g_send_threads = new (cache_aligned_mem) thr_send_threads();
  }

  /**
   * assign trps to receiver threads
   */
  assign_receiver_threads();

  /* Start the send thread(s) */
  if (g_send_threads)
  {
    /**
     * assign trps to send threads
     */
    g_send_threads->assign_trps_to_send_threads();
    g_send_threads->assign_threads_to_assist_send_threads();

    g_send_threads->start_send_threads();
  }

  /*
   * Start threads for all execution threads, except for the receiver
   * thread, which runs in the main thread.
   */
  for (thr_no = 0; thr_no < glob_num_threads; thr_no++)
  {
    NDB_TICKS now = NdbTick_getCurrentTicks();
    rep->m_thread[thr_no].m_ticks = now;
    rep->m_thread[thr_no].m_scan_real_ticks = now;

    if (thr_no == first_receiver_thread_no)
      continue;                 // Will run in the main thread.

    /*
     * The NdbThread_Create() takes void **, but that is cast to void * when
     * passed to the thread function. Which is kind of strange ...
     */
    if (thr_no < first_receiver_thread_no)
    {
      /* Start block threads */
      struct NdbThread *thread_ptr =
        NdbThread_Create(mt_job_thread_main,
                         (void **)(rep->m_thread + thr_no),
                         1024*1024,
                         "execute thread", //ToDo add number
                         NDB_THREAD_PRIO_MEAN);
      require(thread_ptr != NULL);
      rep->m_thread[thr_no].m_thr_index =
        globalEmulatorData.theConfiguration->addThread(thread_ptr,
                                                       BlockThread);
      rep->m_thread[thr_no].m_thread = thread_ptr;
    }
    else
    {
      /* Start a receiver thread, also block thread for TRPMAN */
      struct NdbThread *thread_ptr =
        NdbThread_Create(mt_receiver_thread_main,
                         (void **)(&rep->m_thread[thr_no]),
                         1024*1024,
                         "receive thread", //ToDo add number
                         NDB_THREAD_PRIO_MEAN);
      require(thread_ptr != NULL);
      globalEmulatorData.theConfiguration->addThread(thread_ptr,
                                                     ReceiveThread);
      rep->m_thread[thr_no].m_thread = thread_ptr;
    }
  }

  /* Now run the main loop for first receiver thread directly. */
  rep->m_thread[first_receiver_thread_no].m_thread = pThis;
  mt_receiver_thread_main(&(rep->m_thread[first_receiver_thread_no]));

  /* Wait for all threads to shutdown. */
  for (thr_no = 0; thr_no < glob_num_threads; thr_no++)
  {
    if (thr_no == first_receiver_thread_no)
      continue;
    void *dummy_return_status;
    NdbThread_WaitFor(rep->m_thread[thr_no].m_thread,
                      &dummy_return_status);
    globalEmulatorData.theConfiguration->removeThread(
      rep->m_thread[thr_no].m_thread);
    NdbThread_Destroy(&(rep->m_thread[thr_no].m_thread));
  }

  /* Delete send threads, includes waiting for threads to shutdown */
  if (g_send_threads)
  {
    g_send_threads->~thr_send_threads();
    g_send_threads = NULL;
    delete[] g_send_threads_mem;
    g_send_threads_mem = NULL;
  }
  globalEmulatorData.theConfiguration->removeThread(pThis);
}

int
ThreadConfig::doStart(NodeState::StartLevel startLevel)
{
  SignalT<3> signalT;
  memset(&signalT.header, 0, sizeof(SignalHeader));
  
  signalT.header.theVerId_signalNumber   = GSN_START_ORD;
  signalT.header.theReceiversBlockNumber = CMVMI;
  signalT.header.theSendersBlockRef      = 0;
  signalT.header.theTrace                = 0;
  signalT.header.theSignalId             = 0;
  signalT.header.theLength               = StartOrd::SignalLength;
  
  StartOrd * startOrd = CAST_PTR(StartOrd, &signalT.theData[0]);
  startOrd->restartInfo = 0;
  
  sendprioa(block2ThreadId(CMVMI, 0), &signalT.header, signalT.theData, 0);
  return 0;
}

Uint32
FastScheduler::traceDumpGetNumThreads()
{
  /* The last thread is only for receiver -> no trace file. */
  return glob_num_threads;
}

bool
FastScheduler::traceDumpGetJam(Uint32 thr_no,
                               const JamEvent * & thrdTheEmulatedJam,
                               Uint32 & thrdTheEmulatedJamIndex)
{
  if (thr_no >= glob_num_threads)
    return false;

#ifdef NO_EMULATED_JAM
  thrdTheEmulatedJam = NULL;
  thrdTheEmulatedJamIndex = 0;
#else
  const EmulatedJamBuffer *jamBuffer =
    &g_thr_repository->m_thread[thr_no].m_jam;
  thrdTheEmulatedJam = jamBuffer->theEmulatedJam;
  thrdTheEmulatedJamIndex = jamBuffer->theEmulatedJamIndex;
#endif
  return true;
}

void
FastScheduler::traceDumpPrepare(NdbShutdownType& nst)
{
  /*
   * We are about to generate trace files for all threads.
   *
   * We want to stop all threads processing before we dump, as otherwise the
   * signal buffers could change while dumping, leading to inconsistent
   * results.
   *
   * To stop threads, we send the GSN_STOP_FOR_CRASH signal as prio A to each
   * thread. We then wait for threads to signal they are done (but not forever,
   * so as to not have one hanging thread prevent the generation of trace
   * dumps). We also must be careful not to send to ourself if the crash is
   * being processed by one of the threads processing signals.
   *
   * We do not stop the transporter thread, as it cannot receive signals (but
   * because it does not receive signals it does not really influence dumps in
   * any case).
   */
  const thr_data *selfptr = NDB_THREAD_TLS_THREAD;
  /* The selfptr might be NULL, or pointer to thread that crashed. */

  Uint32 waitFor_count = 0;
  NdbMutex_Lock(&g_thr_repository->stop_for_crash_mutex);
  g_thr_repository->stopped_threads = 0;
  NdbMutex_Unlock(&g_thr_repository->stop_for_crash_mutex);

  for (Uint32 thr_no = 0; thr_no < glob_num_threads; thr_no++)
  {
    if (selfptr != NULL && selfptr->m_thr_no == thr_no)
    {
      /* This is own thread; we have already stopped processing. */
      continue;
    }

    sendprioa_STOP_FOR_CRASH(selfptr, thr_no);

    waitFor_count++;
  }

  static const Uint32 max_wait_seconds = 2;
  const NDB_TICKS start = NdbTick_getCurrentTicks();
  NdbMutex_Lock(&g_thr_repository->stop_for_crash_mutex);
  while (g_thr_repository->stopped_threads < waitFor_count)
  {
    NdbCondition_WaitTimeout(&g_thr_repository->stop_for_crash_cond,
                             &g_thr_repository->stop_for_crash_mutex,
                             10);
    const NDB_TICKS now = NdbTick_getCurrentTicks();
    if (NdbTick_Elapsed(start,now).seconds() > max_wait_seconds)
      break;                    // Give up
  }
  if (g_thr_repository->stopped_threads < waitFor_count)
  {
    if (nst != NST_ErrorInsert)
    {
      nst = NST_Watchdog; // Make this abort fast
    }
    ndbout_c("Warning: %d thread(s) did not stop before starting crash dump.",
             waitFor_count - g_thr_repository->stopped_threads);
  }
  NdbMutex_Unlock(&g_thr_repository->stop_for_crash_mutex);

  /* Now we are ready (or as ready as can be) for doing crash dump. */
}

/**
 * In ndbmtd we could have a case where we actually have multiple threads
 * crashing at the same time. This causes several threads to start processing
 * the crash handling in parallel and eventually lead to a deadlock since
 * the crash handling thread waits for other threads to stop before completing
 * the crash handling.
 * 
 * To avoid this we use this function that only is useful in ndbmtd where
 * we check if the crash handling has already started. We protect this
 * check using the stop_for_crash-mutex. This function is called twice,
 * first to write an entry in the error log and second to specify that the
 * error log write is completed.
 * 
 * We proceed only from the first call if the crash handling hasn't started
 * or if the crash is not caused by an error insert. If it is caused by an
 * error insert it is a normal situation with multiple crashes, so we won't
 * clutter the error log with multiple entries in this case. If it is a real
 * crash and we have more than one thread crashing, then this is vital
 * information to write in the error log, we do however not want more than
 * one set of trace files.
 *
 * To ensure that writes of the error log happens for one thread at a time we
 * protect it with the stop_for_crash-mutex. We hold this mutex between the
 * first and second call of this function from the error reporter thread.
 *
 * We proceed from the first call only if we are the first thread that
 * reported an error. To handle this properly we start by acquiring the
 * mutex, then we write the error log, when we come back we set the
 * crash_started flag and release the mutex to enable other threads to
 * write into the error log, but still stopping them from proceeding to
 * write another set of trace files.
 *
 * We will not come back from this function the second time unless we are
 * the first crashing thread.
 */

static bool crash_started = false;

void
ErrorReporter::prepare_to_crash(bool first_phase, bool error_insert_crash)
{
  if (first_phase)
  {
    NdbMutex_Lock(&g_thr_repository->stop_for_crash_mutex);
    if (crash_started && error_insert_crash)
    {
      /**
       * Some other thread has already started the crash handling.
       * We call the below method which we will never return from.
       * We need not write multiple entries in error log for
       * error insert crashes since it is a normal event.
       */
      NdbMutex_Unlock(&g_thr_repository->stop_for_crash_mutex);
      mt_execSTOP_FOR_CRASH();
    }
    /**
     * Proceed to write error log before returning to this method
     * again with start set to 0.
     */
  }
  else if (crash_started)
  {
    (void)error_insert_crash;
    /**
     * No need to proceed since somebody already started handling the crash.
     * We proceed by calling mt_execSTOP_FOR_CRASH to stop this thread
     * in a manner that is similar to if we received the signal
     * STOP_FOR_CRASH.
     */
    NdbMutex_Unlock(&g_thr_repository->stop_for_crash_mutex);
    mt_execSTOP_FOR_CRASH();
  }
  else
  {
    /**
     * No crash had started previously, we will take care of it. Before
     * handling it we will mark the crash handling as started.
     */
    crash_started = true;
    NdbMutex_Unlock(&g_thr_repository->stop_for_crash_mutex);
  }
}

void mt_execSTOP_FOR_CRASH()
{
  const thr_data *selfptr = NDB_THREAD_TLS_THREAD;
  require(selfptr != NULL);

  NdbMutex_Lock(&g_thr_repository->stop_for_crash_mutex);
  g_thr_repository->stopped_threads++;
  NdbCondition_Signal(&g_thr_repository->stop_for_crash_cond);
  NdbMutex_Unlock(&g_thr_repository->stop_for_crash_mutex);

  /* ToDo: is this correct? */
  globalEmulatorData.theWatchDog->unregisterWatchedThread(selfptr->m_thr_no);

  my_thread_exit(NULL);
}

void
FastScheduler::dumpSignalMemory(Uint32 thr_no, FILE* out)
{
  thr_data *selfptr = NDB_THREAD_TLS_THREAD;
  const thr_repository *rep = g_thr_repository;
  /*
   * The selfptr might be NULL, or pointer to thread that is doing the crash
   * jump.
   * If non-null, we should update the watchdog counter while dumping.
   */
  Uint32 *watchDogCounter;
  if (selfptr)
    watchDogCounter = &selfptr->m_watchdog_counter;
  else
    watchDogCounter = NULL;

  /*
   * We want to dump the signal buffers from last executed to first executed.
   * So we first need to find the correct sequence to output signals in, stored
   * in this arrray.
   *
   * We will check any buffers in the cyclic m_free_fifo. In addition,
   * we also need to scan the already executed part of the current
   * buffer in m_jba.
   *
   * Due to partial execution of prio A buffers, we will use signal ids to know
   * where to interleave prio A signals into the stream of prio B signals
   * read. So we will keep a pointer to a prio A buffer around; and while
   * scanning prio B buffers we will interleave prio A buffers from that buffer
   * when the signal id fits the sequence.
   *
   * This also means that we may have to discard the earliest part of available
   * prio A signal data due to too little prio B data present, or vice versa.
   */
  static const Uint32 MAX_SIGNALS_TO_DUMP = 4096;
  struct {
    const SignalHeader *ptr;
    bool prioa;
  } signalSequence[MAX_SIGNALS_TO_DUMP];
  Uint32 seq_start = 0;
  Uint32 seq_end = 0;

  const struct thr_data *thr_ptr = &rep->m_thread[thr_no];
  if (watchDogCounter)
    *watchDogCounter = 4;

  /*
   * ToDo: Might do some sanity check to avoid crashing on not yet initialised
   * thread.
   */

  /* Scan all available buffers with already executed signals. */

  /*
   * Keep track of all available buffers, so that we can pick out signals in
   * the same order they were executed (order obtained from signal id).
   *
   * We may need to keep track of THR_FREE_BUF_MAX buffers for fully executed
   * (and freed) buffers, plus MAX_BLOCK_THREADS buffers for currently active
   * prio B buffers, plus one active prio A buffer.
   */
  struct {
    const thr_job_buffer *m_jb;
    Uint32 m_pos;
    Uint32 m_max;
  } jbs[THR_FREE_BUF_MAX + MAX_BLOCK_THREADS + 1];

  Uint32 num_jbs = 0;

  /* Load released buffers. */
  Uint32 idx = thr_ptr->m_first_free;
  while (idx != thr_ptr->m_first_unused)
  {
    const thr_job_buffer *q = thr_ptr->m_free_fifo[idx];
    if (q->m_len > 0)
    {
      jbs[num_jbs].m_jb = q;
      jbs[num_jbs].m_pos = 0;
      jbs[num_jbs].m_max = q->m_len;
      num_jbs++;
    }
    idx = (idx + 1) % THR_FREE_BUF_MAX;
  }
  /* Load any active prio B buffers. */
  for (Uint32 thr_no = 0; thr_no < rep->m_thread_count; thr_no++)
  {
    const thr_job_queue *q = thr_ptr->m_in_queue + thr_no;
    const thr_jb_read_state *r = thr_ptr->m_read_states + thr_no;
    Uint32 read_pos = r->m_read_pos;
    if (r->is_open() && read_pos > 0)
    {
      jbs[num_jbs].m_jb = q->m_buffers[r->m_read_index];
      jbs[num_jbs].m_pos = 0;
      jbs[num_jbs].m_max = read_pos;
      num_jbs++;
    }
  }
  /* Load any active prio A buffer. */
  const thr_jb_read_state *r = &thr_ptr->m_jba_read_state;
  Uint32 read_pos = r->m_read_pos;
  if (read_pos > 0)
  {
    jbs[num_jbs].m_jb = thr_ptr->m_jba.m_buffers[r->m_read_index];
    jbs[num_jbs].m_pos = 0;
    jbs[num_jbs].m_max = read_pos;
    num_jbs++;
  }

  /* Use the next signal id as the smallest (oldest).
   *
   * Subtracting two signal ids with the smallest makes
   * them comparable using standard comparision of Uint32,
   * there the biggest value is the newest.
   * For example,
   *   (m_signal_id_counter - smallest_signal_id) == UINT32_MAX
   */
  const Uint32 smallest_signal_id = thr_ptr->m_signal_id_counter + 1;

  /* Now pick out one signal at a time, in signal id order. */
  while (num_jbs > 0)
  {
    if (watchDogCounter)
      *watchDogCounter = 4;

    /* Search out the smallest signal id remaining. */
    Uint32 idx_min = 0;
    const Uint32 *p = jbs[idx_min].m_jb->m_data + jbs[idx_min].m_pos;
    const SignalHeader *s_min = reinterpret_cast<const SignalHeader*>(p);
    Uint32 sid_min_adjusted = s_min->theSignalId - smallest_signal_id;

    for (Uint32 i = 1; i < num_jbs; i++)
    {
      p = jbs[i].m_jb->m_data + jbs[i].m_pos;
      const SignalHeader *s = reinterpret_cast<const SignalHeader*>(p);
      const Uint32 sid_adjusted = s->theSignalId - smallest_signal_id;
      if (sid_adjusted < sid_min_adjusted)
      {
        idx_min = i;
        s_min = s;
        sid_min_adjusted = sid_adjusted;
      }
    }

    /* We found the next signal, now put it in the ordered cyclic buffer. */
    signalSequence[seq_end].ptr = s_min;
    signalSequence[seq_end].prioa = jbs[idx_min].m_jb->m_prioa;
    Uint32 siglen =
      (sizeof(SignalHeader)>>2) + s_min->m_noOfSections + s_min->theLength;
#if SIZEOF_CHARP == 8
    /* Align to 8-byte boundary, to ensure aligned copies. */
    siglen= (siglen+1) & ~((Uint32)1);
#endif
    jbs[idx_min].m_pos += siglen;
    if (jbs[idx_min].m_pos >= jbs[idx_min].m_max)
    {
      /* We are done with this job buffer. */
      num_jbs--;
      jbs[idx_min] = jbs[num_jbs];
    }
    seq_end = (seq_end + 1) % MAX_SIGNALS_TO_DUMP;
    /* Drop old signals if too many available in history. */
    if (seq_end == seq_start)
      seq_start = (seq_start + 1) % MAX_SIGNALS_TO_DUMP;
  }

  /* Now, having build the correct signal sequence, we can dump them all. */
  fprintf(out, "\n");
  bool first_one = true;
  bool out_of_signals = false;
  Uint32 lastSignalId = 0;
  while (seq_end != seq_start)
  {
    if (watchDogCounter)
      *watchDogCounter = 4;

    if (seq_end == 0)
      seq_end = MAX_SIGNALS_TO_DUMP;
    seq_end--;
    SignalT<25> signal;
    const SignalHeader *s = signalSequence[seq_end].ptr;
    unsigned siglen = (sizeof(*s)>>2) + s->theLength;
    if (siglen > MAX_SIGNAL_SIZE)
      siglen = MAX_SIGNAL_SIZE;              // Sanity check
    memcpy(&signal.header, s, 4*siglen);
    // instance number in trace file is confusing if not MT LQH
    if (globalData.ndbMtLqhWorkers == 0)
      signal.header.theReceiversBlockNumber &= NDBMT_BLOCK_MASK;

    const Uint32 *posptr = reinterpret_cast<const Uint32 *>(s);
    signal.m_sectionPtrI[0] = posptr[siglen + 0];
    signal.m_sectionPtrI[1] = posptr[siglen + 1];
    signal.m_sectionPtrI[2] = posptr[siglen + 2];
    bool prioa = signalSequence[seq_end].prioa;

    /* Make sure to display clearly when there is a gap in the dump. */
    if (!first_one && !out_of_signals && (s->theSignalId + 1) != lastSignalId)
    {
      out_of_signals = true;
      fprintf(out, "\n\n\nNo more prio %s signals, rest of dump will be "
              "incomplete.\n\n\n\n", prioa ? "B" : "A");
    }
    first_one = false;
    lastSignalId = s->theSignalId;

    fprintf(out, "--------------- Signal ----------------\n");
    Uint32 prio = (prioa ? JBA : JBB);
    SignalLoggerManager::printSignalHeader(out, 
                                           signal.header,
                                           prio,
                                           globalData.ownId, 
                                           true);
    SignalLoggerManager::printSignalData  (out, 
                                           signal.header,
                                           &signal.theData[0]);
  }
  fflush(out);
}

int
FastScheduler::traceDumpGetCurrentThread()
{
  const thr_data *selfptr = NDB_THREAD_TLS_THREAD;

  /* The selfptr might be NULL, or pointer to thread that crashed. */
  if (selfptr == 0)
  {
    return -1;
  }
  else
  {
    return (int)selfptr->m_thr_no;
  }
}

void
mt_section_lock()
{
  lock(&(g_thr_repository->m_section_lock));
}

void
mt_section_unlock()
{
  unlock(&(g_thr_repository->m_section_lock));
}

void
mt_mem_manager_init()
{
}

void
mt_mem_manager_lock()
{
  lock(&(g_thr_repository->m_mem_manager_lock));
}

void
mt_mem_manager_unlock()
{
  unlock(&(g_thr_repository->m_mem_manager_lock));
}

Vector<mt_lock_stat> g_locks;
template class Vector<mt_lock_stat>;

static
void
register_lock(const void * ptr, const char * name)
{
  if (name == 0)
    return;

  mt_lock_stat* arr = g_locks.getBase();
  for (size_t i = 0; i<g_locks.size(); i++)
  {
    if (arr[i].m_ptr == ptr)
    {
      if (arr[i].m_name)
      {
        free(arr[i].m_name);
      }
      arr[i].m_name = strdup(name);
      return;
    }
  }

  mt_lock_stat ln;
  ln.m_ptr = ptr;
  ln.m_name = strdup(name);
  ln.m_contended_count = 0;
  ln.m_spin_count = 0;
  g_locks.push_back(ln);
}

#if defined(NDB_HAVE_XCNG) && defined(NDB_USE_SPINLOCK)
static
mt_lock_stat *
lookup_lock(const void * ptr)
{
  mt_lock_stat* arr = g_locks.getBase();
  for (size_t i = 0; i<g_locks.size(); i++)
  {
    if (arr[i].m_ptr == ptr)
      return arr + i;
  }

  return 0;
}
#endif

Uint32
mt_get_threads_for_blocks_no_proxy(const Uint32 blocks[],
                                   BlockThreadBitmask& mask)
{
  Uint32 cnt = 0;
  for (Uint32 i = 0; blocks[i] != 0; i++)
  {
    Uint32 block = blocks[i];
    /**
     * Find each thread that has instance of block
     */
    assert(block == blockToMain(block));
    const Uint32 index = block - MIN_BLOCK_NO;
    const Uint32 instance_count = block_instance_count[index];
    require(instance_count <= NDB_ARRAY_SIZE(thr_map[index]));
    // If more than one instance, avoid proxy instance 0
    const Uint32 first_instance = (instance_count > 1) ? 1 : 0;
    for (Uint32 instance = first_instance;
         instance < instance_count;
         instance++)
    {
      Uint32 thr_no = thr_map[index][instance].thr_no;
      require(thr_no != thr_map_entry::NULL_THR_NO);

      if (mask.get(thr_no))
        continue;

      mask.set(thr_no);
      cnt++;
    }
  }
  require(mask.count() == cnt);
  return cnt;
}

Uint32
mt_get_addressable_threads(const Uint32 my_thr_no, BlockThreadBitmask& mask)
{
  const Uint32 thr_cnt = get_total_number_of_block_threads();
  Uint32 cnt = 0;
  for (Uint32 thr_no = 0; thr_no < thr_cnt; thr_no++)
  {
    if (may_communicate(my_thr_no, thr_no))
    {
      mask.set(thr_no);
      cnt++;
    }
  }
  if (!mask.get(my_thr_no))
  {
    mask.set(my_thr_no);
    cnt++;
  }
  require(mask.count() == cnt);
  return cnt;
}

void
mt_wakeup(class SimulatedBlock* block)
{
  Uint32 thr_no = block->getThreadId();
  struct thr_data *thrptr = &g_thr_repository->m_thread[thr_no];
  wakeup(&thrptr->m_waiter);
}

#ifdef VM_TRACE
void
mt_assert_own_thread(SimulatedBlock* block)
{
  Uint32 thr_no = block->getThreadId();
  struct thr_data *thrptr = &g_thr_repository->m_thread[thr_no];

  if (unlikely(my_thread_equal(thrptr->m_thr_id, my_thread_self()) == 0))
  {
    fprintf(stderr, "mt_assert_own_thread() - assertion-failure\n");
    fflush(stderr);
    abort();
  }
}
#endif


Uint32
mt_get_blocklist(SimulatedBlock * block, Uint32 arr[], Uint32 len)
{
  Uint32 thr_no = block->getThreadId();
  struct thr_data *thr_ptr = &g_thr_repository->m_thread[thr_no];

  for (Uint32 i = 0; i < thr_ptr->m_instance_count; i++)
  {
    arr[i] = thr_ptr->m_instance_list[i];
  }

  return thr_ptr->m_instance_count;
}

void
mt_get_spin_stat(class SimulatedBlock *block, ndb_spin_stat *dst)
{
  Uint32 thr_no = block->getThreadId();
  struct thr_data *selfptr = &g_thr_repository->m_thread[thr_no];
  dst->m_sleep_longer_spin_time = selfptr->m_spin_stat.m_sleep_longer_spin_time;
  dst->m_sleep_shorter_spin_time =
    selfptr->m_spin_stat.m_sleep_shorter_spin_time;
  dst->m_num_waits = selfptr->m_spin_stat.m_num_waits;
  for (Uint32 i = 0; i < NUM_SPIN_INTERVALS; i++)
  {
    dst->m_micros_sleep_times[i] =
      selfptr->m_spin_stat.m_micros_sleep_times[i];
    dst->m_spin_interval[i] = selfptr->m_spin_stat.m_spin_interval[i];
  }
}

void mt_set_spin_stat(class SimulatedBlock *block, ndb_spin_stat *src)
{
  Uint32 thr_no = block->getThreadId();
  struct thr_data *selfptr = &g_thr_repository->m_thread[thr_no];
  memset(&selfptr->m_spin_stat, 0, sizeof(selfptr->m_spin_stat));
  for (Uint32 i = 0; i < NUM_SPIN_INTERVALS; i++)
  {
    selfptr->m_spin_stat.m_spin_interval[i] = src->m_spin_interval[i];
  }
}

void
mt_get_thr_stat(class SimulatedBlock * block, ndb_thr_stat* dst)
{
  bzero(dst, sizeof(* dst));
  Uint32 thr_no = block->getThreadId();
  struct thr_data *selfptr = &g_thr_repository->m_thread[thr_no];

  THRConfigApplier & conf = globalEmulatorData.theConfiguration->m_thr_config;
  dst->thr_no = thr_no;
  dst->name = conf.getName(selfptr->m_instance_list, selfptr->m_instance_count);
  dst->os_tid = NdbThread_GetTid(selfptr->m_thread);
  dst->loop_cnt = selfptr->m_stat.m_loop_cnt;
  dst->exec_cnt = selfptr->m_stat.m_exec_cnt;
  dst->wait_cnt = selfptr->m_stat.m_wait_cnt;
  dst->local_sent_prioa = selfptr->m_stat.m_prioa_count;
  dst->local_sent_priob = selfptr->m_stat.m_priob_count;
}

TransporterReceiveHandle *
mt_get_trp_receive_handle(unsigned instance)
{
  assert(instance > 0 && instance <= MAX_NDBMT_RECEIVE_THREADS);
  if (instance > 0 && instance <= MAX_NDBMT_RECEIVE_THREADS)
  {
    return g_trp_receive_handle_ptr[instance - 1 /* proxy */];
  }
  return 0;
}

#if defined(USE_INIT_GLOBAL_VARIABLES)
void
mt_clear_global_variables(thr_data *selfptr)
{
  if (selfptr->m_global_variables_enabled)
  {
    for (Uint32 i = 0; i < selfptr->m_global_variables_ptr_instances; i++)
    {
      Ptr<void> *tmp = (Ptr<void>*)selfptr->m_global_variables_ptrs[i];
      tmp->i = RNIL;
      tmp->p = 0;
    }
    for (Uint32 i = 0; i < selfptr->m_global_variables_uint32_ptr_instances; i++)
    {
      void **tmp = (void**)selfptr->m_global_variables_uint32_ptrs[i];
      (*tmp) = 0;
    }
    for (Uint32 i = 0; i < selfptr->m_global_variables_uint32_instances; i++)
    {
      Uint32 *tmp = (Uint32*)selfptr->m_global_variables_uint32[i];
      (*tmp) = Uint32(~0);
    }
  }
}

void
mt_enable_global_variables(Uint32 self)
{
  struct thr_repository* rep = g_thr_repository;
  struct thr_data *selfptr = &rep->m_thread[self];
  selfptr->m_global_variables_enabled = true;
}

void
mt_disable_global_variables(Uint32 self)
{
  struct thr_repository* rep = g_thr_repository;
  struct thr_data *selfptr = &rep->m_thread[self];
  selfptr->m_global_variables_enabled = false;
}

void
mt_init_global_variables_ptr_instances(Uint32 self,
                                       void ** tmp,
                                       size_t cnt)
{
  struct thr_repository* rep = g_thr_repository;
  struct thr_data *selfptr = &rep->m_thread[self];
  for (size_t i = 0; i < cnt; i++)
  {
    Uint32 inx = selfptr->m_global_variables_ptr_instances;
    selfptr->m_global_variables_ptrs[inx] = tmp[i];
    selfptr->m_global_variables_ptr_instances = inx + 1;
  }
}

void
mt_init_global_variables_uint32_ptr_instances(Uint32 self,
                                              void **tmp,
                                              size_t cnt)
{
  struct thr_repository* rep = g_thr_repository;
  struct thr_data *selfptr = &rep->m_thread[self];
  for (size_t i = 0; i < cnt; i++)
  {
    Uint32 inx = selfptr->m_global_variables_uint32_ptr_instances;
    selfptr->m_global_variables_uint32_ptrs[inx] = tmp[i];
    selfptr->m_global_variables_uint32_ptr_instances = inx + 1;
  }
}

void
mt_init_global_variables_uint32_instances(Uint32 self,
                                          void **tmp,
                                          size_t cnt)
{
  struct thr_repository* rep = g_thr_repository;
  struct thr_data *selfptr = &rep->m_thread[self];
  for (size_t i = 0; i < cnt; i++)
  {
    Uint32 inx = selfptr->m_global_variables_uint32_instances;
    selfptr->m_global_variables_uint32[inx] = tmp[i];
    selfptr->m_global_variables_uint32_instances = inx + 1;
  }
}
#endif

/**
 * Global data
 */
static struct trp_callback g_trp_callback;

TransporterRegistry globalTransporterRegistry(&g_trp_callback, NULL);
