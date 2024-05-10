/* Copyright (c) 2008, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <ndb_global.h>
#include <atomic>
#include <cstring>
#include "my_config.h"
#include "ndb_config.h"
#include "portlib/ndb_compiler.h"
#include "util/require.h"

#define NDBD_MULTITHREADED

#include <NdbGetRUsage.h>
#include <NdbSleep.h>
#include <NdbSpin.h>
#include <kernel_types.h>
#include <portlib/ndb_prefetch.h>
#include <DebuggerNames.hpp>
#include <ErrorHandlingMacros.hpp>
#include <GlobalData.hpp>
#include <Pool.hpp>
#include <Prio.hpp>
#include <SignalLoggerManager.hpp>
#include <SimulatedBlock.hpp>
#include <TransporterDefinitions.hpp>
#include <TransporterRegistry.hpp>
#include <VMSignal.hpp>
#include <WatchDog.hpp>
#include <blocks/pgman.hpp>
#include <blocks/thrman.hpp>
#include <signaldata/StopForCrash.hpp>
#include "FastScheduler.hpp"
#include "TransporterCallbackKernel.hpp"
#include "mt.hpp"

#include "mt-lock.hpp"
#include "portlib/mt-asm.h"

#include <signaldata/StartOrd.hpp>
#include "ThreadConfig.hpp"

#include <NdbCondition.h>
#include <NdbMutex.h>
#include <NdbTick.h>
#include <Bitmask.hpp>
#include <ErrorReporter.hpp>
#include <EventLogger.hpp>

/**
 * Using 1 and 2 job buffers per thread can lead to hotspots for tc threads
 * when many LDMs send data to it as part of SPJ query execution. 4 is enough,
 * but we set it to 8 to ensure that it is many enough to not have any issues.
 *
 * Could define it based on number of threads in the node.
 */
static constexpr Uint32 NUM_JOB_BUFFERS_PER_THREAD = 32;
static constexpr Uint32 SIGNAL_RNIL = 0xFFFFFFFF;

#if (defined(VM_TRACE) || defined(ERROR_INSERT))
// #define DEBUG_MULTI_TRP 1
#endif

#ifdef DEBUG_MULTI_TRP
#define DEB_MULTI_TRP(arglist)   \
  do {                           \
    g_eventLogger->info arglist; \
  } while (0)
#else
#define DEB_MULTI_TRP(arglist) \
  do {                         \
  } while (0)
#endif

/**
 * Two new manual(recompile) error-injections in mt.cpp :
 *
 *     NDB_BAD_SEND: Causes send buffer code to mess with a byte in a send
 *                   buffer
 *     NDB_LUMPY_SEND: Causes transporters to be given small, oddly aligned
 *                     and sized IOVECs to send, testing ability of new and
 *                     existing code to handle this.
 *
 *   These are useful for testing the correctness of the new code, and
 *   the resulting behaviour / debugging output.
 */
// #define NDB_BAD_SEND
// #define NDB_LUMPY_SEND

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

inline SimulatedBlock *GlobalData::mt_getBlock(BlockNumber blockNo,
                                               Uint32 instanceNo) {
  require(blockNo >= MIN_BLOCK_NO && blockNo <= MAX_BLOCK_NO);
  SimulatedBlock *b = getBlock(blockNo);
  if (b != 0 && instanceNo != 0) b = b->getInstance(instanceNo);
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
static constexpr Uint32 MAX_SIGNALS_PER_JB = 75;

/**
 * Max signals written to other thread before calling wakeup_pending_signals
 */
static constexpr Uint32 MAX_SIGNALS_BEFORE_WAKEUP = 128;

/* Max signals written to other thread before calling flush_local_signals */
static constexpr Uint32 MAX_SIGNALS_BEFORE_FLUSH_RECEIVER = 2;
static constexpr Uint32 MAX_SIGNALS_BEFORE_FLUSH_OTHER = 20;

static constexpr Uint32 MAX_LOCAL_BUFFER_USAGE = 8140;

// #define NDB_MT_LOCK_TO_CPU

static Uint32 glob_num_threads = 0;
static Uint32 glob_num_tc_threads = 1;
static Uint32 first_receiver_thread_no = 0;
static Uint32 max_send_delay = 0;
static Uint32 glob_ndbfs_thr_no = 0;
static Uint32 glob_wakeup_latency = 25;
static Uint32 glob_num_job_buffers_per_thread = 0;
static Uint32 glob_num_writers_per_job_buffers = 0;
static bool glob_use_write_lock_mutex = false;
/**
 * Ensure that the above variables that are read-only after startup are
 * not sharing CPU cache line with anything else that is updated.
 */
alignas(NDB_CL) static Uint32 glob_unused[NDB_CL / 4];

#define NO_SEND_THREAD (MAX_BLOCK_THREADS + MAX_NDBMT_SEND_THREADS + 1)

/* max signal is 32 words, 7 for signal header and 25 datawords */
#define MAX_SIGNAL_SIZE 32
#define MIN_SIGNALS_PER_PAGE \
  ((thr_job_buffer::SIZE / MAX_SIGNAL_SIZE) - MAX_SIGNALS_BEFORE_FLUSH_OTHER)

#if defined(HAVE_LINUX_FUTEX) && defined(NDB_HAVE_XCNG)
#define USE_FUTEX
#endif

#ifdef USE_FUTEX
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#define FUTEX_WAIT 0
#define FUTEX_WAKE 1
#define FUTEX_FD 2
#define FUTEX_REQUEUE 3
#define FUTEX_CMP_REQUEUE 4
#define FUTEX_WAKE_OP 5

static inline int futex_wait(volatile unsigned *addr, int val,
                             const struct timespec *timeout) {
  return syscall(SYS_futex, addr, FUTEX_WAIT, val, timeout, 0, 0) == 0 ? 0
                                                                       : errno;
}

static inline int futex_wake(volatile unsigned *addr) {
  return syscall(SYS_futex, addr, FUTEX_WAKE, 1, 0, 0, 0) == 0 ? 0 : errno;
}

static inline int futex_wake_all(volatile unsigned *addr) {
  return syscall(SYS_futex, addr, FUTEX_WAKE, INT_MAX, 0, 0, 0) == 0 ? 0
                                                                     : errno;
}

struct alignas(NDB_CL) thr_wait {
  volatile unsigned m_futex_state;
  enum { FS_RUNNING = 0, FS_SLEEPING = 1 };
  thr_wait() {
    assert((sizeof(*this) % NDB_CL) == 0);  // Maintain any CL-alignment
    xcng(&m_futex_state, FS_RUNNING);
  }
  void init() {}
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
template <typename T>
static inline bool yield(struct thr_wait *wait, const Uint32 nsec,
                         bool (*check_callback)(T *), T *check_arg) {
  volatile unsigned *val = &wait->m_futex_state;
  xcng(val, thr_wait::FS_SLEEPING);

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
  if (waited) {
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

static inline int wakeup(struct thr_wait *wait) {
  volatile unsigned *val = &wait->m_futex_state;
  /**
   * We must ensure that any state update (new data in buffers...) are visible
   * to the other thread before we can look at the sleep state of that other
   * thread.
   */
  if (xcng(val, thr_wait::FS_RUNNING) == thr_wait::FS_SLEEPING) {
    return futex_wake(val);
  }
  return 0;
}

static inline int wakeup_all(struct thr_wait *wait) {
  volatile unsigned *val = &wait->m_futex_state;
  /**
   * We must ensure that any state update (new data in buffers...) are visible
   * to the other thread before we can look at the sleep state of that other
   * thread.
   */
  if (xcng(val, thr_wait::FS_RUNNING) == thr_wait::FS_SLEEPING) {
    return futex_wake_all(val);
  }
  return 0;
}

static inline int try_wakeup(struct thr_wait *wait) { return wakeup(wait); }
#else

struct alignas(NDB_CL) thr_wait {
  NdbMutex *m_mutex;
  NdbCondition *m_cond;
  bool m_need_wakeup;
  thr_wait() : m_mutex(0), m_cond(0), m_need_wakeup(false) {
    assert((sizeof(*this) % NDB_CL) == 0);  // Maintain any CL-alignment
  }

  void init() {
    m_mutex = NdbMutex_Create();
    m_cond = NdbCondition_Create();
  }
};

template <typename T>
static inline bool yield(struct thr_wait *wait, const Uint32 nsec,
                         bool (*check_callback)(T *), T *check_arg) {
  struct timespec end;
  NdbCondition_ComputeAbsTime(&end, (nsec >= 1000000) ? nsec / 1000000 : 1);
  NdbMutex_Lock(wait->m_mutex);

  /**
   * Any spurious wakeups are handled by simply running the scheduler code.
   * The check_callback is needed to ensure that we don't miss wakeups. But
   * that a spurious wakeups causes one loop in the scheduler compared to
   * the cost of always checking through buffers to check condition.
   */
  Uint32 waits = 0;
  if ((*check_callback)(check_arg)) {
    wait->m_need_wakeup = true;
    waits++;
    if (NdbCondition_WaitTimeoutAbs(wait->m_cond, wait->m_mutex, &end) ==
        ETIMEDOUT) {
      wait->m_need_wakeup = false;
    }
  }
  NdbMutex_Unlock(wait->m_mutex);
  return (waits > 0);
}

static inline int try_wakeup(struct thr_wait *wait) {
  int success = NdbMutex_Trylock(wait->m_mutex);
  if (success != 0) return success;

  // We should avoid signaling when not waiting for wakeup
  if (wait->m_need_wakeup) {
    wait->m_need_wakeup = false;
    NdbCondition_Signal(wait->m_cond);
  }
  NdbMutex_Unlock(wait->m_mutex);
  return 0;
}

static inline int wakeup(struct thr_wait *wait) {
  NdbMutex_Lock(wait->m_mutex);
  // We should avoid signaling when not waiting for wakeup
  if (wait->m_need_wakeup) {
    wait->m_need_wakeup = false;
    NdbCondition_Signal(wait->m_cond);
  }
  NdbMutex_Unlock(wait->m_mutex);
  return 0;
}

static inline int wakeup_all(struct thr_wait *wait) {
  NdbMutex_Lock(wait->m_mutex);
  // We should avoid signaling when not waiting for wakeup
  if (wait->m_need_wakeup) {
    wait->m_need_wakeup = false;
    NdbCondition_Broadcast(wait->m_cond);
  }
  NdbMutex_Unlock(wait->m_mutex);
  return 0;
}

#endif

#define JAM_FILE_ID 236

/**
 * thr_safe_pool
 */
template <typename T>
struct alignas(NDB_CL) thr_safe_pool {
  struct alignas(NDB_CL) thr_safe_pool_lock {
    struct thr_spin_lock m_lock;

    T *m_free_list;
    Uint32 m_cnt;
    bool m_used_all_reserved;
  };
  thr_safe_pool_lock m_safe_lock[MAX_NDBMT_SEND_THREADS];
  struct thr_spin_lock m_alloc_lock;
  Uint32 m_allocated;

  thr_safe_pool(const char *name) {
    m_allocated = 0;
    for (Uint32 i = 0; i < MAX_NDBMT_SEND_THREADS; i++) {
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
    assert((sizeof(*this) % NDB_CL) == 0);  // Maintain any CL-alignment
  }

  T *seize(Ndbd_mem_manager *mm, Uint32 rg) {
    /* This function is used by job buffer allocation. */
    Uint32 instance_no = 0;
    thr_safe_pool_lock *lock_ptr = &m_safe_lock[instance_no];
    T *ret = 0;
    lock(&lock_ptr->m_lock);
    if (lock_ptr->m_free_list) {
      assert(lock_ptr->m_cnt);
      lock_ptr->m_cnt--;
      ret = lock_ptr->m_free_list;
      lock_ptr->m_free_list = ret->m_next;
      unlock(&lock_ptr->m_lock);
    } else {
      unlock(&lock_ptr->m_lock);
      Uint32 dummy;
      ret = reinterpret_cast<T *>(
          mm->alloc_page(rg, &dummy, Ndbd_mem_manager::NDB_ZONE_LE_32));
      // ToDo: How to deal with failed allocation?!?
      // I think in this case we need to start grabbing buffers kept for signal
      // trace.
      if (ret != NULL) {
        lock(&m_alloc_lock);
        m_allocated++;
        unlock(&m_alloc_lock);
      }
    }
    return ret;
  }

#define RG_REQUIRED_PAGES 96
  bool found_instance(Uint32 instance, Uint32 &max_found, Uint32 &instance_no) {
    thr_safe_pool_lock *lock_ptr = &m_safe_lock[instance];
    Uint32 cnt = lock_ptr->m_cnt;
    if (cnt > RG_REQUIRED_PAGES) {
      return true;
    }
    if (cnt > max_found) {
      instance_no = instance;
      max_found = cnt;
    }
    return false;
  }

  Uint32 get_least_empty_instance(Uint32 skip_instance) {
    /**
     * Read without mutex protection since it is ok to not get a perfect
     * result.
     */
    Uint32 instance_no_found = 0;
    Uint32 cnt_found = 0;
    for (Uint32 i = skip_instance + 1; i < globalData.ndbMtSendThreads; i++) {
      if (found_instance(i, cnt_found, instance_no_found)) return i;
    }
    for (Uint32 i = 0; i < skip_instance; i++) {
      if (found_instance(i, cnt_found, instance_no_found)) return i;
    }
    return instance_no_found;
  }

  Uint32 seize_list(Ndbd_mem_manager *mm, Uint32 rg, Uint32 requested, T **head,
                    T **tail, Uint32 instance_no, bool first_call) {
    /* This function is used by send buffer allocation. */
    assert(instance_no < MAX_NDBMT_SEND_THREADS);
    thr_safe_pool_lock *lock_ptr = &m_safe_lock[instance_no];
    lock(&lock_ptr->m_lock);
    if (unlikely(lock_ptr->m_cnt == 0)) {
      unlock(&lock_ptr->m_lock);
      if (likely(first_call)) {
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
        for (Uint32 step = 0; step < 2; step++) {
          Uint32 dummy;
          bool locked = false;
          bool use_max_part = (globalData.ndbMtSendThreads < 2 || step == 1);
          if (use_max_part || !lock_ptr->m_used_all_reserved) {
            T *ret = reinterpret_cast<T *>(
                mm->alloc_page(rg, &dummy, Ndbd_mem_manager::NDB_ZONE_LE_32,
                               locked, use_max_part));
            if (ret != 0) {
              ret->m_next = 0;
              *head = *tail = ret;
              if (ret != NULL) {
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
          if (globalData.ndbMtSendThreads < 2) {
            return 0;
          }
          if (step == 0) {
            filled_instance_no = get_least_empty_instance(instance_no);
            Uint32 returned = seize_list(mm, rg, requested, head, tail,
                                         filled_instance_no, false);
            if (likely(returned > 0)) {
              return returned;
            }
          } else {
            for (Uint32 i = 0; i < globalData.ndbMtSendThreads; i++) {
              if (i != instance_no && i != filled_instance_no) {
                Uint32 returned =
                    seize_list(mm, rg, requested, head, tail, i, false);
                if (returned != 0) {
                  g_eventLogger->info("seize_list: returns %u from instance %u",
                                      returned, i);
                  return returned;
                }
              }
            }
          }
        }
        return 0;
      } else {
        return 0;
      }
    } else {
      if (lock_ptr->m_cnt < requested) requested = lock_ptr->m_cnt;

      T *first = lock_ptr->m_free_list;
      T *last = first;
      for (Uint32 i = 1; i < requested; i++) {
        last = last->m_next;
      }
      lock_ptr->m_cnt -= requested;
      lock_ptr->m_free_list = last->m_next;
      unlock(&lock_ptr->m_lock);
      last->m_next = 0;
      *head = first;
      *tail = last;
      return requested;
    }
  }

  void release(Ndbd_mem_manager *mm, Uint32 rg, T *t) {
    /* This function is used by job buffer release. */
    Uint32 instance_no = 0;
    thr_safe_pool_lock *lock_ptr = &m_safe_lock[instance_no];
    lock(&lock_ptr->m_lock);
    t->m_next = lock_ptr->m_free_list;
    lock_ptr->m_free_list = t;
    lock_ptr->m_cnt++;
    unlock(&lock_ptr->m_lock);
  }

  void release_list(Ndbd_mem_manager *mm, Uint32 rg, T *head, T *tail,
                    Uint32 cnt, Uint32 instance_no) {
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
template <typename T>
class thread_local_pool {
 public:
  thread_local_pool(thr_safe_pool<T> *global_pool, unsigned max_free,
                    unsigned alloc_size = 1)
      : m_max_free(max_free),
        m_alloc_size(alloc_size),
        m_free(0),
        m_freelist(0),
        m_global_pool(global_pool) {}

  T *seize(Ndbd_mem_manager *mm, Uint32 rg, Uint32 instance_no) {
    T *tmp = m_freelist;
    if (tmp == nullptr) {
      T *tail;
      m_free = m_global_pool->seize_list(mm, rg, m_alloc_size, &tmp, &tail,
                                         instance_no, true);
    }
    if (tmp) {
      m_freelist = tmp->m_next;
      assert(m_free > 0);
      m_free--;
    }

    validate();
    return tmp;
  }

  /**
   * Release to local pool even if it gets "too" full
   *   (wrt to m_max_free)
   */
  void release_local(T *t) {
    m_free++;
    t->m_next = m_freelist;
    m_freelist = t;

    validate();
  }

  void validate() const {
#ifdef VM_TRACE
    Uint32 cnt = 0;
    T *t = m_freelist;
    while (t) {
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
  void release_global(Ndbd_mem_manager *mm, Uint32 rg, Uint32 instance_no) {
    validate();
    unsigned free = m_free;
    Uint32 maxfree = m_max_free;
    assert(maxfree > 0);

    if (unlikely(free > maxfree)) {
      T *head = m_freelist;
      T *tail = m_freelist;
      unsigned cnt = 1;
      free--;

      /**
       * Reduce contention on global_pool locks:
       * Releases usually called from a thread doing send. It is likely to soon
       * send and release more buffers. -> Release to 66% of max_free now, such
       * that we do not hit max_free immediately in next release.
       * NOTE: Need to be >= THR_SEND_BUFFER_PRE_ALLOC as well -> it is.
       */
      maxfree = (m_max_free * 2) / 3;
      while (free > maxfree) {
        cnt++;
        free--;
        tail = tail->m_next;
      }

      assert(free == maxfree);

      m_free = free;
      m_freelist = tail->m_next;
      m_global_pool->release_list(mm, rg, head, tail, cnt, instance_no);
    }
    validate();
  }

  void release_all(Ndbd_mem_manager *mm, Uint32 rg, Uint32 instance_no) {
    validate();
    T *head = m_freelist;
    T *tail = m_freelist;
    if (tail) {
      unsigned cnt = 1;
      while (tail->m_next != 0) {
        cnt++;
        tail = tail->m_next;
      }
      m_global_pool->release_list(mm, rg, head, tail, cnt, instance_no);
      m_free = 0;
      m_freelist = 0;
    }
    validate();
  }

  /**
   * release everything if more than m_max_free
   *   else do nothing
   */
  void release_chunk(Ndbd_mem_manager *mm, Uint32 rg, Uint32 instance_no) {
    if (m_free > m_max_free) {
      release_all(mm, rg, instance_no);
    }
  }

  /**
   * prealloc up to <em>cnt</em> pages into this pool
   */
  bool fill(Ndbd_mem_manager *mm, Uint32 rg, Uint32 cnt, Uint32 instance_no) {
    if (m_free >= cnt) {
      return true;
    }

    T *head, *tail;
    Uint32 allocated = m_global_pool->seize_list(mm, rg, m_alloc_size, &head,
                                                 &tail, instance_no, true);
    if (allocated) {
      tail->m_next = m_freelist;
      m_freelist = head;
      m_free += allocated;
      return m_free >= cnt;
    }

    return false;
  }

  void set_pool(thr_safe_pool<T> *pool) { m_global_pool = pool; }

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
struct thr_job_buffer  // 32k
{
  thr_job_buffer()  // Construct an empty thr_job_buffer
      : m_len(0), m_prioa(false) {}

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

    thr_job_buffer *m_next;  // For free-list
  };
};

// The 'empty_job_buffer' is a sentinel for a job_queue possibly never used.
static thr_job_buffer empty_job_buffer;

/**
 * thr_job_queue is shared between a single consumer / multiple producers.
 *
 * -> Updating any write_* indexes need the write lock.
 * -> The single reader depends on memory barriers for the read_* indexes,
 *
 * For the reader side snapshots of the job_queue 'state' is uploaded into
 * thr_jb_read_state from where the signals are executed. This avoid some
 * of the overhead caused by writers constantly invalidating cache lines
 * when new signals are added to the queue.
 */
struct alignas(NDB_CL) thr_job_queue {
  /**
   * Size of A and B buffer must be in the form 2^n since we
   * use & (size - 1) for modulo which only works when it is
   * on the form 2^n.
   */
  static constexpr unsigned SIZE = 32;

  /**
   * There is a SAFETY limit on free buffers we never allocate,
   * but may allow these to be implicitly used as a last resort
   * when job scheduler is really stuck. ('sleeploop 10')
   *
   * Note that 'free' calculations are with the SAFETY limit
   * subtracted, such that max-free is 'SIZE - SAFETY' (30).
   *
   * In addition there is an additional 'safety' in that all partially
   * filled JB-pages are counted as completely used when calculating 'free'.
   *
   * There is also an implicit safety limit in allowing 'flush' from
   * m_local_buffer to fail, and execution to continue until even that
   * buffer is full. In such cases we are in a CRITICAL JB-state.
   */
  static constexpr unsigned SAFETY = 2;

  /**
   * Some more free buffers on top of SAFETY are RESERVED. Normally the JB's
   * are regarded being 'full' when reaching the RESERVED limit. However, they
   * are allowed to be used to avoid or resolve circular wait-locks between
   * threads waiting for buffers to become available. In such cases these are
   * allocated as 'extra_signals' allowed to execute.
   */
  static constexpr unsigned RESERVED = 4;  // In addition to 'SAFETY'

  /**
   * We start being CONGESTED a bit before reaching the RESERVED limit.
   * We will then start reducing quota of signals run_job_buffers may
   * execute in each round, giving a tighter control of job-buffer overruns.
   *
   * Note that there will be some execution overhead when running in a
   * congested state (Smaller JB quotas, tighter checking of JB-state,
   * optionally requiring the write_lock to be taken.)
   */
  static constexpr unsigned CONGESTED = RESERVED + 4;  // 4+4

  /**
   * As there are multiple writers, 'm_write_lock' has to be set before
   * updating the m_write_index.
   */
  struct thr_spin_lock m_write_lock;

  /**
   * m_read_index is written only by the executing thread. It is read at times
   * to check for congestion and out of job buffer situations. This variable
   * is thus placed in its own cache line to avoid extra CPU cache misses when
   * m_write_index is updated.
   *
   * m_write_index is updated by different CPUs every time we have completed
   * writing a job buffer page. This is placed in its own cache line to avoid
   * that its update affects any other activity.
   *
   * m_cached_read_index is kept in the same cache line as m_write_index and
   * is maintained by all senders together while holding the above
   * m_write_lock mutex. Holding this mutex means that we have exclusive
   * access to this cache line and thus accessing this variable is almost
   * for free.
   *
   * m_pending_signals specifies how many signals have been put into the
   * job buffer without a wakeup being issued. We keep it in the same
   * cache line as m_write_index.
   *
   * Job buffer queue is implemented as a wrapping FIFO list:
   */
  alignas(NDB_CL) unsigned m_read_index;
  alignas(NDB_CL) unsigned m_write_index;
  unsigned m_cached_read_index;

  /**
   * Producer thread-local shortcuts to current write pos:
   * NOT under memory barrier control - Can't be consistently read by consumer!
   * (When adding a new write_buffer, the consumer could see either the 'end'
   * of the previous, the new one, or a mix.)
   */
  struct thr_job_buffer *m_current_write_buffer;
  unsigned m_current_write_buffer_len;

  /**
   * Number of signals inserted by all producers into this 'queue' since
   * last wakeup() of the consumer. When MAX_SIGNALS_BEFORE_WAKEUP
   * limit is reached, the current thread do the wakeup, also on behalf
   * of the other threads sending on the same 'queue'. After such a
   * 'wakeup', the m_pending_signals count is cleared.
   *
   * In addition each thread keep track of which nodes it has sent to.
   * Before it can 'yield', it has to wakeup all such threads potentially
   * having pending signals, even if MAX_SIGNALS_BEFORE_WAKEUP had
   * not been reached yet. (thr_data::m_wake_threads_mask)
   */
  unsigned m_pending_signals;

  /**
   * Job buffer size is const: Max number of JB-pages allowed
   */
  static constexpr unsigned m_size = SIZE;  // Job queue size (SIZE)

  /**
   * Ensure that the busy cache line isn't shared with job buffers.
   */
  alignas(NDB_CL) struct thr_job_buffer *m_buffers[SIZE];
};

/**
 * Calculate remaining free slots in the job_buffer queue.
 * The SAFETY limit is subtracted from the 'free'.
 *
 * Note that calc free consider a JB-page as non-free as soon as it
 * has been allocated to the JB-queue.
 *  -> A partial filled 'write-page', or even empty page, is non-free.
 *  -> A partial consumed 'read-page is non-free, until fully consumed
 *     and released back to the page pool
 *
 * This also implies that max- 'fifo_free' is 'SIZE-SAFETY-1'.
 * (We do not care to handle the special initial case where
 *  there is just an empty_job_buffer/nullptr in the JB-queue)
 */
static inline unsigned calc_fifo_free(Uint32 ri, Uint32 wi, Uint32 sz) {
  // Note: The 'wi' 'write-in-progress' page is not 'free, thus 'wi+1'
  const unsigned free = (ri > wi) ? ri - (wi + 1) : (sz - (wi + 1)) + ri;
  if (likely(free >= thr_job_queue::SAFETY))
    return free - thr_job_queue::SAFETY;
  else
    return 0;
}

/**
 * Identify type of thread.
 * Based on assumption that threads are allocated in the order:
 *  main, ldm, query, recover, tc, recv, send
 */
static bool is_main_thread(unsigned thr_no) {
  if (globalData.ndbMtMainThreads > 0)
    return (thr_no < globalData.ndbMtMainThreads);
  unsigned first_recv_thread =
      globalData.ndbMtLqhThreads + globalData.ndbMtQueryThreads +
      globalData.ndbMtRecoverThreads + globalData.ndbMtTcThreads;
  return (thr_no == first_recv_thread);
}

static bool is_ldm_thread(unsigned thr_no) {
  if (glob_num_threads == 1) return (thr_no == 0);
  return thr_no >= globalData.ndbMtMainThreads &&
         thr_no < globalData.ndbMtMainThreads + globalData.ndbMtLqhThreads;
}

static bool is_query_thread(unsigned thr_no) {
  Uint32 num_query_threads = globalData.ndbMtQueryThreads;
  unsigned query_base =
      globalData.ndbMtMainThreads + globalData.ndbMtLqhThreads;
  return thr_no >= query_base && thr_no < query_base + num_query_threads;
}

static bool is_recover_thread(unsigned thr_no) {
  Uint32 num_recover_threads = globalData.ndbMtRecoverThreads;
  unsigned query_base = globalData.ndbMtMainThreads +
                        globalData.ndbMtLqhThreads +
                        globalData.ndbMtQueryThreads;
  return thr_no >= query_base && thr_no < query_base + num_recover_threads;
}

static bool is_tc_thread(unsigned thr_no) {
  if (globalData.ndbMtTcThreads == 0) return false;
  Uint32 num_query_threads =
      globalData.ndbMtQueryThreads + globalData.ndbMtRecoverThreads;
  unsigned tc_base = globalData.ndbMtMainThreads + num_query_threads +
                     globalData.ndbMtLqhThreads;
  return thr_no >= tc_base && thr_no < tc_base + globalData.ndbMtTcThreads;
}

static bool is_recv_thread(unsigned thr_no) {
  Uint32 num_query_threads =
      globalData.ndbMtQueryThreads + globalData.ndbMtRecoverThreads;
  unsigned recv_base = globalData.ndbMtMainThreads +
                       globalData.ndbMtLqhThreads + num_query_threads +
                       globalData.ndbMtTcThreads;
  return thr_no >= recv_base &&
         thr_no < recv_base + globalData.ndbMtReceiveThreads;
}

/**
 * thr_jb_read_state is tightly associated with thr_job_queue.
 *
 * It hold a snapshot of the thr_job_queue state uploaded into
 * the single consumer thread local storage with read_all_jbb_state()
 * or read_jba_state(). It allows the reader to consume these signals
 * without having to interact with thread-shared memory in the thr_job_queue.
 *
 * More signals could have been added to the thr_job_queue while consuming
 * the signals seen in thr_jb_read_state, which will not be available until
 * next read_*_state().
 *
 * This structure is also used when dumping signal traces, to dump executed
 * signals from the buffer(s) currently being processed.
 */
struct thr_jb_read_state {
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
  Uint32 m_read_end;  // End within current thr_job_buffer. (*m_read_buffer)

  Uint32 m_write_index;  // Last available thr_job_buffer.

  bool is_empty() const {
    assert(m_read_index != m_write_index || m_read_pos <= m_read_end);
    return (m_read_index == m_write_index) && (m_read_pos >= m_read_end);
  }
};

/**
 * time-queue
 */
struct thr_tq {
  static const unsigned ZQ_SIZE = 256;
  static const unsigned SQ_SIZE = 512;
  static const unsigned LQ_SIZE = 512;
  static const unsigned PAGES =
      (MAX_SIGNAL_SIZE * (ZQ_SIZE + SQ_SIZE + LQ_SIZE)) / 8192;

  Uint32 *m_delayed_signals[PAGES];
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
 * THR_SEND_BUFFER_PRE_ALLOC is the amount of 32k pages that are
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
struct thr_send_page {
  static constexpr Uint32 PGSIZE = 32 * 1024;
#if SIZEOF_CHARP == 4
  static constexpr Uint32 HEADER_SIZE = 8;
#else
  static constexpr Uint32 HEADER_SIZE = 12;
#endif

  static constexpr Uint32 max_bytes() {
    return PGSIZE - offsetof(thr_send_page, m_data);
  }

  /* Next page */
  thr_send_page *m_next;

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
struct thr_send_buffer {
  thr_send_page *m_first_page;
  thr_send_page *m_last_page;
};

/**
 * a ring buffer with linked list of thr_send_page
 */
struct thr_send_queue {
  unsigned m_write_index;
#if SIZEOF_CHARP == 8
  unsigned m_unused;
  static constexpr unsigned SIZE = 7;
#else
  static constexpr unsigned SIZE = 15;
#endif
  thr_send_page *m_buffers[SIZE];
};

struct thr_first_signal {
  Uint32 m_num_signals;
  Uint32 m_first_signal;
  Uint32 m_last_signal;
};

struct thr_send_thread_instance;

struct alignas(NDB_CL) thr_data {
  thr_data()
      : m_signal_id_counter(0),
        m_send_buffer_pool(0, THR_SEND_BUFFER_MAX_FREE,
                           THR_SEND_BUFFER_ALLOC_SIZE)
#if defined(USE_INIT_GLOBAL_VARIABLES)
        ,
        m_global_variables_ptr_instances(0),
        m_global_variables_uint32_ptr_instances(0),
        m_global_variables_uint32_instances(0),
        m_global_variables_enabled(true)
#endif
  {

    // Check cacheline alignment
    assert((((UintPtr)this) % NDB_CL) == 0);
    assert((((UintPtr)&m_waiter) % NDB_CL) == 0);
    assert((((UintPtr)&m_jba) % NDB_CL) == 0);
    for (uint i = 0; i < NUM_JOB_BUFFERS_PER_THREAD; i++) {
      assert((((UintPtr)&m_jbb[i]) % NDB_CL) == 0);
    }
  }

  /**
   * We start with the data structures that are shared globally to
   * ensure that they get the proper cache line alignment
   */
  thr_wait m_waiter; /* Cacheline aligned*/

  /**
   * When in congestion we can only be woken up by timeout and by
   * congestion being removed.
   */
  alignas(NDB_CL) thr_wait m_congestion_waiter;

  /*
   * Prio A signal incoming queue. This area is used from many threads
   * protected by a lock. Thus it is also important to protect
   * surrounding thread-local variables from CPU cache line sharing
   * with this part.
   */
  alignas(NDB_CL) struct thr_job_queue m_jba;

  /*
   * These are the thread input queues, where other threads deliver signals
   * into.
   * These cache lines are going to be updated by many different CPU's
   * all the time whereas other neighbour variables are thread-local variables.
   * Avoid false cacheline sharing by require an alignment.
   */
  alignas(NDB_CL) struct thr_job_queue m_jbb[NUM_JOB_BUFFERS_PER_THREAD];

  /**
   * The remainder of the variables in thr_data are thread-local,
   * meaning that they are always updated by the thread that owns those
   * data structures and thus those variables aren't shared with other
   * CPUs.
   */

  alignas(NDB_CL) unsigned m_thr_no;

  /**
   * JBB resume point: We might return from run_job_buffers without executing
   * signals from all the JBB buffers.
   * This variable keeps track of where to resume execution from in next 'run'.
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
  alignas(NDB_CL) unsigned m_max_signals_per_jb;

  /**
   * Extra JBB signal execute quota allowed to be used to
   * drain (almost) full in-buffers. Reserved for usage where
   * we are about to end up in a circular wait-lock between
   * threads where none of them will be able to proceed.
   * Allocated from the RESERVED signal quota.
   */
  unsigned m_total_extra_signals;

  /**
   * Extra signals allowed to be execute from each specific JBB.
   * Allocated to each job_buffer from the m_total_extra_signals.
   * Only set up / to be used, if thread is JBB congested.
   */
  unsigned m_extra_signals[NUM_JOB_BUFFERS_PER_THREAD];

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
   * Flag indicating that we have sent a local Prio A signal. Used to know
   * if to scan for more prio A signals after executing those signals.
   * This is used to ensure that if we execute at prio A level and send a
   * prio A signal it will be immediately executed (or at least before any
   * prio B signal).
   */
  bool m_sent_local_prioa_signal;

  NDB_TICKS m_jbb_estimate_start;
  Uint32 m_jbb_execution_steps;
  Uint32 m_jbb_accumulated_queue_size;
  Uint32 m_load_indicator;
  Uint64 m_jbb_estimate_signal_count_start;

  /**
   * The following cache line is only written by the local thread.
   * But it is read by all receive threads and TC threads frequently,
   * so ensure that only updates of this cache line effects the
   * other threads.
   */
  alignas(NDB_CL) Uint32 m_jbb_estimated_queue_size_in_words;
  Uint32 m_ldm_multiplier;

  alignas(NDB_CL) bool m_jbb_estimate_next_set;
#ifdef DEBUG_SCHED_STATS
  Uint64 m_jbb_estimated_queue_stats[10];
  Uint64 m_jbb_total_words;
#endif
  bool m_read_jbb_state_consumed;
  bool m_cpu_percentage_changed;
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

  /**
   * Keep track of signals stored in local buffer before being copied to real
   * job buffers. We keep a number of linked lists for this.
   */
  struct thr_first_signal m_first_local[NDB_MAX_BLOCK_THREADS];

  /**
   * A local job buffer used to buffer a number of signals. This ensures that
   * we don't have to grab the job buffer mutex of the receiver for each and
   * every signal sent. In particular communication from LDM threads to TC
   * threads tend to send a lot of signals to a single thread and also the
   * receive thread does a fair bit of sending to other threads and can
   * benefit from this scheme.
   */
  struct thr_job_buffer *m_local_buffer;

  /*
   * In m_next_buffer we keep a free buffer at all times, so that when
   * we hold the lock and find we need a new buffer, we can use this and this
   * way defer allocation to after releasing the lock.
   */
  struct thr_job_buffer *m_next_buffer;

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

  /* Thread-local read state of prio B buffer(s). */
  struct thr_jb_read_state m_jbb_read_state[NUM_JOB_BUFFERS_PER_THREAD];

  /* Bitmask of thr_jb_read_state[] having data to read. */
  Bitmask<(NUM_JOB_BUFFERS_PER_THREAD + 31) / 32> m_jbb_read_mask;

  /**
   * Threads might need a wakeup() to be signalled before it can yield.
   * They are registered in the Bitmask m_wake_threads_mask
   */
  BlockThreadBitmask m_wake_threads_mask;

  /**
   * We have signals buffered for each of the threads in the thread-local
   * m_local_signals_mask. When buffer is full, or when the flush level is
   * reached, we will flush those signals to the thread-shared m_buffer[]
   */
  BlockThreadBitmask m_local_signals_mask;

  /**
   * Set of destination threads where the JB's are CONGESTED *and*
   * contributed to a reduction in 'm_max_signals_per_jb' to be executed.
   */
  BlockThreadBitmask m_congested_threads_mask;

  /* Jam buffers for making trace files at crashes. */
  EmulatedJamBuffer m_jam;
  /* Watchdog counter for this thread. */
  Uint32 m_watchdog_counter;
  /* Latest executed signal id assigned in this thread */
  Uint32 m_signal_id_counter;

  struct thr_send_thread_instance *m_send_instance;
  Uint32 m_send_instance_no;

  /* Signal delivery statistics. */
  struct {
    Uint64 m_loop_cnt;
    Uint64 m_exec_cnt;
    Uint64 m_wait_cnt;
    Uint64 m_prioa_count;
    Uint64 m_prioa_size;
    Uint64 m_priob_count;
    Uint64 m_priob_size;
  } m_stat;

  struct {
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
  Bitmask<(MAX_NTRANSPORTERS + 31) / 32> m_pending_send_mask;

  /* pool for send buffers */
  class thread_local_pool<thr_send_page> m_send_buffer_pool;

  /* Send buffer for this thread, these are not touched by any other thread */
  struct thr_send_buffer m_send_buffers[MAX_NTRANSPORTERS];

  /* Block instances (main and worker) handled by this thread. */
  /* Used for sendpacked (send-at-job-buffer-end). */
  Uint32 m_instance_count;
  BlockNumber m_instance_list[MAX_INSTANCES_PER_THREAD];

  /* Register of blocks needing SEND_PACKED to be called */
  struct SendPacked {
    struct PackBlock {
      SimulatedBlock::ExecFunction m_func;  // The execSEND_PACKED func
      SimulatedBlock *m_block;              // Block to execute func in

      PackBlock() : m_func(NULL), m_block() {}
      PackBlock(SimulatedBlock::ExecFunction f, SimulatedBlock *b)
          : m_func(f), m_block(b) {}
    };

    SendPacked() : m_instances(0), m_ndbfs(-1) {}

    /* Register block for needing SEND_PACKED to be called */
    void insert(SimulatedBlock *block) {
      const SimulatedBlock::ExecFunction func =
          block->getExecuteFunction(GSN_SEND_PACKED);
      if (func != NULL && func != &SimulatedBlock::execSEND_PACKED) {
        // Might be a NDBFS reply handler, pick that up.
        if (blockToMain(block->number()) == NDBFS) {
          m_ndbfs = m_instances.size();
        }
        m_instances.push_back(PackBlock(func, block));
      }
    }

    /* Call the registered SEND_PACKED function for all blocks needing it */
    void pack(Signal *signal) const {
      const Uint32 count = m_instances.size();
      const PackBlock *instances = m_instances.getBase();
      for (Uint32 i = 0; i < count; i++) {
        instances[i].m_block->EXECUTE_DIRECT_FN(instances[i].m_func, signal);
      }
    }

    bool check_reply_from_ndbfs(Signal *signal) const {
      /**
       * The manner to check for input from NDBFS file threads misuses
       * the SEND_PACKED signal. For ndbmtd this is intended to be
       * replaced by using signals directly from NDBFS file threads to
       * the issuer of the file request. This is WL#8890.
       */
      assert(m_ndbfs >= 0);
      const PackBlock &instance = m_instances[m_ndbfs];
      instance.m_block->EXECUTE_DIRECT_FN(instance.m_func, signal);
      return (signal->theData[0] == 1);
    }

   private:
    Vector<PackBlock> m_instances;

    /* PackBlock instance used for check_reply_from_ndbfs() */
    Int32 m_ndbfs;
  } m_send_packer;

  SectionSegmentPool::Cache m_sectionPoolCache;

  Uint32 m_cpu;
  my_thread_t m_thr_id;
  NdbThread *m_thread;
  Signal *m_signal;
  Uint32 m_sched_responsiveness;
  Uint32 m_max_signals_before_send;
  Uint32 m_max_signals_before_send_flush;

#ifdef ERROR_INSERT
  bool m_delayed_prepare;
#endif

#if defined(USE_INIT_GLOBAL_VARIABLES)
  Uint32 m_global_variables_ptr_instances;
  Uint32 m_global_variables_uint32_ptr_instances;
  Uint32 m_global_variables_uint32_instances;
  bool m_global_variables_enabled;
  void *m_global_variables_ptrs[1024];
  void *m_global_variables_uint32_ptrs[1024];
  void *m_global_variables_uint32[1024];
#endif
};

struct mt_send_handle : public TransporterSendBufferHandle {
  struct thr_data *m_selfptr;
  mt_send_handle(thr_data *ptr) : m_selfptr(ptr) {}
  ~mt_send_handle() override {}

  Uint32 *getWritePtr(TrpId trp_id, Uint32 len, Uint32 prio, Uint32 max,
                      SendStatus *error) override;
  Uint32 updateWritePtr(TrpId trp_id, Uint32 lenBytes, Uint32 prio) override;
  // void getSendBufferLevel(TrpId, SB_LevelType &level) override;
  bool forceSend(TrpId) override;
};

struct trp_callback : public TransporterCallback {
  trp_callback() {}

  /* Callback interface. */
  void enable_send_buffer(TrpId) override;
  void disable_send_buffer(TrpId) override;

  void reportSendLen(NodeId nodeId, Uint32 count, Uint64 bytes) override;
  void lock_transporter(TrpId) override;
  void unlock_transporter(TrpId) override;
  void lock_send_transporter(TrpId) override;
  void unlock_send_transporter(TrpId) override;
  Uint32 get_bytes_to_send_iovec(TrpId trp_id, struct iovec *dst,
                                 Uint32 max) override;
  Uint32 bytes_sent(TrpId, Uint32 bytes) override;
};

static char *g_thr_repository_mem = NULL;
static struct thr_repository *g_thr_repository = NULL;

struct thr_repository {
  thr_repository()
      : m_section_lock("sectionlock"),
        m_mem_manager_lock("memmanagerlock"),
        m_jb_pool("jobbufferpool"),
        m_sb_pool("sendbufferpool") {
    // Verify assumed cacheline alignment
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
  Ndbd_mem_manager *m_mm;
  unsigned m_thread_count;

  /**
   * Protect m_mm and m_thread_count from CPU cache misses, first
   * part of m_thread (struct thr_data) is globally shared variables.
   * So sharing cache line with these for these read only variables
   * isn't a good idea
   */
  alignas(NDB_CL) struct thr_data m_thread[MAX_BLOCK_THREADS];

  /* The buffers that are to be sent */
  struct send_buffer {
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
    struct thr_spin_lock m_buffer_lock;  // Protect m_buffer
    struct thr_send_buffer m_buffer;

    struct thr_spin_lock m_send_lock;  // Protect m_sending + transporter
    struct thr_send_buffer m_sending;

    /* Size of resp. 'm_buffer' and 'm_sending' buffered data */
    Uint64 m_buffered_size;  // Protected by m_buffer_lock
    Uint64 m_sending_size;   // Protected by m_send_lock

    bool m_enabled;  // Protected by m_send_lock

    /**
     * Flag used to coordinate sending to same remote trp from different
     * threads when there are contention on m_send_lock.
     *
     * If two threads need to send to the same trp at the same time, the
     * second thread, rather than wait for the first to finish, will just
     * set this flag. The first thread will will then take responsibility
     * for sending to this trp when done with its own sending.
     */
    Uint32 m_force_send;  // Check after release of m_send_lock

    /**
     * Which thread is currently holding the m_send_lock
     * This is the thr_no of the thread sending, this can be both a
     * send thread and a block thread. Send thread start their
     * thr_no at glob_num_threads. So it is easy to check this
     * thr_no to see if it is a block thread or a send thread.
     * This variable is used to find the proper place to return
     * the send buffer pages after completing the send.
     */
    Uint32 m_send_thread;  // Protected by m_send_lock

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

struct thr_send_thread_instance {
  thr_send_thread_instance()
      : m_instance_no(0),
        m_watchdog_counter(0),
        m_thr_index(0),
        m_thread(NULL),
        m_waiter_struct(),
        m_send_buffer_pool(0, THR_SEND_BUFFER_MAX_FREE,
                           THR_SEND_BUFFER_ALLOC_SIZE),
        m_exec_time(0),
        m_sleep_time(0),
        m_user_time_os(0),
        m_kernel_time_os(0),
        m_elapsed_time_os(0),
        m_measured_spintime(0),
        m_awake(false),
        m_first_trp(0),
        m_last_trp(0),
        m_next_is_high_prio_trp(false),
        m_more_trps(false),
        m_num_neighbour_trps(0),
        m_neighbour_trp_index(0) {}

  /**
   * Instance number of send thread, this is set at creation of
   * send thread and after that not changed, so no need to protect
   * it when reading it.
   */
  Uint32 m_instance_no;

  /**
   * This variable is registered in the watchdog, it is set by the
   * send thread and reset every now and then by watchdog thread.
   * No special protection is required in setting it.
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
  TrpId m_first_trp;

  /* Last trp in list of trps with data available for sending */
  TrpId m_last_trp;

  /* Which list should I get trp from next time. */
  bool m_next_is_high_prio_trp;

  /* 'true': More trps became available -> Need recheck ::get_trp() */
  bool m_more_trps;

  /**
   * The send thread has a list of transporters to neighbour nodes which
   * are given extra priority when sending. The neighbour list is simply
   * an array of the neighbour trps and we will send on one of these
   * transporters if it is_in_queue() AND 'm_next_is_high_prio_trp'. Else we
   * pick a transporter from the head of the queue (which could also turn out to
   * be a neighbour)
   */
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
  bool data_available() const {
    rmb();
    return (m_more_trps == true);
  }

  bool check_pending_data() { return m_more_trps; }
};

struct thr_send_trps {
  /**
   * 'm_prev' & 'm_next' implements a list of 'send_trps' with PENDING
   * data, not yet assigned to a send thread. 0 means NULL.
   */
  TrpId m_prev;
  TrpId m_next;

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

  /* Send to this trp has caused a Transporter overload */
  Uint16 m_send_overload;

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

class thr_send_threads {
 public:
  /* Create send thread environment */
  thr_send_threads();

  /* Destroy send thread environment and ensure threads are stopped */
  ~thr_send_threads();

  struct thr_send_thread_instance *get_send_thread_instance_by_num(Uint32);
  /**
   * A block thread provides assistance to send thread by executing send
   * to one of the trps.
   */
  bool assist_send_thread(
      Uint32 max_num_trps, Uint32 thr_no, NDB_TICKS now,
      Uint32 &watchdog_counter, struct thr_send_thread_instance *send_instance,
      class thread_local_pool<thr_send_page> &send_buffer_pool);

  /* Send thread method to send to a transporter picked by get_trp */
  bool handle_send_trp(TrpId id, Uint32 &num_trp_sent, Uint32 thr_no,
                       NDB_TICKS &now, Uint32 &watchdog_counter,
                       struct thr_send_thread_instance *send_instance);

  /* A block thread has flushed data for a trp and wants it sent */
  Uint32 alert_send_thread(TrpId trp_id, NDB_TICKS now,
                           struct thr_send_thread_instance *send_instance);

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
  thread_local_pool<thr_send_page> *get_send_buffer_pool(Uint32 thr_no) {
    return &m_send_threads[thr_no - glob_num_threads].m_send_buffer_pool;
  }

  void wake_my_send_thread_if_needed(
      TrpId *trp_id_array, Uint32 count,
      struct thr_send_thread_instance *my_send_instance);
  Uint32 get_send_instance(TrpId trp_id);

 private:
  struct thr_send_thread_instance *get_send_thread_instance_by_trp(TrpId);

  /* Insert a trp in list of trps that has data available to send */
  void insert_trp(TrpId trp_id, struct thr_send_thread_instance *);

  /* Get a trp id in order to send to it */
  TrpId get_trp(Uint32 instance_no, NDB_TICKS now,
                struct thr_send_thread_instance *send_instance);

  /* Is the TrpId inserted in the list of trps */
  bool is_enqueued(TrpId trp_id, struct thr_send_thread_instance *) const;

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
  struct thr_send_thread_instance *get_not_awake_send_thread(
      TrpId trp_id, struct thr_send_thread_instance *send_instance);

  /* Try to lock send_buffer for this trp. */
  static int trylock_send_trp(TrpId trp_id);

  /* Perform the actual send to the trp, release send_buffer lock.
   * Return 'true' if there are still more to be sent to this trp.
   */
  static bool perform_send(TrpId trp_id, Uint32 thr_no, Uint32 &bytes_sent);

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
  void getSendPerformanceTimers(Uint32 send_instance, Uint64 &exec_time,
                                Uint64 &sleep_time, Uint64 &spin_time,
                                Uint64 &user_time_os, Uint64 &kernel_time_os,
                                Uint64 &elapsed_time_os) {
    require(send_instance < globalData.ndbMtSendThreads);
    NdbMutex_Lock(m_send_threads[send_instance].send_thread_mutex);
    exec_time = m_send_threads[send_instance].m_exec_time;
    sleep_time = m_send_threads[send_instance].m_sleep_time;
    spin_time = m_send_threads[send_instance].m_measured_spintime;
    user_time_os = m_send_threads[send_instance].m_user_time_os;
    kernel_time_os = m_send_threads[send_instance].m_kernel_time_os;
    elapsed_time_os = m_send_threads[send_instance].m_elapsed_time_os;
    NdbMutex_Unlock(m_send_threads[send_instance].send_thread_mutex);
  }

  /**
   * The send threads may give higher priorities to send over trps connecting
   * neighbour nodes. Such nodes are in the same node group as ourselves.
   * This means that we are likely to communicate with this trp more heavily
   * than other trps. Also delays in this communication will make the updates
   * take much longer since updates has to traverse this link and the
   * corresponding link back 6 times as part of an updating transaction.
   *
   * Thus for good performance of updates it is essential to prioritise
   * these links a bit.
   *
   * The '*NeighbourNode()' methods below are the interface to set up
   * up such neighbours.
   */
  void startChangeNeighbourNode() {
    for (Uint32 i = 0; i < globalData.ndbMtSendThreads; i++) {
      NdbMutex_Lock(m_send_threads[i].send_thread_mutex);
      for (Uint32 j = 0; j < m_send_threads[i].m_num_neighbour_trps; j++) {
        m_send_threads[i].m_neighbour_trps[j] = 0;
      }
      m_send_threads[i].m_num_neighbour_trps = 0;
    }
  }
  void setNeighbourNode(NodeId nodeId) {
    TrpId trpId[MAX_NODE_GROUP_TRANSPORTERS];
    Uint32 num_ids;
    if (globalData.ndbMtSendThreads == 0) {
      return;
    }
    globalTransporterRegistry.get_trps_for_node(nodeId, &trpId[0], num_ids,
                                                MAX_NODE_GROUP_TRANSPORTERS);
    for (Uint32 index = 0; index < num_ids; index++) {
      const TrpId this_id = trpId[index];
      const Uint32 send_instance = get_send_instance(this_id);
      for (Uint32 i = 0; i < MAX_NEIGHBOURS; i++) {
        require(m_send_threads[send_instance].m_neighbour_trps[i] != this_id);
        if (m_send_threads[send_instance].m_neighbour_trps[i] == 0) {
          DEB_MULTI_TRP(
              ("Neighbour(%u) of node %u is trp %u", i, nodeId, this_id));
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
  void endChangeNeighbourNode() {
    for (Uint32 i = 0; i < globalData.ndbMtSendThreads; i++) {
      m_send_threads[i].m_neighbour_trp_index = 0;
      NdbMutex_Unlock(m_send_threads[i].send_thread_mutex);
    }
  }

  void setNodeOverloadStatus(OverloadStatus new_status) {
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
static char *g_send_threads_mem = NULL;
static thr_send_threads *g_send_threads = NULL;

extern "C" void *mt_send_thread_main(void *thr_arg) {
  struct thr_send_thread_instance *this_send_thread =
      (thr_send_thread_instance *)thr_arg;

  Uint32 instance_no = this_send_thread->m_instance_no;
  g_send_threads->run_send_thread(instance_no);
  return NULL;
}

thr_send_threads::thr_send_threads()
    : m_started_threads(false),
      m_node_overload_status((OverloadStatus)LIGHT_LOAD_CONST) {
  struct thr_repository *rep = g_thr_repository;

  for (Uint32 i = 0; i < NDB_ARRAY_SIZE(m_trp_state); i++) {
    m_trp_state[i].m_prev = 0;
    m_trp_state[i].m_next = 0;
    m_trp_state[i].m_data_available = 0;
    m_trp_state[i].m_send_overload = false;
    m_trp_state[i].m_micros_delayed = 0;
    m_trp_state[i].m_overload_counter = 0;
    NdbTick_Invalidate(&m_trp_state[i].m_inserted_time);
  }
  for (Uint32 i = 0; i < NDB_ARRAY_SIZE(m_send_threads); i++) {
    m_send_threads[i].m_more_trps = false;
    m_send_threads[i].m_first_trp = 0;
    m_send_threads[i].m_last_trp = 0;
    m_send_threads[i].m_next_is_high_prio_trp = false;
    m_send_threads[i].m_num_neighbour_trps = 0;
    m_send_threads[i].m_neighbour_trp_index = 0;
    for (Uint32 j = 0; j < MAX_NEIGHBOURS; j++) {
      m_send_threads[i].m_neighbour_trps[j] = 0;
    }
    m_send_threads[i].m_waiter_struct.init();
    m_send_threads[i].m_instance_no = i;
    m_send_threads[i].m_send_buffer_pool.set_pool(&rep->m_sb_pool);
    m_send_threads[i].send_thread_mutex = NdbMutex_Create();
  }
  memset(&m_send_thread_instance_by_trp[0], 0xFF,
         sizeof(m_send_thread_instance_by_trp));
  m_next_send_thread_instance_by_trp = 0;
  m_num_trps = 0;
}

thr_send_threads::~thr_send_threads() {
  if (!m_started_threads) return;

  for (Uint32 i = 0; i < globalData.ndbMtSendThreads; i++) {
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
void thr_send_threads::assign_trps_to_send_threads() {
  Uint32 num_trps = globalTransporterRegistry.get_num_trps();
  m_num_trps = num_trps;
  /* Transporter instance 0 isn't used */
  m_send_thread_instance_by_trp[0] = Uint16(~0);
  Uint32 send_instance = 0;
  for (Uint32 i = 1; i <= num_trps; i++) {
    m_send_thread_instance_by_trp[i] = send_instance;
    send_instance++;
    if (send_instance == globalData.ndbMtSendThreads) {
      send_instance = 0;
    }
  }
  m_next_send_thread_instance_by_trp = 0;
}

void mt_assign_multi_trps_to_send_threads() {
  DEB_MULTI_TRP(("mt_assign_multi_trps_to_send_threads()"));
  if (g_send_threads) {
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
void thr_send_threads::assign_multi_trps_to_send_threads() {
  DEB_MULTI_TRP(("assign_multi_trps_to_send_threads()"));
  Uint32 new_num_trps = globalTransporterRegistry.get_num_trps();
  Uint32 send_instance = m_next_send_thread_instance_by_trp;
  DEB_MULTI_TRP(
      ("assign_multi_trps_to_send_threads(): new_num_trps = %u", new_num_trps));
  for (Uint32 i = m_num_trps + 1; i <= new_num_trps; i++) {
    m_send_thread_instance_by_trp[i] = send_instance;
    send_instance++;
    if (send_instance == globalData.ndbMtSendThreads) {
      send_instance = 0;
    }
  }
  m_num_trps = new_num_trps;
  m_next_send_thread_instance_by_trp = send_instance;
}

void thr_send_threads::assign_threads_to_assist_send_threads() {
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
  THRConfigApplier &conf = globalEmulatorData.theConfiguration->m_thr_config;
  struct thr_repository *rep = g_thr_repository;
  unsigned int thr_no;
  unsigned next_send_instance = 0;
  for (thr_no = 0; thr_no < glob_num_threads; thr_no++) {
    thr_data *selfptr = &rep->m_thread[thr_no];
    selfptr->m_nosend =
        conf.do_get_nosend(selfptr->m_instance_list, selfptr->m_instance_count);
    if (is_recv_thread(thr_no) || selfptr->m_nosend == 1) {
      selfptr->m_send_instance_no = 0;
      selfptr->m_send_instance = NULL;
      selfptr->m_nosend = 1;
    } else if (is_ldm_thread(thr_no)) {
      selfptr->m_send_instance_no = next_send_instance;
      selfptr->m_send_instance =
          get_send_thread_instance_by_num(next_send_instance);
      next_send_instance++;
      if (next_send_instance == globalData.ndbMtSendThreads) {
        next_send_instance = 0;
      }
    } else {
    }
  }
  for (thr_no = 0; thr_no < glob_num_threads; thr_no++) {
    thr_data *selfptr = &rep->m_thread[thr_no];
    if (is_recv_thread(thr_no) || selfptr->m_nosend == 1 ||
        is_ldm_thread(thr_no)) {
      continue;
    } else {
      selfptr->m_send_instance_no = next_send_instance;
      selfptr->m_send_instance =
          get_send_thread_instance_by_num(next_send_instance);
      next_send_instance++;
      if (next_send_instance == globalData.ndbMtSendThreads) {
        next_send_instance = 0;
      }
    }
  }
}

void thr_send_threads::start_send_threads() {
  for (Uint32 i = 0; i < globalData.ndbMtSendThreads; i++) {
    m_send_threads[i].m_thread = NdbThread_Create(
        mt_send_thread_main, (void **)&m_send_threads[i], 1024 * 1024,
        "send thread",  // ToDo add number
        NDB_THREAD_PRIO_MEAN);
    m_send_threads[i].m_thr_index =
        globalEmulatorData.theConfiguration->addThread(
            m_send_threads[i].m_thread, SendThread);
  }
  m_started_threads = true;
}

struct thr_send_thread_instance *
thr_send_threads::get_send_thread_instance_by_num(Uint32 instance_no) {
  return &m_send_threads[instance_no];
}

Uint32 thr_send_threads::get_send_instance(TrpId trp_id) {
  require(trp_id < MAX_NTRANSPORTERS);
  Uint32 send_thread_instance = m_send_thread_instance_by_trp[trp_id];
  require(send_thread_instance < globalData.ndbMtSendThreads);
  return send_thread_instance;
}

struct thr_send_thread_instance *
thr_send_threads::get_send_thread_instance_by_trp(TrpId trp_id) {
  require(trp_id < MAX_NTRANSPORTERS);
  Uint32 send_thread_instance = m_send_thread_instance_by_trp[trp_id];
  require(send_thread_instance < globalData.ndbMtSendThreads);
  return &m_send_threads[send_thread_instance];
}

/**
 * Called under mutex protection of send_thread_mutex
 */
void thr_send_threads::insert_trp(
    TrpId trp_id, struct thr_send_thread_instance *send_instance) {
  struct thr_send_trps &trp_state = m_trp_state[trp_id];
  assert(trp_state.m_data_available > 0);

  send_instance->m_more_trps = true;
  /* Ensure the lock free ::data_available see 'm_more_trps == true' */
  wmb();

  // Not in list already
  assert(!is_enqueued(trp_id, send_instance));

  // Inserts trp_id last in the TrpId list
  const TrpId first_trp = send_instance->m_first_trp;
  const TrpId last_trp = send_instance->m_last_trp;
  struct thr_send_trps &last_trp_state = m_trp_state[last_trp];
  trp_state.m_prev = 0;
  trp_state.m_next = 0;
  send_instance->m_last_trp = trp_id;

  if (first_trp == 0) {
    // TrpId list was empty
    send_instance->m_first_trp = trp_id;
  } else {
    last_trp_state.m_next = trp_id;
    trp_state.m_prev = last_trp;
  }
}

/**
 * Check whether the specified TrpId is inserted in the list of
 * trps available to be picked up by a (assist-)send-thread.
 *
 * With DEBUG enabled some consistency check of the list
 * structures is also performed.
 *
 * It is a prereq. that the specified send_instance must be
 * the send_thread assigned to handle this TrpId
 *
 * Called under mutex protection of send_thread_mutex
 */
bool thr_send_threads::is_enqueued(
    const TrpId trp_id, struct thr_send_thread_instance *send_instance) const {
#ifndef NDEBUG
  // Verify consistency of TrpId list
  if (send_instance->m_first_trp == 0 || send_instance->m_last_trp == 0) {
    assert(send_instance->m_first_trp == 0);
    assert(send_instance->m_last_trp == 0);
    assert(m_trp_state[trp_id].m_prev == 0);
    assert(m_trp_state[trp_id].m_next == 0);
  }
  if (send_instance->m_last_trp != 0 && m_trp_state[trp_id].m_next != 0) {
    TrpId id = trp_id;
    while (m_trp_state[id].m_next != 0) {
      id = m_trp_state[id].m_next;
    }
    assert(id == send_instance->m_last_trp);
  }
  if (send_instance->m_first_trp != 0 && m_trp_state[trp_id].m_prev != 0) {
    TrpId id = trp_id;
    while (m_trp_state[id].m_prev != 0) {
      id = m_trp_state[id].m_prev;
    }
    assert(id == send_instance->m_first_trp);
  }
#endif

  return send_instance->m_first_trp == trp_id ||
         m_trp_state[trp_id].m_prev != 0;
}

/**
 * Called under mutex protection of send_thread_mutex
 * The timer is taken before grabbing the mutex and can thus be a
 * bit older than now when compared to other times.
 */
void thr_send_threads::set_max_delay(TrpId trp_id, NDB_TICKS now,
                                     Uint32 delay_usec) {
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
void thr_send_threads::set_overload_delay(TrpId trp_id, NDB_TICKS now,
                                          Uint32 delay_usec) {
  struct thr_send_trps &trp_state = m_trp_state[trp_id];
  assert(trp_state.m_data_available > 0);
  trp_state.m_send_overload = true;
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
Uint32 thr_send_threads::check_delay_expired(TrpId trp_id, NDB_TICKS now) {
  struct thr_send_trps &trp_state = m_trp_state[trp_id];
  assert(trp_state.m_data_available > 0);
  Uint64 micros_delayed = Uint64(trp_state.m_micros_delayed);

  if (micros_delayed == 0) return 0;

  Uint64 micros_passed;
  if (now.getUint64() > trp_state.m_inserted_time.getUint64()) {
    micros_passed = NdbTick_Elapsed(trp_state.m_inserted_time, now).microSec();
  } else {
    now = trp_state.m_inserted_time;
    micros_passed = micros_delayed;
  }
  if (micros_passed >= micros_delayed)  // Expired
  {
    trp_state.m_inserted_time = now;
    trp_state.m_micros_delayed = 0;
    trp_state.m_send_overload = false;
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

static Uint64 mt_get_send_buffer_bytes(TrpId trp_id);

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
 * to expire its delay first will be returned. It is then up to
 * the callee to either accept this trp, or reinsert it
 * such that it can be returned and retried later.
 *
 * Called under mutex protection of send_thread_mutex
 */
static constexpr TrpId DELAYED_PREV_NODE_IS_NEIGHBOUR = UINT_MAX16;

TrpId thr_send_threads::get_trp(
    Uint32 instance_no, NDB_TICKS now,
    struct thr_send_thread_instance *send_instance) {
  TrpId next;
  TrpId trp_id;
  bool retry = false;
  TrpId delayed_trp = 0;
  TrpId delayed_prev_trp = 0;
  Uint32 min_wait_usec = UINT_MAX32;
  do {
    if (send_instance->m_next_is_high_prio_trp) {
      Uint32 num_neighbour_trps = send_instance->m_num_neighbour_trps;
      Uint32 neighbour_trp_index = send_instance->m_neighbour_trp_index;
      for (Uint32 i = 0; i < num_neighbour_trps; i++) {
        trp_id = send_instance->m_neighbour_trps[neighbour_trp_index];
        neighbour_trp_index++;
        if (neighbour_trp_index == num_neighbour_trps) neighbour_trp_index = 0;
        send_instance->m_neighbour_trp_index = neighbour_trp_index;

        if (is_enqueued(trp_id, send_instance)) {
          const Uint32 send_delay = check_delay_expired(trp_id, now);
          if (likely(send_delay == 0)) {
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
          if (send_delay < min_wait_usec) {
            min_wait_usec = send_delay;
            delayed_trp = trp_id;
            delayed_prev_trp = DELAYED_PREV_NODE_IS_NEIGHBOUR;
          }
        }
      }
      if (retry) {
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
    } else {
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
    if (!trp_id) {
      if (!retry) {
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
    while (trp_id) {
      next = m_trp_state[trp_id].m_next;

      const Uint32 send_delay = check_delay_expired(trp_id, now);
      if (likely(send_delay == 0)) {
        /**
         * We found a non-neighbour trp to return, handle this
         * and set the next get_trp to start looking for
         * neighbour trps.
         */
        send_instance->m_next_is_high_prio_trp = true;
        goto found_non_neighbour;
      }

      /* Find remaining minimum wait: */
      if (min_wait_usec > send_delay) {
        min_wait_usec = send_delay;
        delayed_trp = trp_id;
        delayed_prev_trp = m_trp_state[trp_id].m_prev;
      }
      trp_id = next;
    }

    // As 'first_trp != 0', there has to be a 'delayed_trp'
    assert(delayed_trp != 0);

    if (!retry) {
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
  if (delayed_trp == 0) {
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
  assert(is_enqueued(delayed_trp, send_instance));
  trp_id = delayed_trp;
  if (delayed_prev_trp == DELAYED_PREV_NODE_IS_NEIGHBOUR) {
    /**
     * If we now returns a priority neighbour,
     * return from head of queue next time.
     */
    send_instance->m_next_is_high_prio_trp = false;
  } else {
    send_instance->m_next_is_high_prio_trp = true;
  }
  /**
   * Fall through to found_neighbour since we have decided that this
   * delayed trp will be returned.
   */

found_neighbour:
  next = m_trp_state[trp_id].m_next;

found_non_neighbour:
  /**
   * We found a TrpId to send to, either delayed or not.
   * Both neighbour and non-neighbour trps are in the list
   * of trps to send to, need to remove it now.
   */
  struct thr_send_trps &trp_state = m_trp_state[trp_id];
  const TrpId first_trp = send_instance->m_first_trp;
  const TrpId last_trp = send_instance->m_last_trp;
  const TrpId prev = trp_state.m_prev;
  assert(next == trp_state.m_next);

  // Remove from TrpId list
  if (trp_id == first_trp) {
    assert(prev == 0);
    send_instance->m_first_trp = next;
    m_trp_state[next].m_prev = prev;
  } else {
    assert(prev != 0);
    m_trp_state[prev].m_next = next;
  }

  if (trp_id == last_trp) {
    assert(next == 0);
    send_instance->m_last_trp = prev;
  } else {
    m_trp_state[next].m_prev = prev;
  }
  trp_state.m_prev = 0;
  trp_state.m_next = 0;

  /**
   * We have a trp ready to be returned, we will update the data_available
   * such that an ACTIVE-state is reflected.
   */
  assert(trp_state.m_data_available > 0);
  trp_state.m_data_available = 1;
  assert(!is_enqueued(trp_id, send_instance));
  return trp_id;
}

/* Called under mutex protection of send_thread_mutex */
bool thr_send_threads::check_done_trp(TrpId trp_id) {
  struct thr_send_trps &trp_state = m_trp_state[trp_id];
  assert(trp_state.m_data_available > 0);
  trp_state.m_data_available--;
  return (trp_state.m_data_available == 0);
}

/* Called under mutex protection of send_thread_mutex */
struct thr_send_thread_instance *thr_send_threads::get_not_awake_send_thread(
    TrpId trp_id, struct thr_send_thread_instance *send_instance) {
  struct thr_send_thread_instance *used_send_thread;
  if (trp_id != 0) {
    Uint32 send_thread = get_send_instance(trp_id);
    if (!m_send_threads[send_thread].m_awake) {
      used_send_thread = &m_send_threads[send_thread];
      assert(used_send_thread == send_instance);
      return used_send_thread;
    }
  }
  if (!send_instance->m_awake) return send_instance;
  return NULL;
}

/**
 * We have assisted our send thread instance, check if it still
 * need to be woken up.
 */
void thr_send_threads::wake_my_send_thread_if_needed(
    TrpId *trp_id_array, Uint32 count,
    struct thr_send_thread_instance *my_send_instance) {
  bool mutex_locked = false;
  struct thr_send_thread_instance *wake_send_instance = NULL;
  for (Uint32 i = 0; i < count; i++) {
    TrpId trp_id = trp_id_array[i];
    struct thr_send_thread_instance *send_instance =
        get_send_thread_instance_by_trp(trp_id);
    if (send_instance != my_send_instance) continue;
    if (!mutex_locked) {
      mutex_locked = true;
      NdbMutex_Lock(my_send_instance->send_thread_mutex);
    }
    struct thr_send_trps &trp_state = m_trp_state[trp_id];
    if (trp_state.m_data_available > 0) {
      wake_send_instance = my_send_instance;
      break;
    }
  }
  if (mutex_locked) {
    NdbMutex_Unlock(my_send_instance->send_thread_mutex);
  }
  if (wake_send_instance != NULL) {
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
Uint32 thr_send_threads::alert_send_thread(
    TrpId trp_id, NDB_TICKS now,
    struct thr_send_thread_instance *my_send_instance) {
  struct thr_send_thread_instance *send_instance =
      get_send_thread_instance_by_trp(trp_id);
  struct thr_send_trps &trp_state = m_trp_state[trp_id];

  NdbMutex_Lock(send_instance->send_thread_mutex);
  trp_state.m_data_available++;  // There is more to send
  if (trp_state.m_data_available > 1) {
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
  assert(!trp_state.m_send_overload);  // Caught above as ACTIVE
  assert(!is_enqueued(trp_id, send_instance));
  insert_trp(trp_id, send_instance);  // IDLE -> PENDING

  /**
   * We need to delay sending the data, as set in config.
   * This is the first send to this trp, so we start the
   * delay timer now.
   */
  if (max_send_delay > 0)  // Wait for more payload?
  {
    set_max_delay(trp_id, now, max_send_delay);
  }

  if (send_instance == my_send_instance) {
    NdbMutex_Unlock(send_instance->send_thread_mutex);
    return 1;
  }

  /*
   * Check if the send thread especially responsible for this transporter
   * is awake, if not wake it up.
   */
  struct thr_send_thread_instance *avail_send_thread =
      get_not_awake_send_thread(trp_id, send_instance);

  NdbMutex_Unlock(send_instance->send_thread_mutex);

  if (avail_send_thread) {
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

static bool check_available_send_data(
    struct thr_send_thread_instance *send_instance) {
  return !send_instance->data_available();
}

// static
int thr_send_threads::trylock_send_trp(TrpId trp_id) {
  thr_repository::send_buffer *sb = g_thr_repository->m_send_buffers + trp_id;
  return trylock(&sb->m_send_lock);
}

// static
bool thr_send_threads::perform_send(TrpId trp_id, Uint32 thr_no,
                                    Uint32 &bytes_sent) {
  thr_repository::send_buffer *sb = g_thr_repository->m_send_buffers + trp_id;

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

static void update_send_sched_config(THRConfigApplier &conf,
                                     unsigned instance_no, bool &real_time) {
  real_time = conf.do_get_realtime_send(instance_no);
}

static void yield_rt_break(NdbThread *thread, enum ThreadTypes type,
                           bool real_time) {
  Configuration *conf = globalEmulatorData.theConfiguration;
  conf->setRealtimeScheduler(thread, type, false, false);
  conf->setRealtimeScheduler(thread, type, real_time, false);
}

static void check_real_time_break(NDB_TICKS now, NDB_TICKS *yield_time,
                                  NdbThread *thread, enum ThreadTypes type) {
  if (unlikely(NdbTick_Compare(now, *yield_time) < 0)) {
    /**
     * Timer was adjusted backwards, or the monotonic timer implementation
     * on this platform is unstable. Best we can do is to restart
     * RT-yield timers from new current time.
     */
    *yield_time = now;
  }

  const Uint64 micros_passed = NdbTick_Elapsed(*yield_time, now).microSec();

  if (micros_passed > 50000) {
    /**
     * Lower scheduling prio to time-sharing mode to ensure that
     * other threads and processes gets a chance to be scheduled
     * if we run for an extended time.
     */
    yield_rt_break(thread, type, true);
    *yield_time = now;
  }
}

#define NUM_WAITS_TO_CHECK_SPINTIME 6
static void wait_time_tracking(thr_data *selfptr, Uint64 wait_time_in_us) {
  for (Uint32 i = 0; i < NUM_SPIN_INTERVALS; i++) {
    if (wait_time_in_us <= selfptr->m_spin_stat.m_spin_interval[i]) {
      selfptr->m_spin_stat.m_micros_sleep_times[i]++;
      selfptr->m_spin_stat.m_num_waits++;
      if (unlikely(selfptr->m_spintime == 0 && selfptr->m_conf_spintime != 0 &&
                   selfptr->m_spin_stat.m_num_waits ==
                       NUM_WAITS_TO_CHECK_SPINTIME)) {
        /**
         * React quickly to changes in environment, if we don't have
         * spinning activated and have already seen 15 wait times, it means
         * that there is a good chance that spinning is a good idea now.
         * So invoke a check if we should activate spinning now.
         */
        SimulatedBlock *b = globalData.getBlock(THRMAN, selfptr->m_thr_no + 1);
        ((Thrman *)b)->check_spintime(false);
      }
      return;
    }
  }
  require(false);
}

static bool check_queues_empty(thr_data *selfptr);
static Uint32 scan_time_queues(struct thr_data *selfptr, NDB_TICKS now);
static bool do_send(struct thr_data *selfptr, bool must_send, bool assist_send);
/**
 * We call this function only after executing no jobs and thus it is
 * safe to spin for a short time.
 */
static bool check_yield(thr_data *selfptr,
                        Uint64 min_spin_timer,  // microseconds
                        Uint32 *spin_time_in_us, NDB_TICKS start_spin_ticks) {
#ifndef NDB_HAVE_CPU_PAUSE
  /**
   * If cpu_pause() was not implemented, 'min_spin_timer == 0' is enforced,
   * and spin-before-yield is never attempted.
   */
  assert(false);
  return true;  // -> yield immediately

#else
  NDB_TICKS now;
  bool cont_flag = true;
  /**
   * If not NdbSpin_is_supported(), it will force 'min_spin_timer==0'.
   * -> We should never attempt to spin in this function before yielding.
   */
  assert(NdbSpin_is_supported());
  assert(min_spin_timer > 0);
  do {
    for (Uint32 i = 0; i < 50; i++) {
      /**
       * During around 50 us we only check for JBA and JBB
       * queues to not be empty. This happens when another thread or
       * the receive thread sends a signal to the thread.
       */
      NdbSpin();
      if (!check_queues_empty(selfptr)) {
        /* Found jobs to execute, successful spin */
        cont_flag = false;
        now = NdbTick_getCurrentTicks();
        break;
      }
      now = NdbTick_getCurrentTicks();
      Uint64 spin_micros = NdbTick_Elapsed(start_spin_ticks, now).microSec();
      if (spin_micros > min_spin_timer) {
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
    if (!cont_flag) break;
    /**
     * Every 50 us we also scan time queues to see if any delayed signals
     * need to be delivered. After checking if this generates any new
     * messages we also check if we have completed spinning for this
     * time.
     */
    const Uint32 lagging_timers = scan_time_queues(selfptr, now);
    if (lagging_timers != 0 || !check_queues_empty(selfptr)) {
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
#endif
}

/**
 * We call this function only after executing no jobs and thus it is
 * safe to spin for a short time.
 */
static bool check_recv_yield(thr_data *selfptr,
                             TransporterReceiveHandle &recvdata,
                             Uint64 min_spin_timer,  // microseconds
                             Uint32 &num_events, Uint32 *spin_time_in_us,
                             NDB_TICKS start_spin_ticks) {
#ifndef NDB_HAVE_CPU_PAUSE
  /**
   * If cpu_pause() was not implemented, 'min_spin_timer == 0' is enforced,
   * and spin-before-yield is never attempted.
   */
  assert(false);
  return true;  // -> yield immediately

#else
  NDB_TICKS now;
  bool cont_flag = true;
  /**
   * If not NdbSpin_is_supported(), it will force 'min_spin_timer==0'.
   * -> We should never attempt to spin in this function before yielding.
   */
  assert(NdbSpin_is_supported());
  assert(min_spin_timer > 0);
  do {
    for (Uint32 i = 0; i < 60; i++) {
      /**
       * During around 50 us we only check for JBA and JBB
       * queues to not be empty. This happens when another thread or
       * the receive thread sends a signal to the thread.
       */
      NdbSpin();
      if ((!check_queues_empty(selfptr)) ||
          ((num_events = globalTransporterRegistry.pollReceive(0, recvdata)) >
           0)) {
        /* Found jobs to execute, successful spin */
        cont_flag = false;
        now = NdbTick_getCurrentTicks();
        break;
      }
      /* Check if we have done enough spinning */
      now = NdbTick_getCurrentTicks();
      Uint64 spin_micros = NdbTick_Elapsed(start_spin_ticks, now).microSec();
      if (spin_micros > min_spin_timer) {
        /**
         * We have spun for the required time, but to no avail, there was no
         * work to do, so it is now time to yield and go to sleep.
         */
        selfptr->m_measured_spintime += spin_micros;
        selfptr->m_spin_stat.m_sleep_longer_spin_time++;
        return true;
      }
    }
    if (!cont_flag) break;
    /**
     * Every 50 us we also scan time queues to see if any delayed signals
     * need to be delivered. After checking if this generates any new
     * messages we also check if we have completed spinning for this
     * time.
     */
    const Uint32 lagging_timers = scan_time_queues(selfptr, now);
    if (lagging_timers != 0 || !check_queues_empty(selfptr)) {
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
#endif
}

/**
 * We enter this function holding the send_thread_mutex if lock is
 * false and we leave no longer holding the mutex.
 */
bool thr_send_threads::assist_send_thread(
    Uint32 max_num_trps, Uint32 thr_no, NDB_TICKS now, Uint32 &watchdog_counter,
    struct thr_send_thread_instance *send_instance,
    class thread_local_pool<thr_send_page> &send_buffer_pool) {
  Uint32 num_trps_sent = 0;
  Uint32 loop = 0;
  NDB_TICKS spin_ticks_dummy;
  TrpId trp_id = 0;

  NdbMutex_Lock(send_instance->send_thread_mutex);

  while (globalData.theRestartFlag != perform_stop && loop < max_num_trps &&
         (trp_id = get_trp(NO_SEND_THREAD, now, send_instance)) != 0)
  // PENDING -> ACTIVE
  {
    if (!handle_send_trp(trp_id, num_trps_sent, thr_no, now, watchdog_counter,
                         send_instance)) {
      /**
       * Only transporters waiting for delay to expire was waiting to send,
       * we will skip sending in this case and leave it for the send
       * thread to handle it. No reason to set pending_send to true since
       * there is no hurry to send (through setting id = 0 below).
       */
      assert(!is_enqueued(trp_id, send_instance));
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
  if (trp_id == 0) {
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
bool thr_send_threads::handle_send_trp(
    TrpId trp_id, Uint32 &num_trps_sent, Uint32 thr_no, NDB_TICKS &now,
    Uint32 &watchdog_counter, struct thr_send_thread_instance *send_instance) {
  assert(send_instance == get_send_thread_instance_by_trp(trp_id));
  assert(!is_enqueued(trp_id, send_instance));
  if (m_trp_state[trp_id].m_micros_delayed > 0)  // Trp send is delayed
  {
    /**
     * The only transporter ready for send was a transporter that still
     * required waiting. We will not send yet if:
     * 1) We are overloaded,   ...or...
     * 2) Size of the buffered sends has not yet reached the limit where
     *    we cancel the wait for a larger send message to be collected.
     */
    if (m_trp_state[trp_id].m_send_overload ||  // 1) Pause overloaded trp
        mt_get_send_buffer_bytes(trp_id) <      // 2)
            MAX_SEND_BUFFER_SIZE_TO_DELAY) {
      if (is_send_thread(thr_no)) {
        /**
         * When encountering max_send_delay from send thread we
         * will let the send thread go to sleep for as long as
         * this trp has to wait (it is the shortest sleep we
         * have. For non-send threads the trp will simply
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
    /**
     * We waited for a larger payload to accumulate. We have
     * met the limit and now cancel any further delays.
     */
    set_max_delay(trp_id, now, 0);  // Large packet -> Send now
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
  assert(!is_enqueued(trp_id, send_instance));
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
  if (likely(trylock_send_trp(trp_id) == 0)) {
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
  assert(!is_enqueued(trp_id, send_instance));
  if (more ||                   // ACTIVE   -> PENDING
      !check_done_trp(trp_id))  // ACTIVE-P -> PENDING
  {
    insert_trp(trp_id, send_instance);

    if (unlikely(more && bytes_sent == 0))  // Trp is overloaded
    {
      set_overload_delay(trp_id, now, 200);  // Delay send-retry by 200 us
    }
  }  // ACTIVE   -> IDLE
  else {
    num_trps_sent++;
  }
  return true;
}

void thr_send_threads::update_rusage(
    struct thr_send_thread_instance *this_send_thread, Uint64 elapsed_time) {
  struct ndb_rusage rusage;

  int res = Ndb_GetRUsage(&rusage, false);
  if (res != 0) {
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
void thr_send_threads::run_send_thread(Uint32 instance_no) {
  struct thr_send_thread_instance *this_send_thread =
      &m_send_threads[instance_no];
  const Uint32 thr_no = glob_num_threads + instance_no;

  {
    /**
     * Wait for thread object to be visible
     */
    while (this_send_thread->m_thread == 0) NdbSleep_MilliSleep(30);
  }

  {
    /**
     * Print out information about starting thread
     *   (number, tid, name, the CPU it's locked into (if locked at all))
     * Also perform the locking to CPU.
     */
    BaseString tmp;
    bool fail = false;
    THRConfigApplier &conf = globalEmulatorData.theConfiguration->m_thr_config;
    tmp.appfmt("thr: %u ", thr_no);
    int tid = NdbThread_GetTid(this_send_thread->m_thread);
    if (tid != -1) {
      tmp.appfmt("tid: %u ", tid);
    }
    conf.appendInfoSendThread(tmp, instance_no);
    int res = conf.do_bind_send(this_send_thread->m_thread, instance_no);
    if (res < 0) {
      fail = true;
      tmp.appfmt("err: %d ", -res);
    } else if (res > 0) {
      tmp.appfmt("OK ");
    }

    unsigned thread_prio;
    res = conf.do_thread_prio_send(this_send_thread->m_thread, instance_no,
                                   thread_prio);
    if (res < 0) {
      fail = true;
      res = -res;
      tmp.appfmt("Failed to set thread prio to %u, ", thread_prio);
      if (res == SET_THREAD_PRIO_NOT_SUPPORTED_ERROR) {
        tmp.appfmt("not supported on this OS");
      } else {
        tmp.appfmt("error: %d", res);
      }
    } else if (res > 0) {
      tmp.appfmt("Successfully set thread prio to %u ", thread_prio);
    }

    g_eventLogger->info("%s", tmp.c_str());
    if (fail) {
      abort();
    }
  }

  /**
   * register watchdog
   */
  bool succ = globalEmulatorData.theWatchDog->registerWatchedThread(
      &this_send_thread->m_watchdog_counter, thr_no);
  require(succ);

  NdbMutex_Lock(this_send_thread->send_thread_mutex);
  this_send_thread->m_awake = false;
  NdbMutex_Unlock(this_send_thread->send_thread_mutex);

  NDB_TICKS yield_ticks;
  bool real_time = false;

  yield_ticks = NdbTick_getCurrentTicks();
  THRConfigApplier &conf = globalEmulatorData.theConfiguration->m_thr_config;
  update_send_sched_config(conf, instance_no, real_time);

  TrpId trp_id = 0;
  Uint64 micros_sleep = 0;
  NDB_TICKS last_now = NdbTick_getCurrentTicks();
  NDB_TICKS last_rusage = last_now;
  NDB_TICKS first_now = last_now;

  while (globalData.theRestartFlag != perform_stop) {
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
    if (time_since_update_rusage > Uint64(50 * 1000)) {
      Uint64 elapsed_time = NdbTick_Elapsed(first_now, now).microSec();
      last_rusage = last_now;
      NdbMutex_Lock(this_send_thread->send_thread_mutex);
      update_rusage(this_send_thread, elapsed_time);
    } else {
      NdbMutex_Lock(this_send_thread->send_thread_mutex);
    }
    this_send_thread->m_exec_time += exec_time;
    this_send_thread->m_sleep_time += sleep_time;
    this_send_thread->m_awake = true;

    /**
     * If waited for a specific transporter, reinsert it such that
     * it can be re-evaluated for send by get_trp().
     *
     * This happens when handle_send_trp returns false due to that the
     * only transporter ready for execute was a transporter that still
     * waited for expiration of delay and no other condition allowed it
     * to be sent.
     */
    if (trp_id != 0) {
      /**
       * The trp was locked during our sleep. We now release the
       * lock again such that we can acquire the lock again after
       * a short sleep.
       */
      assert(!is_enqueued(trp_id, this_send_thread));
      insert_trp(trp_id, this_send_thread);
      trp_id = 0;
    }
    while (globalData.theRestartFlag != perform_stop &&
           (trp_id = get_trp(instance_no, now, this_send_thread)) != 0)
    // PENDING -> ACTIVE
    {
      Uint32 num_trps_sent_dummy;
      if (!handle_send_trp(trp_id, num_trps_sent_dummy, thr_no, now,
                           this_send_thread->m_watchdog_counter,
                           this_send_thread)) {
        /**
         * For now we keep this trp_id for ourself, without a re-insert
         * into the trps lists. Thus it is effectively locked for other
         * (assist-)send-threads. This trp has the shortest m_micros_delayed
         * among the waiting transporters, thus we are going to yield
         * for this periode.
         * When we wake up again, the transporter is reinserted into the
         * list of transporters (above) and get_trp() will find the
         * transporter now being the most suitable - Possibly the same
         * we just waited for, now with the wait time expired.
         */
        assert(m_trp_state[trp_id].m_micros_delayed > 0);
        assert(!is_enqueued(trp_id, this_send_thread));
        break;
      }

      /* Release chunk-wise to decrease pressure on lock */
      this_send_thread->m_watchdog_counter = 3;
      this_send_thread->m_send_buffer_pool.release_chunk(
          g_thr_repository->m_mm, RG_TRANSPORTER_BUFFERS, instance_no);

      /**
       * We set trp_id = 0 for the very rare case where theRestartFlag is set
       * to perform_stop, we should never need this, but add it in just in
       * case.
       */
      trp_id = 0;
    }  // while (get_trp()...)

    /* No more trps having data to send right now, prepare to sleep */
    this_send_thread->m_awake = false;
    const Uint32 trp_wait =
        (trp_id != 0) ? m_trp_state[trp_id].m_micros_delayed : 0;
    NdbMutex_Unlock(this_send_thread->send_thread_mutex);

    if (real_time) {
      check_real_time_break(now, &yield_ticks, this_send_thread->m_thread,
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
      if (trp_wait == 0) {
        // 50ms, has to wakeup before 100ms watchdog alert.
        max_wait_nsec = 50 * 1000 * 1000;
      } else {
        max_wait_nsec = trp_wait * 1000;
      }
      NDB_TICKS before = NdbTick_getCurrentTicks();
      bool waited = yield(&this_send_thread->m_waiter_struct, max_wait_nsec,
                          check_available_send_data, this_send_thread);
      if (waited) {
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
static void job_buffer_full(struct thr_data *selfptr) {
  g_eventLogger->info("job buffer full");
  dumpJobQueues();
  abort();
}

ATTRIBUTE_NOINLINE
static void out_of_job_buffer(struct thr_data *selfptr) {
  g_eventLogger->info("out of job buffer");
  dumpJobQueues();
  abort();
}

static thr_job_buffer *seize_buffer(struct thr_repository *rep, int thr_no,
                                    bool prioa) {
  thr_job_buffer *jb;
  struct thr_data *selfptr = &rep->m_thread[thr_no];
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
  Uint32 buffers =
      (first_free > first_unused ? first_unused + THR_FREE_BUF_MAX - first_free
                                 : first_unused - first_free);
  if (unlikely(buffers <= THR_FREE_BUF_MIN)) {
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
      jb = rep->m_jb_pool.seize(rep->m_mm, RG_JOBBUFFER);
      if (unlikely(jb == nullptr)) {
        if (unlikely(cnt == 0)) {
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

  jb = selfptr->m_free_fifo[first_free];
  selfptr->m_first_free = (first_free + 1) % THR_FREE_BUF_MAX;
  /* Init here rather than in release_buffer() so signal dump will work. */
  jb->m_len = 0;
  jb->m_prioa = prioa;
  return jb;
}

static void release_buffer(struct thr_repository *rep, int thr_no,
                           thr_job_buffer *jb) {
  struct thr_data *selfptr = &rep->m_thread[thr_no];
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

  if (unlikely(first_unused == first_free)) {
    /* FIFO full, need to release to global pool. */
    Uint32 batch = THR_FREE_BUF_MAX / THR_FREE_BUF_BATCH;
    assert(batch > 0);
    assert(batch < THR_FREE_BUF_MAX);
    do {
      rep->m_jb_pool.release(rep->m_mm, RG_JOBBUFFER,
                             selfptr->m_free_fifo[first_free]);
      first_free = (first_free + 1) % THR_FREE_BUF_MAX;
      batch--;
    } while (batch > 0);
    selfptr->m_first_free = first_free;
  }
}

static inline Uint32 scan_queue(struct thr_data *selfptr, Uint32 cnt,
                                Uint32 end, Uint32 *ptr) {
  Uint32 thr_no = selfptr->m_thr_no;
  Uint32 **pages = selfptr->m_tq.m_delayed_signals;
  Uint32 free = selfptr->m_tq.m_next_free;
  Uint32 *save = ptr;
  for (Uint32 i = 0; i < cnt; i++, ptr++) {
    Uint32 val = *ptr;
    if ((val & 0xFFFF) <= end) {
      Uint32 idx = val >> 16;
      Uint32 buf = idx >> 8;
      Uint32 pos = MAX_SIGNAL_SIZE * (idx & 0xFF);

      Uint32 *page = *(pages + buf);

      const SignalHeader *s = reinterpret_cast<SignalHeader *>(page + pos);
      const Uint32 *data = page + pos + (sizeof(*s) >> 2);
      if (0)
        g_eventLogger->info("found %p val: %d end: %d", s, val & 0xFFFF, end);
      /*
       * ToDo: Do measurements of the frequency of these prio A timed signals.
       *
       * If they are frequent, we may want to optimize, as sending one prio A
       * signal is somewhat expensive compared to sending one prio B.
       */
      sendprioa(thr_no, s, data, data + s->theLength);
      *(page + pos) = free;
      free = idx;
    } else if (i > 0) {
      selfptr->m_tq.m_next_free = free;
      memmove(save, ptr, 4 * (cnt - i));
      return i;
    } else {
      return 0;
    }
  }
  selfptr->m_tq.m_next_free = free;
  return cnt;
}

static void handle_time_wrap(struct thr_data *selfptr) {
  Uint32 i;
  struct thr_tq *tq = &selfptr->m_tq;
  Uint32 cnt0 = tq->m_cnt[0];
  Uint32 cnt1 = tq->m_cnt[1];
  Uint32 tmp0 = scan_queue(selfptr, cnt0, 32767, tq->m_short_queue);
  Uint32 tmp1 = scan_queue(selfptr, cnt1, 32767, tq->m_long_queue);
  cnt0 -= tmp0;
  cnt1 -= tmp1;
  tq->m_cnt[0] = cnt0;
  tq->m_cnt[1] = cnt1;
  for (i = 0; i < cnt0; i++) {
    assert((tq->m_short_queue[i] & 0xFFFF) > 32767);
    tq->m_short_queue[i] -= 32767;
  }
  for (i = 0; i < cnt1; i++) {
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
 * be delayed or arriving too fast. Where exact timing is critical,
 * these signals should do their own time calculation by reading
 * the clock, instead of trusting that the signal is delivered as
 * specified by the 'delay' argument
 *
 * If there are leaps larger than 1500ms, we try a hybrid
 * solution by moving the 'm_ticks' forward, close to the
 * actual current time, then continue as above from that
 * point in time. A 'time leap Warning' will also be printed
 * in the logs.
 */
static Uint32 scan_time_queues_impl(struct thr_data *selfptr, Uint32 diff,
                                    NDB_TICKS now) {
  NDB_TICKS last = selfptr->m_ticks;
  Uint32 step = diff;

  if (unlikely(diff > 20))  // Break up into max 20ms steps
  {
    if (unlikely(diff > 1500))  // Time leaped more than 1500ms
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
      if ((curr_rusage.ru_utime == 0 && curr_rusage.ru_stime == 0) ||
          (selfptr->m_scan_time_queue_rusage.ru_utime == 0 &&
           selfptr->m_scan_time_queue_rusage.ru_stime == 0)) {
        /**
         * get_rusage failed for some reason, print old variant of warning
         * message.
         */
        g_eventLogger->warning("thr: %u: Overslept %u ms, expected ~10ms",
                               selfptr->m_thr_no, diff);
      } else {
        Uint32 diff_real =
            NdbTick_Elapsed(selfptr->m_scan_real_ticks, now).milliSec();
        Uint64 exec_time =
            curr_rusage.ru_utime - selfptr->m_scan_time_queue_rusage.ru_utime;
        Uint64 sys_time =
            curr_rusage.ru_stime - selfptr->m_scan_time_queue_rusage.ru_stime;
        g_eventLogger->warning(
            "thr: %u Overslept %u ms, expected ~10ms"
            ", user time: %llu us, sys_time: %llu us",
            selfptr->m_thr_no, diff_real, exec_time, sys_time);
      }
      last = NdbTick_AddMilliseconds(last, diff - 1000);
    }
    step = 20;  // Max expire interval handled is 20ms
  }

  struct thr_tq *tq = &selfptr->m_tq;
  Uint32 curr = tq->m_current_time;
  Uint32 cnt0 = tq->m_cnt[0];
  Uint32 cnt1 = tq->m_cnt[1];
  Uint32 end = (curr + step);
  if (end >= 32767) {
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
static void scan_time_queues_backtick(struct thr_data *selfptr, NDB_TICKS now) {
  const NDB_TICKS last = selfptr->m_ticks;
  assert(NdbTick_Compare(now, last) < 0);

  const Uint64 backward = NdbTick_Elapsed(now, last).milliSec();

  /**
   * Silently ignore sub millisecond backticks.
   * Such 'noise' is unfortunately common, even for monotonic timers.
   */
  if (backward > 0) {
    g_eventLogger->warning("thr: %u Time ticked backwards %llu ms.",
                           selfptr->m_thr_no, backward);

    /* Long backticks should never happen for monotonic timers */
    // assert(backward < 100 || !NdbTick_IsMonotonic());

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
static inline void scan_zero_queue(struct thr_data *selfptr) {
  struct thr_tq *tq = &selfptr->m_tq;
  Uint32 cnt = tq->m_cnt[2];
  if (cnt) {
    Uint32 num_found =
        scan_queue(selfptr, cnt, tq->m_current_time, tq->m_zero_queue);
    require(num_found == cnt);
  }
  tq->m_cnt[2] = 0;
}

static inline Uint32 scan_time_queues(struct thr_data *selfptr, NDB_TICKS now) {
  scan_zero_queue(selfptr);
  const NDB_TICKS last = selfptr->m_ticks;
  if (unlikely(NdbTick_Compare(now, last) < 0)) {
    scan_time_queues_backtick(selfptr, now);
    return 0;
  }

  const Uint32 diff = (Uint32)NdbTick_Elapsed(last, now).milliSec();
  if (unlikely(diff > 0)) {
    return scan_time_queues_impl(selfptr, diff, now);
  }
  return 0;
}

static inline Uint32 *get_free_slot(struct thr_repository *rep,
                                    struct thr_data *selfptr, Uint32 *idxptr) {
  struct thr_tq *tq = &selfptr->m_tq;
  Uint32 idx = tq->m_next_free;
retry:

  if (idx != RNIL) {
    Uint32 buf = idx >> 8;
    Uint32 pos = idx & 0xFF;
    Uint32 *page = *(tq->m_delayed_signals + buf);
    Uint32 *ptr = page + (MAX_SIGNAL_SIZE * pos);
    tq->m_next_free = *ptr;
    *idxptr = idx;
    return ptr;
  }

  Uint32 thr_no = selfptr->m_thr_no;
  for (Uint32 i = 0; i < thr_tq::PAGES; i++) {
    if (tq->m_delayed_signals[i] == 0) {
      struct thr_job_buffer *jb = seize_buffer(rep, thr_no, false);
      Uint32 *page = reinterpret_cast<Uint32 *>(jb);
      tq->m_delayed_signals[i] = page;
      /**
       * Init page
       */
      for (Uint32 j = 0; j < MIN_SIGNALS_PER_PAGE; j++) {
        page[j * MAX_SIGNAL_SIZE] = (i << 8) + (j + 1);
      }
      page[MIN_SIGNALS_PER_PAGE * MAX_SIGNAL_SIZE] = RNIL;
      idx = (i << 8);
      goto retry;
    }
  }
  abort();
  return NULL;
}

void senddelay(Uint32 thr_no, const SignalHeader *s, Uint32 delay) {
  struct thr_repository *rep = g_thr_repository;
  struct thr_data *selfptr = &rep->m_thread[thr_no];
  assert(my_thread_equal(selfptr->m_thr_id, my_thread_self()));
  unsigned siglen = (sizeof(*s) >> 2) + s->theLength + s->m_noOfSections;

  Uint32 max;
  Uint32 *cntptr;
  Uint32 *queueptr;

  Uint32 alarm;
  Uint32 nexttimer = selfptr->m_tq.m_next_timer;
  if (delay == SimulatedBlock::BOUNDED_DELAY) {
    alarm = selfptr->m_tq.m_current_time;
    cntptr = selfptr->m_tq.m_cnt + 2;
    queueptr = selfptr->m_tq.m_zero_queue;
    max = thr_tq::ZQ_SIZE;
  } else {
    alarm = selfptr->m_tq.m_current_time + delay;
    if (delay < 100) {
      cntptr = selfptr->m_tq.m_cnt + 0;
      queueptr = selfptr->m_tq.m_short_queue;
      max = thr_tq::SQ_SIZE;
    } else {
      cntptr = selfptr->m_tq.m_cnt + 1;
      queueptr = selfptr->m_tq.m_long_queue;
      max = thr_tq::LQ_SIZE;
    }
  }

  Uint32 idx;
  Uint32 *ptr = get_free_slot(rep, selfptr, &idx);
  memcpy(ptr, s, 4 * siglen);

  if (0)
    g_eventLogger->info(
        "now: %d alarm: %d send %s from %s to %s delay: %d idx: %x %p",
        selfptr->m_tq.m_current_time, alarm,
        getSignalName(s->theVerId_signalNumber),
        getBlockName(refToBlock(s->theSendersBlockRef)),
        getBlockName(s->theReceiversBlockNumber), delay, idx, ptr);

  Uint32 i;
  Uint32 cnt = *cntptr;
  Uint32 newentry = (idx << 16) | (alarm & 0xFFFF);

  *cntptr = cnt + 1;
  selfptr->m_tq.m_next_timer = alarm < nexttimer ? alarm : nexttimer;

  if (cnt == 0 || delay == SimulatedBlock::BOUNDED_DELAY) {
    /* First delayed signal needs no order and bounded delay is FIFO */
    queueptr[cnt] = newentry;
    return;
  } else if (cnt < max) {
    for (i = 0; i < cnt; i++) {
      Uint32 save = queueptr[i];
      if ((save & 0xFFFF) > alarm) {
        memmove(queueptr + i + 1, queueptr + i, 4 * (cnt - i));
        queueptr[i] = newentry;
        return;
      }
    }
    assert(i == cnt);
    queueptr[i] = newentry;
    return;
  } else {
    /* Out of entries in time queue, issue proper error */
    if (cntptr == (selfptr->m_tq.m_cnt + 0)) {
      /* Error in short time queue */
      ERROR_SET(ecError, NDBD_EXIT_TIME_QUEUE_SHORT,
                "Too many in Short Time Queue", "mt.cpp");
    } else if (cntptr == (selfptr->m_tq.m_cnt + 1)) {
      /* Error in long time queue */
      ERROR_SET(ecError, NDBD_EXIT_TIME_QUEUE_LONG,
                "Too many in Long Time Queue", "mt.cpp");
    } else {
      /* Error in zero time queue */
      ERROR_SET(ecError, NDBD_EXIT_TIME_QUEUE_ZERO,
                "Too many in Zero Time Queue", "mt.cpp");
    }
  }
}

/**
 *  Compute *total* max signals that this thread can execute wo/ risking
 *  job-buffer-full.
 *
 * 1) min_free_buffers are number of free job-buffer pages in (one of) the
 *    job buffer queues this thread may send signals to.
 * 2) Compute how many signals this corresponds to.
 * 3) Divide 'max_signals' among the threads writing to each 'queue'.
 *
 *  Note, that there might be multiple threads writing to each job-buffer,
 *  each seeing the same number of initial min_free_buffers. Thus, we need
 *  to divide the 'free_buffers' between these threads.
 *
 *  Note, that min_free_buffers is updated when we last flushed thread-local
 *  signals to the job-buffer queue. It might be outdated if other threads
 *  flushed to the same job-buffer queue in between. Thus, there are no
 *  guarantees that we do not compute a too large max_signals-quota. However,
 *  we have a 'RESERVED' area to provide for this, and we will flush and update
 *  the free_buffers view every MAX_SIGNALS_BEFORE_FLUSH_*.
 *
 *   Assumption: each signal may send *at most* 4 signals
 *     - this assumption is made the same in ndbd and ndbmtd and is
 *       mostly followed by block-code, although not it all places :-(
 */
static Uint32 compute_max_signals_to_execute(Uint32 min_free_buffers) {
  const Uint32 max_signals_to_execute =
      ((min_free_buffers * MIN_SIGNALS_PER_PAGE) + 3) / 4;
  return max_signals_to_execute / glob_num_writers_per_job_buffers;
}

/**
 * Compute max signals to execute from a single job buffer.
 * ... Note that MAX_SIGNALS_PER_JB also applies, not checked here.
 *
 * Assumption is that we have a total quota of max_signals_to_execute
 * by this thread (see above) in a round of run_job_buffers. We are limited
 * by the worst case scenario, where all signals executed from the incoming
 * 'glob_num_job_buffers_per_thread' job-buffers, produce their max quota of
 * 4 outgoing signals to the same job-buffer out-queue.
 */
static Uint32 compute_max_signals_per_jb(Uint32 max_signals_to_execute) {
  const Uint32 per_jb =
      (max_signals_to_execute + glob_num_job_buffers_per_thread - 1) /
      glob_num_job_buffers_per_thread;
  return per_jb;
}

/**
 * Out queue to 'congested' has reached a CONGESTED level.
 *
 * Reduce the 'm_max_signals_per_jb' execute quota to account for this.
 * In case we have reached the 'RESERVED' congestion level, we stop the
 * 'normal' execute paths by setting 'm_max_signals_per_jb = 0'. In this
 * state we can only execute extra signals from the 'm_extra_signals[]'
 * quota. These are assigned to drain from in-queues which at detected
 * to be congested as well.
 *
 * Note that we only handle quota reduction due to the specified 'congested'
 * queue. There may be other congestions as well, thus we take MIN's of
 *  the calculated quotas below.
 */
static void set_congested_jb_quotas(thr_data *selfptr, Uint32 congested,
                                    Uint32 free) {
  assert(free <= thr_job_queue::CONGESTED);
  // JB-page usage is congested, reduce execution quota
  if (unlikely(free <= thr_job_queue::RESERVED)) {
    // Can't do 'normal' JB-execute anymore, only 'extra' signals
    const Uint32 reserved = free;
    const Uint32 extra = compute_max_signals_to_execute(reserved);
    selfptr->m_congested_threads_mask.set(congested);
    selfptr->m_max_signals_per_jb = 0;
    selfptr->m_total_extra_signals = MIN(extra, selfptr->m_total_extra_signals);
  } else {
    // Might need to reduce JB-quota. As we have not reached the 'RESERVED',
    // this congestion does not affect amount of extra signals.
    const Uint32 avail =
        compute_max_signals_to_execute(free - thr_job_queue::RESERVED);
    const Uint32 perjb = compute_max_signals_per_jb(avail);
    if (perjb < MAX_SIGNALS_PER_JB) {
      selfptr->m_congested_threads_mask.set(congested);
      selfptr->m_max_signals_per_jb = MIN(perjb, selfptr->m_max_signals_per_jb);
    }
  }
}

void trp_callback::reportSendLen(NodeId nodeId [[maybe_unused]], Uint32 count,
                                 Uint64 bytes) {
#ifdef RONM_TODO
  SignalT<3> signal[1] = {};
#endif

  if (g_send_threads) {
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
  signal.theData[2] = (Uint32)(bytes / count);
  signal.header.theVerId_signalNumber = GSN_EVENT_REP;
  signal.header.theReceiversBlockNumber = CMVMI;
  sendlocal(g_thr_repository->m_send_buffers[trp_id].m_send_thread,
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
void trp_callback::lock_transporter(TrpId trp_id) {
  Uint32 recv_thread_idx = mt_get_recv_thread_idx(trp_id);
  struct thr_repository *rep = g_thr_repository;
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

void trp_callback::unlock_transporter(TrpId trp_id) {
  Uint32 recv_thread_idx = mt_get_recv_thread_idx(trp_id);
  struct thr_repository *rep = g_thr_repository;
  unlock(&rep->m_receive_lock[recv_thread_idx]);
  unlock(&rep->m_send_buffers[trp_id].m_send_lock);
}

void trp_callback::lock_send_transporter(TrpId trp_id) {
  struct thr_repository *rep = g_thr_repository;
  lock(&rep->m_send_buffers[trp_id].m_send_lock);
}

void trp_callback::unlock_send_transporter(TrpId trp_id) {
  struct thr_repository *rep = g_thr_repository;
  unlock(&rep->m_send_buffers[trp_id].m_send_lock);
}

/**
 * Provide a producer side estimate for number of free JB-pages
 * in a specific 'out-'thr_job_queue.
 *
 * Is an 'estimate' as if we are in a non-congested state, we use the
 * 'cached_read_index' to calculate the 'used'. This may return a too high,
 * but still 'uncongested', value of used pages, as the 'dst' consumer might
 * have moved the read_index since cached_read_index was updated. This is ok
 * as the upper levels should only use this function to check for congestions.
 *
 * If the cached_read indicate congestion, we check the non-cached read_index
 * to get a more accurate estimate - Possible uncongested if read_index was
 * moved since we updated the cached_read_index
 *
 * Rational is to avoid reading the read_index-cache-line, which was
 * likely invalidated by the consumer, too frequently.
 *
 * Need to be called with the m_write_lock held if there are
 * multiple writers. (glob_use_write_lock_mutex==true)
 */
static unsigned get_free_estimate_out_queue(thr_job_queue *q) {
  const Uint32 cached_read_index = q->m_cached_read_index;
  const Uint32 write_index = q->m_write_index;
  const unsigned free =
      calc_fifo_free(cached_read_index, write_index, q->m_size);

  if (free > thr_job_queue::CONGESTED)
    // As long as we are unCONGESTED, we do not care about exact free-amount
    return free;

  /**
   * NOTE: m_read_index is read wo/ lock (and updated by different thread(s))
   *       but since the different thread can only consume
   *       signals this means that the value returned from this
   *       function is always conservative (i.e it can be better than
   *       returned value, if read-index has moved but we didn't see it)
   */
  const Uint32 read_index = q->m_read_index;
  q->m_cached_read_index = read_index;
  return calc_fifo_free(read_index, write_index, q->m_size);
}

/**
 * Compute free buffers in specified in-queue.
 *
 * Is lock free, with same assumption as rest of JB-queue
 * algorithms. .. Only a single reader (this) which do not need locks.
 * Concurrent writer(s) might have written more though, which we
 * will not see until rechecked again later.
 */
static unsigned get_free_in_queue(const thr_job_queue *q) {
  return calc_fifo_free(q->m_read_index, q->m_write_index, q->m_size);
}

/**
 * Callback functions used by yield() to recheck
 * 'job buffers congested/full' condition before going to sleep.
 * Set write_lock as required. (if having multiple writers)
 *
 * Check if the specified congested waitfor-thread (arg) still has
 * job buffer congestion (-> outgoing JBs too full), return true if so.
 */
static bool check_congested_job_queue(thr_job_queue *waitfor) {
  unsigned free;
  if (unlikely(glob_use_write_lock_mutex)) {
    lock(&waitfor->m_write_lock);
    free = get_free_estimate_out_queue(waitfor);
    unlock(&waitfor->m_write_lock);
  } else {
    free = get_free_estimate_out_queue(waitfor);
  }
  return (free <= thr_job_queue::CONGESTED);
}

static bool check_full_job_queue(thr_job_queue *waitfor) {
  unsigned free;
  if (unlikely(glob_use_write_lock_mutex)) {
    lock(&waitfor->m_write_lock);
    free = get_free_estimate_out_queue(waitfor);
    unlock(&waitfor->m_write_lock);
  } else {
    free = get_free_estimate_out_queue(waitfor);
  }
  return (free <= thr_job_queue::RESERVED);
}

/**
 * Get a FULL JB-queue, preferably not 'self'.
 *
 * Get the thread whose our out-JB-queue is FULL-congested on.
 * Try to avoid returning the 'self-queue' if there are other
 * FULL queues we need to wait on. (We can not wait on 'self')
 *
 * Assumption is that function is called only when execution thread has
 * reached m_max_signals_per_jb == 0. Thus, one of the congested threads
 * should have reached the 'RESERVED' limit.
 *
 * Note that we 'get_free_estimate' without holding the write_lock.
 * Thus, congestion can later get more severe due to other writers, or it
 * could have cleared due to yet undetected read-consumption. Anyhow,
 * we will later set lock and either recheck_congested_job_buffers() or
 * recheck in the yield-callback function.
 *
 * We might then possibly not yield, which results in another call
 * to this function.
 *
 * If full: Return 'thr_data*' for (one of) the thread(s)
 *          which we have to wait for. (to consume from queue)
 */
static thr_data *get_congested_job_queue(thr_data *selfptr) {
  thr_repository *rep = g_thr_repository;
  const unsigned self = selfptr->m_thr_no;
  const unsigned self_jbb = self % NUM_JOB_BUFFERS_PER_THREAD;
  thr_data *self_is_full = nullptr;

  // Precondition: Had full job_queues:
  assert(selfptr->m_max_signals_per_jb == 0);

  for (unsigned thr_no = selfptr->m_congested_threads_mask.find_first();
       thr_no != BitmaskImpl::NotFound;
       thr_no = selfptr->m_congested_threads_mask.find_next(thr_no + 1)) {
    thr_data *congested_thr = &rep->m_thread[thr_no];
    thr_job_queue *congested_queue = &congested_thr->m_jbb[self_jbb];
    const unsigned free = get_free_estimate_out_queue(congested_queue);

    if (free <= thr_job_queue::RESERVED) {  // is 'FULL'
      /**
       * We try to find another thread than 'self' to wait for:
       * - If self_is_full, we just note it for later.
       * - Any other non-self thread is returned immediately
       */
      if (thr_no != self) {
        return congested_thr;
      } else {
        self_is_full = selfptr;
      }
    }
  }
  /**
   * We possibly didn't find a FULL-congested job_buffer: it could have been
   * consumed from after we set 'per_jb==0'. We will then need to
   * recheck_congested_job_buffers() in order to relcalculate 'per_jb'
   * and 'extra' execution quotas, and recheck the FULL-congestions.
   */
  return self_is_full;  // selfptr or nullptr
}

static void dumpJobQueues(void) {
  BaseString tmp;
  const struct thr_repository *rep = g_thr_repository;
  for (unsigned to = 0; to < glob_num_threads; to++) {
    for (unsigned from = 0; from < glob_num_job_buffers_per_thread; from++) {
      const thr_data *thrptr = rep->m_thread + to;
      const thr_job_queue *q = thrptr->m_jbb + from;
      const unsigned free = get_free_in_queue(q);
      const unsigned used = q->m_size - thr_job_queue::SAFETY - free;
      if (used > 1)  // At least 1 jb-page in use, even if 'empty'
      {
        tmp.appfmt("\n job buffer %d --> %d, used %d", from, to, used);
        if (free <= 0) {
          tmp.appfmt(" FULL!");
        } else if (free <= thr_job_queue::RESERVED) {
          tmp.appfmt(" HIGH LOAD (free:%d)", free);
        }
      }
    }
  }
  if (!tmp.empty()) {
    g_eventLogger->info("Dumping non-empty job queues: %s", tmp.c_str());
  }
}

int mt_checkDoJob(Uint32 recv_thread_idx) {
  struct thr_repository *rep = g_thr_repository;
  // Find the thr_data for the specified recv_thread
  const unsigned recv_thr_no = first_receiver_thread_no + recv_thread_idx;
  struct thr_data *recv_thr = &rep->m_thread[recv_thr_no];

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
  return !recv_thr->m_congested_threads_mask.isclear();
}

/**
 * Collect all send-buffer-pages to be sent by TrpId
 * from each thread. Link them together and append them to
 * the single send_buffer list 'sb->m_buffer'.
 *
 * The 'sb->m_buffer_lock' has to be held prior to calling
 * this function.
 *
 * TODO: This is not completely fair,
 *       it would be better to get one entry from each thr_send_queue
 *       per thread instead (until empty)
 */
static void link_thread_send_buffers(thr_repository::send_buffer *sb,
                                     TrpId trp_id) {
  Uint32 ri[MAX_BLOCK_THREADS];
  Uint32 wi[MAX_BLOCK_THREADS];
  thr_send_queue *src = g_thr_repository->m_thread_send_buffers[trp_id];
  for (unsigned thr = 0; thr < glob_num_threads; thr++) {
    ri[thr] = sb->m_read_index[thr];
    wi[thr] = src[thr].m_write_index;
  }

  Uint64 sentinel[thr_send_page::HEADER_SIZE >> 1];
  thr_send_page *sentinel_page = new (&sentinel[0]) thr_send_page;
  sentinel_page->m_next = nullptr;

  struct thr_send_buffer tmp;
  tmp.m_first_page = sentinel_page;
  tmp.m_last_page = sentinel_page;

  Uint64 bytes = 0;

#ifdef ERROR_INSERT

#define MIXOLOGY_MIX_MT_SEND 2

  if (unlikely(globalEmulatorData.theConfiguration->getMixologyLevel() &
               MIXOLOGY_MIX_MT_SEND)) {
    /**
     * DEBUGGING only
     * Interleave at the page level from all threads with
     * pages to send - intended to help expose signal
     * order dependency bugs
     * TODO : Avoid having a whole separate implementation
     * like this.
     */
    bool more_pages;

    do {
      src = g_thr_repository->m_thread_send_buffers[trp_id];
      more_pages = false;
      for (unsigned thr = 0; thr < glob_num_threads; thr++, src++) {
        Uint32 r = ri[thr];
        Uint32 w = wi[thr];
        if (r != w) {
          rmb();
          /* Take one page from this thread's send buffer for this trp */
          thr_send_page *p = src->m_buffers[r];
          assert(p->m_start == 0);
          bytes += p->m_bytes;
          tmp.m_last_page->m_next = p;
          tmp.m_last_page = p;

          /* Take page out of read_index slot list */
          thr_send_page *next = p->m_next;
          p->m_next = nullptr;
          src->m_buffers[r] = next;

          if (next == nullptr) {
            /**
             * Used up read slot, any more slots available to read
             * from this thread?
             */
            r = (r + 1) % thr_send_queue::SIZE;
            more_pages |= (r != w);

            /* Update global and local per thread read indices */
            sb->m_read_index[thr] = r;
            ri[thr] = r;
          } else {
            more_pages |= true;
          }
        }
      }
    } while (more_pages);
  } else
#endif

  {
    for (unsigned thr = 0; thr < glob_num_threads; thr++, src++) {
      Uint32 r = ri[thr];
      Uint32 w = wi[thr];
      if (r != w) {
        rmb();
        while (r != w) {
          thr_send_page *p = src->m_buffers[r];
          assert(p->m_start == 0);
          bytes += p->m_bytes;
          tmp.m_last_page->m_next = p;
          while (p->m_next != nullptr) {
            p = p->m_next;
            assert(p->m_start == 0);
            bytes += p->m_bytes;
          }
          tmp.m_last_page = p;
          assert(tmp.m_last_page != nullptr); /* Impossible */
          r = (r + 1) % thr_send_queue::SIZE;
        }
        sb->m_read_index[thr] = r;
      }
    }
  }
  if (bytes > 0) {
    const Uint64 buffered_size = sb->m_buffered_size;
    /**
     * Append send buffers collected from threads
     * to end of existing m_buffers.
     */
    if (sb->m_buffer.m_first_page != nullptr) {
      assert(sb->m_buffer.m_last_page != nullptr);
      sb->m_buffer.m_last_page->m_next = tmp.m_first_page->m_next;
      sb->m_buffer.m_last_page = tmp.m_last_page;
    } else {
      assert(sb->m_buffer.m_last_page == nullptr);
      sb->m_buffer.m_first_page = tmp.m_first_page->m_next;
      sb->m_buffer.m_last_page = tmp.m_last_page;
    }
    sb->m_buffered_size = buffered_size + bytes;
  }
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
static Uint32 pack_sb_pages(thread_local_pool<thr_send_page> *pool,
                            struct thr_send_buffer *buffer) {
  assert(buffer->m_first_page != NULL);
  assert(buffer->m_last_page != NULL);
  assert(buffer->m_last_page->m_next == NULL);

  thr_send_page *curr = buffer->m_first_page;
  Uint32 curr_free = curr->max_bytes() - (curr->m_bytes + curr->m_start);
  Uint32 bytes = curr->m_bytes;
  while (curr->m_next != 0) {
    thr_send_page *next = curr->m_next;
    bytes += next->m_bytes;
    assert(next->m_start == 0);  // only first page should have half sent bytes
    if (next->m_bytes <= curr_free) {
      /**
       * There is free space in the current page and it is sufficient to
       * store the entire next-page. Copy from next page to current page
       * and update current page and release next page to local pool.
       */
      thr_send_page *save = next;
      memcpy(curr->m_data + (curr->m_bytes + curr->m_start), next->m_data,
             next->m_bytes);

      curr_free -= next->m_bytes;

      curr->m_bytes += next->m_bytes;
      curr->m_next = next->m_next;

      pool->release_local(save);

#ifdef NDB_BAD_SEND
      if ((curr->m_bytes % 40) == 24) {
        /* Oops */
        curr->m_data[curr->m_start + 21] = 'F';
      }
#endif
    } else {
      /* Not enough free space in current, move to next page */
      curr = next;
      curr_free = curr->max_bytes() - (curr->m_bytes + curr->m_start);
    }
  }

  buffer->m_last_page = curr;
  assert(bytes > 0);
  return bytes;
}

static void release_list(thread_local_pool<thr_send_page> *pool,
                         thr_send_page *head, thr_send_page *tail) {
  while (head != tail) {
    thr_send_page *tmp = head;
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
 * 'm_sending' buffers with appropriate locks taken.
 *
 * If sending to trp is not enabled, the buffered pages
 * are released instead of being returned from this method.
 */
Uint32 trp_callback::get_bytes_to_send_iovec(TrpId trp_id, struct iovec *dst,
                                             Uint32 max) {
  thr_repository::send_buffer *sb = g_thr_repository->m_send_buffers + trp_id;
  sb->m_bytes_sent = 0;

  /**
   * Collect any available send pages from the thread queues
   * and 'm_buffers'. Append them to the end of m_sending buffers
   */
  {
    lock(&sb->m_buffer_lock);
    link_thread_send_buffers(sb, trp_id);

    if (sb->m_buffer.m_first_page != NULL) {
      // If first page is not NULL, the last page also can't be NULL
      require(sb->m_buffer.m_last_page != NULL);
      if (sb->m_sending.m_first_page == NULL) {
        sb->m_sending = sb->m_buffer;
      } else {
        assert(sb->m_sending.m_last_page != NULL);
        sb->m_sending.m_last_page->m_next = sb->m_buffer.m_first_page;
        sb->m_sending.m_last_page = sb->m_buffer.m_last_page;
      }
      sb->m_buffer.m_first_page = NULL;
      sb->m_buffer.m_last_page = NULL;

      sb->m_sending_size += sb->m_buffered_size;
      sb->m_buffered_size = 0;
    }
    unlock(&sb->m_buffer_lock);

    if (sb->m_sending.m_first_page == NULL) return 0;
  }

  /**
   * If sending to trp is not enabled; discard the send buffers.
   */
  if (unlikely(!sb->m_enabled)) {
    thread_local_pool<thr_send_page> pool(&g_thr_repository->m_sb_pool, 0);
    release_list(&pool, sb->m_sending.m_first_page, sb->m_sending.m_last_page);
    pool.release_all(
        g_thr_repository->m_mm, RG_TRANSPORTER_BUFFERS,
        g_send_threads == NULL ? 0 : g_send_threads->get_send_instance(trp_id));

    sb->m_sending.m_first_page = NULL;
    sb->m_sending.m_last_page = NULL;
    sb->m_sending_size = 0;
    return 0;
  }

  /**
   * Process linked-list and put into iovecs
   */
fill_iovec:
  Uint64 bytes = 0;
  Uint32 pages = 0;
  thr_send_page *p = sb->m_sending.m_first_page;

#ifdef NDB_LUMPY_SEND
  /* Drip feed transporter a few bytes at a time to send */
  do {
    Uint32 offset = 0;
    while ((offset < p->m_bytes) && (pages < max)) {
      /* 0 -+1-> 1 -+6-> (7)3 -+11-> (18)2 -+10-> 0 */
      Uint32 lumpSz = 1;
      switch (offset % 4) {
        case 0:
          lumpSz = 1;
          break;
        case 1:
          lumpSz = 6;
          break;
        case 2:
          lumpSz = 10;
          break;
        case 3:
          lumpSz = 11;
          break;
      }
      const Uint32 remain = p->m_bytes - offset;
      lumpSz = (remain < lumpSz) ? remain : lumpSz;

      dst[pages].iov_base = p->m_data + p->m_start + offset;
      dst[pages].iov_len = lumpSz;
      pages++;
      offset += lumpSz;
    }
    if (pages == max) {
      return pages;
    }
    assert(offset == p->m_bytes);
    p = p->m_next;
  } while (p != nullptr);

  return pages;
#endif

  do {
    dst[pages].iov_len = p->m_bytes;
    dst[pages].iov_base = p->m_data + p->m_start;
    assert(p->m_start + p->m_bytes <= p->max_bytes());
    bytes += p->m_bytes;
    pages++;
    p = p->m_next;
    if (p == nullptr) {
      /**
       * Note that we do not account for sb->m_buffered_size which
       * may have arrived after we released the 'm_buffer_lock' above.
       * That should be ok as we effectively now update_send_buffer_usage()
       * with the state we had when we 'linked' the send buffers above.
       * TODO?: Maintain 'pages' in 'sb->' as well as 'bytes'.
       */
      assert(bytes == sb->m_sending_size);
      globalTransporterRegistry.update_send_buffer_usage(
          trp_id, Uint64(pages) * thr_send_page::PGSIZE, bytes);
      return pages;
    }
  } while (pages < max);

  /**
   * Possibly pack send-buffers to get better utilization:
   * If we were unable to fill all sendbuffers into iovec[],
   * we pack the sendbuffers now if they have a low fill degree.
   * This could save us another OS-send for sending the remaining.
   */
  if (pages == max && max > 1 &&                         // Exhausted iovec[]
      bytes < (pages * thr_send_page::max_bytes()) / 4)  // < 25% filled
  {
    const Uint32 thr_no = sb->m_send_thread;
    assert(thr_no != NO_SEND_THREAD);

    if (!is_send_thread(thr_no)) {
      thr_data *thrptr = &g_thr_repository->m_thread[thr_no];
      pack_sb_pages(&thrptr->m_send_buffer_pool, &sb->m_sending);
    } else {
      pack_sb_pages(g_send_threads->get_send_buffer_pool(thr_no),
                    &sb->m_sending);
    }

    /**
     * Retry filling iovec[]. As 'pack' will ensure at least 50% fill degree,
     * we will not do another 'pack' after the retry.
     */
    goto fill_iovec;
  }
  /**
   * There are more than 'max' pages, count these as well in order to
   * update_send_buffer_usage(). We only return the 'max' in iovec[] though.
   */
  const Uint32 iovec_pages = pages;
  while (p != nullptr) {
    bytes += p->m_bytes;
    pages++;
    p = p->m_next;
  }
  assert(bytes == sb->m_sending_size);
  globalTransporterRegistry.update_send_buffer_usage(
      trp_id, Uint64(pages) * thr_send_page::PGSIZE, bytes);
  return iovec_pages;
}

static Uint32 bytes_sent(thread_local_pool<thr_send_page> *pool,
                         thr_repository::send_buffer *sb, Uint32 bytes) {
  const Uint64 sending_size = sb->m_sending_size;
  assert(bytes && bytes <= sending_size);

  sb->m_bytes_sent = bytes;
  sb->m_sending_size = sending_size - bytes;

  Uint32 remain = bytes;
  thr_send_page *prev = nullptr;
  thr_send_page *curr = sb->m_sending.m_first_page;

  /* Some, or all, in 'm_sending' was sent, find endpoint. */
  while (remain && remain >= curr->m_bytes) {
    /**
     * Calculate new current page such that we can release the
     * pages that have been completed and update the state of
     * the new current page
     */
    remain -= curr->m_bytes;
    prev = curr;
    curr = curr->m_next;
  }

  if (remain) {
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
    if (prev != nullptr) {
      release_list(pool, sb->m_sending.m_first_page, prev);
    }
  } else {
    /**
     * We sent a couple of full pages and the sending stopped at a
     * page boundary, so we only need to release the sent pages
     * and update the new current page.
     */
    if (prev != nullptr) {
      release_list(pool, sb->m_sending.m_first_page, prev);

      if (prev == sb->m_sending.m_last_page) {
        /**
         * Every thing was released, release the pages in the local pool
         */
        sb->m_sending.m_first_page = nullptr;
        sb->m_sending.m_last_page = nullptr;
        return 0;
      }
    } else {
      assert(sb->m_sending.m_first_page != nullptr);
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
Uint32 trp_callback::bytes_sent(TrpId trp_id, Uint32 bytes) {
  thr_repository::send_buffer *sb = g_thr_repository->m_send_buffers + trp_id;
  Uint32 thr_no = sb->m_send_thread;
  assert(thr_no != NO_SEND_THREAD);
  if (!is_send_thread(thr_no)) {
    thr_data *thrptr = &g_thr_repository->m_thread[thr_no];
    return ::bytes_sent(&thrptr->m_send_buffer_pool, sb, bytes);
  } else {
    return ::bytes_sent(g_send_threads->get_send_buffer_pool(thr_no), sb,
                        bytes);
  }
}

void trp_callback::enable_send_buffer(TrpId trp_id) {
  thr_repository::send_buffer *sb = g_thr_repository->m_send_buffers + trp_id;
  lock(&sb->m_send_lock);
  assert(sb->m_sending_size == 0);
  {
    /**
     * Collect and discard any sent buffered signals while
     * send buffers were disabled.
     */
    lock(&sb->m_buffer_lock);
    link_thread_send_buffers(sb, trp_id);

    if (sb->m_buffer.m_first_page != NULL) {
      thread_local_pool<thr_send_page> pool(&g_thr_repository->m_sb_pool, 0);
      release_list(&pool, sb->m_buffer.m_first_page, sb->m_buffer.m_last_page);
      pool.release_all(g_thr_repository->m_mm, RG_TRANSPORTER_BUFFERS,
                       g_send_threads == NULL
                           ? 0
                           : g_send_threads->get_send_instance(trp_id));
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

void trp_callback::disable_send_buffer(TrpId trp_id) {
  thr_repository::send_buffer *sb = g_thr_repository->m_send_buffers + trp_id;
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
  if (sb->m_sending.m_first_page != NULL) {
    thread_local_pool<thr_send_page> pool(&g_thr_repository->m_sb_pool, 0);
    release_list(&pool, sb->m_sending.m_first_page, sb->m_sending.m_last_page);
    pool.release_all(
        g_thr_repository->m_mm, RG_TRANSPORTER_BUFFERS,
        g_send_threads == NULL ? 0 : g_send_threads->get_send_instance(trp_id));
    sb->m_sending.m_first_page = NULL;
    sb->m_sending.m_last_page = NULL;
    sb->m_sending_size = 0;
  }

  unlock(&sb->m_send_lock);
}

static inline void register_pending_send(thr_data *selfptr, TrpId trp_id) {
  /* Mark that this trp has pending send data. */
  if (!selfptr->m_pending_send_mask.get(trp_id)) {
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
  allocate enough send buffers. In addition we call the thread local
  pack_sb_pages() several places - See header-comment for that function.

  Note that ending up here is a result of send buffers being configured
  too small relative to what is being used.
*/
static void try_pack_send_buffers(thr_data *selfptr) {
  thr_repository *rep = g_thr_repository;
  thread_local_pool<thr_send_page> *pool = &selfptr->m_send_buffer_pool;

  for (TrpId trp_id = 1; trp_id < MAX_NTRANSPORTERS; trp_id++) {
    if (globalTransporterRegistry.get_transporter(trp_id)) {
      thr_repository::send_buffer *sb = rep->m_send_buffers + trp_id;
      if (trylock(&sb->m_buffer_lock) != 0) {
        continue;  // Continue with next if busy
      }

      link_thread_send_buffers(sb, trp_id);
      if (sb->m_buffer.m_first_page != nullptr) {
        pack_sb_pages(pool, &sb->m_buffer);
      }
      unlock(&sb->m_buffer_lock);
    }
  }
  /* Release surplus buffers from local pool to global pool */
  pool->release_global(g_thr_repository->m_mm, RG_TRANSPORTER_BUFFERS,
                       selfptr->m_send_instance_no);
}

/**
 * publish thread-locally prepared send-buffer
 */
static void flush_send_buffer(thr_data *selfptr, TrpId trp_id) {
  unsigned thr_no = selfptr->m_thr_no;
  thr_send_buffer *src = selfptr->m_send_buffers + trp_id;
  thr_repository *rep = g_thr_repository;

  if (src->m_first_page == nullptr) {
    return;
  }
  assert(src->m_last_page != nullptr);

  thr_send_queue *dst = rep->m_thread_send_buffers[trp_id] + thr_no;
  thr_repository::send_buffer *sb = rep->m_send_buffers + trp_id;

  Uint32 wi = dst->m_write_index;
  Uint32 next = (wi + 1) % thr_send_queue::SIZE;
  Uint32 ri = sb->m_read_index[thr_no];

  /**
   * If thread local ring buffer of send-buffers is full:
   * Empty it by transferring them to the global send_buffer list.
   */
  if (unlikely(next == ri)) {
    lock(&sb->m_buffer_lock);
    link_thread_send_buffers(sb, trp_id);
    unlock(&sb->m_buffer_lock);
  }

  dst->m_buffers[wi] = src->m_first_page;
  wmb();
  dst->m_write_index = next;

  src->m_first_page = nullptr;
  src->m_last_page = nullptr;
}

/**
 * This is used in case send buffer gets full, to force an emergency send,
 * hopefully freeing up some buffer space for the next signal.
 */
bool mt_send_handle::forceSend(TrpId trp_id) {
  struct thr_repository *rep = g_thr_repository;
  struct thr_data *selfptr = m_selfptr;
  struct thr_repository::send_buffer *sb = rep->m_send_buffers + trp_id;

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
    selfptr->m_send_buffer_pool.release_global(
        rep->m_mm, RG_TRANSPORTER_BUFFERS, selfptr->m_send_instance_no);
    /**
     * We need a memory barrier here to prevent race between clearing lock
     *   and reading of m_force_send.
     *   CPU can reorder the load to before the clear of the lock
     */
    mb();
    if (unlikely(sb->m_force_send) || more) {
      register_pending_send(selfptr, trp_id);
    }
  }

  return true;
}

/**
 * try sending data
 */
static void try_send(thr_data *selfptr, TrpId trp_id) {
  struct thr_repository *rep = g_thr_repository;
  struct thr_repository::send_buffer *sb = rep->m_send_buffers + trp_id;

  if (trylock(&sb->m_send_lock) == 0) {
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
    selfptr->m_send_buffer_pool.release_global(
        rep->m_mm, RG_TRANSPORTER_BUFFERS, selfptr->m_send_instance_no);

    /**
     * We need a memory barrier here to prevent race between clearing lock
     *   and reading of m_force_send.
     *   CPU can reorder the load to before the clear of the lock
     */
    mb();
    if (unlikely(sb->m_force_send)) {
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
static void do_flush(struct thr_data *selfptr) {
  Uint32 i;
  Uint32 count = selfptr->m_pending_send_count;
  TrpId *trps = selfptr->m_pending_send_trps;

  for (i = 0; i < count; i++) {
    flush_send_buffer(selfptr, trps[i]);
  }
}

/**
 * Use the THRMAN block to send the WAKEUP_THREAD_ORD signal
 * to the block thread that we want to wakeup.
 */
#define MICROS_BETWEEN_WAKEUP_IDLE_THREAD 100
static inline void send_wakeup_thread_ord(struct thr_data *selfptr,
                                          NDB_TICKS now) {
  if (selfptr->m_wakeup_instance > 0) {
    Uint64 since_last =
        NdbTick_Elapsed(selfptr->m_last_wakeup_idle_thread, now).microSec();
    if (since_last > MICROS_BETWEEN_WAKEUP_IDLE_THREAD) {
      selfptr->m_signal->theData[0] = selfptr->m_wakeup_instance;
      SimulatedBlock *b = globalData.getBlock(THRMAN, selfptr->m_thr_no + 1);
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
static bool do_send(struct thr_data *selfptr, bool must_send,
                    bool assist_send) {
  Uint32 count = selfptr->m_pending_send_count;
  TrpId *trps = selfptr->m_pending_send_trps;

  const NDB_TICKS now = NdbTick_getCurrentTicks();
  selfptr->m_curr_ticks = now;
  bool pending_send = false;
  selfptr->m_watchdog_counter = 6;

  if (count == 0) {
    if (must_send && assist_send && g_send_threads &&
        selfptr->m_overload_status <= (OverloadStatus)MEDIUM_LOAD_CONST &&
        (selfptr->m_nosend == 0)) {
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
          num_trps_to_send_to, selfptr->m_thr_no, now,
          selfptr->m_watchdog_counter, selfptr->m_send_instance,
          selfptr->m_send_buffer_pool);
      NDB_TICKS after = NdbTick_getCurrentTicks();
      selfptr->m_micros_send += NdbTick_Elapsed(now, after).microSec();
    }
    return pending_send;  // send-buffers empty
  }

  /* Clear the pending list. */
  selfptr->m_pending_send_mask.clear();
  selfptr->m_pending_send_count = 0;
  selfptr->m_watchdog_counter = 6;
  for (Uint32 i = 0; i < count; i++) {
    /**
     * Make the data available for sending immediately so that
     * any other trp sending will grab this data without having
     * to wait for us to handling the other trps.
     */
    flush_send_buffer(selfptr, trps[i]);
  }
  selfptr->m_watchdog_counter = 6;
  if (g_send_threads) {
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
        selfptr->m_nosend != 0) {
      for (Uint32 i = 0; i < count; i++) {
        g_send_threads->alert_send_thread(trps[i], now, NULL);
      }
    } else {
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
      for (Uint32 i = 0; i < count; i++) {
        num_trps_inserted += g_send_threads->alert_send_thread(
            trps[i], now, selfptr->m_send_instance);
      }
      Uint32 num_trps_to_send_to = num_trps_inserted;
      if (selfptr->m_overload_status != (OverloadStatus)MEDIUM_LOAD_CONST) {
        num_trps_to_send_to++;
      }
      send_wakeup_thread_ord(selfptr, now);
      if (num_trps_to_send_to > 0) {
        pending_send = g_send_threads->assist_send_thread(
            num_trps_to_send_to, selfptr->m_thr_no, now,
            selfptr->m_watchdog_counter, selfptr->m_send_instance,
            selfptr->m_send_buffer_pool);
      }
      NDB_TICKS after = NdbTick_getCurrentTicks();
      selfptr->m_micros_send += NdbTick_Elapsed(now, after).microSec();
      g_send_threads->wake_my_send_thread_if_needed(&trps[0], count,
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
  struct thr_repository *rep = g_thr_repository;

  for (Uint32 i = 0; i < count; i++) {
    TrpId trp_id = trps[i];
    thr_repository::send_buffer *sb = rep->m_send_buffers + trp_id;

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
    if (must_send) {
      sb->m_force_send = 1;
    }

    if (trylock(&sb->m_send_lock) != 0) {
      if (!must_send) {
        /**
         * Not doing this trp now, re-add to pending list.
         *
         * As we only add from the start of an empty list, we are safe from
         * overwriting the list while we are iterating over it.
         */
        register_pending_send(selfptr, trp_id);
      } else {
        /* Other thread will send for us as we set m_force_send. */
      }
    } else  // Got send_lock
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
      const bool more = globalTransporterRegistry.performSend(trp_id);
      made_progress += sb->m_bytes_sent;
      sb->m_send_thread = NO_SEND_THREAD;
      unlock(&sb->m_send_lock);

      if (more)  // Didn't complete all my send work
      {
        register_pending_send(selfptr, trp_id);
      } else {
        /**
         * We need a memory barrier here to prevent race between clearing lock
         *   and reading of m_force_send.
         *   CPU can reorder the load to before the clear of the lock
         */
        mb();
        if (sb->m_force_send)  // Other thread forced us to do more send
        {
          made_progress++;  // Avoid false 'no progress' handling
          register_pending_send(selfptr, trp_id);
        }
      }
    }
  }  // for all trps

  selfptr->m_send_buffer_pool.release_global(rep->m_mm, RG_TRANSPORTER_BUFFERS,
                                             selfptr->m_send_instance_no);

  return (made_progress)                            // Had some progress?
             ? (selfptr->m_pending_send_count > 0)  // More do_send is required
             : false;  // All busy, or didn't find any work (-> -0)
}

#ifdef ERROR_INSERT
void mt_set_delayed_prepare(Uint32 self) {
  thr_repository *rep = g_thr_repository;
  struct thr_data *selfptr = &rep->m_thread[self];

  selfptr->m_delayed_prepare = true;
}
#endif

/**
 * These are the implementations of the TransporterSendBufferHandle methods
 * in ndbmtd.
 */
Uint32 *mt_send_handle::getWritePtr(TrpId trp_id, Uint32 len, Uint32 prio,
                                    Uint32 max, SendStatus *error) {
#ifdef ERROR_INSERT
  if (m_selfptr->m_delayed_prepare) {
    g_eventLogger->info("MT thread %u delaying in prepare",
                        m_selfptr->m_thr_no);
    NdbSleep_MilliSleep(500);
    g_eventLogger->info("MT thread %u finished delay, clearing",
                        m_selfptr->m_thr_no);
    m_selfptr->m_delayed_prepare = false;
  }
#endif

  struct thr_send_buffer *b = m_selfptr->m_send_buffers + trp_id;
  thr_send_page *p = b->m_last_page;
  if (likely(p != nullptr)) {
    assert(p->m_start == 0);  // Nothing sent until flushed

    if (likely(p->m_bytes + len <= thr_send_page::max_bytes())) {
      return (Uint32 *)(p->m_data + p->m_bytes);
    }
    // TODO: maybe dont always flush on page-boundary ???
    flush_send_buffer(m_selfptr, trp_id);
    if (!g_send_threads) try_send(m_selfptr, trp_id);
  }
  if (unlikely(len > thr_send_page::max_bytes())) {
    *error = SEND_MESSAGE_TOO_BIG;
    return 0;
  }

  bool first = true;
  while (first) {
    if (likely((p = m_selfptr->m_send_buffer_pool.seize(
                    g_thr_repository->m_mm, RG_TRANSPORTER_BUFFERS,
                    m_selfptr->m_send_instance_no)) != nullptr)) {
      p->m_bytes = 0;
      p->m_start = 0;
      p->m_next = 0;
      b->m_first_page = b->m_last_page = p;
      return (Uint32 *)p->m_data;
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
static Uint64 mt_get_send_buffer_bytes(TrpId trp_id) {
  thr_repository *rep = g_thr_repository;
  thr_repository::send_buffer *sb = &rep->m_send_buffers[trp_id];
  const Uint64 total_send_buffer_size =
      sb->m_buffered_size + sb->m_sending_size;
  return total_send_buffer_size;
}

#if 0
/**
 * *getSendBufferLevel() is currently unused.
 * As similar functionality is likely needed in near future
 * (overload control, ndbinfo-transporter tables, ...)
 * it is kept for now
 */
void
mt_getSendBufferLevel(Uint32 self[[maybe_unused]]
                      TrpId trp_id, SB_LevelType &level)
{
  Resource_limit rl;
  const Uint32 page_size = thr_send_page::PGSIZE;
  thr_repository *rep = g_thr_repository;
  thr_repository::send_buffer *sb = &rep->m_send_buffers[trp_id];
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
mt_send_handle::getSendBufferLevel(TrpId trp_id,
                                   SB_LevelType &level)
{
  return;
}
#endif

Uint32 mt_send_handle::updateWritePtr(TrpId trp_id, Uint32 lenBytes,
                                      Uint32 prio) {
  struct thr_send_buffer *b = m_selfptr->m_send_buffers + trp_id;
  thr_send_page *p = b->m_last_page;
  p->m_bytes += lenBytes;
  return p->m_bytes;
}

/*
 * Insert a signal in a job queue.
 * Need write_lock protecting thr_job_queue to be held.
 *
 * Beware that the single consumer read the job_queue *without* lock -
 * It only use a memory barrier to get the updated write_pos. We need to
 * take care that signal is fully written before write_pos is updated.
 *
 * The new_buffer is a job buffer to use if the current one gets full. If used,
 * we return true, indicating that the caller should allocate a new one for
 * the next call. (This is done to allow to insert under lock, but do the
 * allocation outside the lock).
 */

static inline void publish_position(thr_job_buffer *write_buffer,
                                    Uint32 write_pos) {
  /*
   * Publish the job-queue write.
   * Need a write memory barrier here, as this might make signal data visible
   * to the (single) reader thread.
   */
  wmb();
  write_buffer->m_len = write_pos;
}

/**
 * Check, and if allowed, add the 'new_buffer' to our job_queue of
 * buffer pages containing signals.
 * If the 'job_queue' is full, the new buffer is not inserted,
 * and 'true' is returned. It is up to the caller to handle 'full'.
 * (In many cases it can be a critical error)
 *
 * Note that the thread always hold a spare 'new_buffer' to be used if we
 * filled the current buffer. Thus, we can always complete a flush/write of
 * signals while holding the write_lock, without having to allocate
 * a new buffer. (To reduce time the write_lock is held.)
 * Another 'new_buffer' will be allocated after lock is released.
 */
static bool check_next_index_position(thr_job_queue *q,
                                      struct thr_job_buffer *new_buffer) {
  /**
   * We make sure that there is always room for at least one signal in the
   * current buffer in the queue, so one insert is always possible without
   * adding a new buffer while holding the write_lock. (To reduce time the
   * write_lock is held). When write_buffer is full, we grab the spare
   * 'new_buffer' and inform the callee
   */
  NDB_PREFETCH_WRITE(&new_buffer->m_len);
  const unsigned queue_size = q->m_size;
  Uint32 write_index = q->m_write_index;
  write_index = (write_index + 1) & (queue_size - 1);
  NDB_PREFETCH_WRITE(&q->m_buffers[write_index]);

  if (unlikely(write_index == q->m_cached_read_index))  // Is full?
  {
    /**
     * We use local cached copy of m_read_index for JBB handling.
     * m_read_index is updated every time we executed around 300
     * signals in the job buffer. Since this is read every time
     * we move to a new page while writing it is an almost certain
     * CPU cache miss. We only need it to avoid running out of
     * job buffer. So we record the current m_read_index in
     * m_cached_read_index. This is stored in the same cache line
     * as m_write_index which we are sure we have access to here.
     */
    Uint32 read_index = q->m_read_index;
    if (write_index == read_index)  // Is really full?
    {
      return true;
    }
    q->m_cached_read_index = read_index;
  }
  assert(new_buffer->m_len == 0);
  q->m_buffers[write_index] = new_buffer;

  // Memory barrier ensures that m_buffers[] contain new_buffer
  // before 'write_index' referring it is written.
  wmb();
  q->m_write_index = write_index;

  // Note that m_current_write_* is only intended for use by the *writer*.
  // Thus, there are no memory barriers protecting the buffer vs len conistency.
  q->m_current_write_buffer = new_buffer;
  q->m_current_write_buffer_len = 0;
  return false;
}

/**
 * insert_prioa_signal and publish_prioa_signal:
 * As naming suggest, these are for JBA signals only.
 * There is a similar insert_local_signal() for JBB signals,
 * which 'flush' and 'publish' chunks of signals.
 *
 * prioa_signals are effectively flushed and published for
 * each signal.
 */
static inline bool publish_prioa_signal(thr_job_queue *q, Uint32 write_pos,
                                        struct thr_job_buffer *write_buffer,
                                        struct thr_job_buffer *new_buffer) {
  publish_position(write_buffer, write_pos);
  if (unlikely(write_pos + MAX_SIGNAL_SIZE > thr_job_buffer::SIZE)) {
    // Not room for one more signal
    new_buffer->m_prioa = true;
    const bool jba_full = check_next_index_position(q, new_buffer);
    if (jba_full) {
      // Assume rather low traffic on JBA.
      // Contrary to JBB, signals are not first stored in a local_buffer.
      // Thus a full JBA is always immediately critical.
      job_buffer_full(0);
    }
    return true;  // Buffer new_buffer used
  }
  return false;  // Buffer new_buffer not used
}

static inline Uint32 copy_signal(Uint32 *dst, const SignalHeader *sh,
                                 const Uint32 *data, const Uint32 secPtr[3]) {
  const Uint32 datalen = sh->theLength;
  memcpy(dst, sh, sizeof(*sh));
  Uint32 siglen = (sizeof(*sh) >> 2);
  memcpy(dst + siglen, data, 4 * datalen);
  siglen += datalen;
  const Uint32 noOfSections = sh->m_noOfSections;
  for (Uint32 i = 0; i < noOfSections; i++) dst[siglen++] = secPtr[i];
  return siglen;
}

static bool insert_prioa_signal(thr_job_queue *q, const SignalHeader *sh,
                                const Uint32 *data, const Uint32 secPtr[3],
                                thr_job_buffer *new_buffer) {
  thr_job_buffer *write_buffer = q->m_current_write_buffer;
  Uint32 write_pos = q->m_current_write_buffer_len;
  NDB_PREFETCH_WRITE(&write_buffer->m_len);
  const Uint32 siglen =
      copy_signal(write_buffer->m_data + write_pos, sh, data, secPtr);
  write_pos += siglen;

#if SIZEOF_CHARP == 8
  /* Align to 8-byte boundary, to ensure aligned copies. */
  write_pos = (write_pos + 1) & ~((Uint32)1);
#endif
  q->m_current_write_buffer_len = write_pos;
  return publish_prioa_signal(q, write_pos, write_buffer, new_buffer);
}

// #define DEBUG_LOAD_INDICATOR 1
#ifdef DEBUG_LOAD_INDICATOR
#define debug_load_indicator(selfptr)                          \
  g_eventLogger->info("thr_no:: %u, set load_indicator to %u", \
                      selfptr->m_thr_no, selfptr->m_load_indicator);
#else
#define debug_load_indicator(x)
#endif

/**
 * Check all incoming m_jbb[] instances for available signals.
 * Set up the thread-local m_jbb_read_state[] to reflect JBB state.
 * Also set jbb_read_mask to contain the JBBs containing data.
 *
 * As reading the m_jbb[] will access thread shared data, which is cache-line
 * invalidated by the writer, we try to avoid loading read_state from shared
 * data when the local read_state already contain a sufficient amount of
 * signals to execute.
 */
static inline bool read_all_jbb_state(thr_data *selfptr,
                                      bool check_before_sleep) {
  if (!selfptr->m_read_jbb_state_consumed) {
    return false;
  }
  /**
   * Prefetching all m_write_index instances gives a small but visible
   * improvement.
   */
#if defined(__GNUC__)
  for (Uint32 jbb_instance = 0; jbb_instance < glob_num_job_buffers_per_thread;
       jbb_instance++) {
    thr_job_queue *jbb = selfptr->m_jbb + jbb_instance;
    NDB_PREFETCH_READ(&jbb->m_write_index);
  }
#endif

  selfptr->m_jbb_read_mask.clear();
  Uint32 tot_num_words = 0;
  for (Uint32 jbb_instance = 0; jbb_instance < glob_num_job_buffers_per_thread;
       jbb_instance++) {
    const thr_job_queue *jbb = selfptr->m_jbb + jbb_instance;
    thr_jb_read_state *r = selfptr->m_jbb_read_state + jbb_instance;

    /**
     * We avoid reading the cache-shared write-pointers until the
     * thread local thr_jb_read_state indicate a possibly empty read queue.
     * Writer side might have updated write pointer, thus invalidating our
     * cache line for it. We will like to avoid memory stalls reading these
     * until we really need to refresh the written positions.
     */
    const Uint32 read_index = r->m_read_index;
    const Uint32 read_pos = r->m_read_pos;
    Uint32 write_index = r->m_write_index;
    Uint32 read_end = r->m_read_end;

    if (write_index == read_index)  // Possibly empty, reload thread-local state
    {
      write_index = jbb->m_write_index;
      if (write_index != r->m_write_index) {
        /**
         * Found new JBB pages. Need to make sure that we do not read-reorder
         * 'm_write_index' vs 'read_buffer->m_len'.
         */
        rmb();
      }
      r->m_write_index = write_index;
      r->m_read_end = read_end = r->m_read_buffer->m_len;
      /**
       * Note ^^ that we will need a later rmb() before we can safely read the
       * m_buffer[] contents up to 'm_read_end': We need to synch on the wmb()
       * in publish_position(), such that the m_buffer[] contents itself
       * has been fully written before we can start executing from it.
       *
       * To reduce the mem-synch stalls, we do this once for all JBB's, just
       * before we execute_signals().
       */
      if (!r->is_empty()) {
        selfptr->m_jbb_read_mask.set(jbb_instance);
      }
    } else {
      /**
       * Only update the thread-local 'write_index'.
       * 'read_end' should already contain the end of current read_buffer.
       */
      r->m_write_index = write_index = jbb->m_write_index;
      selfptr->m_jbb_read_mask.set(jbb_instance);
    }

    // Calculate / estimate 'num_words' being available
    Uint32 num_pages;
    if (likely(write_index >= read_index)) {
      num_pages = write_index - read_index;
    } else {
      num_pages = read_index - write_index;
    }

    assert(read_end >= read_pos);
    Uint32 num_words = read_end - read_pos;  // Remaining on current page
    if (num_pages > 0) {
      // Rest of the pages will be (almost) full:
      num_words += (num_pages - 1) * thr_job_buffer::SIZE;

      /**
       * Estimate the written size in the 'current_write_buffer':
       * Although producer set the 'm_current_write_buffer_len', it is not
       * intended to be used by the consumer (No concurrency control).
       * We just assume as an estimate:
       *  - if num_pages==1 we just wrapped to a new page which is ~empty.
       *  - if multiple pages, the last is assumed half-filled.
       */
      if (num_pages > 1) num_words += thr_job_buffer::SIZE / 2;
    }
    tot_num_words += num_words;
  }
  selfptr->m_cpu_percentage_changed = true;
  /**
   * m_jbb_estimated_queue_size_in_words is the queue size when we started the
   * last non-empty execution in this thread.
   *
   * The normal behaviour of our scheduler is to execute read_all_jbb_state
   * twice before going to sleep, first in run_job_buffers and next in
   * check_queues_empty. We always set the estimated queue size after
   * waking up and when the queue size is non-empty. It is measured in
   * words of signal. Thus we ignore the common checks where it is
   * empty that can happen either just before going to sleep or when
   * spinning.
   */
  const bool jbb_empty = selfptr->m_jbb_read_mask.isclear();
  if (!check_before_sleep) {
    selfptr->m_jbb_execution_steps++;
    selfptr->m_jbb_accumulated_queue_size += tot_num_words;
  } else if (jbb_empty) {
    if (selfptr->m_load_indicator > 1) {
      selfptr->m_load_indicator = 1;
      debug_load_indicator(selfptr);
    }
  }
  if (!jbb_empty || selfptr->m_jbb_estimate_next_set) {
    selfptr->m_jbb_estimate_next_set = false;
    Uint32 current_queue_size = selfptr->m_jbb_estimated_queue_size_in_words;
    Uint32 new_queue_size = tot_num_words;
    Uint32 diff = AVERAGE_SIGNAL_SIZE;
    if (new_queue_size > 8 * AVERAGE_SIGNAL_SIZE) {
      diff = 3 * AVERAGE_SIGNAL_SIZE;
    } else if (new_queue_size > 4 * AVERAGE_SIGNAL_SIZE) {
      diff = 2 * AVERAGE_SIGNAL_SIZE;
    }
    if (new_queue_size >= (current_queue_size + diff) ||
        (current_queue_size >= (new_queue_size + diff))) {
      if (!(new_queue_size < 2 * AVERAGE_SIGNAL_SIZE &&
            current_queue_size < 2 * AVERAGE_SIGNAL_SIZE)) {
        /**
         * Update m_jbb_estimated_queue_size_in_words only if the new
         * queue size has at least changed by AVERAGE_SIGNAL_SIZE and
         * also if both the new and the current queue size is very
         * low we can also ignore updating it. Similarly if both are
         * very high we can also ignore updating it.
         *
         * The reason to avoid updating it is that it will cause a
         * CPU cache miss in all readers of the variable. Readers
         * are all receive threads and TC threads. So can cause a
         * significant extra CPU load and memory load if it is
         * updated to often.
         */
        selfptr->m_jbb_estimated_queue_size_in_words = new_queue_size;
#ifdef DEBUG_SCHED_STATS
        Uint32 inx =
            selfptr->m_jbb_estimated_queue_size_in_words / AVERAGE_SIGNAL_SIZE;
        if (inx >= 10) {
          inx = 9;
        }
        selfptr->m_jbb_estimated_queue_stats[inx]++;
#endif
      }
    }
  } else {
    selfptr->m_jbb_estimate_next_set = check_before_sleep;
  }
#ifdef DEBUG_SCHED_STATS
  selfptr->m_jbb_total_words += tot_num_words;
#endif
  selfptr->m_read_jbb_state_consumed = jbb_empty;
  return jbb_empty;
}

static inline bool read_jba_state(thr_data *selfptr) {
  thr_jb_read_state *r = &(selfptr->m_jba_read_state);
  const Uint32 new_write_index = selfptr->m_jba.m_write_index;
  if (r->m_write_index != new_write_index) {
    /**
     * There are new JBA pages, we need to make sure that any updates to
     * 'read_buffer->m_len' are not read-reordered relative to 'write_index'
     * (Missing the signals appended to m_len before prev block became 'full')
     */
    r->m_write_index = new_write_index;
    rmb();
  }

  /**
   * Will need a later rmb()-synch before we can execute_signals() up to
   * '...buffer->m_len', see comment in read_all_jbb_state().
   */
  r->m_read_end = r->m_read_buffer->m_len;
  return r->is_empty();
}

static inline bool check_for_input_from_ndbfs(struct thr_data *thr_ptr,
                                              Signal *signal) {
  return thr_ptr->m_send_packer.check_reply_from_ndbfs(signal);
}

/* Check all job queues, return true only if all are empty. */
static bool check_queues_empty(thr_data *selfptr) {
  if (selfptr->m_thr_no == glob_ndbfs_thr_no) {
    if (check_for_input_from_ndbfs(selfptr, selfptr->m_signal)) return false;
  }
  bool empty = read_jba_state(selfptr);
  if (!empty) return false;

  return read_all_jbb_state(selfptr, true);
}

static inline void sendpacked(struct thr_data *thr_ptr, Signal *signal) {
  thr_ptr->m_watchdog_counter = 15;
  thr_ptr->m_send_packer.pack(signal);
}

static void flush_all_local_signals_and_wakeup(struct thr_data *selfptr);

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
static void handle_scheduling_decisions(thr_data *selfptr, Signal *signal,
                                        Uint32 &send_sum, Uint32 &flush_sum,
                                        bool &pending_send) {
  if (send_sum >= selfptr->m_max_signals_before_send) {
    /* Try to send, but skip for now in case of lock contention. */
    sendpacked(selfptr, signal);
    selfptr->m_watchdog_counter = 6;
    flush_all_local_signals_and_wakeup(selfptr);
    pending_send = do_send(selfptr, false, false);
    selfptr->m_watchdog_counter = 20;
    send_sum = 0;
    flush_sum = 0;
  } else if (flush_sum >= selfptr->m_max_signals_before_send_flush) {
    /* Send buffers append to send queues to dst. trps. */
    sendpacked(selfptr, signal);
    selfptr->m_watchdog_counter = 6;
    flush_all_local_signals_and_wakeup(selfptr);
    do_flush(selfptr);
    selfptr->m_watchdog_counter = 20;
    flush_sum = 0;
  }
}

#if defined(USE_INIT_GLOBAL_VARIABLES)
void mt_clear_global_variables(thr_data *);
#endif

/**
 * prepare_congested_execution()
 *
 * If this thread is in a congested JBB state, its perjb-quota will be
 * reduced, possibly even set to '0'. If we didn't get a max 'perjb' quota,
 * our out buffers are about to fill up. This thread is thus effectively
 * slowed down in order to let other threads consume from our out buffers.
 * Eventually, when 'perjb==0', we will have to wait/sleep for buffers to
 * become available.
 *
 * This can bring us into a circular wait-lock, where threads are stalled
 * due to full out buffers. The same thread may also have full in buffers,
 * thus blocking other threads from progressing. The entire scheduler will
 * then be stuck.
 *
 * This function check the JBB queues we are about to execute signals from.
 * if they are filled to a level where the producer will detect a congestion,
 * we allow some 'extra_signals' to be executed from this JBB, such that the
 * congestion hopefully can be reduced.
 *
 * The amount of extra_signals allowed are scaled proportional to
 * the congestion level in each job buffer.
 */
static void prepare_congested_execution(thr_data *selfptr) {
  unsigned congestion[NUM_JOB_BUFFERS_PER_THREAD];
  unsigned total_congestion = 0;

  // Assumed precondition:
  assert(!selfptr->m_congested_threads_mask.isclear());

  /**
   * Two steps:
   * 1. Collect amount of congestion (in job_buffer pages) in the JBBs.
   * 2. Allocate extra_signals proportional to congestion level
   */
  for (unsigned jbb_instance = selfptr->m_jbb_read_mask.find_first();
       jbb_instance != BitmaskImpl::NotFound;
       jbb_instance = selfptr->m_jbb_read_mask.find_next(jbb_instance + 1)) {
    selfptr->m_extra_signals[jbb_instance] = 0;

    thr_job_queue *queue = &selfptr->m_jbb[jbb_instance];
    const unsigned free = get_free_in_queue(queue);
    if (free <= thr_job_queue::CONGESTED) {
      // In queue is congested as well. Calculate the total number of
      // job_buffers to consume from in-queue(s) to get out of overload.
      congestion[jbb_instance] = (thr_job_queue::CONGESTED - free) + 1;
      total_congestion += congestion[jbb_instance];
    } else {
      congestion[jbb_instance] = 0;
    }
  }

  if (unlikely(total_congestion > 0) && selfptr->m_total_extra_signals > 0) {
    // Found congestion, allocate 'extra_signals' proportional to congestion
    for (unsigned jbb_instance = selfptr->m_jbb_read_mask.find_first();
         jbb_instance != BitmaskImpl::NotFound;
         jbb_instance = selfptr->m_jbb_read_mask.find_next(jbb_instance + 1)) {
      if (congestion[jbb_instance] > 0) {
        selfptr->m_extra_signals[jbb_instance] =
            std::max(1u, congestion[jbb_instance] *
                             selfptr->m_total_extra_signals / total_congestion);
      } else if (selfptr->m_max_signals_per_jb == 0) {
        // Need to run a bit in order to avoid starvation of the JBB's
        selfptr->m_extra_signals[jbb_instance] = 1;
      }
    }
  }
}

static void recheck_congested_job_buffers(thr_data *selfptr);

/*
 * Execute at most MAX_SIGNALS signals from one job queue, updating local read
 * state as appropriate.
 *
 * Returns number of signals actually executed.
 */
static Uint32 execute_signals(thr_data *selfptr, thr_job_queue *q,
                              thr_jb_read_state *r, Signal *sig,
                              Uint32 max_signals) {
  Uint32 num_signals;
  Uint32 extra_signals = 0;
  Uint32 read_index = r->m_read_index;
  Uint32 write_index = r->m_write_index;
  Uint32 read_pos = r->m_read_pos;
  Uint32 read_end = r->m_read_end;
  Uint32 *watchDogCounter = &selfptr->m_watchdog_counter;

  if (read_index == write_index && read_pos >= read_end)
    return 0;  // empty read_state

  thr_job_buffer *read_buffer = r->m_read_buffer;
  NDB_PREFETCH_READ(read_buffer->m_data + read_pos);  // Load cache

  for (num_signals = 0; num_signals < max_signals; num_signals++) {
    *watchDogCounter = 12;
    while (unlikely(read_pos >= read_end)) {
      if (read_index == write_index) {
        /* No more available now. */
        selfptr->m_stat.m_exec_cnt += num_signals;
        return num_signals;
      } else {
        /* Move to next buffer. */
        const unsigned queue_size = q->m_size;
        read_index = (read_index + 1) & (queue_size - 1);
        NDB_PREFETCH_READ(q->m_buffers[read_index]->m_data);
        if (likely(read_buffer != &empty_job_buffer)) {
          release_buffer(g_thr_repository, selfptr->m_thr_no, read_buffer);
        }
        read_buffer = q->m_buffers[read_index];
        read_pos = 0;
        read_end = read_buffer->m_len;
        /* Update thread-local read state. */
        r->m_read_index = q->m_read_index = read_index;
        r->m_read_buffer = read_buffer;
        r->m_read_pos = read_pos;
        r->m_read_end = read_end;
        wakeup_all(&selfptr->m_congestion_waiter);
      }
    }
    /*
     * These pre-fetching were found using OProfile to reduce cache misses.
     * (Though on Intel Core 2, they do not give much speedup, as apparently
     * the hardware prefetcher is already doing a fairly good job).
     */
    NDB_PREFETCH_READ(read_buffer->m_data + read_pos + 16);
    NDB_PREFETCH_WRITE((Uint32 *)&sig->header + 16);

#ifdef VM_TRACE
    /* Find reading / propagation of junk */
    sig->garbage_register();
#endif
    /* Now execute the signal. */
    SignalHeader *s =
        reinterpret_cast<SignalHeader *>(read_buffer->m_data + read_pos);
    Uint32 seccnt = s->m_noOfSections;
    Uint32 siglen = (sizeof(*s) >> 2) + s->theLength;
    if (siglen > 16) {
      NDB_PREFETCH_READ(read_buffer->m_data + read_pos + 32);
    }
    Uint32 bno = blockToMain(s->theReceiversBlockNumber);
    Uint32 ino = blockToInstance(s->theReceiversBlockNumber);
    SimulatedBlock *block = globalData.mt_getBlock(bno, ino);
    assert(block != 0);

    Uint32 gsn = s->theVerId_signalNumber;
    *watchDogCounter = 1 + (bno << 8) + (gsn << 20);

    /* Must update original buffer so signal dump will see it. */
    s->theSignalId = selfptr->m_signal_id_counter++;
    memcpy(&sig->header, s, 4 * siglen);
    for (Uint32 i = 0; i < seccnt; i++) {
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
    if (globalData.testOn) {  // wl4391_todo segments
      SegmentedSectionPtr ptr[3];
      ptr[0].i = sig->m_sectionPtrI[0];
      ptr[1].i = sig->m_sectionPtrI[1];
      ptr[2].i = sig->m_sectionPtrI[2];
      ::getSections(seccnt, ptr);
      globalSignalLoggers.executeSignal(*s, 0, &sig->theData[0],
                                        globalData.ownId, ptr, seccnt);
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

static Uint32 run_job_buffers(thr_data *selfptr, Signal *sig, Uint32 &send_sum,
                              Uint32 &flush_sum, bool &pending_send) {
  Uint32 signal_count = 0;
  Uint32 signal_count_since_last_zero_time_queue = 0;

  if (read_all_jbb_state(selfptr, false)) {
    // JBB is empty, execute any JBA signals
    while (!read_jba_state(selfptr)) {
      rmb();  // See memory barrier reasoning right below
      selfptr->m_sent_local_prioa_signal = false;
      static Uint32 max_prioA = thr_job_queue::SIZE * thr_job_buffer::SIZE;
      Uint32 num_signals =
          execute_signals(selfptr, &(selfptr->m_jba),
                          &(selfptr->m_jba_read_state), sig, max_prioA);
      signal_count += num_signals;
      send_sum += num_signals;
      flush_sum += num_signals;
      if (!selfptr->m_sent_local_prioa_signal) {
        /**
         * Break out of loop if there was no prio A signals generated
         * from the local execution.
         */
        break;
      }
    }
    // As we had no JBB signals, we are done
    assert(selfptr->m_jbb_read_mask.isclear());
    return signal_count;
  }

  /**
   * A load memory barrier to ensure that we see the m_buffers[] content,
   * referred by the jbb_states, before we start executing signals.
   * See comments in read_all_jbb_state() and read_jba_state as well.
   */
  rmb();

  if (unlikely(!selfptr->m_congested_threads_mask.isclear())) {
    // Will assign 'extra' signal execution to be used by congested JBB's
    prepare_congested_execution(selfptr);
  }

  /**
   * We might have a JBB resume point:
   *  - For the main thread we can stop at any job buffer.
   *  - Other threads could stop execution due to JB congestion.
   */
  const Uint32 first_jbb_no = selfptr->m_next_jbb_no;
  selfptr->m_watchdog_counter = 13;
  for (unsigned jbb_instance = selfptr->m_jbb_read_mask.find_next(first_jbb_no);
       jbb_instance != BitmaskImpl::NotFound;
       jbb_instance = selfptr->m_jbb_read_mask.find_next(jbb_instance + 1)) {
    /* Read the prio A state often, to avoid starvation of prio A. */
    while (!read_jba_state(selfptr)) {
      rmb();  // See memory barrier reasoning above
      selfptr->m_sent_local_prioa_signal = false;
      static Uint32 max_prioA = thr_job_queue::SIZE * thr_job_buffer::SIZE;
      Uint32 num_signals =
          execute_signals(selfptr, &(selfptr->m_jba),
                          &(selfptr->m_jba_read_state), sig, max_prioA);
      signal_count += num_signals;
      send_sum += num_signals;
      flush_sum += num_signals;
      if (!selfptr->m_sent_local_prioa_signal) {
        /**
         * Break out of loop if there was no prio A signals generated
         * from the local execution.
         */
        break;
      }
    }

    thr_job_queue *queue = selfptr->m_jbb + jbb_instance;
    thr_jb_read_state *read_state = selfptr->m_jbb_read_state + jbb_instance;
    /**
     * Contended queues get an extra execute quota:
     *
     * If we didn't get a max 'perjb' quota, our out buffers
     * are about to fill up. This thread is thus effectively
     * slowed down in order to let other threads consume from
     * our out buffers. Eventually, when 'perjb==0', we will
     * have to wait/sleep for buffers to become available.
     *
     * This can bring us into a circular wait-lock, where
     * threads are stalled due to full out buffers. The same
     * thread may also have full in buffers, thus blocking other
     * threads from progressing. This could bring us into a
     * circular wait-lock, where no threads are able to progress.
     * The entire scheduler will then be stuck.
     *
     * We try to avoid this situation by reserving some
     * 'm_extra_signals[]' which are only used to consume
     * from 'almost full' in-buffers. We will then reduce the
     * risk of ending up in the above wait-lock.
     *
     * Exclude receiver threads, as there can't be a
     * circular wait between recv-thread and workers.
     */
    Uint32 perjb = selfptr->m_max_signals_per_jb;
    Uint32 extra = 0;

    if (perjb < MAX_SIGNALS_PER_JB)  // Has a job buffer contention
    {
      // Prefer a tighter JB-quota control when executing in congested state:
      recheck_congested_job_buffers(selfptr);
      perjb = selfptr->m_max_signals_per_jb;
      extra = selfptr->m_extra_signals[jbb_instance];
    }

#ifdef ERROR_INSERT

#define MIXOLOGY_MIX_MT_JBB 1

    if (unlikely(globalEmulatorData.theConfiguration->getMixologyLevel() &
                 MIXOLOGY_MIX_MT_JBB)) {
      /**
       * Let's maximise interleaving to find inter-thread
       * signal order dependency bugs
       */
      perjb = 1;
      extra = 0;
    }
#endif

    /* Now execute prio B signals from one thread. */
    const Uint32 max_signals = std::min(perjb + extra, MAX_SIGNALS_PER_JB);
    const Uint32 num_signals =
        execute_signals(selfptr, queue, read_state, sig, max_signals);

    if (likely(num_signals > 0)) {
      signal_count += num_signals;
      send_sum += num_signals;
      flush_sum += num_signals;
      handle_scheduling_decisions(selfptr, sig, send_sum, flush_sum,
                                  pending_send);

      if (signal_count - signal_count_since_last_zero_time_queue >
          (MAX_SIGNALS_EXECUTED_BEFORE_ZERO_TIME_QUEUE_SCAN -
           MAX_SIGNALS_PER_JB)) {
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
      /**
       * We might return before all JBB's has been executed when:
       * 1. When execution in main thread, which can sometimes be a bit
       *    more lengthy.
       * 2. Last execute_signals() filled the job_buffers to a level
       *    where normal execution can't continue.
       *
       * We ensure that we don't miss out on heartbeats and other
       * important things by returning to upper levels, where we handle_full,
       * checking scan_time_queues and decide further scheduling strategies.
       */
      if (selfptr->m_thr_no == 0 ||                           // 1.
          (selfptr->m_max_signals_per_jb == 0 && perjb > 0))  // 2.
      {
        // We will resume execution from next jbb_instance later.
        jbb_instance = selfptr->m_jbb_read_mask.find_next(jbb_instance + 1);
        if (jbb_instance == BitmaskImpl::NotFound) {
          jbb_instance = 0;
        }
        selfptr->m_next_jbb_no = jbb_instance;
        return signal_count;
      }
    }
  }
  // We completed all jbb_instances
  selfptr->m_read_jbb_state_consumed = true;
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

static inline Uint32 block2ThreadId(Uint32 block, Uint32 instance) {
  assert(block >= MIN_BLOCK_NO && block <= MAX_BLOCK_NO);
  Uint32 index = block - MIN_BLOCK_NO;
  assert(instance < NDB_ARRAY_SIZE(thr_map[index]));
  const thr_map_entry &entry = thr_map[index][instance];
  assert(entry.thr_no < glob_num_threads);
  return entry.thr_no;
}

void add_thr_map(Uint32 main, Uint32 instance, Uint32 thr_no) {
  assert(main == blockToMain(main));
  Uint32 index = main - MIN_BLOCK_NO;
  assert(index < NO_OF_BLOCKS);
  assert(instance < NDB_ARRAY_SIZE(thr_map[index]));

  SimulatedBlock *b = globalData.getBlock(main, instance);
  require(b != 0);

  /* Block number including instance. */
  Uint32 block = numberToBlock(main, instance);

  require(thr_no < glob_num_threads);
  struct thr_repository *rep = g_thr_repository;
  struct thr_data *thr_ptr = &rep->m_thread[thr_no];

  /* Add to list. */
  {
    Uint32 i;
    for (i = 0; i < thr_ptr->m_instance_count; i++)
      require(thr_ptr->m_instance_list[i] != block);
  }
  require(thr_ptr->m_instance_count < MAX_INSTANCES_PER_THREAD);
  thr_ptr->m_instance_list[thr_ptr->m_instance_count++] = block;
  thr_ptr->m_send_packer.insert(b);

  SimulatedBlock::ThreadContext ctx;
  ctx.threadId = thr_no;
  ctx.jamBuffer = &thr_ptr->m_jam;
  ctx.watchDogCounter = &thr_ptr->m_watchdog_counter;
  ctx.sectionPoolCache = &thr_ptr->m_sectionPoolCache;
  ctx.pHighResTimer = &thr_ptr->m_curr_ticks;
  b->assignToThread(ctx);

  /* Create entry mapping block to thread. */
  thr_map_entry &entry = thr_map[index][instance];
  require(entry.thr_no == thr_map_entry::NULL_THR_NO);
  entry.thr_no = thr_no;
}

/* Static assignment of main instances (before first signal). */
void mt_init_thr_map() {
  /**
   * Keep mt-classic assignments in MT LQH.
   *
   * thr_GLOBAL refers to the main thread blocks, thus they are located
   * where the main thread blocks are located.
   *
   * thr_LOCAL refers to the rep thread blocks, thus they are located
   * where the rep thread blocks are located.
   */
  Uint32 thr_GLOBAL = 0;
  Uint32 thr_LOCAL = 1;

  if (globalData.ndbMtMainThreads == 1) {
    /**
     * No rep thread is created, this means that we will put all blocks
     * into the main thread that are not multi-threaded.
     */
    thr_LOCAL = 0;
  } else if (globalData.ndbMtMainThreads == 0) {
    Uint32 main_thread_no =
        globalData.ndbMtLqhThreads + globalData.ndbMtQueryThreads +
        globalData.ndbMtRecoverThreads + globalData.ndbMtTcThreads;
    thr_LOCAL = main_thread_no;
    thr_GLOBAL = main_thread_no;
  }

  /**
   * For multithreaded blocks we will assign the
   * Proxy blocks below to their thread.
   *
   * The mapping of instance to block object is handled
   * by the call to create the block object.
   */
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
  add_thr_map(DBQLQH, 0, thr_LOCAL);
  add_thr_map(DBQACC, 0, thr_LOCAL);
  add_thr_map(DBQTUP, 0, thr_LOCAL);
  add_thr_map(DBQTUX, 0, thr_LOCAL);
  add_thr_map(QBACKUP, 0, thr_LOCAL);
  add_thr_map(QRESTORE, 0, thr_LOCAL);
}

Uint32 mt_get_instance_count(Uint32 block) {
  switch (block) {
    case DBLQH:
    case DBACC:
    case DBTUP:
    case DBTUX:
    case BACKUP:
    case RESTORE:
      return globalData.ndbMtLqhWorkers;
      break;
    case DBQLQH:
    case DBQACC:
    case DBQTUP:
    case DBQTUX:
    case QBACKUP:
    case QRESTORE:
      return globalData.ndbMtQueryThreads + globalData.ndbMtRecoverThreads;
    case PGMAN:
      return globalData.ndbMtLqhWorkers + 1;
      break;
    case DBTC:
    case DBSPJ:
      return globalData.ndbMtTcWorkers;
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

void mt_add_thr_map(Uint32 block, Uint32 instance) {
  Uint32 num_lqh_threads = globalData.ndbMtLqhThreads;
  Uint32 num_tc_threads = globalData.ndbMtTcThreads;
  Uint32 thr_no = globalData.ndbMtMainThreads;
  Uint32 num_query_threads =
      globalData.ndbMtQueryThreads + globalData.ndbMtRecoverThreads;

  if (num_lqh_threads == 0 && globalData.ndbMtMainThreads == 0) {
    /**
     * ndbd emulation, all blocks are in the receive thread.
     */
    thr_no = 0;
    require(num_tc_threads == 0);
    require(num_query_threads == 0);
    require(globalData.ndbMtMainThreads == 0);
    require(globalData.ndbMtReceiveThreads == 1);
    add_thr_map(block, instance, thr_no);
    return;
  } else if (num_lqh_threads == 0) {
    /**
     * Configuration optimised for 1 CPU core with 2 CPUs.
     * This has a receive thread + 1 thread for main, rep, ldm and tc
     * And also for 3 CPUs where the number of ndbMtRecoverThreads is 2.
     */
    thr_no = 0;
    require(num_tc_threads == 0);
    require(globalData.ndbMtQueryThreads == 0);
    require(globalData.ndbMtRecoverThreads == 0 ||
            globalData.ndbMtRecoverThreads == 1 ||
            globalData.ndbMtRecoverThreads == 2);
    require(globalData.ndbMtMainThreads == 1);
    require(globalData.ndbMtReceiveThreads == 1);
    num_lqh_threads = 1;
  }
  require(instance != 0);
  switch (block) {
    case DBLQH:
    case DBACC:
    case DBTUP:
    case DBTUX:
    case BACKUP:
    case RESTORE:
      thr_no += (instance - 1) % num_lqh_threads;
      break;
    case DBQLQH:
    case DBQACC:
    case DBQTUP:
    case DBQTUX:
    case QBACKUP:
    case QRESTORE:
      thr_no += num_lqh_threads + (instance - 1);
      break;
    case PGMAN:
      if (instance == num_lqh_threads + 1) {
        // Put extra PGMAN together with it's Proxy
        thr_no = block2ThreadId(block, 0);
      } else {
        thr_no += (instance - 1) % num_lqh_threads;
      }
      break;
    case DBTC:
    case DBSPJ: {
      if (globalData.ndbMtTcThreads == 0 && globalData.ndbMtMainThreads > 0) {
        /**
         * No TC threads and not ndbd emulation and there is at
         * at least one main thread, use the first main thread as
         * thread to handle the the DBTC worker.
         */
        thr_no = 0;
      } else {
        /* TC threads comes after LDM and Query threads */
        thr_no += num_lqh_threads + num_query_threads + (instance - 1);
      }
      break;
    }
    case THRMAN:
      thr_no = instance - 1;
      break;
    case TRPMAN:
      thr_no +=
          num_lqh_threads + num_query_threads + num_tc_threads + (instance - 1);
      break;
    default:
      require(false);
  }
  add_thr_map(block, instance, thr_no);
}

/**
 * create the duplicate entries needed so that
 *   sender doesn't need to know how many instances there
 *   actually are in this node...
 *
 * if only 1 instance...then duplicate that for all slots
 * else assume instance 0 is proxy...and duplicate workers (modulo)
 *
 * NOTE: extra pgman worker is instance 5
 */
void mt_finalize_thr_map() {
  for (Uint32 b = 0; b < NO_OF_BLOCKS; b++) {
    Uint32 bno = b + MIN_BLOCK_NO;
    Uint32 cnt = 0;
    while (cnt < NDB_ARRAY_SIZE(thr_map[b]) &&
           thr_map[b][cnt].thr_no != thr_map_entry::NULL_THR_NO) {
      cnt++;
    }
    block_instance_count[b] = cnt;
    if (cnt != NDB_ARRAY_SIZE(thr_map[b])) {
      SimulatedBlock *main = globalData.getBlock(bno, 0);
      if (main != nullptr) {
        for (Uint32 i = cnt; i < NDB_ARRAY_SIZE(thr_map[b]); i++) {
          Uint32 dup = (cnt == 1) ? 0 : 1 + ((i - 1) % (cnt - 1));
          if (thr_map[b][i].thr_no == thr_map_entry::NULL_THR_NO) {
            thr_map[b][i] = thr_map[b][dup];
            main->addInstance(globalData.getBlock(bno, dup), i);
          } else {
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
}

static void calculate_max_signals_parameters(thr_data *selfptr) {
  switch (selfptr->m_sched_responsiveness) {
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
      assert(false);
  }
  return;
}

static void init_thread(thr_data *selfptr) {
  selfptr->m_waiter.init();
  selfptr->m_congestion_waiter.init();
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
  NDB_THREAD_TLS_THREAD = selfptr;

  unsigned thr_no = selfptr->m_thr_no;
  bool succ = globalEmulatorData.theWatchDog->registerWatchedThread(
      &selfptr->m_watchdog_counter, thr_no);
  require(succ);
  {
    while (selfptr->m_thread == 0) NdbSleep_MilliSleep(30);
  }

  THRConfigApplier &conf = globalEmulatorData.theConfiguration->m_thr_config;
  BaseString tmp;
  tmp.appfmt("thr: %u ", thr_no);

  bool fail = false;
  int tid = NdbThread_GetTid(selfptr->m_thread);
  if (tid != -1) {
    tmp.appfmt("tid: %u ", tid);
  }

  conf.appendInfo(tmp, selfptr->m_instance_list, selfptr->m_instance_count);
  int res = conf.do_bind(selfptr->m_thread, selfptr->m_instance_list,
                         selfptr->m_instance_count);
  if (res < 0) {
    fail = true;
    tmp.appfmt("err: %d ", -res);
  } else if (res > 0) {
    tmp.appfmt("OK ");
  }

  unsigned thread_prio;
  res = conf.do_thread_prio(selfptr->m_thread, selfptr->m_instance_list,
                            selfptr->m_instance_count, thread_prio);
  if (res < 0) {
    fail = true;
    res = -res;
    tmp.appfmt("Failed to set thread prio to %u, ", thread_prio);
    if (res == SET_THREAD_PRIO_NOT_SUPPORTED_ERROR) {
      tmp.appfmt("not supported on this OS");
    } else {
      tmp.appfmt("error: %d", res);
    }
  } else if (res > 0) {
    tmp.appfmt("Successfully set thread prio to %u ", thread_prio);
  }

  selfptr->m_realtime =
      conf.do_get_realtime(selfptr->m_instance_list, selfptr->m_instance_count);
  selfptr->m_conf_spintime =
      conf.do_get_spintime(selfptr->m_instance_list, selfptr->m_instance_count);

#ifndef NDB_HAVE_CPU_PAUSE
  /**
   * We require NDB_HAVE_CPU_PAUSE in order to support NdbSpin().
   * Note that we may still not support it, even if NDB_HAVE_CPU_PAUSE.
   * In such cases 'spintime==0' will be forced.
   */
  require(!NdbSpin_is_supported());
#endif

  /* spintime always 0 on platforms not supporting spin */
  if (!NdbSpin_is_supported()) {
    selfptr->m_conf_spintime = 0;
  }
  selfptr->m_spintime = 0;
  memset(&selfptr->m_spin_stat, 0, sizeof(selfptr->m_spin_stat));
  selfptr->m_spin_stat.m_spin_interval[NUM_SPIN_INTERVALS - 1] = 0xFFFFFFFF;

  selfptr->m_sched_responsiveness =
      globalEmulatorData.theConfiguration->schedulerResponsiveness();
  calculate_max_signals_parameters(selfptr);

  selfptr->m_thr_id = my_thread_self();

  for (Uint32 i = 0; i < selfptr->m_instance_count; i++) {
    BlockReference block = selfptr->m_instance_list[i];
    Uint32 main = blockToMain(block);
    Uint32 instance = blockToInstance(block);
    tmp.appfmt("%s(%u) ", getBlockName(main), instance);
  }
  /* Report parameters used by thread to node log */
  tmp.appfmt(
      "realtime=%u, spintime=%u, max_signals_before_send=%u"
      ", max_signals_before_send_flush=%u",
      selfptr->m_realtime, selfptr->m_conf_spintime,
      selfptr->m_max_signals_before_send,
      selfptr->m_max_signals_before_send_flush);

  g_eventLogger->info("%s", tmp.c_str());
  if (fail) {
#ifndef HAVE_MAC_OS_X_THREAD_INFO
    abort();
#endif
  }
}

/**
 * Align signal buffer for better cache performance.
 * Also skew it a little for each thread to avoid cache pollution.
 */
#define SIGBUF_SIZE (sizeof(Signal) + 63 + 256 * MAX_BLOCK_THREADS)
static Signal *aligned_signal(unsigned char signal_buf[SIGBUF_SIZE],
                              unsigned thr_no) {
  UintPtr sigtmp = (UintPtr)signal_buf;
  sigtmp = (sigtmp + 63) & (~(UintPtr)63);
  sigtmp += thr_no * 256;
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
static TransporterReceiveHandleKernel
    *g_trp_receive_handle_ptr[MAX_NDBMT_RECEIVE_THREADS];

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
static void update_rt_config(struct thr_data *selfptr, bool &real_time,
                             enum ThreadTypes type) {
  bool old_real_time = real_time;
  real_time = selfptr->m_realtime;
  if (old_real_time == true && real_time == false) {
    yield_rt_break(selfptr->m_thread, type, false);
  }
}

/**
 * We use this method both to initialise the spintime variable
 * and also for updating it. Currently there is no method to
 * update it, but it's likely that we will soon invent one and
 * thus the code is prepared for this case.
 */
static void update_spin_config(struct thr_data *selfptr,
                               Uint64 &min_spin_timer) {
  min_spin_timer = selfptr->m_spintime;
}

extern "C" void *mt_receiver_thread_main(void *thr_arg) {
  unsigned char signal_buf[SIGBUF_SIZE];
  Signal *signal;
  struct thr_repository *rep = g_thr_repository;
  struct thr_data *selfptr = (struct thr_data *)thr_arg;
  unsigned thr_no = selfptr->m_thr_no;
  Uint32 &watchDogCounter = selfptr->m_watchdog_counter;
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
  recvdata.assign_trpman(
      (void *)globalData.getBlock(TRPMAN, recv_thread_idx + 1));
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

  while (globalData.theRestartFlag != perform_stop) {
    if (cnt == 0) {
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
      globalTransporterRegistry.update_connections(recvdata, max_spintime);
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
    /**
     * Need to call sendpacked even when no signals have been executed since
     * it can be used for NDBFS communication.
     */
    sendpacked(selfptr, signal);
    if (sum || has_received) {
      watchDogCounter = 6;
      flush_all_local_signals_and_wakeup(selfptr);
    }

    const bool pending_send = do_send(selfptr, true, false);

    watchDogCounter = 7;

    if (real_time) {
      check_real_time_break(now, &yield_ticks, selfptr->m_thread,
                            ReceiveThread);
    }

    /**
     * Only allow to sleep in pollReceive when:
     * 1) We are not lagging behind in handling timer events.
     * 2) No more pending sends, or no send progress.
     * 3) There are no jobs waiting to be executed in the job buffer
     * 4) There are no 'min_spin' configured or min_spin has elapsed
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

    if (lagging_timers == 0 &&          // 1)
        pending_send == false &&        // 2)
        check_queues_empty(selfptr) &&  // 3)
        (min_spin_timer == 0 ||         // 4)
         (sum == 0 && !has_received &&
          check_recv_yield(selfptr, recvdata, min_spin_timer, num_events,
                           &spin_micros, before)))) {
      delay = 10;  // 10 ms
      if (globalData.ndbMtMainThreads == 0) {
        delay = 1;
      }
    }

    has_received = false;
    if (num_events == 0) {
      /* Need to call pollReceive if not already done in check_recv_yield */
      num_events = globalTransporterRegistry.pollReceive(delay, recvdata);
    }
    if (delay > 0) {
      NDB_TICKS after = NdbTick_getCurrentTicks();
      Uint64 micros_sleep = NdbTick_Elapsed(before, after).microSec();
      selfptr->m_micros_sleep += micros_sleep;
      wait_time_tracking(selfptr, micros_sleep);
    }
    if (num_events) {
      watchDogCounter = 8;
      lock(&rep->m_receive_lock[recv_thread_idx]);
      const bool buffersFull = (globalTransporterRegistry.performReceive(
                                    recvdata, recv_thread_idx) != 0);
      unlock(&rep->m_receive_lock[recv_thread_idx]);
      has_received = true;

      if (buffersFull) /* Receive queues(s) are full */
      {
        /**
         * Will wait for congestion to disappear or 1 ms has passed.
         */
        watchDogCounter = 18;                                // "Yielding to OS"
        static constexpr Uint32 nano_wait_1ms = 1000 * 1000; /* -> 1 ms */
        NDB_TICKS before = NdbTick_getCurrentTicks();

        /**
         * Find (one of) the congested receive queues we need to wait for
         * in order to get out of 'buffersFull' state. We will be woken up
         * when consumer has freed a JB-page from the 'congested_queue'.
         */
        assert(!selfptr->m_congested_threads_mask.isclear());
        const unsigned thr_no = selfptr->m_congested_threads_mask.find_first();
        struct thr_data *congested_thr = &rep->m_thread[thr_no];
        const unsigned self_jbb = thr_no % NUM_JOB_BUFFERS_PER_THREAD;
        thr_job_queue *congested_queue = &congested_thr->m_jbb[self_jbb];

        const bool waited =
            yield(&congested_thr->m_congestion_waiter, nano_wait_1ms,
                  check_congested_job_queue, congested_queue);
        if (waited) {
          NDB_TICKS after = NdbTick_getCurrentTicks();
          selfptr->m_read_jbb_state_consumed = true;
          selfptr->m_buffer_full_micros_sleep +=
              NdbTick_Elapsed(before, after).microSec();
        }
        /**
         * We waited due to congestion, or didn't find the expected congestion.
         * Recheck if it cleared while we (not-)waited.
         */
        recheck_congested_job_buffers(selfptr);
      }
    }
    selfptr->m_stat.m_loop_cnt++;
  }

  globalEmulatorData.theWatchDog->unregisterWatchedThread(thr_no);
  return NULL;  // Return value not currently used
}

/**
 * has_full_in_queues()
 *
 * Avoid circular waits between block-threads:
 * A thread is not allowed to sleep due to full 'out' job-buffers if there
 * are other threads already having full in-queues, blocked on this thread.
 *
 * prepare_congested_execute() will set up the m_extra_signals[] prior to
 * executing from its JBB instances. As the queues are lock-free, more signals
 * could have been added to the in-queues since m_extra_signals[] was set up.
 * Some of the available m_total_extra_signals could have been consumed by
 * other writers as well.
 *
 * Thus, the algorithm provide no guarantee for the correct drain/yield
 * decision to be taken. However, a possibly incorrect sleep will be short,
 * and there is a SAFETY limit to take care of over provisioning of 'extra'.
 *
 * Returns 'true' if we need to continue execute_signals() from (only!)
 * the full in-queues, else we may yield() the thread.
 */
static bool has_full_in_queues(struct thr_data *selfptr) {
  // Precondition: About to execute signals while being FULL-congested
  assert(!selfptr->m_congested_threads_mask.isclear());
  assert(selfptr->m_max_signals_per_jb == 0);

  // Check the JBB in-queues known to contain signals to be executed
  for (unsigned jbb_instance = selfptr->m_jbb_read_mask.find_first();
       jbb_instance != BitmaskImpl::NotFound;
       jbb_instance = selfptr->m_jbb_read_mask.find_next(jbb_instance + 1)) {
    if (selfptr->m_extra_signals[jbb_instance] > 0) return true;
  }
  return false;
}

/**
 * handle_full_job_buffers
 *
 * One or more job buffers are 'full', such that we could not continue
 * without using the RESERVED signals.
 *
 * We need to either yield() the CPU in order to wait for the consumer
 * to make more job buffers available, or continue using 'extra' signals
 * from the RESERVED area.
 *
 * As a last resort we may also time-out on the wait and let the thread
 * continue with a small m_max_signals_per_jb quota, possibly with some
 * 'extra_signals' in order to solve circular wait queues.
 *
 *   Assumption: each signal may send *at most* 4 signals
 *     - this assumption is made the same in ndbd and ndbmtd and is
 *       mostly followed by block-code, although not in all places :-(
 *
 *   This function return true, if it slept
 *     (i.e that it concluded that it could not execute *any* signals, wo/
 *      risking job-buffer-full)
 */
static bool handle_full_job_buffers(struct thr_data *selfptr, bool pending_send,
                                    Uint32 &send_sum, Uint32 &flush_sum) {
  unsigned sleeploop = 0;
  const unsigned self_jbb = selfptr->m_thr_no % NUM_JOB_BUFFERS_PER_THREAD;
  selfptr->m_watchdog_counter = 16;

  while (selfptr->m_max_signals_per_jb == 0)  // or return
  {
    if (unlikely(sleeploop >= 10)) {
      /**
       * we've slept for 10ms...run a bit anyway
       */
      g_eventLogger->info(
          "thr_no:%u - sleeploop 10!! "
          "(Worker thread blocked (>= 10ms) by slow consumer threads)",
          selfptr->m_thr_no);
      return true;
    }

    struct thr_data *const congested = get_congested_job_queue(selfptr);
    if (unlikely(congested == nullptr)) {
      // Recalculate congestions w/ locks, recalculate per_jb-quota as well:
      recheck_congested_job_buffers(selfptr);
      continue;  // Recheck if FULL-congested
    }
    if (congested == selfptr) {
      // Found a 'self' congestion - can't wait for FULL blockage on 'self'
      return sleeploop > 0;
    }
    /**
     * Avoid 'self-wait', where 'self' participate in a cyclic wait graph.
     */
    if (has_full_in_queues(selfptr)) {
      /**
       * 'extra_signals' need to be used to drain 'full_in_queues'.
       */
      return sleeploop > 0;
    }

    if (pending_send) {
      /* About to sleep, _must_ send now. */
      pending_send = do_send(selfptr, true, true);
      send_sum = 0;
      flush_sum = 0;
    }
    thr_job_queue *congested_queue = &congested->m_jbb[self_jbb];
    static constexpr Uint32 nano_wait_1ms = 1000 * 1000; /* -> 1 ms */
    /**
     * Wait for congested-thread' to consume some of the
     * pending signals from its jbb queue.
     * Will recheck queue status with 'check_full_job_queue'
     * after latch has been set, and *before* going to sleep.
     */
    selfptr->m_watchdog_counter = 18;  // "Yielding to OS"
    const NDB_TICKS before = NdbTick_getCurrentTicks();
    const bool waited = yield(&congested->m_congestion_waiter, nano_wait_1ms,
                              check_full_job_queue, congested_queue);
    if (waited) {
      const NDB_TICKS after = NdbTick_getCurrentTicks();
      selfptr->m_curr_ticks = after;
      selfptr->m_read_jbb_state_consumed = true;
      selfptr->m_buffer_full_micros_sleep +=
          NdbTick_Elapsed(before, after).microSec();
      sleeploop++;
    }
    /**
     * We waited due to congestion, or didn't find the expected congestion.
     * Recheck if it cleared while we (not-)waited.
     */
    recheck_congested_job_buffers(selfptr);
  }

  return sleeploop > 0;
}

static void init_jbb_estimate(struct thr_data *selfptr, NDB_TICKS now) {
  selfptr->m_jbb_estimate_signal_count_start = selfptr->m_stat.m_exec_cnt;
  selfptr->m_jbb_execution_steps = 0;
  selfptr->m_jbb_accumulated_queue_size = 0;
  selfptr->m_jbb_estimate_start = now;
}

#define NO_LOAD_INDICATOR 16
#define LOW_LOAD_INDICATOR 24
#define MEDIUM_LOAD_INDICATOR 34
#define HIGH_LOAD_INDICATOR 48
#define EXTREME_LOAD_INDICATOR 64
static void handle_queue_size_stats(struct thr_data *selfptr, NDB_TICKS now) {
  Uint32 mean_queue_size = 0;
  Uint32 mean_execute_size = 0;
  if (selfptr->m_jbb_execution_steps > 0) {
    mean_queue_size =
        selfptr->m_jbb_accumulated_queue_size / selfptr->m_jbb_execution_steps;
    mean_execute_size = (selfptr->m_stat.m_exec_cnt -
                         selfptr->m_jbb_estimate_signal_count_start) /
                        selfptr->m_jbb_execution_steps;
  }
  Uint32 calc_execute_size = mean_queue_size / AVERAGE_SIGNAL_SIZE;
  if (calc_execute_size > mean_execute_size) {
    if (calc_execute_size < (2 * mean_execute_size)) {
      mean_execute_size = calc_execute_size;
    } else {
      mean_execute_size *= 2;
    }
  }
  if (mean_execute_size < NO_LOAD_INDICATOR) {
    if (selfptr->m_load_indicator != 1) {
      selfptr->m_load_indicator = 1;
      debug_load_indicator(selfptr);
    }
  } else if (mean_execute_size < LOW_LOAD_INDICATOR) {
    if (selfptr->m_load_indicator != 2) {
      selfptr->m_load_indicator = 2;
      debug_load_indicator(selfptr);
    }
  } else if (mean_execute_size < MEDIUM_LOAD_INDICATOR) {
    if (selfptr->m_load_indicator != 3) {
      selfptr->m_load_indicator = 3;
      debug_load_indicator(selfptr);
    }
  } else if (mean_execute_size < HIGH_LOAD_INDICATOR) {
    if (selfptr->m_load_indicator != 4) {
      selfptr->m_load_indicator = 4;
      debug_load_indicator(selfptr);
    }
  } else {
    if (selfptr->m_load_indicator != 5) {
      selfptr->m_load_indicator = 5;
      debug_load_indicator(selfptr);
    }
  }
  init_jbb_estimate(selfptr, now);
}

extern "C" void *mt_job_thread_main(void *thr_arg) {
  unsigned char signal_buf[SIGBUF_SIZE];
  Signal *signal;

  struct thr_data *selfptr = (struct thr_data *)thr_arg;
  init_thread(selfptr);
  Uint32 &watchDogCounter = selfptr->m_watchdog_counter;

  unsigned thr_no = selfptr->m_thr_no;
  signal = aligned_signal(signal_buf, thr_no);

  /* Avoid false watchdog alarms caused by race condition. */
  watchDogCounter = 21;

  bool pending_send = false;
  Uint32 send_sum = 0;
  Uint32 flush_sum = 0;
  Uint32 loops = 0;
  Uint32 maxloops =
      10; /* Loops before reading clock, fuzzy adapted to 1ms freq. */
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
  init_jbb_estimate(selfptr, now);

  while (globalData.theRestartFlag != perform_stop) {
    loops++;

    /**
     * prefill our thread local send buffers
     *   up to THR_SEND_BUFFER_PRE_ALLOC (1Mb)
     *
     * and if this doesn't work pack buffers before start to execute signals
     */
    watchDogCounter = 11;
    if (!selfptr->m_send_buffer_pool.fill(
            g_thr_repository->m_mm, RG_TRANSPORTER_BUFFERS,
            THR_SEND_BUFFER_PRE_ALLOC, selfptr->m_send_instance_no)) {
      try_pack_send_buffers(selfptr);
    }

    watchDogCounter = 2;
    const Uint32 lagging_timers = scan_time_queues(selfptr, now);

    Uint32 sum =
        run_job_buffers(selfptr, signal, send_sum, flush_sum, pending_send);

    if (sum) {
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
       * No need to flush however if no signals have been executed since
       * last flush.
       *
       * No need to check for send packed signals if we didn't send
       * any signals, packed signals are sent as a result of an
       * executed signal.
       */
      sendpacked(selfptr, signal);
      watchDogCounter = 6;
      if (flush_sum > 0) {
        // OJA: Will not yield -> wakeup not needed yet
        flush_all_local_signals_and_wakeup(selfptr);
        do_flush(selfptr);
        flush_sum = 0;
      }
    }
    /**
     * Scheduler is not allowed to yield until its internal
     * time has caught up on real time.
     */
    else if (lagging_timers == 0) {
      /* No signals processed, prepare to sleep to wait for more */
      if (send_sum > 0 || pending_send == true) {
        /* About to sleep, _must_ send now. */
        flush_all_local_signals_and_wakeup(selfptr);
        pending_send = do_send(selfptr, true, true);
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
            check_yield(selfptr, min_spin_timer, &spin_time_in_us, before)) {
          /**
           * Sleep, either a short nap if send failed due to send overload,
           * or a longer sleep if there are no more work waiting.
           */
          Uint32 maxwait_in_us = (selfptr->m_node_overload_status >=
                                  (OverloadStatus)MEDIUM_LOAD_CONST)
                                     ? 1 * 1000
                                     : 10 * 1000;
          if (maxwait_in_us < spin_time_in_us) {
            maxwait_in_us = 0;
          } else {
            maxwait_in_us -= spin_time_in_us;
          }
          selfptr->m_watchdog_counter = 18;
          const Uint32 used_maxwait_in_ns = maxwait_in_us * 1000;
          bool waited = yield(&selfptr->m_waiter, used_maxwait_in_ns,
                              check_queues_empty, selfptr);
          if (waited) {
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
            selfptr->m_read_jbb_state_consumed = true;
            init_jbb_estimate(selfptr, now);
            if (selfptr->m_overload_status <=
                (OverloadStatus)MEDIUM_LOAD_CONST) {
              /**
               * To ensure that we at least check for trps to send to
               * before we yield we set pending_send to true. We will
               * quickly discover if nothing is pending.
               */
              pending_send = true;
            }
            waits = loops = 0;
            if (selfptr->m_thr_no == glob_ndbfs_thr_no) {
              /**
               * NDBFS is using thread 0, here we need to call SEND_PACKED
               * to scan the memory channel for messages from NDBFS threads.
               * We want to do this here to avoid an extra loop in scheduler
               * before we discover those messages from NDBFS.
               */
              selfptr->m_watchdog_counter = 17;
              check_for_input_from_ndbfs(selfptr, signal);
            }
          } else if (has_spun) {
            selfptr->m_micros_sleep += spin_time_in_us;
            wait_time_tracking(selfptr, spin_time_in_us);
          }
        }
      }
    }

    /**
     * If job-buffers are full, we need to handle that somehow:
     *  - yield() and wait for more JB's to become available.
     *  - continue using the 'extra' signal quota (see RESERVED)
     */
    if (unlikely(selfptr->m_max_signals_per_jb == 0))  // JB's are full?
    {
      if (handle_full_job_buffers(selfptr, send_sum + Uint32(pending_send),
                                  send_sum, flush_sum)) {
        selfptr->m_stat.m_wait_cnt += waits;
        selfptr->m_stat.m_loop_cnt += loops;
        waits = loops = 0;
        update_rt_config(selfptr, real_time, BlockThread);
        calculate_max_signals_parameters(selfptr);
      }
    }

    /**
     * Adaptive update of statistics and check of real-time break.
     * Always read time to avoid problems with delaying timer signals
     * too much.
     */
    now = NdbTick_getCurrentTicks();
    selfptr->m_curr_ticks = now;

    if (NdbTick_Elapsed(selfptr->m_jbb_estimate_start, now).microSec() > 400) {
      /**
       * Report queue size to other threads in our data node after executing
       * for at least 400 microseconds. We will always report idle mode when
       * we go to sleep, thus the only manner to report higher load is if we
       * execute without going to sleep for at least 400 microseconds. On top
       * of that we need to have many jobs queued such that each job only gets
       * a small portion of the used CPU.
       *
       * When CPU isn't fully utilised we use the CPU load measurements that
       * shows long term behaviour. But if we start up a number of jobs that
       * constantly execute we will run constantly (e.g. a scan on a very
       * large table, or a number of complex queries that are evaluated by the
       * SPJ block constantly. In these cases load can very quickly build up
       * from an idle or a light load to a very high load in just a few
       * microseconds.
       *
       * The action we perform here is to set a load indicator that all other
       * threads can see. This means that each change will cause a cache miss
       * where we will need to fetch the load indicator again. Thus we don't
       * want to toggle this value frequently since this might cause high
       * overhead.
       */
      handle_queue_size_stats(selfptr, now);
    }
    if (loops > maxloops) {
      if (real_time) {
        check_real_time_break(now, &yield_ticks, selfptr->m_thread,
                              BlockThread);
      }
      const Uint64 diff = NdbTick_Elapsed(selfptr->m_ticks, now).milliSec();

      /* Adjust 'maxloop' to achieve frequency of 1ms */
      if (diff < 1)
        maxloops +=
            ((maxloops / 10) + 1); /* No change: less frequent reading */
      else if (diff > 1 && maxloops > 1)
        maxloops -=
            ((maxloops / 10) + 1); /* Overslept: Need more frequent read*/

      selfptr->m_stat.m_wait_cnt += waits;
      selfptr->m_stat.m_loop_cnt += loops;
      waits = loops = 0;
    }
  }

  globalEmulatorData.theWatchDog->unregisterWatchedThread(thr_no);
  return NULL;  // Return value not currently used
}

/**
 * Get number of pending signals at B-level in our own thread. Used
 * to make some decisions in rate-critical parts of the data node.
 *
 * We perform the read as a dirty read, to get true number we need to
 * lock the job queue. This number is only used to control rates, so
 * a modest error here is ok.
 */
bool mt_isEstimatedJobBufferLevelChanged(Uint32 self) {
  struct thr_repository *rep = g_thr_repository;
  struct thr_data *selfptr = &rep->m_thread[self];
  bool changed = selfptr->m_cpu_percentage_changed;
  selfptr->m_cpu_percentage_changed = false;
  return changed;
}

#define AVERAGE_SIGNAL_SIZE 16
Uint32 mt_getEstimatedJobBufferLevel(Uint32 self) {
  struct thr_repository *rep = g_thr_repository;
  struct thr_data *selfptr = &rep->m_thread[self];
  return selfptr->m_jbb_estimated_queue_size_in_words / AVERAGE_SIGNAL_SIZE;
}

#ifdef DEBUG_SCHED_STATS
void get_jbb_estimated_stats(Uint32 block, Uint32 instance,
                             Uint64 **total_words, Uint64 **est_stats) {
  struct thr_repository *rep = g_thr_repository;
  Uint32 dst = block2ThreadId(block, instance);
  struct thr_data *dstptr = &rep->m_thread[dst];
  (*total_words) = &dstptr->m_jbb_total_words;
  (*est_stats) = &dstptr->m_jbb_estimated_queue_stats[0];
}
#endif

void prefetch_load_indicators(Uint32 *rr_groups, Uint32 rr_group) {
  struct thr_repository *rep = g_thr_repository;
  Uint32 num_ldm_threads = globalData.ndbMtLqhThreads;
  Uint32 first_ldm_instance = globalData.ndbMtMainThreads;
  Uint32 num_query_threads = globalData.ndbMtQueryThreads;
  Uint32 num_distr_threads = num_ldm_threads + num_query_threads;
  for (Uint32 i = 0; i < num_ldm_threads; i++) {
    if (rr_groups[i] == rr_group) {
      Uint32 dst = i + first_ldm_instance;
      struct thr_data *dstptr = &rep->m_thread[dst];
      NDB_PREFETCH_READ(&dstptr->m_load_indicator);
    }
  }
  for (Uint32 i = num_ldm_threads; i < num_distr_threads; i++) {
    if (rr_groups[i] == rr_group) {
      Uint32 dst = i + first_ldm_instance;
      struct thr_data *dstptr = &rep->m_thread[dst];
      NDB_PREFETCH_READ(&dstptr->m_load_indicator);
    }
  }
}

Uint32 get_load_indicator(Uint32 dst) {
  struct thr_repository *rep = g_thr_repository;
  struct thr_data *dstptr = &rep->m_thread[dst];
  return dstptr->m_load_indicator;
}

Uint32 get_qt_jbb_level(Uint32 instance_no) {
  assert(instance_no > 0);
  struct thr_repository *rep = g_thr_repository;
  Uint32 num_main_threads = globalData.ndbMtMainThreads;
  Uint32 num_ldm_threads = globalData.ndbMtLqhThreads;
  Uint32 first_qt = num_main_threads + num_ldm_threads;
  Uint32 qt_thr_no = first_qt + (instance_no - 1);
  struct thr_data *qt_ptr = &rep->m_thread[qt_thr_no];
  return qt_ptr->m_jbb_estimated_queue_size_in_words;
}

NDB_TICKS
mt_getHighResTimer(Uint32 self) {
  struct thr_repository *rep = g_thr_repository;
  struct thr_data *selfptr = &rep->m_thread[self];
  return selfptr->m_curr_ticks;
}

void mt_setNoSend(Uint32 self) {
  struct thr_repository *rep = g_thr_repository;
  struct thr_data *selfptr = &rep->m_thread[self];
  selfptr->m_nosend = 1;
}

void mt_startChangeNeighbourNode() {
  if (g_send_threads) {
    g_send_threads->startChangeNeighbourNode();
  }
}

void mt_setNeighbourNode(NodeId node) {
  if (g_send_threads) {
    g_send_threads->setNeighbourNode(node);
  }
}

void mt_endChangeNeighbourNode() {
  if (g_send_threads) {
    g_send_threads->endChangeNeighbourNode();
  }
}

void mt_setOverloadStatus(Uint32 self, OverloadStatus new_status) {
  struct thr_repository *rep = g_thr_repository;
  struct thr_data *selfptr = &rep->m_thread[self];
  selfptr->m_overload_status = new_status;
}

void mt_setWakeupThread(Uint32 self, Uint32 wakeup_instance) {
  struct thr_repository *rep = g_thr_repository;
  struct thr_data *selfptr = &rep->m_thread[self];
  selfptr->m_wakeup_instance = wakeup_instance;
}

void mt_setNodeOverloadStatus(Uint32 self, OverloadStatus new_status) {
  struct thr_repository *rep = g_thr_repository;
  struct thr_data *selfptr = &rep->m_thread[self];
  selfptr->m_node_overload_status = new_status;
}

void mt_setSendNodeOverloadStatus(OverloadStatus new_status) {
  if (g_send_threads) {
    g_send_threads->setNodeOverloadStatus(new_status);
  }
}

void mt_setSpintime(Uint32 self, Uint32 new_spintime) {
  struct thr_repository *rep = g_thr_repository;
  struct thr_data *selfptr = &rep->m_thread[self];
  /* spintime always 0 on platforms not supporting spin */
  if (!NdbSpin_is_supported()) {
    new_spintime = 0;
  }
  selfptr->m_spintime = new_spintime;
}

Uint32 mt_getConfiguredSpintime(Uint32 self) {
  struct thr_repository *rep = g_thr_repository;
  struct thr_data *selfptr = &rep->m_thread[self];

  return selfptr->m_conf_spintime;
}

Uint32 mt_getWakeupLatency(void) { return glob_wakeup_latency; }

void mt_setWakeupLatency(Uint32 latency) {
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

void mt_flush_send_buffers(Uint32 self) {
  struct thr_repository *rep = g_thr_repository;
  struct thr_data *selfptr = &rep->m_thread[self];
  do_flush(selfptr);
}

void mt_set_watchdog_counter(Uint32 self) {
  struct thr_repository *rep = g_thr_repository;
  struct thr_data *selfptr = &rep->m_thread[self];
  selfptr->m_watchdog_counter = 12;
}

void mt_getPerformanceTimers(Uint32 self, Uint64 &micros_sleep,
                             Uint64 &spin_time,
                             Uint64 &buffer_full_micros_sleep,
                             Uint64 &micros_send) {
  struct thr_repository *rep = g_thr_repository;
  struct thr_data *selfptr = &rep->m_thread[self];

  /**
   * Internally in mt.cpp sleep time now includes spin time. However
   * to ensure backwards compatibility we report them separate to
   * any block users of this information.
   */
  micros_sleep = selfptr->m_micros_sleep;
  spin_time = selfptr->m_measured_spintime;
  if (micros_sleep >= spin_time) {
    micros_sleep -= spin_time;
  } else {
    micros_sleep = 0;
  }
  buffer_full_micros_sleep = selfptr->m_buffer_full_micros_sleep;
  micros_send = selfptr->m_micros_send;
}

const char *mt_getThreadDescription(Uint32 self) {
  if (is_main_thread(self)) {
    if (globalData.ndbMtMainThreads == 2) {
      if (self == 0)
        return "main thread, schema and distribution handling";
      else if (self == 1)
        return "rep thread, asynch replication and proxy block handling";
    } else if (globalData.ndbMtMainThreads == 1) {
      return "main and rep thread, schema, distribution, proxy block and "
             "asynch replication handling";
    } else if (globalData.ndbMtMainThreads == 0) {
      return "main, rep and recv thread, schema, distribution, proxy block and "
             "asynch replication handling and handling receive and polling for "
             "new receives";
    }
    require(false);
  } else if (is_ldm_thread(self)) {
    return "ldm thread, handling a set of data partitions";
  } else if (is_query_thread(self)) {
    return "query thread, handling queries and recovery";
  } else if (is_recover_thread(self)) {
    return "recover thread, handling restore of data";
  } else if (is_tc_thread(self)) {
    return "tc thread, transaction handling, unique index and pushdown join"
           " handling";
  } else if (is_recv_thread(self)) {
    return "receive thread, performing receieve and polling for new receives";
  } else {
    require(false);
  }
  return NULL;
}

const char *mt_getThreadName(Uint32 self) {
  if (is_main_thread(self)) {
    if (globalData.ndbMtMainThreads == 2) {
      if (self == 0)
        return "main";
      else if (self == 1)
        return "rep";
    } else if (globalData.ndbMtMainThreads == 1) {
      return "main_rep";
    } else if (globalData.ndbMtMainThreads == 0) {
      return "main_rep_recv";
    }
    require(false);
  } else if (is_ldm_thread(self)) {
    return "ldm";
  } else if (is_query_thread(self)) {
    return "query";
  } else if (is_recover_thread(self)) {
    return "recover";
  } else if (is_tc_thread(self)) {
    return "tc";
  } else if (is_recv_thread(self)) {
    return "recv";
  } else {
    require(false);
  }
  return NULL;
}

void mt_getSendPerformanceTimers(Uint32 send_instance, Uint64 &exec_time,
                                 Uint64 &sleep_time, Uint64 &spin_time,
                                 Uint64 &user_time_os, Uint64 &kernel_time_os,
                                 Uint64 &elapsed_time_os) {
  assert(g_send_threads != NULL);
  if (g_send_threads != NULL) {
    g_send_threads->getSendPerformanceTimers(
        send_instance, exec_time, sleep_time, spin_time, user_time_os,
        kernel_time_os, elapsed_time_os);
  }
}

Uint32 mt_getNumSendThreads() { return globalData.ndbMtSendThreads; }

Uint32 mt_getNumThreads() { return glob_num_threads; }

/**
 * Copy out signals one-by-one from the 'm_local_buffer' into the thread-shared
 * 'm_current_write_buffer'. If the write_buffer becomes full, the available
 * 'm_next_buffer' will be used. The copied signals will be 'published'
 * such that they becomes visible for the consumer side.
 *
 * Assumed to be called with write_lock held, if the ThreadConfig is
 * such that multiple writer are possible.
 */
static Uint32 copy_out_local_buffer(struct thr_data *selfptr, thr_job_queue *q,
                                    Uint32 &next) {
  Uint32 num_signals = 0;
  const thr_job_buffer *const local_buffer = selfptr->m_local_buffer;
  Uint32 next_signal = next;

  thr_job_buffer *write_buffer = q->m_current_write_buffer;
  Uint32 write_pos = q->m_current_write_buffer_len;
  NDB_PREFETCH_WRITE(&write_buffer->m_len);
  NDB_PREFETCH_WRITE(&write_buffer->m_data[write_pos]);
  do {
    assert(next_signal != SIGNAL_RNIL);
    const Uint32 *const signal_buffer = &local_buffer->m_data[next_signal];
    const Uint32 siglen = signal_buffer[1];
    if (unlikely(write_pos + siglen > thr_job_buffer::SIZE)) {
      // job_buffer was filled & consumed.
      if (num_signals > 0) {
        publish_position(write_buffer, write_pos);
      }
      // Add a new job_buffer?
      const bool full = check_next_index_position(q, selfptr->m_next_buffer);
      if (unlikely(full)) {
        break;
      }
      write_pos = 0;
      write_buffer = selfptr->m_next_buffer;
      selfptr->m_next_buffer = nullptr;
    }
    memcpy(write_buffer->m_data + write_pos, &signal_buffer[2], 4 * siglen);
    next_signal = signal_buffer[0];
    /**
     * We update write_pos without publishing the position until we're done
     * with all writes. The reason is that the same job buffer page could be
     * read by the executing thread and we want to avoid those from getting
     * into cache line bouncing.
     */
    write_pos += siglen;
    num_signals++;
  } while (next_signal != SIGNAL_RNIL);

  q->m_current_write_buffer_len = write_pos;
  publish_position(write_buffer, write_pos);
  next = next_signal;
  return num_signals;
}

/**
 * Copy signals to the specified 'dst' thread from m_local_buffer into
 * the thread-shared signal buffer - Updates the write-indexes and wakeup
 * the destination thread if needed.
 */
static void flush_local_signals(struct thr_data *selfptr, Uint32 dst) {
  struct thr_job_buffer *const local_buffer = selfptr->m_local_buffer;
  unsigned self = selfptr->m_thr_no;
  const unsigned jbb_instance = self % NUM_JOB_BUFFERS_PER_THREAD;
  struct thr_repository *rep = g_thr_repository;
  struct thr_data *dstptr = &rep->m_thread[dst];
  thr_job_queue *q = dstptr->m_jbb + jbb_instance;

  Uint32 num_signals = 0;
  Uint32 next_signal = selfptr->m_first_local[dst].m_first_signal;

  if (unlikely(selfptr->m_congested_threads_mask.get(dst))) {
    // Assume uncongested, set again further below if still congested
    selfptr->m_congested_threads_mask.clear(dst);
    if (selfptr->m_congested_threads_mask.isclear()) {
      // Last congestion cleared, assume full JB quotas
      selfptr->m_max_signals_per_jb = MAX_SIGNALS_PER_JB;
      selfptr->m_total_extra_signals =
          compute_max_signals_to_execute(thr_job_queue::RESERVED);
    }
  }

  if (likely(!glob_use_write_lock_mutex)) {
    /**
     * No locking used, thus no need to perform extra copying step to
     * minimise the lock hold time.
     */
    num_signals = copy_out_local_buffer(selfptr, q, next_signal);
  } else if (selfptr->m_first_local[dst].m_num_signals <=
             MAX_SIGNALS_BEFORE_FLUSH_OTHER) {
    /**
     * Copy data into local flush_buffer before grabbing the write mutex.
     * The purpose is to decrease the amount of time we spend holding the
     * write mutex by 'loading' the flush_buffer into L1 cache.
     *
     * For the same reason, and also to improve performance, we prefetch the
     * cache line that contains the m_write_index before taking the mutex.
     * We expect mutex contention on this mutex to be rare and when
     * contention arises we will only prefetch it for read and thus at most
     * we will have to fetch it again if one or more writers are queued in
     * front of us.
     */
    Uint32 copy_len = 0;
    Uint64 flush_buffer[MAX_SIGNALS_BEFORE_FLUSH_OTHER * MAX_SIGNAL_SIZE / 2];
    Uint32 *flush_buffer_ptr = (Uint32 *)&flush_buffer[0];
    do {
      Uint32 *signal_buffer = &local_buffer->m_data[next_signal];
      Uint32 siglen = signal_buffer[1];
      memcpy(&flush_buffer_ptr[copy_len], &signal_buffer[2], 4 * siglen);
      next_signal = signal_buffer[0];
      copy_len += siglen;
      num_signals++;
    } while (next_signal != SIGNAL_RNIL);

    NDB_PREFETCH_READ(&q->m_write_index);
    lock(&q->m_write_lock);
    thr_job_buffer *write_buffer = q->m_current_write_buffer;
    Uint32 write_pos = q->m_current_write_buffer_len;
    NDB_PREFETCH_WRITE(&write_buffer->m_len);
    if (likely(write_pos + copy_len <= thr_job_buffer::SIZE)) {
      memcpy(write_buffer->m_data + write_pos, flush_buffer_ptr, 4 * copy_len);
      write_pos += copy_len;
      q->m_current_write_buffer_len = write_pos;
      publish_position(write_buffer, write_pos);
    } else {
      /**
       * We could not append the prepared set of signals in flush_buffer.
       * Copy them one-by-one until full.
       * Even if the memcpy into flush_buffer[] was wasted, the signals will
       * now at least be in the cache.
       */
      next_signal = selfptr->m_first_local[dst].m_first_signal;
      num_signals = copy_out_local_buffer(selfptr, q, next_signal);
    }
  } else  // unlikely case:
  {
    /**
     * Too many signals to fit the flush_buffer[]. Will only
     * happen when we previously hit a full out-queue.
     * (Will likely happen again now, but we need to keep trying)
     */
    lock(&q->m_write_lock);
    num_signals = copy_out_local_buffer(selfptr, q, next_signal);
  }

  // Check *total* pending_signals in this queue, wakeup consumer?
  bool need_wakeup = false;
  if (dst != self) {
    q->m_pending_signals += num_signals;
    if (q->m_pending_signals >= MAX_SIGNALS_BEFORE_WAKEUP) {
      // This thread will wakeup 'dst' now, restart counting of 'pending'
      q->m_pending_signals = 0;
      need_wakeup = true;
    }
  }
  const unsigned free = get_free_estimate_out_queue(q);
  if (unlikely(glob_use_write_lock_mutex)) {
    unlock(&q->m_write_lock);
  }

  if (unlikely(free <= thr_job_queue::CONGESTED)) {
    set_congested_jb_quotas(selfptr, dst, free);
  }

  // Handle wakeup decision taken above
  if (dst != self) {
    if (need_wakeup) {
      // Wakeup immediately
      selfptr->m_wake_threads_mask.clear(dst);
      wakeup(&dstptr->m_waiter);
    } else {
      // Need wakeup of 'dst' at latest before thread suspends
      selfptr->m_wake_threads_mask.set(dst);
    }
  }
  if (unlikely(selfptr->m_next_buffer == nullptr)) {
    selfptr->m_next_buffer = seize_buffer(rep, self, false);
  }
  selfptr->m_first_local[dst].m_num_signals -= num_signals;
  selfptr->m_first_local[dst].m_first_signal = next_signal;
  if (next_signal == SIGNAL_RNIL) {
    selfptr->m_first_local[dst].m_last_signal = SIGNAL_RNIL;
    selfptr->m_local_signals_mask.clear(dst);
  }
}

/**
 * recheck_congested_job_buffers is intended to be used after
 * we slept for a while, waiting for some JB-congestion to be cleared.
 * It recheck the known congested buffers.
 *
 * In a well behaved system where there are no congestions, it is expected
 * to be called very infrequently. Thus, the locks taken by the congestion
 * check should not really be a performance problem.
 */
static void recheck_congested_job_buffers(struct thr_data *selfptr) {
  unsigned self = selfptr->m_thr_no;
  const Uint32 self_jbb = self % NUM_JOB_BUFFERS_PER_THREAD;
  struct thr_repository *rep = g_thr_repository;

  // Assume full JB quotas, reduce below if congested
  selfptr->m_max_signals_per_jb = MAX_SIGNALS_PER_JB;
  selfptr->m_total_extra_signals =
      compute_max_signals_to_execute(thr_job_queue::RESERVED);

  for (unsigned thr_no = selfptr->m_congested_threads_mask.find_first();
       thr_no != BitmaskImpl::NotFound;
       thr_no = selfptr->m_congested_threads_mask.find_next(thr_no + 1)) {
    struct thr_data *thrptr = &rep->m_thread[thr_no];
    thr_job_queue *q = &thrptr->m_jbb[self_jbb];

    // Assume congestion cleared, set again if needed
    selfptr->m_congested_threads_mask.clear(thr_no);

    unsigned free;
    if (unlikely(glob_use_write_lock_mutex)) {
      lock(&q->m_write_lock);
      free = get_free_estimate_out_queue(q);
      unlock(&q->m_write_lock);
    } else {
      free = get_free_estimate_out_queue(q);
    }

    if (unlikely(free <= thr_job_queue::CONGESTED)) {
      // JB-page usage is congested, reduce execution quota
      set_congested_jb_quotas(selfptr, thr_no, free);
    }
  }
}

/**
 * 'Pack' the signal contents in 'm_local_buffer' in order to make any
 * fragmented free space in between the signals available. We use the
 * already pre-allocated (and unused) 'm_next_buffer' to copy the signals
 * into, and just swap m_local_buffer with m_next_buffer when completed.
 */
static void pack_local_signals(struct thr_data *selfptr) {
  thr_job_buffer *const local_buffer = selfptr->m_local_buffer;
  thr_job_buffer *write_buffer = selfptr->m_next_buffer;
  Uint32 write_pos = 0;
  for (Uint32 dst = selfptr->m_local_signals_mask.find_first();
       dst != BitmaskImpl::NotFound;
       dst = selfptr->m_local_signals_mask.find_next(dst + 1)) {
    Uint32 siglen = 0;
    Uint32 next_signal = selfptr->m_first_local[dst].m_first_signal;
    selfptr->m_first_local[dst].m_first_signal = write_pos;
    do {
      assert(next_signal != SIGNAL_RNIL);
      Uint32 *signal_buffer = &local_buffer->m_data[next_signal];
      next_signal = signal_buffer[0];
      siglen = signal_buffer[1];
      write_buffer->m_data[write_pos] = write_pos + siglen + 2;
      memcpy(&write_buffer->m_data[write_pos + 1], &signal_buffer[1],
             4 * (siglen + 1));
      write_pos += siglen + 2;
    } while (next_signal != SIGNAL_RNIL);
    Uint32 last_pos = write_pos - siglen - 2;
    write_buffer->m_data[last_pos] = SIGNAL_RNIL;
    selfptr->m_first_local[dst].m_last_signal = last_pos;
  }
  write_buffer->m_len = write_pos;

  // Swap m_next_buffer / selfptr->m_local_buffer
  thr_job_buffer *const tmp = selfptr->m_local_buffer;
  selfptr->m_local_buffer = write_buffer;
  selfptr->m_next_buffer = tmp;

  // Reset the swapped m_next_buffer:
  selfptr->m_next_buffer->m_len = 0;
  selfptr->m_next_buffer->m_prioa = false;
}

/**
 * flush_all_local_signals copy signals from thread local buffer
 * into the job-buffer queues to the destination thread(s).
 *  It is typically called when:
 *   - The local buffer is full.
 *   - run_job_buffers executed a round of signals.
 *   - We prepare to yield the CPU.
 *
 * A 'flush' might not complete entirely if all page slots in the
 * out-queue is full. We will then have a 'critical' JB congestion.
 *
 * For each destination JB flushed to, it will also check for JB's
 * being congested, and if needed reduce the 'max_signals_per_jb' quota
 * each round of run_job_buffers is allowed to execute.
 *
 * If 'max_signals_per_jb' became '0', we are blocked from further
 * signal execution. (Except where 'extra' signals are assigned).
 * Upper level will call handle_full_job_buffers(), which
 * decide how to handle the 'full'.
 */
static void flush_all_local_signals(struct thr_data *selfptr) {
  for (Uint32 thr_no = selfptr->m_local_signals_mask.find_first();
       thr_no != BitmaskImpl::NotFound;
       thr_no = selfptr->m_local_signals_mask.find_next(thr_no + 1)) {
    assert(selfptr->m_local_signals_mask.get(thr_no));
    flush_local_signals(selfptr, thr_no);
  }

  if (likely(selfptr->m_local_signals_mask.isclear())) {
    // Normal exit: Flushed all local signals.
    selfptr->m_local_buffer->m_len = 0;
    return;
  }
  /**
   * Failed to flush all signals - This is a CRITICAL JBB state:
   *
   * Having remaining local_signals is only expected when a JBB queue
   * has been completely filled - Even the SAFETY limit has been consumed.
   * We can still continue though, as long as we have remaining
   * m_local_buffer.
   */
  if (unlikely(selfptr->m_local_buffer->m_len > MAX_LOCAL_BUFFER_USAGE)) {
    // Try to free up some space
    pack_local_signals(selfptr);
    if (selfptr->m_local_buffer->m_len > MAX_LOCAL_BUFFER_USAGE) {
      // Still full
      job_buffer_full(0);  // -> WILL CRASH
    }
  }
  return;  // We survived this time ... for a while
}

/**
 * When the thread 'selfptr' insert_signal() it will also keep
 * track of which thr_no it has sent to without a 'wakeup'.
 *
 * Before thread can be suspended, it has to ensure that all
 * such treads gets a wakeup, else the receiver may never notice
 * that it had signals.
 *
 * The same wakeup mechanism is also used for efficiency reason
 * when certain other 'milestones' have been reached - Like
 * completed processing of a larger chunk of incoming signals.
 *
 * Note that we do not clear the shared m_pending_signals at this point.
 * That would have been preferable in order to avoid later redundant
 * wakeups from other threads, however that would require setting the
 * m_write_lock which is likely to have a higher cost.
 */
static inline void wakeup_pending_signals(thr_data *selfptr) {
  for (Uint32 thr_no = selfptr->m_wake_threads_mask.find_first();
       thr_no != BitmaskImpl::NotFound;
       thr_no = selfptr->m_wake_threads_mask.find_next(thr_no + 1)) {
    require(selfptr->m_wake_threads_mask.get(thr_no));
    thr_data *thrptr = &g_thr_repository->m_thread[thr_no];
    wakeup(&thrptr->m_waiter);
  }
  selfptr->m_wake_threads_mask.clear();
}

static void flush_all_local_signals_and_wakeup(struct thr_data *selfptr) {
  flush_all_local_signals(selfptr);
  wakeup_pending_signals(selfptr);
}

static inline void insert_local_signal(struct thr_data *selfptr,
                                       const SignalHeader *sh,
                                       const Uint32 *data,
                                       const Uint32 secPtr[3],
                                       const Uint32 dst) {
  struct thr_job_buffer *const local_buffer = selfptr->m_local_buffer;
  Uint32 last_signal = selfptr->m_first_local[dst].m_last_signal;
  Uint32 first_signal = selfptr->m_first_local[dst].m_first_signal;
  Uint32 num_signals = selfptr->m_first_local[dst].m_num_signals;
  Uint32 write_pos = local_buffer->m_len;
  Uint32 *buffer_data = &local_buffer->m_data[write_pos];
  num_signals++;
  buffer_data[0] = SIGNAL_RNIL;
  selfptr->m_first_local[dst].m_last_signal = write_pos;
  selfptr->m_first_local[dst].m_num_signals = num_signals;
  if (first_signal == SIGNAL_RNIL) {
    selfptr->m_first_local[dst].m_first_signal = write_pos;
  } else {
    local_buffer->m_data[last_signal] = write_pos;
  }
  Uint32 siglen = copy_signal(buffer_data + 2, sh, data, secPtr);
  selfptr->m_stat.m_priob_count++;
  selfptr->m_stat.m_priob_size += siglen;
#if SIZEOF_CHARP == 8
  /* Align to 8-byte boundary, to ensure aligned copies. */
  siglen = (siglen + 1) & ~((Uint32)1);
#endif
  buffer_data[1] = siglen;
  local_buffer->m_len = 2 + write_pos + siglen;
  assert(sh->theLength + sh->m_noOfSections <= 25);
  selfptr->m_local_signals_mask.set(dst);

  const unsigned self = selfptr->m_thr_no;
  const unsigned MAX_SIGNALS_BEFORE_FLUSH =
      (self >= first_receiver_thread_no) ? MAX_SIGNALS_BEFORE_FLUSH_RECEIVER
                                         : MAX_SIGNALS_BEFORE_FLUSH_OTHER;

  if (unlikely(local_buffer->m_len > MAX_LOCAL_BUFFER_USAGE)) {
    flush_all_local_signals(selfptr);
  } else if (unlikely(num_signals >= MAX_SIGNALS_BEFORE_FLUSH)) {
    flush_local_signals(selfptr, dst);
    if (selfptr->m_local_signals_mask.isclear()) {
      // All signals flushed, we have an empty local_buffer.
      selfptr->m_local_buffer->m_len = 0;
    }
  }
}

Uint32 mt_getMainThrmanInstance() {
  if (globalData.ndbMtMainThreads == 2 || globalData.ndbMtMainThreads == 1)
    return 1;
  else if (globalData.ndbMtMainThreads == 0)
    return 1 + globalData.ndbMtLqhThreads + globalData.ndbMtQueryThreads +
           globalData.ndbMtRecoverThreads + globalData.ndbMtTcThreads;
  else
    require(false);
  return 0;
}

void sendlocal(Uint32 self, const SignalHeader *s, const Uint32 *data,
               const Uint32 secPtr[3]) {
  Uint32 block = blockToMain(s->theReceiversBlockNumber);
  Uint32 instance = blockToInstance(s->theReceiversBlockNumber);

  Uint32 dst = block2ThreadId(block, instance);
  struct thr_repository *rep = g_thr_repository;
  struct thr_data *selfptr = &rep->m_thread[self];
  assert(my_thread_equal(selfptr->m_thr_id, my_thread_self()));
  insert_local_signal(selfptr, s, data, secPtr, dst);
}

void sendprioa(Uint32 self, const SignalHeader *s, const uint32 *data,
               const Uint32 secPtr[3]) {
  Uint32 block = blockToMain(s->theReceiversBlockNumber);
  Uint32 instance = blockToInstance(s->theReceiversBlockNumber);

  Uint32 dst = block2ThreadId(block, instance);
  struct thr_repository *rep = g_thr_repository;
  struct thr_data *selfptr = &rep->m_thread[self];
  assert(s->theVerId_signalNumber == GSN_START_ORD ||
         my_thread_equal(selfptr->m_thr_id, my_thread_self()));
  struct thr_data *dstptr = &rep->m_thread[dst];

  selfptr->m_stat.m_prioa_count++;
  Uint32 siglen = (sizeof(*s) >> 2) + s->theLength + s->m_noOfSections;
  selfptr->m_stat.m_prioa_size += siglen;

  thr_job_queue *q = &(dstptr->m_jba);
  if (selfptr == dstptr) {
    /**
     * Indicate that we sent Prio A signal to ourself.
     */
    selfptr->m_sent_local_prioa_signal = true;
  }

  lock(&dstptr->m_jba.m_write_lock);
  const bool buf_used =
      insert_prioa_signal(q, s, data, secPtr, selfptr->m_next_buffer);
  unlock(&dstptr->m_jba.m_write_lock);
  if (selfptr != dstptr) {
    wakeup(&(dstptr->m_waiter));
  }
  if (buf_used) selfptr->m_next_buffer = seize_buffer(rep, self, true);
}

/**
 * Send a signal to a remote node.
 *
 * (The signal is only queued here, and actually sent later in do_send()).
 */
SendStatus mt_send_remote(Uint32 self, const SignalHeader *sh, Uint8 prio,
                          const Uint32 *data, NodeId nodeId,
                          const LinearSectionPtr ptr[3]) {
  thr_repository *rep = g_thr_repository;
  struct thr_data *selfptr = &rep->m_thread[self];
  SendStatus ss;

  mt_send_handle handle(selfptr);
  /* prepareSend() is lock-free, as we have per-thread send buffers. */
  TrpId trp_id = 0;
  ss = globalTransporterRegistry.prepareSend(&handle, sh, prio, data, nodeId,
                                             trp_id, ptr);
  if (likely(ss == SEND_OK)) {
    register_pending_send(selfptr, trp_id);
  }
  return ss;
}

SendStatus mt_send_remote(Uint32 self, const SignalHeader *sh, Uint8 prio,
                          const Uint32 *data, NodeId nodeId,
                          class SectionSegmentPool *thePool,
                          const SegmentedSectionPtr ptr[3]) {
  thr_repository *rep = g_thr_repository;
  struct thr_data *selfptr = &rep->m_thread[self];
  SendStatus ss;

  mt_send_handle handle(selfptr);
  TrpId trp_id = 0;
  ss = globalTransporterRegistry.prepareSend(&handle, sh, prio, data, nodeId,
                                             trp_id, *thePool, ptr);
  if (likely(ss == SEND_OK)) {
    register_pending_send(selfptr, trp_id);
  }
  return ss;
}

SendStatus mt_send_remote_over_all_links(Uint32 self, const SignalHeader *sh,
                                         Uint8 prio, const Uint32 *data,
                                         NodeId nodeId) {
  thr_repository *rep = g_thr_repository;
  struct thr_data *selfptr = &rep->m_thread[self];
  SendStatus ss;

  mt_send_handle handle(selfptr);
  /* prepareSend() is lock-free, as we have per-thread send buffers. */
  TrpBitmask trp_ids;
  ss = globalTransporterRegistry.prepareSendOverAllLinks(&handle, sh, prio,
                                                         data, nodeId, trp_ids);
  if (likely(ss == SEND_OK)) {
    unsigned trp_id = trp_ids.find(0);
    while (trp_id != trp_ids.NotFound) {
      require(trp_id < MAX_NTRANSPORTERS);
      register_pending_send(selfptr, trp_id);
      trp_id = trp_ids.find(trp_id + 1);
    }
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
static void sendprioa_STOP_FOR_CRASH(const struct thr_data *selfptr,
                                     Uint32 dst) {
  SignalT<StopForCrash::SignalLength> signalT;
  struct thr_repository *rep = g_thr_repository;
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

  std::memset(&signalT.header, 0, sizeof(SignalHeader));
  signalT.header.theVerId_signalNumber = GSN_STOP_FOR_CRASH;
  signalT.header.theReceiversBlockNumber = bno;
  signalT.header.theSendersBlockRef = 0;
  signalT.header.theTrace = 0;
  signalT.header.theSendersSignalId = 0;
  signalT.header.theSignalId = 0;
  signalT.header.theLength = StopForCrash::SignalLength;
  StopForCrash *stopForCrash = CAST_PTR(StopForCrash, &signalT.theData[0]);
  stopForCrash->flags = 0;

  thr_job_queue *q = &(dstptr->m_jba);
  /**
   * Ensure that a crash while holding m_write_lock won't block
   * dump process forever.
   */
  Uint64 loop_count = 0;
  const NDB_TICKS start_try_lock = NdbTick_getCurrentTicks();
  while (trylock(&dstptr->m_jba.m_write_lock) != 0) {
    if (++loop_count >= 10000) {
      const NDB_TICKS now = NdbTick_getCurrentTicks();
      if (NdbTick_Elapsed(start_try_lock, now).milliSec() > MAX_WAIT) {
        return;
      }
      NdbSleep_MilliSleep(1);
      loop_count = 0;
    }
  }
  insert_prioa_signal(q, &signalT.header, signalT.theData, NULL, &dummy_buffer);
  unlock(&dstptr->m_jba.m_write_lock);
  {
    loop_count = 0;
    /**
     * Ensure that a crash while holding wakeup lock won't block
     * dump process forever. We will wait at most 3 seconds.
     */
    const NDB_TICKS start_try_wakeup = NdbTick_getCurrentTicks();
    while (try_wakeup(&(dstptr->m_waiter)) != 0) {
      if (++loop_count >= 10000) {
        const NDB_TICKS now = NdbTick_getCurrentTicks();
        if (NdbTick_Elapsed(start_try_wakeup, now).milliSec() > MAX_WAIT) {
          return;
        }
        NdbSleep_MilliSleep(1);
        loop_count = 0;
      }
    }
  }
}

/**
 * init functions
 */
static void queue_init(struct thr_tq *tq) {
  tq->m_next_timer = 0;
  tq->m_current_time = 0;
  tq->m_next_free = RNIL;
  tq->m_cnt[0] = tq->m_cnt[1] = tq->m_cnt[2] = 0;
  std::memset(tq->m_delayed_signals, 0, sizeof(tq->m_delayed_signals));
}

static bool may_communicate(unsigned from, unsigned to);

static void thr_init(struct thr_repository *rep, struct thr_data *selfptr,
                     unsigned int cnt, unsigned thr_no) {
  Uint32 i;

  selfptr->m_thr_no = thr_no;
  selfptr->m_next_jbb_no = 0;
  selfptr->m_max_signals_per_jb = MAX_SIGNALS_PER_JB;
  selfptr->m_total_extra_signals =
      compute_max_signals_to_execute(thr_job_queue::RESERVED);
  selfptr->m_first_free = 0;
  selfptr->m_first_unused = 0;
  selfptr->m_send_instance_no = 0;
  selfptr->m_send_instance = NULL;
  selfptr->m_nosend = 1;
  selfptr->m_local_signals_mask.clear();
  selfptr->m_congested_threads_mask.clear();
  selfptr->m_wake_threads_mask.clear();
  selfptr->m_jbb_estimated_queue_size_in_words = 0;
  selfptr->m_ldm_multiplier = 1;
  selfptr->m_jbb_estimate_next_set = true;
  selfptr->m_load_indicator = 1;
#ifdef DEBUG_SCHED_STATS
  for (Uint32 i = 0; i < 10; i++) selfptr->m_jbb_estimated_queue_stats[i] = 0;
  selfptr->m_jbb_total_words = 0;
#endif
  selfptr->m_read_jbb_state_consumed = true;
  selfptr->m_cpu_percentage_changed = true;
  {
    char buf[100];
    BaseString::snprintf(buf, sizeof(buf), "jbalock thr: %u", thr_no);
    register_lock(&selfptr->m_jba.m_write_lock, buf);

    selfptr->m_jba.m_read_index = 0;
    selfptr->m_jba.m_cached_read_index = 0;
    selfptr->m_jba.m_write_index = 0;
    selfptr->m_jba.m_pending_signals = 0;
    thr_job_buffer *buffer = seize_buffer(rep, thr_no, true);
    selfptr->m_jba.m_buffers[0] = buffer;
    selfptr->m_jba.m_current_write_buffer = buffer;
    selfptr->m_jba.m_current_write_buffer_len = 0;

    selfptr->m_jba_read_state.m_read_index = 0;
    selfptr->m_jba_read_state.m_read_buffer = buffer;
    selfptr->m_jba_read_state.m_read_pos = 0;
    selfptr->m_jba_read_state.m_read_end = 0;
    selfptr->m_jba_read_state.m_write_index = 0;
    for (Uint32 i = 0; i < NDB_MAX_BLOCK_THREADS; i++) {
      selfptr->m_first_local[i].m_num_signals = 0;
      selfptr->m_first_local[i].m_first_signal = SIGNAL_RNIL;
      selfptr->m_first_local[i].m_last_signal = SIGNAL_RNIL;
    }
    selfptr->m_local_buffer = seize_buffer(rep, thr_no, false);
    selfptr->m_next_buffer = seize_buffer(rep, thr_no, false);
    selfptr->m_send_buffer_pool.set_pool(&rep->m_sb_pool);
  }
  for (Uint32 i = 0; i < glob_num_job_buffers_per_thread; i++) {
    char buf[100];
    BaseString::snprintf(buf, sizeof(buf), "jbblock(%u)", i);
    register_lock(&selfptr->m_jbb[i].m_write_lock, buf);

    selfptr->m_jbb[i].m_read_index = 0;
    selfptr->m_jbb[i].m_write_index = 0;
    selfptr->m_jbb[i].m_pending_signals = 0;
    selfptr->m_jbb[i].m_cached_read_index = 0;

    /**
     * Initially no job_buffers are assigned and 'len' set to 'full',
     * such that we will not write into the null-buffer.
     * First write into the buffer will detect current as 'full',
     * and assign a real job_buffer.
     */
    selfptr->m_jbb[i].m_buffers[0] = nullptr;
    selfptr->m_jbb[i].m_current_write_buffer = nullptr;
    selfptr->m_jbb[i].m_current_write_buffer_len = thr_job_buffer::SIZE;

    // jbb_read_state is inited to an empty sentinel -> no signals
    selfptr->m_jbb_read_state[i].m_read_buffer = &empty_job_buffer;

    selfptr->m_jbb_read_state[i].m_read_index = 0;
    selfptr->m_jbb_read_state[i].m_read_pos = 0;
    selfptr->m_jbb_read_state[i].m_read_end = 0;
    selfptr->m_jbb_read_state[i].m_write_index = 0;
  }
  queue_init(&selfptr->m_tq);

  std::memset(&selfptr->m_stat, 0, sizeof(selfptr->m_stat));

  selfptr->m_pending_send_count = 0;
  selfptr->m_pending_send_mask.clear();

  selfptr->m_instance_count = 0;
  for (i = 0; i < MAX_INSTANCES_PER_THREAD; i++)
    selfptr->m_instance_list[i] = 0;

  std::memset(&selfptr->m_send_buffers, 0, sizeof(selfptr->m_send_buffers));

  selfptr->m_thread = 0;
  selfptr->m_cpu = NO_LOCK_CPU;
#ifdef ERROR_INSERT
  selfptr->m_delayed_prepare = false;
#endif
}

static void receive_lock_init(Uint32 recv_thread_id, thr_repository *rep) {
  char buf[100];
  BaseString::snprintf(buf, sizeof(buf), "receive lock thread id %d",
                       recv_thread_id);
  register_lock(&rep->m_receive_lock[recv_thread_id], buf);
}

static void send_buffer_init(Uint32 id, thr_repository::send_buffer *sb) {
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
  std::memset(&sb->m_buffer, 0, sizeof(sb->m_buffer));
  std::memset(&sb->m_sending, 0, sizeof(sb->m_sending));
  std::memset(sb->m_read_index, 0, sizeof(sb->m_read_index));
}

static void rep_init(struct thr_repository *rep, unsigned int cnt,
                     Ndbd_mem_manager *mm) {
  rep->m_mm = mm;

  rep->m_thread_count = cnt;
  for (unsigned int i = 0; i < cnt; i++) {
    thr_init(rep, &rep->m_thread[i], cnt, i);
  }

  rep->stopped_threads = 0;
  NdbMutex_Init(&rep->stop_for_crash_mutex);
  NdbCondition_Init(&rep->stop_for_crash_cond);

  for (Uint32 i = 0; i < NDB_ARRAY_SIZE(rep->m_receive_lock); i++) {
    receive_lock_init(i, rep);
  }
  for (int i = 0; i < MAX_NTRANSPORTERS; i++) {
    send_buffer_init(i, rep->m_send_buffers + i);
  }

  std::memset(rep->m_thread_send_buffers, 0,
              sizeof(rep->m_thread_send_buffers));
}

/**
 * Thread Config
 */

static Uint32 get_total_number_of_block_threads(void) {
  return (globalData.ndbMtMainThreads + globalData.ndbMtLqhThreads +
          globalData.ndbMtQueryThreads + globalData.ndbMtRecoverThreads +
          globalData.ndbMtTcThreads + globalData.ndbMtReceiveThreads);
}

static Uint32 get_num_trps() {
  Uint32 count = 0;
  for (Uint32 id = 1; id < MAX_NTRANSPORTERS; id++) {
    if (globalTransporterRegistry.get_transporter(id)) {
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
#define MIN_SEND_BUFFER_GENERAL (512)    // 16M
#define MIN_SEND_BUFFER_PER_NODE (8)     // 256k
#define MIN_SEND_BUFFER_PER_THREAD (64)  // 2M

Uint32 mt_get_extra_send_buffer_pages(Uint32 curr_num_pages,
                                      Uint32 extra_mem_pages) {
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

  if (extra_mem_pages == 0) {
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

    if ((curr_num_pages + extra_pages) < min_pages) {
      extra_pages = min_pages - curr_num_pages;
    }
  }
  return extra_pages;
}

Uint32 compute_jb_pages(struct EmulatorData *ed) {
  Uint32 tot = 0;
  Uint32 cnt = get_total_number_of_block_threads();
  Uint32 num_job_buffers_per_thread = MIN(cnt, NUM_JOB_BUFFERS_PER_THREAD);
  Uint32 num_main_threads = globalData.ndbMtMainThreads;
  Uint32 num_receive_threads = globalData.ndbMtReceiveThreads;
  Uint32 num_lqh_threads =
      globalData.ndbMtLqhThreads > 0 ? globalData.ndbMtLqhThreads : 1;
  Uint32 num_tc_threads = globalData.ndbMtTcThreads;
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
  perthread += thr_job_queue::SIZE;

  if (cnt > NUM_JOB_BUFFERS_PER_THREAD) {
    /**
     * The case when we have a shared pool of buffers for each thread.
     *
     * Each threads has its own job_queues for 'prio B' signals
     * There are glob_num_job_buffers_per_thread of this in each thread.
     */
    perthread += (thr_job_queue::SIZE * num_job_buffers_per_thread);
  } else {
    /**
     * All communication links are one-to-one and no mutex used, no need
     * to add buffers for unused links.
     *
     * Receiver threads will be able to communicate with all other
     * threads except other receive threads.
     */
    tot +=
        num_receive_threads * (cnt - num_receive_threads) * thr_job_queue::SIZE;
    /**
     * LQH threads can communicate with TC threads and main threads.
     * Cannot communicate with receive threads and other LQH threads,
     * but it can communicate with itself.
     */
    tot += num_lqh_threads * (num_tc_threads + num_main_threads + 1) *
           thr_job_queue::SIZE;

    /**
     * First LDM thread is special as it will act as client
     * during backup. It will send to, and receive from (2x)
     * the 'num_lqh_threads - 1' other LQH threads.
     */
    tot += 2 * (num_lqh_threads - 1) * thr_job_queue::SIZE;

    /**
     * TC threads can communicate with SPJ-, LQH- and main threads.
     * Cannot communicate with receive threads and other TC threads,
     * but as SPJ is located together with TC, it is counted as it
     * communicate with all TC threads.
     */
    tot += num_tc_threads *
           (num_lqh_threads + num_main_threads + num_tc_threads) *
           thr_job_queue::SIZE;

    /**
     * Main threads can communicate with all other threads
     */
    tot += num_main_threads * cnt * thr_job_queue::SIZE;
  }

  /**
   * Each thread keeps a available free page in 'm_next_buffer'
   * in case it is required by insert_*_signal() into JBA or JBB.
   */
  perthread += 1;

  /**
   * Each thread use a single buffer as temporary storage for
   * local signals. (m_local_buffer)
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
   * it will never cache more than MAX-1 pages. Pages are also returned to
   * global allocator as soon as MAX is reached.
   */
  perthread += THR_FREE_BUF_MAX - 1;

  /**
   * Start by calculating the basic number of pages required for
   * our 'cnt' block threads.
   * (no inter-thread communication assumed so far)
   */
  tot += cnt * perthread;

  return tot;
}

ThreadConfig::ThreadConfig() {
  /**
   * We take great care within struct thr_repository to optimize
   * cache line placement of the different members. This all
   * depends on that the base address of thr_repository itself
   * is cache line aligned.
   *
   * So we allocate a char[] sufficient large to hold the
   * thr_repository object, with added bytes for placing
   * g_thr_repository on a CL-alligned offset within it.
   */
  g_thr_repository_mem = new char[sizeof(thr_repository) + NDB_CL];
  const int alligned_offs = NDB_CL_PADSZ((UintPtr)g_thr_repository_mem);
  char *cache_alligned_mem = &g_thr_repository_mem[alligned_offs];
  require((((UintPtr)cache_alligned_mem) % NDB_CL) == 0);
  g_thr_repository = new (cache_alligned_mem) thr_repository();
}

ThreadConfig::~ThreadConfig() {
  g_thr_repository->~thr_repository();
  g_thr_repository = NULL;
  delete[] g_thr_repository_mem;
  g_thr_repository_mem = NULL;
}

/*
 * We must do the init here rather than in the constructor, since at
 * constructor time the global memory manager is not available.
 */
void ThreadConfig::init() {
  Uint32 num_lqh_threads = globalData.ndbMtLqhThreads;
  Uint32 num_tc_threads = globalData.ndbMtTcThreads;
  Uint32 num_recv_threads = globalData.ndbMtReceiveThreads;
  Uint32 num_query_threads = globalData.ndbMtQueryThreads;
  Uint32 num_recover_threads = globalData.ndbMtRecoverThreads;

  first_receiver_thread_no = globalData.ndbMtMainThreads + num_lqh_threads +
                             num_query_threads + num_recover_threads +
                             num_tc_threads;
  glob_num_threads = first_receiver_thread_no + num_recv_threads;
  glob_unused[0] = 0;  // Silence compiler
  if (globalData.ndbMtMainThreads == 0)
    glob_ndbfs_thr_no = first_receiver_thread_no;
  else
    glob_ndbfs_thr_no = 0;
  require(glob_num_threads <= MAX_BLOCK_THREADS);
  glob_num_job_buffers_per_thread =
      MIN(glob_num_threads, NUM_JOB_BUFFERS_PER_THREAD);
  glob_num_writers_per_job_buffers =
      (glob_num_threads + NUM_JOB_BUFFERS_PER_THREAD - 1) /
      NUM_JOB_BUFFERS_PER_THREAD;
  if (glob_num_job_buffers_per_thread < glob_num_threads) {
    glob_use_write_lock_mutex = true;
  } else {
    glob_use_write_lock_mutex = false;
  }

  glob_num_tc_threads = num_tc_threads;
  if (glob_num_tc_threads == 0) glob_num_tc_threads = 1;

  g_eventLogger->info("NDBMT: number of block threads=%u", glob_num_threads);

  ::rep_init(g_thr_repository, glob_num_threads,
             globalEmulatorData.m_mem_manager);
}

/**
 * return receiver thread handling a particular trp
 *   returned number is indexed from 0 and upwards to #receiver threads
 *   (or MAX_NODES is none)
 */
Uint32 mt_get_recv_thread_idx(TrpId trp_id) {
  assert(trp_id < NDB_ARRAY_SIZE(g_trp_to_recv_thr_map));
  return g_trp_to_recv_thr_map[trp_id];
}

static void assign_receiver_threads(void) {
  Uint32 num_recv_threads = globalData.ndbMtReceiveThreads;
  Uint32 recv_thread_idx = 0;
  Uint32 recv_thread_idx_shm = 0;
  for (Uint32 trp_id = 1; trp_id < MAX_NTRANSPORTERS; trp_id++) {
    Transporter *trp = globalTransporterRegistry.get_transporter(trp_id);

    /**
     * Ensure that shared memory transporters are well distributed
     * over all receive threads, so distribute those independent of
     * rest of transporters.
     */
    if (trp) {
      if (globalTransporterRegistry.is_shm_transporter(trp_id)) {
        g_trp_to_recv_thr_map[trp_id] = recv_thread_idx_shm;
        globalTransporterRegistry.set_recv_thread_idx(trp, recv_thread_idx_shm);
        DEB_MULTI_TRP(("SHM trp %u uses recv_thread_idx: %u", trp_id,
                       recv_thread_idx_shm));
        recv_thread_idx_shm++;
        if (recv_thread_idx_shm == num_recv_threads) recv_thread_idx_shm = 0;
      } else {
        g_trp_to_recv_thr_map[trp_id] = recv_thread_idx;
        DEB_MULTI_TRP(
            ("TCP trp %u uses recv_thread_idx: %u", trp_id, recv_thread_idx));
        globalTransporterRegistry.set_recv_thread_idx(trp, recv_thread_idx);
        recv_thread_idx++;
        if (recv_thread_idx == num_recv_threads) recv_thread_idx = 0;
      }
    } else {
      /* Flag for no transporter */
      g_trp_to_recv_thr_map[trp_id] = MAX_NTRANSPORTERS;
    }
  }
  return;
}

void mt_assign_recv_thread_new_trp(TrpId trp_id) {
  if (g_trp_to_recv_thr_map[trp_id] != MAX_NTRANSPORTERS) {
    /* Already assigned in the past, keep assignment */
    return;
  }
  Uint32 num_recv_threads = globalData.ndbMtReceiveThreads;
  Uint32 next_recv_thread_tcp = 0;
  Uint32 next_recv_thread_shm = 0;
  for (Uint32 id = 1; id < MAX_NTRANSPORTERS; id++) {
    if (id == trp_id) continue;
    Transporter *trp = globalTransporterRegistry.get_transporter(id);
    if (trp) {
      if (globalTransporterRegistry.is_shm_transporter(id)) {
        next_recv_thread_shm = g_trp_to_recv_thr_map[id];
      } else {
        next_recv_thread_tcp = g_trp_to_recv_thr_map[id];
      }
    }
  }
  Transporter *trp = globalTransporterRegistry.get_transporter(trp_id);
  require(trp);
  Uint32 choosen_recv_thread;
  if (globalTransporterRegistry.is_shm_transporter(trp_id)) {
    next_recv_thread_shm++;
    if (next_recv_thread_shm == num_recv_threads) next_recv_thread_shm = 0;
    g_trp_to_recv_thr_map[trp_id] = next_recv_thread_shm;
    choosen_recv_thread = next_recv_thread_shm;
    globalTransporterRegistry.set_recv_thread_idx(trp, next_recv_thread_shm);
    DEB_MULTI_TRP(("SHM multi trp %u uses recv_thread_idx: %u", trp_id,
                   next_recv_thread_shm));
  } else {
    next_recv_thread_tcp++;
    if (next_recv_thread_tcp == num_recv_threads) next_recv_thread_tcp = 0;
    g_trp_to_recv_thr_map[trp_id] = next_recv_thread_tcp;
    choosen_recv_thread = next_recv_thread_tcp;
    globalTransporterRegistry.set_recv_thread_idx(trp, next_recv_thread_tcp);
    DEB_MULTI_TRP(("TCP multi trp %u uses recv_thread_idx: %u", trp_id,
                   next_recv_thread_tcp));
  }
  TransporterReceiveHandleKernel *recvdata =
      g_trp_receive_handle_ptr[choosen_recv_thread];
  recvdata->m_transporters.set(trp_id);
}

bool mt_is_recv_thread_for_new_trp(Uint32 self, TrpId trp_id) {
  struct thr_repository *rep = g_thr_repository;
  struct thr_data *selfptr = &rep->m_thread[self];
  unsigned thr_no = selfptr->m_thr_no;
  require(thr_no >= first_receiver_thread_no);
  Uint32 recv_thread_idx = thr_no - first_receiver_thread_no;
  if (recv_thread_idx != g_trp_to_recv_thr_map[trp_id]) {
    return false;
  }
  return true;
}

void ThreadConfig::ipControlLoop(NdbThread *pThis) {
  unsigned int thr_no;
  struct thr_repository *rep = g_thr_repository;

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

#ifdef DBG_NDB_HAVE_CPU_PAUSE  // Intentionally not defined
  for (Uint32 i = 0; i < 5; i++) {
    const NDB_TICKS start = NdbTick_getCurrentTicks();
    NdbSpin();
    const NDB_TICKS now = NdbTick_getCurrentTicks();
    const Uint64 nanos_passed = NdbTick_Elapsed(start, now).nanoSec();
    g_eventLogger->info("::ipControlLoop, NdbSpin() took %llu ns, loops:%llu\n",
                        nanos_passed, NdbSpin_get_num_spin_loops());
  }
#endif

  if (globalData.ndbMtSendThreads) {
    /**
     * new operator do not ensure alignment for overaligned data types.
     * As for g_thr_repository, overallocate memory and construct the
     * thr_send_threads object within at aligned address.
     */
    g_send_threads_mem = new char[sizeof(thr_send_threads) + NDB_CL];
    const int aligned_offs = NDB_CL_PADSZ((UintPtr)g_send_threads_mem);
    char *cache_aligned_mem = &g_send_threads_mem[aligned_offs];
    require((((UintPtr)cache_aligned_mem) % NDB_CL) == 0);
    g_send_threads = new (cache_aligned_mem) thr_send_threads();
  }

  /**
   * assign trps to receiver threads
   */
  assign_receiver_threads();

  /* Start the send thread(s) */
  if (g_send_threads) {
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
  for (thr_no = 0; thr_no < glob_num_threads; thr_no++) {
    NDB_TICKS now = NdbTick_getCurrentTicks();
    rep->m_thread[thr_no].m_ticks = now;
    rep->m_thread[thr_no].m_scan_real_ticks = now;

    if (thr_no == first_receiver_thread_no)
      continue;  // Will run in the main thread.

    /*
     * The NdbThread_Create() takes void **, but that is cast to void * when
     * passed to the thread function. Which is kind of strange ...
     */
    if (thr_no < first_receiver_thread_no) {
      /* Start block threads */
      struct NdbThread *thread_ptr = NdbThread_Create(
          mt_job_thread_main, (void **)(rep->m_thread + thr_no), 1024 * 1024,
          "execute thread",  // ToDo add number
          NDB_THREAD_PRIO_MEAN);
      require(thread_ptr != NULL);
      rep->m_thread[thr_no].m_thr_index =
          globalEmulatorData.theConfiguration->addThread(thread_ptr,
                                                         BlockThread);
      rep->m_thread[thr_no].m_thread = thread_ptr;
    } else {
      /* Start a receiver thread, also block thread for TRPMAN */
      struct NdbThread *thread_ptr =
          NdbThread_Create(mt_receiver_thread_main,
                           (void **)(&rep->m_thread[thr_no]), 1024 * 1024,
                           "receive thread",  // ToDo add number
                           NDB_THREAD_PRIO_MEAN);
      require(thread_ptr != NULL);
      globalEmulatorData.theConfiguration->addThread(thread_ptr, ReceiveThread);
      rep->m_thread[thr_no].m_thread = thread_ptr;
    }
  }

  /* Now run the main loop for first receiver thread directly. */
  rep->m_thread[first_receiver_thread_no].m_thread = pThis;
  mt_receiver_thread_main(&(rep->m_thread[first_receiver_thread_no]));

  /* Wait for all threads to shutdown. */
  for (thr_no = 0; thr_no < glob_num_threads; thr_no++) {
    if (thr_no == first_receiver_thread_no) continue;
    void *dummy_return_status;
    NdbThread_WaitFor(rep->m_thread[thr_no].m_thread, &dummy_return_status);
    globalEmulatorData.theConfiguration->removeThread(
        rep->m_thread[thr_no].m_thread);
    NdbThread_Destroy(&(rep->m_thread[thr_no].m_thread));
  }

  /* Delete send threads, includes waiting for threads to shutdown */
  if (g_send_threads) {
    g_send_threads->~thr_send_threads();
    g_send_threads = NULL;
    delete[] g_send_threads_mem;
    g_send_threads_mem = NULL;
  }
  globalEmulatorData.theConfiguration->removeThread(pThis);
}

int ThreadConfig::doStart(NodeState::StartLevel startLevel) {
  SignalT<3> signalT;
  std::memset(&signalT.header, 0, sizeof(SignalHeader));

  signalT.header.theVerId_signalNumber = GSN_START_ORD;
  signalT.header.theReceiversBlockNumber = CMVMI;
  signalT.header.theSendersBlockRef = 0;
  signalT.header.theTrace = 0;
  signalT.header.theSignalId = 0;
  signalT.header.theLength = StartOrd::SignalLength;

  StartOrd *startOrd = CAST_PTR(StartOrd, &signalT.theData[0]);
  startOrd->restartInfo = 0;

  sendprioa(block2ThreadId(CMVMI, 0), &signalT.header, signalT.theData, 0);
  return 0;
}

Uint32 FastScheduler::traceDumpGetNumThreads() {
  /* The last thread is only for receiver -> no trace file. */
  return glob_num_threads;
}

bool FastScheduler::traceDumpGetJam(Uint32 thr_no,
                                    const JamEvent *&thrdTheEmulatedJam,
                                    Uint32 &thrdTheEmulatedJamIndex) {
  if (thr_no >= glob_num_threads) return false;

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

void FastScheduler::traceDumpPrepare(NdbShutdownType &nst) {
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
  thr_data *selfptr = NDB_THREAD_TLS_THREAD;
  /* The selfptr might be NULL, or pointer to thread that crashed. */

  Uint32 waitFor_count = 0;
  NdbMutex_Lock(&g_thr_repository->stop_for_crash_mutex);
  g_thr_repository->stopped_threads = 0;
  NdbMutex_Unlock(&g_thr_repository->stop_for_crash_mutex);

  for (Uint32 thr_no = 0; thr_no < glob_num_threads; thr_no++) {
    if (selfptr != NULL && selfptr->m_thr_no == thr_no) {
      /* This is own thread; we have already stopped processing. */
      continue;
    }

    sendprioa_STOP_FOR_CRASH(selfptr, thr_no);

    waitFor_count++;
  }

  static const Uint32 max_wait_seconds = 2;
  const NDB_TICKS start = NdbTick_getCurrentTicks();
  NdbMutex_Lock(&g_thr_repository->stop_for_crash_mutex);
  while (g_thr_repository->stopped_threads < waitFor_count) {
    NdbCondition_WaitTimeout(&g_thr_repository->stop_for_crash_cond,
                             &g_thr_repository->stop_for_crash_mutex, 10);
    const NDB_TICKS now = NdbTick_getCurrentTicks();
    if (NdbTick_Elapsed(start, now).seconds() > max_wait_seconds)
      break;  // Give up
  }
  if (g_thr_repository->stopped_threads < waitFor_count) {
    if (nst != NST_ErrorInsert) {
      nst = NST_Watchdog;  // Make this abort fast
    }
    g_eventLogger->info(
        "Warning: %d thread(s) did not stop before starting crash dump.",
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

void ErrorReporter::prepare_to_crash(bool first_phase,
                                     bool error_insert_crash) {
  if (first_phase) {
    NdbMutex_Lock(&g_thr_repository->stop_for_crash_mutex);
    if (crash_started && error_insert_crash) {
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
  } else if (crash_started) {
    (void)error_insert_crash;
    /**
     * No need to proceed since somebody already started handling the crash.
     * We proceed by calling mt_execSTOP_FOR_CRASH to stop this thread
     * in a manner that is similar to if we received the signal
     * STOP_FOR_CRASH.
     */
    NdbMutex_Unlock(&g_thr_repository->stop_for_crash_mutex);
    mt_execSTOP_FOR_CRASH();
  } else {
    /**
     * No crash had started previously, we will take care of it. Before
     * handling it we will mark the crash handling as started.
     */
    crash_started = true;
    NdbMutex_Unlock(&g_thr_repository->stop_for_crash_mutex);
  }
}

void mt_execSTOP_FOR_CRASH() {
  const thr_data *selfptr = NDB_THREAD_TLS_THREAD;

  /* Signal exec threads have some state cleanup to do
   * We can be executed from other threads.
   * (Thread Watchdog, others via Unix signal handler)
   */
  if (selfptr != NULL) {
    /* Signal exec thread, some state cleanup to do */
    NdbMutex_Lock(&g_thr_repository->stop_for_crash_mutex);
    g_thr_repository->stopped_threads++;
    NdbCondition_Signal(&g_thr_repository->stop_for_crash_cond);
    NdbMutex_Unlock(&g_thr_repository->stop_for_crash_mutex);

    globalEmulatorData.theWatchDog->unregisterWatchedThread(selfptr->m_thr_no);
  }

  my_thread_exit(NULL);
}

void FastScheduler::dumpSignalMemory(Uint32 thr_no, FILE *out) {
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
   * in this array.
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
  if (watchDogCounter) *watchDogCounter = 4;

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
  while (idx != thr_ptr->m_first_unused) {
    const thr_job_buffer *q = thr_ptr->m_free_fifo[idx];
    if (q->m_len > 0) {
      jbs[num_jbs].m_jb = q;
      jbs[num_jbs].m_pos = 0;
      jbs[num_jbs].m_max = q->m_len;
      num_jbs++;
    }
    idx = (idx + 1) % THR_FREE_BUF_MAX;
  }

  /* Load any active prio B buffers. */
  for (Uint32 i = 0; i < glob_num_job_buffers_per_thread; i++) {
    const thr_job_queue *q = thr_ptr->m_jbb + i;
    const thr_jb_read_state *r = thr_ptr->m_jbb_read_state + i;
    Uint32 read_pos = r->m_read_pos;
    if (read_pos > 0) {
      jbs[num_jbs].m_jb = q->m_buffers[r->m_read_index];
      jbs[num_jbs].m_pos = 0;
      jbs[num_jbs].m_max = read_pos;
      num_jbs++;
    }
  }

  /* Load any active prio A buffer. */
  const thr_jb_read_state *r = &thr_ptr->m_jba_read_state;
  Uint32 read_pos = r->m_read_pos;
  if (read_pos > 0) {
    jbs[num_jbs].m_jb = thr_ptr->m_jba.m_buffers[r->m_read_index];
    jbs[num_jbs].m_pos = 0;
    jbs[num_jbs].m_max = read_pos;
    num_jbs++;
  }

  /* Use the next signal id as the smallest (oldest).
   *
   * Subtracting two signal ids with the smallest makes
   * them comparable using standard comparison of Uint32,
   * there the biggest value is the newest.
   * For example,
   *   (m_signal_id_counter - smallest_signal_id) == UINT32_MAX
   */
  const Uint32 smallest_signal_id = thr_ptr->m_signal_id_counter + 1;

  /* Now pick out one signal at a time, in signal id order. */
  while (num_jbs > 0) {
    if (watchDogCounter) *watchDogCounter = 4;

    /* Search out the smallest signal id remaining. */
    Uint32 idx_min = 0;
    const Uint32 *p = jbs[idx_min].m_jb->m_data + jbs[idx_min].m_pos;
    const SignalHeader *s_min = reinterpret_cast<const SignalHeader *>(p);
    Uint32 sid_min_adjusted = s_min->theSignalId - smallest_signal_id;

    for (Uint32 i = 1; i < num_jbs; i++) {
      p = jbs[i].m_jb->m_data + jbs[i].m_pos;
      const SignalHeader *s = reinterpret_cast<const SignalHeader *>(p);
      const Uint32 sid_adjusted = s->theSignalId - smallest_signal_id;
      if (sid_adjusted < sid_min_adjusted) {
        idx_min = i;
        s_min = s;
        sid_min_adjusted = sid_adjusted;
      }
    }

    /* We found the next signal, now put it in the ordered cyclic buffer. */
    signalSequence[seq_end].ptr = s_min;
    signalSequence[seq_end].prioa = jbs[idx_min].m_jb->m_prioa;
    Uint32 siglen =
        (sizeof(SignalHeader) >> 2) + s_min->m_noOfSections + s_min->theLength;
#if SIZEOF_CHARP == 8
    /* Align to 8-byte boundary, to ensure aligned copies. */
    siglen = (siglen + 1) & ~((Uint32)1);
#endif
    jbs[idx_min].m_pos += siglen;
    if (jbs[idx_min].m_pos >= jbs[idx_min].m_max) {
      /* We are done with this job buffer. */
      num_jbs--;
      jbs[idx_min] = jbs[num_jbs];
    }
    seq_end = (seq_end + 1) % MAX_SIGNALS_TO_DUMP;
    /* Drop old signals if too many available in history. */
    if (seq_end == seq_start) seq_start = (seq_start + 1) % MAX_SIGNALS_TO_DUMP;
  }

  /* Now, having build the correct signal sequence, we can dump them all. */
  fprintf(out, "\n");
  bool first_one = true;
  bool out_of_signals = false;
  Uint32 lastSignalId = 0;
  while (seq_end != seq_start) {
    if (watchDogCounter) *watchDogCounter = 4;

    if (seq_end == 0) seq_end = MAX_SIGNALS_TO_DUMP;
    seq_end--;
    SignalT<25> signal;
    const SignalHeader *s = signalSequence[seq_end].ptr;
    unsigned siglen = (sizeof(*s) >> 2) + s->theLength;
    if (siglen > MAX_SIGNAL_SIZE) siglen = MAX_SIGNAL_SIZE;  // Sanity check
    memcpy(&signal.header, s, 4 * siglen);
    // instance number in trace file is confusing if not MT LQH
    if (globalData.ndbMtLqhWorkers == 0)
      signal.header.theReceiversBlockNumber &= NDBMT_BLOCK_MASK;

    const Uint32 *posptr = reinterpret_cast<const Uint32 *>(s);
    signal.m_sectionPtrI[0] = posptr[siglen + 0];
    signal.m_sectionPtrI[1] = posptr[siglen + 1];
    signal.m_sectionPtrI[2] = posptr[siglen + 2];
    bool prioa = signalSequence[seq_end].prioa;

    /* Make sure to display clearly when there is a gap in the dump. */
    if (!first_one && !out_of_signals && (s->theSignalId + 1) != lastSignalId) {
      out_of_signals = true;
      fprintf(out,
              "\n\n\nNo more prio %s signals, rest of dump will be "
              "incomplete.\n\n\n\n",
              prioa ? "B" : "A");
    }
    first_one = false;
    lastSignalId = s->theSignalId;

    fprintf(out, "--------------- Signal ----------------\n");
    Uint32 prio = (prioa ? JBA : JBB);
    SignalLoggerManager::printSignalHeader(out, signal.header, prio,
                                           globalData.ownId, true);
    SignalLoggerManager::printSignalData(out, signal.header,
                                         &signal.theData[0]);
  }
  fflush(out);
}

int FastScheduler::traceDumpGetCurrentThread() {
  const thr_data *selfptr = NDB_THREAD_TLS_THREAD;

  /* The selfptr might be NULL, or pointer to thread that crashed. */
  if (selfptr == 0) {
    return -1;
  } else {
    return (int)selfptr->m_thr_no;
  }
}

void mt_section_lock() { lock(&(g_thr_repository->m_section_lock)); }

void mt_section_unlock() { unlock(&(g_thr_repository->m_section_lock)); }

void mt_mem_manager_init() {}

void mt_mem_manager_lock() { lock(&(g_thr_repository->m_mem_manager_lock)); }

void mt_mem_manager_unlock() {
  unlock(&(g_thr_repository->m_mem_manager_lock));
}

Vector<mt_lock_stat> g_locks;
template class Vector<mt_lock_stat>;

static void register_lock(const void *ptr, const char *name) {
  if (name == 0) return;

  mt_lock_stat *arr = g_locks.getBase();
  for (size_t i = 0; i < g_locks.size(); i++) {
    if (arr[i].m_ptr == ptr) {
      if (arr[i].m_name) {
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
static mt_lock_stat *lookup_lock(const void *ptr) {
  mt_lock_stat *arr = g_locks.getBase();
  for (size_t i = 0; i < g_locks.size(); i++) {
    if (arr[i].m_ptr == ptr) return arr + i;
  }

  return 0;
}
#endif

Uint32 mt_get_threads_for_blocks_no_proxy(const Uint32 blocks[],
                                          BlockThreadBitmask &mask) {
  Uint32 cnt = 0;
  for (Uint32 i = 0; blocks[i] != 0; i++) {
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
    for (Uint32 instance = first_instance; instance < instance_count;
         instance++) {
      Uint32 thr_no = thr_map[index][instance].thr_no;
      require(thr_no != thr_map_entry::NULL_THR_NO);

      if (mask.get(thr_no)) continue;

      mask.set(thr_no);
      cnt++;
    }
  }
  require(mask.count() == cnt);
  return cnt;
}

/**
 * Implements the rules for which threads are allowed to have
 * communication with each other.
 * Also see compute_jb_pages() which has similar logic.
 */
static bool may_communicate(unsigned from, unsigned to) {
  if (is_main_thread(from) || is_main_thread(to)) {
    // Main threads communicates with all other threads
    return true;
  } else if (is_tc_thread(from)) {
    // TC threads can communicate with SPJ-, LQH-, main- and itself
    return is_ldm_thread(to) || is_query_thread(to) ||
           is_tc_thread(to);  // Cover both SPJs and itself
  } else if (is_ldm_thread(from)) {
    // All LDM threads can communicates with TC-, main- and ldm.
    return is_tc_thread(to) || is_ldm_thread(to) || is_query_thread(to) ||
           is_recover_thread(to) || (to == from);
  } else if (is_query_thread(from)) {
    return is_tc_thread(to) || is_ldm_thread(to) || (to == from);
  } else if (is_recover_thread(from)) {
    return is_ldm_thread(to) || (to == from);
  } else {
    assert(is_recv_thread(from));
    // Receive treads communicate with all, except other receivers
    return !is_recv_thread(to);
  }
}

Uint32 mt_get_addressable_threads(const Uint32 my_thr_no,
                                  BlockThreadBitmask &mask) {
  const Uint32 thr_cnt = get_total_number_of_block_threads();
  Uint32 cnt = 0;
  for (Uint32 thr_no = 0; thr_no < thr_cnt; thr_no++) {
    if (may_communicate(my_thr_no, thr_no)) {
      mask.set(thr_no);
      cnt++;
    }
  }
  if (!mask.get(my_thr_no)) {
    mask.set(my_thr_no);
    cnt++;
  }
  require(mask.count() == cnt);
  return cnt;
}

void mt_wakeup(class SimulatedBlock *block) {
  Uint32 thr_no = block->getThreadId();
  struct thr_data *thrptr = &g_thr_repository->m_thread[thr_no];
  wakeup(&thrptr->m_waiter);
}

#ifdef VM_TRACE
void mt_assert_own_thread(SimulatedBlock *block) {
  Uint32 thr_no = block->getThreadId();
  struct thr_data *thrptr = &g_thr_repository->m_thread[thr_no];

  if (unlikely(my_thread_equal(thrptr->m_thr_id, my_thread_self()) == 0)) {
    g_eventLogger->info("mt_assert_own_thread() - assertion-failure");
    abort();
  }
}
#endif

Uint32 mt_get_blocklist(SimulatedBlock *block, Uint32 arr[], Uint32 len) {
  Uint32 thr_no = block->getThreadId();
  struct thr_data *thr_ptr = &g_thr_repository->m_thread[thr_no];

  require(len >= thr_ptr->m_instance_count);
  for (Uint32 i = 0; i < thr_ptr->m_instance_count; i++) {
    arr[i] = thr_ptr->m_instance_list[i];
  }

  return thr_ptr->m_instance_count;
}

void mt_get_spin_stat(class SimulatedBlock *block, ndb_spin_stat *dst) {
  Uint32 thr_no = block->getThreadId();
  struct thr_data *selfptr = &g_thr_repository->m_thread[thr_no];
  dst->m_sleep_longer_spin_time = selfptr->m_spin_stat.m_sleep_longer_spin_time;
  dst->m_sleep_shorter_spin_time =
      selfptr->m_spin_stat.m_sleep_shorter_spin_time;
  dst->m_num_waits = selfptr->m_spin_stat.m_num_waits;
  for (Uint32 i = 0; i < NUM_SPIN_INTERVALS; i++) {
    dst->m_micros_sleep_times[i] = selfptr->m_spin_stat.m_micros_sleep_times[i];
    dst->m_spin_interval[i] = selfptr->m_spin_stat.m_spin_interval[i];
  }
}

void mt_set_spin_stat(class SimulatedBlock *block, ndb_spin_stat *src) {
  Uint32 thr_no = block->getThreadId();
  struct thr_data *selfptr = &g_thr_repository->m_thread[thr_no];
  memset(&selfptr->m_spin_stat, 0, sizeof(selfptr->m_spin_stat));
  for (Uint32 i = 0; i < NUM_SPIN_INTERVALS; i++) {
    selfptr->m_spin_stat.m_spin_interval[i] = src->m_spin_interval[i];
  }
}

void mt_get_thr_stat(class SimulatedBlock *block, ndb_thr_stat *dst) {
  std::memset(dst, 0, sizeof(*dst));
  Uint32 thr_no = block->getThreadId();
  struct thr_data *selfptr = &g_thr_repository->m_thread[thr_no];

  THRConfigApplier &conf = globalEmulatorData.theConfiguration->m_thr_config;
  dst->thr_no = thr_no;
  dst->name = conf.getName(selfptr->m_instance_list, selfptr->m_instance_count);
  dst->os_tid = NdbThread_GetTid(selfptr->m_thread);
  dst->loop_cnt = selfptr->m_stat.m_loop_cnt;
  dst->exec_cnt = selfptr->m_stat.m_exec_cnt;
  dst->wait_cnt = selfptr->m_stat.m_wait_cnt;
  dst->local_sent_prioa = selfptr->m_stat.m_prioa_count;
  dst->local_sent_priob = selfptr->m_stat.m_priob_count;
}

TransporterReceiveHandle *mt_get_trp_receive_handle(unsigned instance) {
  assert(instance > 0 && instance <= MAX_NDBMT_RECEIVE_THREADS);
  if (instance > 0 && instance <= MAX_NDBMT_RECEIVE_THREADS) {
    return g_trp_receive_handle_ptr[instance - 1 /* proxy */];
  }
  return 0;
}

#if defined(USE_INIT_GLOBAL_VARIABLES)
void mt_clear_global_variables(thr_data *selfptr) {
  if (selfptr->m_global_variables_enabled) {
    for (Uint32 i = 0; i < selfptr->m_global_variables_ptr_instances; i++) {
      Ptr<void> *tmp = (Ptr<void> *)selfptr->m_global_variables_ptrs[i];
      tmp->i = RNIL;
      tmp->p = 0;
    }
    for (Uint32 i = 0; i < selfptr->m_global_variables_uint32_ptr_instances;
         i++) {
      void **tmp = (void **)selfptr->m_global_variables_uint32_ptrs[i];
      (*tmp) = 0;
    }
    for (Uint32 i = 0; i < selfptr->m_global_variables_uint32_instances; i++) {
      Uint32 *tmp = (Uint32 *)selfptr->m_global_variables_uint32[i];
      (*tmp) = Uint32(~0);
    }
  }
}

void mt_enable_global_variables(Uint32 self) {
  struct thr_repository *rep = g_thr_repository;
  struct thr_data *selfptr = &rep->m_thread[self];
  selfptr->m_global_variables_enabled = true;
}

void mt_disable_global_variables(Uint32 self) {
  struct thr_repository *rep = g_thr_repository;
  struct thr_data *selfptr = &rep->m_thread[self];
  selfptr->m_global_variables_enabled = false;
}

void mt_init_global_variables_ptr_instances(Uint32 self, void **tmp,
                                            size_t cnt) {
  struct thr_repository *rep = g_thr_repository;
  struct thr_data *selfptr = &rep->m_thread[self];
  for (size_t i = 0; i < cnt; i++) {
    Uint32 inx = selfptr->m_global_variables_ptr_instances;
    selfptr->m_global_variables_ptrs[inx] = tmp[i];
    selfptr->m_global_variables_ptr_instances = inx + 1;
  }
}

void mt_init_global_variables_uint32_ptr_instances(Uint32 self, void **tmp,
                                                   size_t cnt) {
  struct thr_repository *rep = g_thr_repository;
  struct thr_data *selfptr = &rep->m_thread[self];
  for (size_t i = 0; i < cnt; i++) {
    Uint32 inx = selfptr->m_global_variables_uint32_ptr_instances;
    selfptr->m_global_variables_uint32_ptrs[inx] = tmp[i];
    selfptr->m_global_variables_uint32_ptr_instances = inx + 1;
  }
}

void mt_init_global_variables_uint32_instances(Uint32 self, void **tmp,
                                               size_t cnt) {
  struct thr_repository *rep = g_thr_repository;
  struct thr_data *selfptr = &rep->m_thread[self];
  for (size_t i = 0; i < cnt; i++) {
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
