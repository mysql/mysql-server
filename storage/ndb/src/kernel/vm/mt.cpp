/* Copyright (c) 2008, 2011, 2012, Oracle and/or its affiliates. All rights reserved.

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
#include <portlib/ndb_prefetch.h>

#include "mt-asm.h"
#include "mt-lock.hpp"

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

/* Maximum number of signals to execute before sending to remote nodes. */
static const Uint32 MAX_SIGNALS_BEFORE_SEND = 200;

/*
 * Max. signals to execute from one job buffer before considering other
 * possible stuff to do.
 */
static const Uint32 MAX_SIGNALS_PER_JB = 100;

/**
 * Max signals written to other thread before calling flush_jbb_write_state
 */
static const Uint32 MAX_SIGNALS_BEFORE_FLUSH_RECEIVER = 2;
static const Uint32 MAX_SIGNALS_BEFORE_FLUSH_OTHER = 20;
static const Uint32 MAX_SIGNALS_BEFORE_WAKEUP = 128;

//#define NDB_MT_LOCK_TO_CPU

#define NUM_MAIN_THREADS 2 // except receiver
/*
  MAX_BLOCK_THREADS need not include the send threads since it's
  used to set size of arrays used by all threads that contains a
  job buffer and executes signals. The send threads only sends
  messages directed to other nodes and contains no blocks and
  executes thus no signals.
*/
#define MAX_BLOCK_THREADS (NUM_MAIN_THREADS +       \
                           MAX_NDBMT_LQH_THREADS +  \
                           MAX_NDBMT_TC_THREADS +   \
                           MAX_NDBMT_RECEIVE_THREADS)

/* If this is too small it crashes before first signal. */
#define MAX_INSTANCES_PER_THREAD (16 + 8 * MAX_NDBMT_LQH_THREADS)

static Uint32 num_threads = 0;
static Uint32 first_receiver_thread_no = 0;

#define NO_SEND_THREAD (MAX_BLOCK_THREADS + MAX_NDBMT_SEND_THREADS + 1)

/* max signal is 32 words, 7 for signal header and 25 datawords */
#define MIN_SIGNALS_PER_PAGE (thr_job_buffer::SIZE / 32)

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

struct thr_wait
{
  volatile unsigned m_futex_state;
  char padding[NDB_CL_PADSZ(sizeof(unsigned))];
  enum {
    FS_RUNNING = 0,
    FS_SLEEPING = 1
  };
  thr_wait() { xcng(&m_futex_state, FS_RUNNING);}
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
static inline
bool
yield(struct thr_wait* wait, const Uint32 nsec,
      bool (*check_callback)(struct thr_data *), struct thr_data *check_arg)
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
  bool waited = (*check_callback)(check_arg);
  if (waited)
  {
    struct timespec timeout;
    timeout.tv_sec = 0;
    timeout.tv_nsec = nsec;
    futex_wait(val, thr_wait::FS_SLEEPING, &timeout);
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
#else
#include <NdbMutex.h>
#include <NdbCondition.h>

struct thr_wait
{
  NdbMutex *m_mutex;
  NdbCondition *m_cond;
  bool m_need_wakeup;
  char padding[NDB_CL_PADSZ(sizeof(bool) + (2*sizeof(void*)))];
  thr_wait() : m_need_wakeup(false), m_mutex(0), m_cond(0) {}

  void init() {
    m_mutex = NdbMutex_Create();
    m_cond = NdbCondition_Create();
  }
};

static inline
bool
yield(struct thr_wait* wait, const Uint32 nsec,
      bool (*check_callback)(struct thr_data *), struct thr_data *check_arg)
{
  struct timespec end;
  NdbCondition_ComputeAbsTime(&end, nsec/1000000);
  NdbMutex_Lock(wait->m_mutex);

  Uint32 waits = 0;
  /* May have spurious wakeups: Always recheck condition predicate */
  while ((*check_callback)(check_arg))
  {
    wait->m_need_wakeup = true;
    waits++;
    if (NdbCondition_WaitTimeoutAbs(wait->m_cond,
                                    wait->m_mutex, &end) == ETIMEDOUT)
    {
      wait->m_need_wakeup = false;
      break;
    }
  }
  NdbMutex_Unlock(wait->m_mutex);
  return (waits > 0);
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

/**
 * thr_safe_pool
 */
template<typename T>
struct thr_safe_pool
{
  thr_safe_pool(const char * name) : m_free_list(0), m_cnt(0), m_lock(name) {}

  T* m_free_list;
  Uint32 m_cnt;
  thr_spin_lock<NDB_CL - (sizeof(void*) + sizeof(Uint32))> m_lock;

  T* seize(Ndbd_mem_manager *mm, Uint32 rg) {
    T* ret = 0;
    lock(&m_lock);
    if (m_free_list)
    {
      assert(m_cnt);
      m_cnt--;
      ret = m_free_list;
      m_free_list = ret->m_next;
      unlock(&m_lock);
    }
    else
    {
      unlock(&m_lock);
      Uint32 dummy;
      ret = reinterpret_cast<T*>
        (mm->alloc_page(rg, &dummy,
                        Ndbd_mem_manager::NDB_ZONE_ANY));
      // ToDo: How to deal with failed allocation?!?
      // I think in this case we need to start grabbing buffers kept for signal
      // trace.
    }
    return ret;
  }

  Uint32 seize_list(Ndbd_mem_manager *mm, Uint32 rg,
                    Uint32 requested, T** head, T** tail)
  {
    lock(&m_lock);
    if (m_cnt == 0)
    {
      unlock(&m_lock);
      Uint32 dummy;
      T* ret = reinterpret_cast<T*>
        (mm->alloc_page(rg, &dummy,
                        Ndbd_mem_manager::NDB_ZONE_ANY));

      if (ret == 0)
      {
        return 0;
      }
      else
      {
        ret->m_next = 0;
        * head = * tail = ret;
        return 1;
      }
    }
    else
    {
      if (m_cnt < requested )
        requested = m_cnt;

      T* first = m_free_list;
      T* last = first;
      for (Uint32 i = 1; i < requested; i++)
      {
        last = last->m_next;
      }
      m_cnt -= requested;
      m_free_list = last->m_next;
      unlock(&m_lock);
      last->m_next = 0;
      * head = first;
      * tail = last;
      return requested;
    }
  }

  void release(Ndbd_mem_manager *mm, Uint32 rg, T* t) {
    lock(&m_lock);
    t->m_next = m_free_list;
    m_free_list = t;
    m_cnt++;
    unlock(&m_lock);
  }

  void release_list(Ndbd_mem_manager *mm, Uint32 rg, 
                    T* head, T* tail, Uint32 cnt) {
    lock(&m_lock);
    tail->m_next = m_free_list;
    m_free_list = head;
    m_cnt += cnt;
    unlock(&m_lock);
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

  T *seize(Ndbd_mem_manager *mm, Uint32 rg) {
    T *tmp = m_freelist;
    if (tmp == 0)
    {
      T * tail;
      m_free = m_global_pool->seize_list(mm, rg, m_alloc_size, &tmp, &tail);
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

  void release(Ndbd_mem_manager *mm, Uint32 rg, T *t) {
    unsigned free = m_free;
    if (free < m_max_free)
    {
      m_free = free + 1;
      t->m_next = m_freelist;
      m_freelist = t;
    }
    else
      m_global_pool->release(mm, rg, t);

    validate();
  }

  /**
   * Release to local pool even if it get's "too" full
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
  void release_global(Ndbd_mem_manager *mm, Uint32 rg) {
    validate();
    unsigned cnt = 0;
    unsigned free = m_free;
    Uint32 maxfree = m_max_free;
    assert(maxfree > 0);

    T* head = m_freelist;
    T* tail = m_freelist;
    if (free > maxfree)
    {
      cnt++;
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
      m_global_pool->release_list(mm, rg, head, tail, cnt);
    }
    validate();
  }

  void release_all(Ndbd_mem_manager *mm, Uint32 rg) {
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
      m_global_pool->release_list(mm, rg, head, tail, cnt);
      m_free = 0;
      m_freelist = 0;
    }
    validate();
  }

  /**
   * release everything if more than m_max_free
   *   else do nothing
   */
  void release_chunk(Ndbd_mem_manager *mm, Uint32 rg) {
    if (m_free > m_max_free)
      release_all(mm, rg);
  }

  /**
   * prealloc up to <em>cnt</em> pages into this pool
   */
  bool fill(Ndbd_mem_manager *mm, Uint32 rg, Uint32 cnt)
  {
    if (m_free >= cnt)
    {
      return true;
    }

    T *head, *tail;
    Uint32 allocated = m_global_pool->seize_list(mm, rg, m_alloc_size,
                                                 &head, &tail);
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
  unsigned m_max_free;
  unsigned m_alloc_size;
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

  Uint32 used() const;
};

struct thr_job_queue
{
  static const unsigned SIZE = 32;

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

  /* Number of signals inserted since last flush to thr_job_queue. */
  Uint32 m_pending_signals;

  /* Number of signals inserted since last wakeup */
  Uint32 m_pending_signals_wakeup;

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
  static const unsigned SQ_SIZE = 512;
  static const unsigned LQ_SIZE = 512;
  static const unsigned PAGES = 32 * (SQ_SIZE + LQ_SIZE) / 8192;
  
  Uint32 * m_delayed_signals[PAGES];
  Uint32 m_next_free;
  Uint32 m_next_timer;
  Uint32 m_current_time;
  Uint32 m_cnt[2];
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

struct thr_data
{
  thr_data() : m_jba_write_lock("jbalock"),
               m_send_buffer_pool(0,
                                  THR_SEND_BUFFER_MAX_FREE,
                                  THR_SEND_BUFFER_ALLOC_SIZE) {}

  /**
   * We start with the data structures that are shared globally to
   * ensure that they get the proper padding
   */
  thr_wait m_waiter; /* Cacheline aligned*/

  /*
   * Prio A signal incoming queue. This area is used from many threads
   * protected by the spin lock. Thus it is also important to protect
   * surrounding thread-local variables from CPU cache line sharing
   * with this part.
   */
  struct thr_spin_lock<64> m_jba_write_lock;

  struct thr_job_queue m_jba; /* aligned */
  struct thr_job_queue_head m_jba_head;
  char unused_protection1[NDB_CL_PADSZ(sizeof(struct thr_job_queue_head))];

  /*
   * These are the thread input queues, where other threads deliver signals
   * into.
   * Protect the m_in_queue_head by empty cache line to ensure that we don't
   * get false CPU cacheline sharing. These cache lines are going to be
   * updated by many different CPU's all the time whereas other neighbour
   * variables are thread-local variables.
   */
  struct thr_job_queue_head m_in_queue_head[MAX_BLOCK_THREADS];
  char unused_protection2[
    NDB_CL_PADSZ(MAX_BLOCK_THREADS*sizeof(struct thr_job_queue_head))];
  struct thr_job_queue m_in_queue[MAX_BLOCK_THREADS];

  /**
   * The remainder of the variables in thr_data are thread-local,
   * meaning that they are always updated by the thread that owns those
   * data structures and thus those variables aren't shared with other
   * CPUs.
   */

  unsigned m_thr_no;

  /**
   * max signals to execute per JBB buffer
   */
  unsigned m_max_signals_per_jb;

  /**
   * max signals to execute before recomputing m_max_signals_per_jb
   */
  unsigned m_max_exec_signals;

  Uint64 m_time;
  struct thr_tq m_tq;

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

  /* Array of node ids with pending remote send data. */
  Uint8 m_pending_send_nodes[MAX_NTRANSPORTERS];
  /* Number of node ids in m_pending_send_nodes. */
  Uint32 m_pending_send_count;

  /**
   * Bitmap of pending node ids with send data.
   * Used to quickly check if a node id is already in m_pending_send_nodes.
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
  pthread_t m_thr_id;
  NdbThread* m_thread;
};

struct thr_data_aligned
{
  struct thr_data m_thr_data;
  /* Ensure that the thr_data is aligned on a cacheline boundary */
  char unused_protection[NDB_CL_PADSZ(sizeof(struct thr_data))];
};

struct mt_send_handle  : public TransporterSendBufferHandle
{
  struct thr_data * m_selfptr;
  mt_send_handle(thr_data* ptr) : m_selfptr(ptr) {}
  virtual ~mt_send_handle() {}

  virtual Uint32 *getWritePtr(NodeId node, Uint32 len, Uint32 prio, Uint32 max);
  virtual Uint32 updateWritePtr(NodeId node, Uint32 lenBytes, Uint32 prio);
  virtual bool forceSend(NodeId node);
};

struct trp_callback : public TransporterCallback
{
  trp_callback() {}

  /* Callback interface. */
  void reportSendLen(NodeId nodeId, Uint32 count, Uint64 bytes);
  void lock_transporter(NodeId node);
  void unlock_transporter(NodeId node);
  Uint32 get_bytes_to_send_iovec(NodeId node, struct iovec *dst, Uint32 max);
  Uint32 bytes_sent(NodeId node, Uint32 bytes);
  bool has_data_to_send(NodeId node);
  void reset_send_buffer(NodeId node, bool should_be_empty);
};

extern trp_callback g_trp_callback;             // Forward declaration
extern struct thr_repository g_thr_repository;

#include <NdbMutex.h>
#include <NdbCondition.h>

struct thr_repository
{
  thr_repository()
    : m_section_lock("sectionlock"),
      m_mem_manager_lock("memmanagerlock"),
      m_jb_pool("jobbufferpool"),
      m_sb_pool("sendbufferpool")
    {}

  /**
   * m_receive_lock, m_section_lock, m_mem_manager_lock, m_jb_pool
   * and m_sb_pool are allvariables globally shared among the threads
   * and also heavily updated.
   */
  struct thr_spin_lock<64> m_receive_lock[MAX_NDBMT_RECEIVE_THREADS];
  struct thr_spin_lock<64> m_section_lock;
  struct thr_spin_lock<64> m_mem_manager_lock;
  /* thr_safe_pool is aligned to be also 64 bytes in size */
  struct thr_safe_pool<thr_job_buffer> m_jb_pool;
  struct thr_safe_pool<thr_send_page> m_sb_pool;
  /* m_mm and m_thread_count are globally shared and read only variables */
  Ndbd_mem_manager * m_mm;
  unsigned m_thread_count;
  /**
   * Protect m_mm and m_thread_count from CPU cache misses, first
   * part of m_thread (struct thr_data) is globally shared variables.
   * So sharing cache line with these for these read only variables
   * isn't a good idea
   */
  char protection_unused[NDB_CL_PADSZ(sizeof(void*) + sizeof(unsigned))];

  struct thr_data_aligned m_thread[MAX_BLOCK_THREADS];

  /* The buffers that are to be sent */
  struct send_buffer
  {
    /**
     * lock
     */
    struct thr_spin_lock<8> m_send_lock;

    /**
     * pending data
     */
    struct thr_send_buffer m_buffer;

    /**
     * Flag used to coordinate sending to same remote node from different
     * threads.
     *
     * If two threads need to send to the same node at the same time, the
     * second thread, rather than wait for the first to finish, will just
     * set this flag, and the first thread will do an extra send when done
     * with the first.
     */
    Uint32 m_force_send;

    /**
     * Which thread is currently holding the m_send_lock
     */
    Uint32 m_send_thread;

    /**
     * bytes pending for this node
     */
    Uint32 m_bytes;

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
#define is_send_thread(thr_no) (thr_no >= num_threads)

struct thr_send_thread_instance
{
  thr_send_thread_instance() :
               m_instance_no(0),
               m_watchdog_counter(0),
               m_awake(FALSE),
               m_thread(NULL),
               m_waiter_struct(),
               m_send_buffer_pool(0,
                                  THR_SEND_BUFFER_MAX_FREE,
                                  THR_SEND_BUFFER_ALLOC_SIZE)
  {}
  Uint32 m_instance_no;
  Uint32 m_watchdog_counter;
  Uint32 m_awake;
  NdbThread *m_thread;
  thr_wait m_waiter_struct;
  class thread_local_pool<thr_send_page> m_send_buffer_pool;
};

struct thr_send_nodes
{
  /* 0 means NULL */
  Uint16 m_next;
  Uint16 m_data_available;
};

class thr_send_threads
{
public:
  /* Create send thread environment */
  thr_send_threads();

  /* Destroy send thread environment and ensure threads are stopped */
  ~thr_send_threads();

  /* A block thread has flushed data for a node and wants it sent */
  void alert_send_thread(NodeId node);

  /* Method used to run the send thread */
  void run_send_thread(Uint32 instance_no);

  /* Method to start the send threads */
  void start_send_threads();

  /* Check if at least one node to send to */
  bool data_available()
  {
    rmb();
    return m_first_node;
  }

  bool data_available(NodeId node)
  {
    struct thr_send_nodes *node_state;

    node_state = &m_node_state[node];
    return node_state->m_data_available;
  }

  /* Get send buffer pool for send thread */
  thread_local_pool<thr_send_page>* get_send_buffer_pool(Uint32 thr_no)
  {
    return &m_send_threads[thr_no - num_threads].m_send_buffer_pool;
  }

private:
  /* Insert a node in list of nodes that has data available to send */
  void insert_node(NodeId node);

  /* Get a node from the list in order to send to it */
  NodeId get_node();

  /* Get a send thread which isn't awake currently */
  struct thr_send_thread_instance* get_not_awake_send_thread();
  /*
   * Try to lock send for this node, if not successful, try another
   * node ready for sending, unlock send thread mutex as part of
   * the call.
   */
  NodeId check_and_lock_send_node(NodeId node);

  /* Perform the actual send to the node */
  int perform_send(NodeId node, Uint32 instance_no);

  /* Have threads been started */
  Uint32 m_started_threads;

  /* First node that has data to be sent */
  Uint32 m_first_node;

  /* Last node in list of nodes with data available for sending */
  Uint32 m_last_node;

  /* Is data available and next reference for each node in cluster */
  struct thr_send_nodes m_node_state[MAX_NODES];

  /**
   * Very few compiler (gcc) allow zero length arrays
   */
#if MAX_NDBMT_SEND_THREADS == 0
#define _MAX_SEND_THREADS 1
#else
#define _MAX_SEND_THREADS MAX_NDBMT_SEND_THREADS
#endif

  /* Data and state for the send threads */
  struct thr_send_thread_instance m_send_threads[_MAX_SEND_THREADS];

  /**
   * Mutex protecting the linked list of nodes awaiting sending
   * and also the not_awake variable of the send thread.
   */
  NdbMutex *send_thread_mutex;
};


/*
 * The single instance of the thr_send_threads class, if this variable
 * is non-NULL, then we're using send threads, otherwise if NULL, there
 * are no send threads.
 */
static thr_send_threads *g_send_threads = NULL;

extern "C"
void *
mt_send_thread_main(void *thr_arg)
{
  struct thr_send_thread_instance *this_send_thread =
    (thr_send_thread_instance*)thr_arg;

  Uint32 instance_no = this_send_thread->m_instance_no;
  ndbout_c("Send thread : %u is started", instance_no);
  g_send_threads->run_send_thread(instance_no);
  return NULL;
}

thr_send_threads::thr_send_threads()
{
  struct thr_repository *rep = &g_thr_repository;

  m_started_threads = FALSE;
  m_first_node = 0;
  m_last_node = 0;
  for (Uint32 i = 0; i < NDB_ARRAY_SIZE(m_node_state); i++)
  {
    m_node_state[i].m_next = 0;
    m_node_state[i].m_data_available = FALSE;
  }
  for (Uint32 i = 0; i < NDB_ARRAY_SIZE(m_send_threads); i++)
  {
    m_send_threads[i].m_waiter_struct.init();
    m_send_threads[i].m_instance_no = i;
    m_send_threads[i].m_send_buffer_pool.set_pool(&rep->m_sb_pool);
  }
  send_thread_mutex = NdbMutex_Create();
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
    NdbThread_Destroy(&(m_send_threads[i].m_thread));
  }
}

void
thr_send_threads::start_send_threads()
{
  for (Uint32 i = 0; i < globalData.ndbMtSendThreads; i++)
  {
    ndbout_c("Start send thread: %u", i);
    m_send_threads[i].m_thread =
      NdbThread_Create(mt_send_thread_main,
                       (void **)&m_send_threads[i],
                       1024*1024,
                       "send thread", //ToDo add number
                       NDB_THREAD_PRIO_MEAN);
  }
  m_started_threads = TRUE;
}

/* Called under mutex protection of send_thread_mutex */
void
thr_send_threads::insert_node(NodeId node)
{
  Uint8 last_node = m_last_node;
  Uint8 first_node = m_first_node;
  struct thr_send_nodes *last_node_state = &m_node_state[last_node];
  struct thr_send_nodes *node_state = &m_node_state[node];

  node_state->m_next = 0;
  node_state->m_data_available = TRUE;
  m_last_node = node;
  if (first_node == 0)
  {
    m_first_node = node;
    wmb(); /* Ensure send threads see the change to m_first_node != 0 */
  }
  else
    last_node_state->m_next = node;
}

/* Called under mutex protection of send_thread_mutex */
NodeId
thr_send_threads::get_node()
{
  Uint32 first_node = m_first_node;
  struct thr_send_nodes *node_state;
  Uint32 next;

  if (first_node)
  {
    node_state = &m_node_state[first_node];
    next = node_state->m_next;

    node_state->m_next = 0;
    node_state->m_data_available = FALSE;

    m_first_node = next;
    if (next == 0)
    {
      m_last_node = 0;
      wmb(); /* Ensure send threads see the change to m_first_node == 0 */
    }
    return (NodeId)first_node;
  }
  return 0;
}

struct thr_send_thread_instance*
thr_send_threads::get_not_awake_send_thread()
{
  struct thr_send_thread_instance *used_send_thread;

  for (Uint32 i = 0; i < globalData.ndbMtSendThreads; i++)
  {
    if (!m_send_threads[i].m_awake)
    {
      used_send_thread= &m_send_threads[i];
      return used_send_thread;
    }
  }
  return NULL;
}

void
thr_send_threads::alert_send_thread(NodeId node)
{
  struct thr_send_thread_instance *used_send_thread = NULL;

  NdbMutex_Lock(send_thread_mutex);
  if (m_node_state[node].m_data_available)
  {
    /*
     * The node is already flagged that it has data needing to be sent.
     * This means that someone else also ensured that a send thread
     * was woken up if necessary, so we can rely on this that the send
     * threads will now take care of it.
     *
     * We are safe that the buffers we have flushed will be read by a send
     * thread since this variable is always reset before reading the send
     * buffers of the node.
     *
     * The thread that set this flag should also have ensured that a send
     * thread is awoken if needed, there is no need to wake even more
     * threads up in this case since we piggyback on someone else's request.
    */
    assert(m_first_node);
    NdbMutex_Unlock(send_thread_mutex);
    return;
  }
  else
    insert_node(node);
  /*
   * Search for a send thread which is asleep, if there is one, wake it
   *
   * If everyone is already awake we don't need to wake anyone up since
   * the threads will check if there is nodes available to send to before
   * they go to sleep.
   *
   * The reason to look for anyone asleep is to ensure proper use of CPU
   * resources and ensure that we use all the send thread CPUs available
   * to our disposal when necessary.
   */
  used_send_thread = get_not_awake_send_thread();

  NdbMutex_Unlock(send_thread_mutex);

  if (used_send_thread)
  {
    /*
     * Wake the assigned sleeping send thread, potentially a spurious wakeup,
     * but this is not a problem, important is to ensure that at least one
     * send thread is awoken to handle our request. If someone is already
     * awake and takes of our request before we get to wake someone up it's
     * not a problem.
     */
    wakeup(&(used_send_thread->m_waiter_struct));
  }
}

extern "C"
bool
check_available_send_data(struct thr_data *not_used)
{
  (void)not_used;
  return !g_send_threads->data_available();
}

NodeId
thr_send_threads::check_and_lock_send_node(NodeId node)
{
  thr_repository::send_buffer * sb = g_thr_repository.m_send_buffers+node;
  NodeId in_node = node;

  /* We own send_thread_mutex when entering */
  while (trylock(&sb->m_send_lock) != 0)
  {
    /*
     * We failed to acquire lock, another send thread is busy writing to
     * this node most likely, we'll post it last if there is other nodes
     * to send data to.
     */
    insert_node(node);
    node = get_node();
    sb = g_thr_repository.m_send_buffers+node;
    if (node == in_node)
    {
      /*
       * No other node available to send to, we'll wait for this one to
       * become free by using normal lock.
       */
      NdbMutex_Unlock(send_thread_mutex);
      lock(&sb->m_send_lock);
      return node;
    }
  }
  /* Only gets here when own the send lock of node */
  NdbMutex_Unlock(send_thread_mutex);
  /* We don't want to have send thread mutex while sending */
  return node;
}

int
thr_send_threads::perform_send(NodeId node, Uint32 instance_no)
{
  int res;
  thr_repository::send_buffer * sb = g_thr_repository.m_send_buffers+node;
  sb->m_send_thread = num_threads + instance_no;
  res = globalTransporterRegistry.performSend(node);
  sb->m_send_thread = NO_SEND_THREAD;
  unlock(&sb->m_send_lock);
  return res;
}

void
thr_send_threads::run_send_thread(Uint32 instance_no)
{
  struct thr_send_thread_instance *this_send_thread =
    &m_send_threads[instance_no];
  const Uint32 thr_no = num_threads + instance_no;

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
    THRConfigApplier & conf = globalEmulatorData.theConfiguration->m_thr_config;
    tmp.appfmt("thr: %u ", thr_no);
    int tid = NdbThread_GetTid(this_send_thread->m_thread);
    if (tid != -1)
    {
      tmp.appfmt("tid: %u ", tid);
    }
    conf.appendInfoSendThread(tmp, instance_no);
    int res = conf.do_bind_send(this_send_thread->m_thread, instance_no);
    if (res < 0)
    {
      tmp.appfmt("err: %d ", -res);
    }
    else if (res > 0)
    {
      tmp.appfmt("OK ");
    }
    printf("%s\n", tmp.c_str());
    fflush(stdout);
  }

  /**
   * register watchdog
   */
  globalEmulatorData.theWatchDog->
    registerWatchedThread(&this_send_thread->m_watchdog_counter, thr_no);

  NdbMutex_Lock(send_thread_mutex);
  this_send_thread->m_awake = FALSE;
  NdbMutex_Unlock(send_thread_mutex);

  while (globalData.theRestartFlag != perform_stop)
  {
    this_send_thread->m_watchdog_counter = 1;

    /* Yield for a maximum of 1ms */
    const Uint32 wait = 1000000;
    yield(&this_send_thread->m_waiter_struct, wait,
          check_available_send_data, NULL);

    NdbMutex_Lock(send_thread_mutex);
    this_send_thread->m_awake = TRUE;

    NodeId node;
    while ((node = get_node()) != 0 &&
           globalData.theRestartFlag != perform_stop)
    {
      this_send_thread->m_watchdog_counter = 2;

      /* We enter this method with send thread mutex and come
       * back with send thread mutex released and instead owning
       * the spin lock to send to the node returned
       */
      node = check_and_lock_send_node(node);

      int res = perform_send(node, instance_no);
      /* We return with no spin locks or mutexes held */

      /* Release chunk-wise to decrease pressure on spin lock */
      this_send_thread->m_watchdog_counter = 3;
      this_send_thread->m_send_buffer_pool.
        release_chunk(g_thr_repository.m_mm, RG_TRANSPORTER_BUFFERS);

      NdbMutex_Lock(send_thread_mutex);
      if (res && !data_available(node))
        insert_node(node);
    }

    /* No data to send, prepare to sleep */
    this_send_thread->m_awake = FALSE;
    NdbMutex_Unlock(send_thread_mutex);
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

static
void
job_buffer_full(struct thr_data* selfptr)
{
  ndbout_c("job buffer full");
  abort();
}

static
void
out_of_job_buffer(struct thr_data* selfptr)
{
  ndbout_c("out of job buffer");
  abort();
}

static
thr_job_buffer*
seize_buffer(struct thr_repository* rep, int thr_no, bool prioa)
{
  thr_job_buffer* jb;
  struct thr_data* selfptr = &rep->m_thread[thr_no].m_thr_data;
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
      jb = rep->m_jb_pool.seize(rep->m_mm, RG_JOBBUFFER);
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
  struct thr_data* selfptr = &rep->m_thread[thr_no].m_thr_data;
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
      rep->m_jb_pool.release(rep->m_mm, RG_JOBBUFFER,
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
      Uint32 pos = 32 * (idx & 0xFF);

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

static
void
scan_time_queues_impl(struct thr_data* selfptr, NDB_TICKS now)
{
  struct thr_tq * tq = &selfptr->m_tq;
  NDB_TICKS last = selfptr->m_time;

  Uint32 curr = tq->m_current_time;
  Uint32 cnt0 = tq->m_cnt[0];
  Uint32 cnt1 = tq->m_cnt[1];

  assert(now > last);
  Uint64 diff = now - last;
  Uint32 step = (Uint32)((diff > 20) ? 20 : diff);
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
  selfptr->m_time = last + step;
}

static inline
void
scan_time_queues(struct thr_data* selfptr, NDB_TICKS now)
{
  if (selfptr->m_time != now)
    scan_time_queues_impl(selfptr, now);
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
  Uint32 buf = idx >> 8;
  Uint32 pos = idx & 0xFF;

  if (idx != RNIL)
  {
    Uint32* page = * (tq->m_delayed_signals + buf);
    Uint32* ptr = page + (32 * pos);
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

      ndbout_c("saving %p at %p (%d)", page, tq->m_delayed_signals+i, i);

      /**
       * Init page
       */
      for (Uint32 j = 0; j<255; j ++)
      {
	page[j * 32] = (i << 8) + (j + 1);
      }
      page[255*32] = RNIL;
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
  struct thr_repository* rep = &g_thr_repository;
  struct thr_data* selfptr = &rep->m_thread[thr_no].m_thr_data;
  assert(pthread_equal(selfptr->m_thr_id, pthread_self()));
  unsigned siglen = (sizeof(*s) >> 2) + s->theLength + s->m_noOfSections;

  Uint32 max;
  Uint32 * cntptr;
  Uint32 * queueptr;

  Uint32 alarm = selfptr->m_tq.m_current_time + delay;
  Uint32 nexttimer = selfptr->m_tq.m_next_timer;
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

  if (cnt == 0)
  {
    queueptr[0] = newentry;
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
    abort();
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
  w->m_pending_signals_wakeup = 0;
  w->m_pending_signals = 0;
}

static inline
void
flush_write_state_other(thr_data *dstptr, thr_job_queue_head *q_head,
                        thr_jb_write_state *w)
{
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

  w->m_pending_signals_wakeup += w->m_pending_signals;
  w->m_pending_signals = 0;

  if (w->m_pending_signals_wakeup >= MAX_SIGNALS_BEFORE_WAKEUP)
  {
    w->m_pending_signals_wakeup = 0;
    wakeup(&(dstptr->m_waiter));
  }
}

static inline
void
flush_write_state(const thr_data *selfptr, thr_data *dstptr,
                  thr_job_queue_head *q_head, thr_jb_write_state *w)
{
  if (dstptr == selfptr)
  {
    flush_write_state_self(q_head, w);
  }
  else
  {
    flush_write_state_other(dstptr, q_head, w);
  }
}


static
void
flush_jbb_write_state(thr_data *selfptr)
{
  Uint32 thr_count = g_thr_repository.m_thread_count;
  Uint32 self = selfptr->m_thr_no;

  thr_jb_write_state *w = selfptr->m_write_states;
  thr_data_aligned *thr_align_ptr = g_thr_repository.m_thread;
  for (Uint32 thr_no = 0; thr_no < thr_count; thr_no++, thr_align_ptr++, w++)
  {
    if (w->m_pending_signals || w->m_pending_signals_wakeup)
    {
      struct thr_data *thrptr = &thr_align_ptr->m_thr_data;
      w->m_pending_signals_wakeup = MAX_SIGNALS_BEFORE_WAKEUP;
      thr_job_queue_head *q_head = thrptr->m_in_queue_head + self;
      flush_write_state(selfptr, thrptr, q_head, w);
    }
  }
}

/**
 * Transporter will receive 1024 signals (MAX_RECEIVED_SIGNALS)
 * before running check_job_buffers
 *
 * This function returns 0 if there is space to receive this amount of
 *   signals
 * else 1
 */
static int
check_job_buffers(struct thr_repository* rep, Uint32 recv_thread_id)
{
  const Uint32 minfree = (1024 + MIN_SIGNALS_PER_PAGE - 1)/MIN_SIGNALS_PER_PAGE;
  unsigned thr_no = first_receiver_thread_no + recv_thread_id;
  const thr_data_aligned *thr_align_ptr = rep->m_thread;
  for (unsigned i = 0; i<num_threads; i++, thr_align_ptr++)
  {
    /**
     * NOTE: m_read_index is read wo/ lock (and updated by different thread)
     *       but since the different thread can only consume
     *       signals this means that the value returned from this
     *       function is always conservative (i.e it can be better than
     *       returned value, if read-index has moved but we didnt see it)
     */
    const struct thr_data *thrptr = &thr_align_ptr->m_thr_data;
    const thr_job_queue_head *q_head = thrptr->m_in_queue_head + thr_no;
    unsigned ri = q_head->m_read_index;
    unsigned wi = q_head->m_write_index;
    unsigned busy = (wi >= ri) ? wi - ri : (thr_job_queue::SIZE - ri) + wi;
    if (1 + minfree + busy >= thr_job_queue::SIZE)
    {
      return 1;
    }
  }
  return 0;
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
compute_max_signals_to_execute(Uint32 thr_no)
{
  Uint32 minfree = thr_job_queue::SIZE;
  const struct thr_repository* rep = &g_thr_repository;
  const struct thr_data_aligned *thr_align_ptr = rep->m_thread;

  for (unsigned i = 0; i<num_threads; i++, thr_align_ptr++)
  {
    /**
     * NOTE: m_read_index is read wo/ lock (and updated by different thread)
     *       but since the different thread can only consume
     *       signals this means that the value returned from this
     *       function is always conservative (i.e it can be better than
     *       returned value, if read-index has moved but we didnt see it)
     */
    const struct thr_data *thrptr = &thr_align_ptr->m_thr_data;
    const thr_job_queue_head *q_head = thrptr->m_in_queue_head + thr_no;
    unsigned ri = q_head->m_read_index;
    unsigned wi = q_head->m_write_index;
    unsigned free = (wi < ri) ? ri - wi : (thr_job_queue::SIZE + ri) - wi;

    assert(free <= thr_job_queue::SIZE);

    if (free < minfree)
      minfree = free;
  }

#define SAFETY 2

  if (minfree >= (1 + SAFETY))
  {
    return (3 + (minfree - (1 + SAFETY)) * MIN_SIGNALS_PER_PAGE) / 4;
  }
  else
  {
    return 0;
  }
}

//#define NDBMT_RAND_YIELD
#ifdef NDBMT_RAND_YIELD
static Uint32 g_rand_yield = 0;
static
void
rand_yield(Uint32 limit, void* ptr0, void * ptr1)
{
  return;
  UintPtr tmp = UintPtr(ptr0) + UintPtr(ptr1);
  Uint8* tmpptr = (Uint8*)&tmp;
  Uint32 sum = g_rand_yield;
  for (Uint32 i = 0; i<sizeof(tmp); i++)
    sum = 33 * sum + tmpptr[i];

  if ((sum % 100) < limit)
  {
    g_rand_yield++;
    sched_yield();
  }
}
#else
static inline void rand_yield(Uint32 limit, void* ptr0, void * ptr1) {}
#endif



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
     */
    return;
  }

  signal.header.theLength = 3;
  signal.header.theSendersSignalId = 0;
  signal.header.theSendersBlockRef = numberToRef(0, globalData.ownId);
  signal.theData[0] = NDB_LE_SendBytesStatistic;
  signal.theData[1] = nodeId;
  signal.theData[2] = (Uint32)(bytes/count);
  signal.header.theVerId_signalNumber = GSN_EVENT_REP;
  signal.header.theReceiversBlockNumber = CMVMI;
  sendlocal(g_thr_repository.m_send_buffers[nodeId].m_send_thread,
            &signalT.header, signalT.theData, NULL);
}

/**
 * To lock during connect/disconnect, we take both the send lock for the node
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
trp_callback::lock_transporter(NodeId node)
{
  Uint32 recv_thread_idx = mt_get_recv_thread_idx(node);
  struct thr_repository* rep = &g_thr_repository;
  /**
   * Note: take the send lock _first_, so that we will not hold the receive
   * lock while blocking on the send lock.
   *
   * The reverse case, blocking send lock for one transporter while waiting
   * for receive lock, is not a problem, as the transporter being blocked is
   * in any case disconnecting/connecting at this point in time, and sends are
   * non-waiting (so we will not block sending on other transporters).
   */
  lock(&rep->m_send_buffers[node].m_send_lock);
  lock(&rep->m_receive_lock[recv_thread_idx]);
}

void
trp_callback::unlock_transporter(NodeId node)
{
  Uint32 recv_thread_idx = mt_get_recv_thread_idx(node);
  struct thr_repository* rep = &g_thr_repository;
  unlock(&rep->m_receive_lock[recv_thread_idx]);
  unlock(&rep->m_send_buffers[node].m_send_lock);
}

int
mt_checkDoJob(Uint32 recv_thread_idx)
{
  struct thr_repository* rep = &g_thr_repository;
  if (unlikely(check_job_buffers(rep, recv_thread_idx)))
  {
    do 
    {
      /**
       * theoretically (or when we do single threaded by using ndbmtd with
       * all in same thread) we should execute signals here...to 
       * prevent dead-lock, but...with current ndbmtd only TRPMAN runs in
       * this thread, and other thread is waiting for TRPMAN 
       * except for QMGR open/close connection, but that is not
       * (i think) sufficient to create a deadlock
       */

      /** FIXME:
       *  On a CMT chip where #CPU >= #NDB-threads sched_yield() is
       *  effectively a NOOP as there will normally be an idle CPU available
       *  to immediately resume thread execution.
       *  On a Niagara chip this may severely impact performance as the CPUs
       *  are virtualized by timemultiplexing the physical core.
       *  The thread should really be 'parked' on
       *  a condition to free its execution resources.
       */
//    usleep(a-few-usec);  /* A micro-sleep would likely have been better... */
#if defined HAVE_SCHED_YIELD
      sched_yield();
#elif defined _WIN32
      SwitchToThread();
#else
      NdbSleep_MilliSleep(0);
#endif

    } while (check_job_buffers(rep, recv_thread_idx));
  }

  return 0;
}

/**
 * Link all send-buffer-pages into *one*
 *   single linked list of buffers
 *
 * TODO: This is not completely fair,
 *       it would be better to get one entry from each thr_send_queue
 *       per thread instead (until empty)
 */
static
Uint32
link_thread_send_buffers(thr_repository::send_buffer * sb, Uint32 node)
{
  Uint32 ri[MAX_BLOCK_THREADS];
  Uint32 wi[MAX_BLOCK_THREADS];
  thr_send_queue * src = g_thr_repository.m_thread_send_buffers[node];
  for (unsigned thr = 0; thr < num_threads; thr++)
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
  for (unsigned thr = 0; thr < num_threads; thr++, src++)
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

  if (bytes)
  {
    if (sb->m_bytes)
    {
      assert(sb->m_buffer.m_first_page != 0);
      assert(sb->m_buffer.m_last_page != 0);
      sb->m_buffer.m_last_page->m_next = tmp.m_first_page->m_next;
      sb->m_buffer.m_last_page = tmp.m_last_page;
    }
    else
    {
      assert(sb->m_buffer.m_first_page == 0);
      assert(sb->m_buffer.m_last_page == 0);
      sb->m_buffer.m_first_page = tmp.m_first_page->m_next;
      sb->m_buffer.m_last_page = tmp.m_last_page;
    }
    sb->m_bytes += bytes;
  }

  return sb->m_bytes;
}

Uint32
trp_callback::get_bytes_to_send_iovec(NodeId node,
                                      struct iovec *dst, Uint32 max)
{
  thr_repository::send_buffer * sb = g_thr_repository.m_send_buffers + node;

  Uint32 bytes = link_thread_send_buffers(sb, node);
  if (max == 0 || bytes == 0)
    return 0;

  /**
   * Process linked-list and put into iovecs
   * TODO: Here we would also pack stuff to get better utilization
   */
  Uint32 tot = 0;
  Uint32 pos = 0;
  thr_send_page * p = sb->m_buffer.m_first_page;
  do {
    dst[pos].iov_len = p->m_bytes;
    dst[pos].iov_base = p->m_data + p->m_start;
    assert(p->m_start + p->m_bytes <= p->max_bytes());
    tot += p->m_bytes;
    pos++;
    max--;
    p = p->m_next;
  } while (max && p != 0);

  return pos;
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
 * pack thr_send_pages for a particular send-buffer <em>db</em>
 *   release pages (local) to <em>pool<em>
 *
 * can only be called with sb->m_lock held
 */
static
void
pack_sb_pages(thread_local_pool<thr_send_page>* pool,
              thr_repository::send_buffer* sb)
{
  assert(sb->m_buffer.m_first_page != 0);
  assert(sb->m_buffer.m_last_page != 0);
  assert(sb->m_buffer.m_last_page->m_next == 0);

  thr_send_page* curr = sb->m_buffer.m_first_page;
  Uint32 curr_free = curr->max_bytes() - (curr->m_bytes + curr->m_start);
  while (curr->m_next != 0)
  {
    thr_send_page* next = curr->m_next;
    assert(next->m_start == 0); // only first page should have half sent bytes
    if (next->m_bytes <= curr_free)
    {
      thr_send_page * save = next;
      memcpy(curr->m_data + (curr->m_bytes + curr->m_start),
             next->m_data,
             next->m_bytes);

      curr_free -= next->m_bytes;

      curr->m_bytes += next->m_bytes;
      curr->m_next = next->m_next;

      pool->release_local(save);
    }
    else
    {
      curr = next;
      curr_free = curr->max_bytes() - (curr->m_bytes + curr->m_start);
    }
  }

  sb->m_buffer.m_last_page = curr;
}

static
Uint32
bytes_sent(thread_local_pool<thr_send_page>* pool,
           thr_repository::send_buffer* sb, Uint32 bytes)
{
  assert(bytes);

  Uint32 remain = bytes;
  thr_send_page * prev = 0;
  thr_send_page * curr = sb->m_buffer.m_first_page;

  assert(sb->m_bytes >= bytes);
  while (remain && remain >= curr->m_bytes)
  {
    remain -= curr->m_bytes;
    prev = curr;
    curr = curr->m_next;
  }

  Uint32 total_bytes = sb->m_bytes;
  if (total_bytes == bytes)
  {
    /**
     * Every thing was released
     */
    release_list(pool, sb->m_buffer.m_first_page, sb->m_buffer.m_last_page);
    sb->m_buffer.m_first_page = 0;
    sb->m_buffer.m_last_page = 0;
    sb->m_bytes = 0;
    return 0;
  }
  else if (remain)
  {
    /**
     * Half a page was released
     */
    curr->m_start += remain;
    assert(curr->m_bytes > remain);
    curr->m_bytes -= remain;
    if (prev)
    {
      release_list(pool, sb->m_buffer.m_first_page, prev);
    }
  }
  else
  {
    /**
     * X full page(s) was released
     */
    if (prev)
    {
      release_list(pool, sb->m_buffer.m_first_page, prev);
    }
    else
    {
      pool->release_local(sb->m_buffer.m_first_page);
    }
  }

  sb->m_buffer.m_first_page = curr;
  assert(sb->m_bytes > bytes);
  sb->m_bytes -= bytes;

  /**
   * Since not all bytes were sent...
   *   spend the time to try to pack the pages
   *   possibly releasing send-buffer
   */
  pack_sb_pages(pool, sb);

  return sb->m_bytes;
}

Uint32
trp_callback::bytes_sent(NodeId node, Uint32 bytes)
{
  thr_repository::send_buffer * sb = g_thr_repository.m_send_buffers+node;
  Uint32 thr_no = sb->m_send_thread;
  assert(thr_no != NO_SEND_THREAD);
  if (!is_send_thread(thr_no))
  {
    thr_data * thrptr = &g_thr_repository.m_thread[thr_no].m_thr_data;
    return ::bytes_sent(&thrptr->m_send_buffer_pool,
                        sb, bytes);
  }
  else
  {
    return ::bytes_sent(g_send_threads->get_send_buffer_pool(thr_no),
                        sb, bytes);
  }
}

bool
trp_callback::has_data_to_send(NodeId node)
{
  return true;

  thr_repository::send_buffer * sb = g_thr_repository.m_send_buffers + node;
  Uint32 thr_no = sb->m_send_thread;
  assert(thr_no != NO_SEND_THREAD);
  assert(!is_send_thread(thr_no));
  assert((sb->m_bytes > 0) == (sb->m_buffer.m_first_page != 0));
  if (sb->m_bytes > 0 || sb->m_force_send)
    return true;

  thr_send_queue * dst = g_thr_repository.m_thread_send_buffers[node]+thr_no;
  return sb->m_read_index[thr_no] != dst->m_write_index;
}

void
trp_callback::reset_send_buffer(NodeId node, bool should_be_empty)
{
  struct thr_repository *rep = &g_thr_repository;
  thr_repository::send_buffer * sb = g_thr_repository.m_send_buffers+node;
  struct iovec v[32];

  thread_local_pool<thr_send_page> pool(&rep->m_sb_pool, 0);

  lock(&sb->m_send_lock);

  for (;;)
  {
    Uint32 count = get_bytes_to_send_iovec(node, v, sizeof(v)/sizeof(v[0]));
    if (count == 0)
      break;
    assert(!should_be_empty); // Got data when it should be empty
    int bytes = 0;
    for (Uint32 i = 0; i < count; i++)
      bytes += v[i].iov_len;

    ::bytes_sent(&pool, sb, bytes);
  }

  unlock(&sb->m_send_lock);

  pool.release_all(rep->m_mm, RG_TRANSPORTER_BUFFERS);
}

static inline
void
register_pending_send(thr_data *selfptr, Uint32 nodeId)
{
  /* Mark that this node has pending send data. */
  if (!selfptr->m_pending_send_mask.get(nodeId))
  {
    selfptr->m_pending_send_mask.set(nodeId, 1);
    Uint32 i = selfptr->m_pending_send_count;
    selfptr->m_pending_send_nodes[i] = nodeId;
    selfptr->m_pending_send_count = i + 1;
  }
}

static void try_send(thr_data *, Uint32); // prototype

/**
 * pack send buffers for a specific node
 */
void
pack_send_buffer(thr_data *selfptr, Uint32 node)
{
  thr_repository* rep = &g_thr_repository;
  thr_repository::send_buffer* sb = rep->m_send_buffers+node;
  thread_local_pool<thr_send_page>* pool = &selfptr->m_send_buffer_pool;

  lock(&sb->m_send_lock);
  int bytes = link_thread_send_buffers(sb, node);
  if (bytes)
  {
    pack_sb_pages(pool, sb);
  }
  unlock(&sb->m_send_lock);

  /**
   * release buffers prior to checking m_force_send
   */
  pool->release_global(rep->m_mm, RG_TRANSPORTER_BUFFERS);

  /**
   * After having locked/unlock m_send_lock
   *   "protocol" dictates that we must check the m_force_send
   */

  /**
   * We need a memory barrier here to prevent race between clearing lock
   *   and reading of m_force_send.
   *   CPU can reorder the load to before the clear of the lock
   */
  mb();
  if (sb->m_force_send)
  {
    try_send(selfptr, node);
  }
}

static
void
pack_send_buffers(thr_data* selfptr)
{
  for (Uint32 i = 1; i < NDB_ARRAY_SIZE(selfptr->m_send_buffers); i++)
  {
    if (globalTransporterRegistry.get_transporter(i))
      pack_send_buffer(selfptr, i);
  }
}


/**
 * publish thread-locally prepared send-buffer
 */
static
void
flush_send_buffer(thr_data* selfptr, Uint32 node)
{
  Uint32 thr_no = selfptr->m_thr_no;
  thr_send_buffer * src = selfptr->m_send_buffers + node;
  thr_repository* rep = &g_thr_repository;

  if (src->m_first_page == 0)
  {
    return;
  }
  assert(src->m_last_page != 0);

  thr_send_queue * dst = rep->m_thread_send_buffers[node]+thr_no;
  thr_repository::send_buffer* sb = rep->m_send_buffers+node;

  Uint32 wi = dst->m_write_index;
  Uint32 next = (wi + 1) % thr_send_queue::SIZE;
  Uint32 ri = sb->m_read_index[thr_no];

  if (unlikely(next == ri))
  {
    pack_send_buffer(selfptr, node);
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
mt_send_handle::forceSend(NodeId nodeId)
{
  struct thr_repository *rep = &g_thr_repository;
  struct thr_data *selfptr = m_selfptr;
  struct thr_repository::send_buffer * sb = rep->m_send_buffers + nodeId;

  do
  {
    /**
     * NOTE: we don't need a memory barrier after clearing
     *       m_force_send here as we unconditionally lock m_send_lock
     *       hence there is no way that our data can be "unsent"
     */
    sb->m_force_send = 0;

    lock(&sb->m_send_lock);
    sb->m_send_thread = selfptr->m_thr_no;
    globalTransporterRegistry.performSend(nodeId);
    sb->m_send_thread = NO_SEND_THREAD;
    unlock(&sb->m_send_lock);

    /**
     * release buffers prior to maybe looping on sb->m_force_send
     */
    selfptr->m_send_buffer_pool.release_global(rep->m_mm,
                                               RG_TRANSPORTER_BUFFERS);
    /**
     * We need a memory barrier here to prevent race between clearing lock
     *   and reading of m_force_send.
     *   CPU can reorder the load to before the clear of the lock
     */
    mb();
  } while (sb->m_force_send);

  return true;
}

/**
 * try sending data
 */
static
void
try_send(thr_data * selfptr, Uint32 node)
{
  struct thr_repository *rep = &g_thr_repository;
  struct thr_repository::send_buffer * sb = rep->m_send_buffers + node;

  do
  {
    if (trylock(&sb->m_send_lock) != 0)
    {
      return;
    }

    /**
     * Now clear the flag, and start sending all data available to this node.
     *
     * Put a memory barrier here, so that if another thread tries to grab
     * the send lock but fails due to us holding it here, we either
     * 1) Will see m_force_send[nodeId] set to 1 at the end of the loop, or
     * 2) We clear here the flag just set by the other thread, but then we
     * will (thanks to mb()) be able to see and send all of the data already
     * in the first send iteration.
     */
    sb->m_force_send = 0;
    mb();

    sb->m_send_thread = selfptr->m_thr_no;
    globalTransporterRegistry.performSend(node);
    sb->m_send_thread = NO_SEND_THREAD;
    unlock(&sb->m_send_lock);

    /**
     * release buffers prior to maybe looping on sb->m_force_send
     */
    selfptr->m_send_buffer_pool.release_global(rep->m_mm,
                                               RG_TRANSPORTER_BUFFERS);

    /**
     * We need a memory barrier here to prevent race between clearing lock
     *   and reading of m_force_send.
     *   CPU can reorder the load to before the clear of the lock
     */
    mb();
  } while (sb->m_force_send);
}

/**
 * Flush send buffers and append them to dst. nodes send queue
 *
 * Flushed buffer contents are piggybacked when another thread
 * do_send() to the same dst. node. This makes it possible to have
 * more data included in each message, and thereby reduces total
 * #messages handled by the OS which really impacts performance!
 */
static
void
do_flush(struct thr_data* selfptr)
{
  Uint32 i;
  Uint32 count = selfptr->m_pending_send_count;
  Uint8 *nodes = selfptr->m_pending_send_nodes;

  for (i = 0; i < count; i++)
  {
    flush_send_buffer(selfptr, nodes[i]);
  }
}

/**
 * Send any pending data to remote nodes.
 *
 * If MUST_SEND is false, will only try to lock the send lock, but if it would
 * block, that node is skipped, to be tried again next time round.
 *
 * If MUST_SEND is true, will always take the lock, waiting on it if needed.
 *
 * The list of pending nodes to send to is thread-local, but the per-node send
 * buffer is shared by all threads. Thus we might skip a node for which
 * another thread has pending send data, and we might send pending data also
 * for another thread without clearing the node from the pending list of that
 * other thread (but we will never loose signals due to this).
 */
static
Uint32
do_send(struct thr_data* selfptr, bool must_send)
{
  Uint32 i;
  Uint32 count = selfptr->m_pending_send_count;
  Uint8 *nodes = selfptr->m_pending_send_nodes;
  struct thr_repository* rep = &g_thr_repository;

  if (count == 0)
  {
    return 0; // send-buffers empty
  }

  /* Clear the pending list. */
  selfptr->m_pending_send_mask.clear();
  selfptr->m_pending_send_count = 0;

  for (i = 0; i < count; i++)
  {
    Uint32 node = nodes[i];
    selfptr->m_watchdog_counter = 6;

    flush_send_buffer(selfptr, node);

    if (g_send_threads)
    {
      g_send_threads->alert_send_thread(node);
      continue;
    }
    thr_repository::send_buffer * sb = rep->m_send_buffers + node;

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

    do
    {
      if (trylock(&sb->m_send_lock) != 0)
      {
        if (!must_send)
        {
          /**
           * Not doing this node now, re-add to pending list.
           *
           * As we only add from the start of an empty list, we are safe from
           * overwriting the list while we are iterating over it.
           */
          register_pending_send(selfptr, node);
        }
        else
        {
          /* Other thread will send for us as we set m_force_send. */
        }
        break;
      }

      /**
       * Now clear the flag, and start sending all data available to this node.
       *
       * Put a memory barrier here, so that if another thread tries to grab
       * the send lock but fails due to us holding it here, we either
       * 1) Will see m_force_send[nodeId] set to 1 at the end of the loop, or
       * 2) We clear here the flag just set by the other thread, but then we
       * will (thanks to mb()) be able to see and send all of the data already
       * in the first send iteration.
       */
      sb->m_force_send = 0;
      mb();

      /**
       * Set m_send_thr so that our transporter callback can know which thread
       * holds the send lock for this remote node.
       */
      sb->m_send_thread = selfptr->m_thr_no;
      int res = globalTransporterRegistry.performSend(node);
      sb->m_send_thread = NO_SEND_THREAD;
      unlock(&sb->m_send_lock);
      if (res)
      {
        register_pending_send(selfptr, node);
      }

      /**
       * We need a memory barrier here to prevent race between clearing lock
       *   and reading of m_force_send.
       *   CPU can reorder the load to before the clear of the lock
       */
      mb();
      if (sb->m_force_send)
      {
        /**
         * release buffers prior to looping on sb->m_force_send
         */
        selfptr->m_send_buffer_pool.release_global(rep->m_mm,
                                                   RG_TRANSPORTER_BUFFERS);
      }
    } while (sb->m_force_send);
  }

  selfptr->m_send_buffer_pool.release_global(rep->m_mm, RG_TRANSPORTER_BUFFERS);

  return selfptr->m_pending_send_count;
}

Uint32 *
mt_send_handle::getWritePtr(NodeId node, Uint32 len, Uint32 prio, Uint32 max)
{
  struct thr_send_buffer * b = m_selfptr->m_send_buffers+node;
  thr_send_page * p = b->m_last_page;
  if ((p != 0) && (p->m_bytes + p->m_start + len <= thr_send_page::max_bytes()))
  {
    return (Uint32*)(p->m_data + p->m_start + p->m_bytes);
  }
  else if (p != 0)
  {
    // TODO: maybe dont always flush on page-boundary ???
    flush_send_buffer(m_selfptr, node);
    if (!g_send_threads)
      try_send(m_selfptr, node);
  }

  if ((p = m_selfptr->m_send_buffer_pool.seize(g_thr_repository.m_mm,
                                               RG_TRANSPORTER_BUFFERS)) != 0)
  {
    p->m_bytes = 0;
    p->m_start = 0;
    p->m_next = 0;
    b->m_first_page = b->m_last_page = p;
    return (Uint32*)p->m_data;
  }
  return 0;
}

Uint32
mt_send_handle::updateWritePtr(NodeId node, Uint32 lenBytes, Uint32 prio)
{
  struct thr_send_buffer * b = m_selfptr->m_send_buffers+node;
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
  w->m_pending_signals++;

#if SIZEOF_CHARP == 8
  /* Align to 8-byte boundary, to ensure aligned copies. */
  write_pos= (write_pos+1) & ~((Uint32)1);
#endif

  /*
   * We make sure that there is always room for at least one signal in the
   * current buffer in the queue, so one insert is always possible without
   * adding a new buffer.
   */
  if (likely(write_pos + 32 <= thr_job_buffer::SIZE))
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

/* Check all job queues, return true only if all are empty. */
static bool
check_queues_empty(thr_data *selfptr)
{
  Uint32 thr_count = g_thr_repository.m_thread_count;
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
                Signal *sig, Uint32 max_signals, Uint32 *signalIdCounter)
{
  Uint32 num_signals;
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
    while (read_pos >= read_end)
    {
      if (read_index == write_index)
      {
        /* No more available now. */
        return num_signals;
      }
      else
      {
        /* Move to next buffer. */
        read_index = (read_index + 1) % thr_job_queue::SIZE;
        release_buffer(&g_thr_repository, selfptr->m_thr_no, read_buffer);
        read_buffer = q->m_buffers[read_index];
        read_pos = 0;
        read_end = read_buffer->m_len;
        /* Update thread-local read state. */
        r->m_read_index = h->m_read_index = read_index;
        r->m_read_buffer = read_buffer;
        r->m_read_pos = read_pos;
        r->m_read_end = read_end;
      }
    }

    /*
     * These pre-fetching were found using OProfile to reduce cache misses.
     * (Though on Intel Core 2, they do not give much speedup, as apparently
     * the hardware prefetcher is already doing a fairly good job).
     */
    NDB_PREFETCH_READ (read_buffer->m_data + read_pos + 16);
    NDB_PREFETCH_WRITE ((Uint32 *)&sig->header + 16);

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
    *watchDogCounter = 1;
    /* Must update original buffer so signal dump will see it. */
    s->theSignalId = (*signalIdCounter)++;
    memcpy(&sig->header, s, 4*siglen);
    sig->m_sectionPtrI[0] = read_buffer->m_data[read_pos + siglen + 0];
    sig->m_sectionPtrI[1] = read_buffer->m_data[read_pos + siglen + 1];
    sig->m_sectionPtrI[2] = read_buffer->m_data[read_pos + siglen + 2];

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

    block->executeFunction(gsn, sig);
  }

  return num_signals;
}

static
Uint32
run_job_buffers(thr_data *selfptr, Signal *sig, Uint32 *signalIdCounter)
{
  Uint32 thr_count = g_thr_repository.m_thread_count;
  Uint32 signal_count = 0;
  Uint32 perjb = selfptr->m_max_signals_per_jb;

  read_jbb_state(selfptr, thr_count);
  /*
   * A load memory barrier to ensure that we see any prio A signal sent later
   * than loaded prio B signals.
   */
  rmb();

  thr_job_queue *queue = selfptr->m_in_queue;
  thr_job_queue_head *head = selfptr->m_in_queue_head;
  thr_jb_read_state *read_state = selfptr->m_read_states;
  for (Uint32 send_thr_no = 0; send_thr_no < thr_count;
       send_thr_no++,queue++,read_state++,head++)
  {
    /* Read the prio A state often, to avoid starvation of prio A. */
    bool jba_empty = read_jba_state(selfptr);
    if (!jba_empty)
    {
      static Uint32 max_prioA = thr_job_queue::SIZE * thr_job_buffer::SIZE;
      signal_count += execute_signals(selfptr,
                                      &(selfptr->m_jba),
                                      &(selfptr->m_jba_head),
                                      &(selfptr->m_jba_read_state), sig,
                                      max_prioA, signalIdCounter);
    }

    /* Now execute prio B signals from one thread. */
    signal_count += execute_signals(selfptr, queue, head, read_state,
                                    sig, perjb, signalIdCounter);
  }

  return signal_count;
}

struct thr_map_entry {
  enum { NULL_THR_NO = 0xFF };
  Uint8 thr_no;
  thr_map_entry() : thr_no(NULL_THR_NO) {}
};

static struct thr_map_entry thr_map[NO_OF_BLOCKS][NDBMT_MAX_BLOCK_INSTANCES];

static inline Uint32
block2ThreadId(Uint32 block, Uint32 instance)
{
  assert(block >= MIN_BLOCK_NO && block <= MAX_BLOCK_NO);
  Uint32 index = block - MIN_BLOCK_NO;
  assert(instance < NDB_ARRAY_SIZE(thr_map[index]));
  const thr_map_entry& entry = thr_map[index][instance];
  assert(entry.thr_no < num_threads);
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

  require(thr_no < num_threads);
  struct thr_repository* rep = &g_thr_repository;
  struct thr_data* thr_ptr = &rep->m_thread[thr_no].m_thr_data;

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
    return num_threads;
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
      cnt++;

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
        }
      }
    }
  }
}

static void
init_thread(thr_data *selfptr)
{
  selfptr->m_waiter.init();
  selfptr->m_jam.theEmulatedJamIndex = 0;
  selfptr->m_jam.theEmulatedJamBlockNumber = 0;
  bzero(selfptr->m_jam.theEmulatedJam, sizeof(selfptr->m_jam.theEmulatedJam));
  NdbThread_SetTlsKey(NDB_THREAD_TLS_JAM, &selfptr->m_jam);
  NdbThread_SetTlsKey(NDB_THREAD_TLS_THREAD, selfptr);

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

  int tid = NdbThread_GetTid(selfptr->m_thread);
  if (tid != -1)
  {
    tmp.appfmt("tid: %u ", tid);
  }

  conf.appendInfo(tmp,
                  selfptr->m_instance_list, selfptr->m_instance_count);
  int res = conf.do_bind(selfptr->m_thread,
                         selfptr->m_instance_list, selfptr->m_instance_count);
  if (res < 0)
  {
    tmp.appfmt("err: %d ", -res);
  }
  else if (res > 0)
  {
    tmp.appfmt("OK ");
  }

  selfptr->m_thr_id = pthread_self();

  for (Uint32 i = 0; i < selfptr->m_instance_count; i++) 
  {
    BlockReference block = selfptr->m_instance_list[i];
    Uint32 main = blockToMain(block);
    Uint32 instance = blockToInstance(block);
    tmp.appfmt("%s(%u) ", getBlockName(main), instance);
  }
  printf("%s\n", tmp.c_str());
  fflush(stdout);
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
TransporterReceiveHandleKernel *
  g_trp_receive_handle_ptr[MAX_NDBMT_RECEIVE_THREADS];

/**
 * Array for mapping nodes to receiver threads and function to access it.
 */
static NodeId g_node_to_recv_thr_map[MAX_NODES];

extern "C"
void *
mt_receiver_thread_main(void *thr_arg)
{
  unsigned char signal_buf[SIGBUF_SIZE];
  Signal *signal;
  struct thr_repository* rep = &g_thr_repository;
  struct thr_data* selfptr = (struct thr_data *)thr_arg;
  unsigned thr_no = selfptr->m_thr_no;
  Uint32& watchDogCounter = selfptr->m_watchdog_counter;
  Uint32 thrSignalId = 0;
  const Uint32 recv_thread_idx = thr_no - first_receiver_thread_no;
  bool has_received = false;
  int cnt = 0;

  init_thread(selfptr);
  signal = aligned_signal(signal_buf, thr_no);

  /**
   * Object that keeps track of our pollReceive-state
   */
  TransporterReceiveHandleKernel recvdata(thr_no, recv_thread_idx);
  recvdata.assign_nodes(g_node_to_recv_thr_map);
  globalTransporterRegistry.init(recvdata);

  /**
   * Save pointer to this for management/error-insert
   */
  g_trp_receive_handle_ptr[recv_thread_idx] = &recvdata;

  while (globalData.theRestartFlag != perform_stop)
  {
    if (cnt == 0)
    {
      watchDogCounter = 5;
      globalTransporterRegistry.update_connections(recvdata);
    }
    cnt = (cnt + 1) & 15;

    watchDogCounter = 2;

    NDB_TICKS now = NdbTick_CurrentMillisecond();
    scan_time_queues(selfptr, now);

    Uint32 sum = run_job_buffers(selfptr, signal, &thrSignalId);

    if (sum || has_received)
    {
      watchDogCounter = 6;
      flush_jbb_write_state(selfptr);
    }

    do_send(selfptr, TRUE);

    watchDogCounter = 7;

    has_received = false;
    if (globalTransporterRegistry.pollReceive(1, recvdata))
    {
      if (check_job_buffers(rep, recv_thread_idx) == 0)
      {
	watchDogCounter = 8;
        lock(&rep->m_receive_lock[recv_thread_idx]);
        globalTransporterRegistry.performReceive(recvdata);
        unlock(&rep->m_receive_lock[recv_thread_idx]);
        has_received = true;
      }
    }
    selfptr->m_stat.m_loop_cnt++;
    selfptr->m_stat.m_exec_cnt += sum;
  }

  globalEmulatorData.theWatchDog->unregisterWatchedThread(thr_no);
  return NULL;                  // Return value not currently used
}

static
inline
void
sendpacked(struct thr_data* thr_ptr, Signal* signal)
{
  Uint32 i;
  for (i = 0; i < thr_ptr->m_instance_count; i++)
  {
    BlockReference block = thr_ptr->m_instance_list[i];
    Uint32 main = blockToMain(block);
    Uint32 instance = blockToInstance(block);
    SimulatedBlock* b = globalData.getBlock(main, instance);
    // wl4391_todo remove useless assert
    assert(b != 0 && b->getThreadId() == thr_ptr->m_thr_no);
    /* b->send_at_job_buffer_end(); */
    b->executeFunction(GSN_SEND_PACKED, signal);
  }
}

/**
 * check if out-queues of selfptr is full
 * return true is so
 */
static bool
check_job_buffer_full(thr_data *selfptr)
{
  Uint32 thr_no = selfptr->m_thr_no;
  Uint32 tmp = compute_max_signals_to_execute(thr_no);
#if 0
  Uint32 perjb = tmp / g_thr_repository.m_thread_count;

  if (perjb == 0)
  {
    return true;
  }

  return false;
#else
  if (tmp < g_thr_repository.m_thread_count)
    return true;
  return false;
#endif
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
 *       mostly followed by block-code, although not it all places :-(
 *
 *   This function return true, if it it slept
 *     (i.e that it concluded that it could not execute *any* signals, wo/
 *      risking job-buffer-full)
 */
static
bool
update_sched_config(struct thr_data* selfptr, Uint32 pending_send)
{
  Uint32 sleeploop = 0;
  Uint32 thr_no = selfptr->m_thr_no;
loop:
  Uint32 tmp = compute_max_signals_to_execute(thr_no);
  Uint32 perjb = tmp / g_thr_repository.m_thread_count;

  if (perjb > MAX_SIGNALS_PER_JB)
    perjb = MAX_SIGNALS_PER_JB;

  selfptr->m_max_exec_signals = tmp;
  selfptr->m_max_signals_per_jb = perjb;

  if (unlikely(perjb == 0))
  {
    sleeploop++;
    if (sleeploop == 10)
    {
      /**
       * we've slept for 10ms...try running anyway
       */
      selfptr->m_max_signals_per_jb = 1;
      ndbout_c("%u - sleeploop 10!!", selfptr->m_thr_no);
      return true;
    }

    if (pending_send)
    {
      /* About to sleep, _must_ send now. */
      pending_send = do_send(selfptr, TRUE);
    }

    const Uint32 wait = 1000000;    /* 1 ms */
    yield(&selfptr->m_waiter, wait, check_job_buffer_full, selfptr);
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
  const Uint32 nowait = 10 * 1000000;    /* 10 ms */
  Uint32 thrSignalId = 0;

  struct thr_data* selfptr = (struct thr_data *)thr_arg;
  init_thread(selfptr);
  Uint32& watchDogCounter = selfptr->m_watchdog_counter;

  unsigned thr_no = selfptr->m_thr_no;
  signal = aligned_signal(signal_buf, thr_no);

  /* Avoid false watchdog alarms caused by race condition. */
  watchDogCounter = 1;

  Uint32 pending_send = 0;
  Uint32 send_sum = 0;
  Uint32 loops = 0;
  Uint32 maxloops = 10;/* Loops before reading clock, fuzzy adapted to 1ms freq. */
  Uint32 waits = 0;
  NDB_TICKS now = selfptr->m_time;

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
    if (!selfptr->m_send_buffer_pool.fill(g_thr_repository.m_mm,
                                          RG_TRANSPORTER_BUFFERS,
                                          THR_SEND_BUFFER_PRE_ALLOC))
    {
      pack_send_buffers(selfptr);
    }

    watchDogCounter = 2;
    scan_time_queues(selfptr, now);

    Uint32 sum = run_job_buffers(selfptr, signal, &thrSignalId);
    
    watchDogCounter = 1;
    signal->header.m_noOfSections = 0; /* valgrind */
    sendpacked(selfptr, signal);

    if (sum)
    {
      watchDogCounter = 6;
      flush_jbb_write_state(selfptr);
      send_sum += sum;

      if (send_sum > MAX_SIGNALS_BEFORE_SEND)
      {
        /* Try to send, but skip for now in case of lock contention. */
        pending_send = do_send(selfptr, FALSE);
        send_sum = 0;
      }
      else
      {
        /* Send buffers append to send queues to dst. nodes. */
        do_flush(selfptr);
      }
    }
    else
    {
      /* No signals processed, prepare to sleep to wait for more */
      if ((pending_send + send_sum) > 0)
      {
        /* About to sleep, _must_ send now. */
        pending_send = do_send(selfptr, TRUE);
        send_sum = 0;
      }

      if (pending_send == 0)
      {
        bool waited = yield(&selfptr->m_waiter, nowait, check_queues_empty,
                            selfptr);
        if (waited)
        {
          waits++;
          /* Update current time after sleeping */
          now = NdbTick_CurrentMillisecond();
          selfptr->m_stat.m_wait_cnt += waits;
          selfptr->m_stat.m_loop_cnt += loops;
          waits = loops = 0;
        }
      }
    }

    /**
     * Check if we executed enough signals,
     *   and if so recompute how many signals to execute
     */
    if (sum >= selfptr->m_max_exec_signals)
    {
      if (update_sched_config(selfptr, pending_send + send_sum))
      {
        /* Update current time after sleeping */
        now = NdbTick_CurrentMillisecond();
        selfptr->m_stat.m_wait_cnt += waits;
        selfptr->m_stat.m_loop_cnt += loops;
        waits = loops = 0;
      }
    }
    else
    {
      selfptr->m_max_exec_signals -= sum;
    }

    /**
     * Adaptive reading freq. of systeme time every time 1ms
     * is likely to have passed
     */
    if (loops > maxloops)
    {
      now = NdbTick_CurrentMillisecond();
      Uint64 diff = now - selfptr->m_time;

      /* Adjust 'maxloop' to achieve clock reading frequency of 1ms */
      if (diff < 1)
        maxloops += ((maxloops/10) + 1); /* No change: less frequent reading */
      else if (diff > 1 && maxloops > 1)
        maxloops -= ((maxloops/10) + 1); /* Overslept: Need more frequent read*/

      selfptr->m_stat.m_wait_cnt += waits;
      selfptr->m_stat.m_loop_cnt += loops;
      waits = loops = 0;
    }
    selfptr->m_stat.m_exec_cnt += sum;
  }

  globalEmulatorData.theWatchDog->unregisterWatchedThread(thr_no);
  return NULL;                  // Return value not currently used
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
  struct thr_repository* rep = &g_thr_repository;
  struct thr_data *selfptr = &rep->m_thread[self].m_thr_data;
  assert(pthread_equal(selfptr->m_thr_id, pthread_self()));
  struct thr_data *dstptr = &rep->m_thread[dst].m_thr_data;

  selfptr->m_stat.m_priob_count++;
  Uint32 siglen = (sizeof(*s) >> 2) + s->theLength + s->m_noOfSections;
  selfptr->m_stat.m_priob_size += siglen;

  thr_job_queue *q = dstptr->m_in_queue + self;
  thr_job_queue_head *h = dstptr->m_in_queue_head + self;
  thr_jb_write_state *w = selfptr->m_write_states + dst;
  if (insert_signal(q, h, w, false, s, data, secPtr, selfptr->m_next_buffer))
  {
    selfptr->m_next_buffer = seize_buffer(rep, self, false);
  }
  if (w->m_pending_signals >= MAX_SIGNALS_BEFORE_FLUSH)
    flush_write_state(selfptr, dstptr, h, w);
}

void
sendprioa(Uint32 self, const SignalHeader *s, const uint32 *data,
          const Uint32 secPtr[3])
{
  Uint32 block = blockToMain(s->theReceiversBlockNumber);
  Uint32 instance = blockToInstance(s->theReceiversBlockNumber);

  Uint32 dst = block2ThreadId(block, instance);
  struct thr_repository* rep = &g_thr_repository;
  struct thr_data *selfptr = &rep->m_thread[self].m_thr_data;
  assert(s->theVerId_signalNumber == GSN_START_ORD ||
         pthread_equal(selfptr->m_thr_id, pthread_self()));
  struct thr_data *dstptr = &rep->m_thread[dst].m_thr_data;

  selfptr->m_stat.m_prioa_count++;
  Uint32 siglen = (sizeof(*s) >> 2) + s->theLength + s->m_noOfSections;
  selfptr->m_stat.m_prioa_size += siglen;

  thr_job_queue *q = &(dstptr->m_jba);
  thr_job_queue_head *h = &(dstptr->m_jba_head);
  thr_jb_write_state w;

  lock(&dstptr->m_jba_write_lock);

  Uint32 index = h->m_write_index;
  w.m_write_index = index;
  thr_job_buffer *buffer = q->m_buffers[index];
  w.m_write_buffer = buffer;
  w.m_write_pos = buffer->m_len;
  w.m_pending_signals = 0;
  w.m_pending_signals_wakeup = MAX_SIGNALS_BEFORE_WAKEUP;
  bool buf_used = insert_signal(q, h, &w, true, s, data, secPtr,
                                selfptr->m_next_buffer);
  flush_write_state(selfptr, dstptr, h, &w);

  unlock(&dstptr->m_jba_write_lock);

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
  thr_repository *rep = &g_thr_repository;
  struct thr_data *selfptr = &rep->m_thread[self].m_thr_data;
  SendStatus ss;

  mt_send_handle handle(selfptr);
  register_pending_send(selfptr, nodeId);
  /* prepareSend() is lock-free, as we have per-thread send buffers. */
  ss = globalTransporterRegistry.prepareSend(&handle,
                                             sh, prio, data, nodeId, ptr);
  return ss;
}

SendStatus
mt_send_remote(Uint32 self, const SignalHeader *sh, Uint8 prio,
               const Uint32 *data, NodeId nodeId,
               class SectionSegmentPool *thePool,
               const SegmentedSectionPtr ptr[3])
{
  thr_repository *rep = &g_thr_repository;
  struct thr_data *selfptr = &rep->m_thread[self].m_thr_data;
  SendStatus ss;

  mt_send_handle handle(selfptr);
  register_pending_send(selfptr, nodeId);
  ss = globalTransporterRegistry.prepareSend(&handle,
                                             sh, prio, data, nodeId,
                                             *thePool, ptr);
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
  struct thr_repository* rep = &g_thr_repository;
  /* As this signal will be the last one executed by the other thread, it does
     not matter which buffer we use in case the current buffer is filled up by
     the STOP_FOR_CRASH signal; the data in it will never be read.
  */
  static thr_job_buffer dummy_buffer;

  /**
   * Pick any instance running in this thread
   */
  struct thr_data *dstptr = &rep->m_thread[dst].m_thr_data;
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

  lock(&dstptr->m_jba_write_lock);

  Uint32 index = h->m_write_index;
  w.m_write_index = index;
  thr_job_buffer *buffer = q->m_buffers[index];
  w.m_write_buffer = buffer;
  w.m_write_pos = buffer->m_len;
  w.m_pending_signals = 0;
  w.m_pending_signals_wakeup = MAX_SIGNALS_BEFORE_WAKEUP;
  insert_signal(q, h, &w, true, &signalT.header, signalT.theData, NULL,
                &dummy_buffer);
  flush_write_state(selfptr, dstptr, h, &w);

  unlock(&dstptr->m_jba_write_lock);
}

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
  unsigned recv_base = NUM_MAIN_THREADS+globalData.ndbMtLqhThreads+globalData.ndbMtTcThreads;
  return thr_no >= recv_base &&
         thr_no <  recv_base+globalData.ndbMtReceiveThreads;
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
  tq->m_cnt[0] = tq->m_cnt[1] = 0;
  bzero(tq->m_delayed_signals, sizeof(tq->m_delayed_signals));
}

static
void
thr_init(struct thr_repository* rep, struct thr_data *selfptr, unsigned int cnt,
         unsigned thr_no)
{
  Uint32 i;

  selfptr->m_thr_no = thr_no;
  selfptr->m_max_signals_per_jb = MAX_SIGNALS_PER_JB;
  selfptr->m_max_exec_signals = 0;
  selfptr->m_first_free = 0;
  selfptr->m_first_unused = 0;
  
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
      rep->m_thread[i].m_thr_data.m_in_queue[thr_no].m_buffers[0];
    selfptr->m_write_states[i].m_pending_signals = 0;
    selfptr->m_write_states[i].m_pending_signals_wakeup = 0;
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
send_buffer_init(Uint32 node, thr_repository::send_buffer * sb)
{
  char buf[100];
  BaseString::snprintf(buf, sizeof(buf), "send lock node %d", node);
  register_lock(&sb->m_send_lock, buf);
  sb->m_force_send = 0;
  sb->m_send_thread = NO_SEND_THREAD;
  bzero(&sb->m_buffer, sizeof(sb->m_buffer));
  sb->m_bytes = 0;
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
    thr_init(rep, &rep->m_thread[i].m_thr_data, cnt, i);
  }
  for (unsigned int i = 0; i<cnt; i++)
  {
    thr_init2(rep, &rep->m_thread[i].m_thr_data, cnt, i);
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

#include "ThreadConfig.hpp"
#include <signaldata/StartOrd.hpp>

static Uint32
get_total_number_of_block_threads(void)
{
  return (NUM_MAIN_THREADS +
          globalData.ndbMtLqhThreads + 
          globalData.ndbMtTcThreads +
          globalData.ndbMtReceiveThreads);
}

static Uint32
get_num_nodes()
{
  Uint32 count = 0;
  for (Uint32 nodeId = 1; nodeId < MAX_NODES; nodeId++)
  {
    if (globalTransporterRegistry.get_transporter(nodeId))
    {
      count++;
    }
  }
  return count;
}

/**
 * This function returns the amount of extra send buffer pages
 * that we should allocate in addition to the amount allocated
 * for each node send buffer.
 */
#define MIN_SEND_BUFFER_GENERAL (512) //16M
#define MIN_SEND_BUFFER_PER_NODE (8) //256k
#define MIN_SEND_BUFFER_PER_THREAD (64) //2M

Uint32
mt_get_extra_send_buffer_pages(Uint32 curr_num_pages,
                               Uint32 extra_mem_pages)
{
  Uint32 num_threads = get_total_number_of_block_threads();
  Uint32 num_nodes = get_num_nodes();

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
  extra_pages += num_threads * THR_SEND_BUFFER_MAX_FREE;

  if (extra_mem_pages == 0)
  {
    /**
     * The user have set extra send buffer memory to 0 and left for us
     * to decide on our own how much extra memory is needed.
     *
     * We'll make sure that we have at least a minimum of 16M +
     * 2M per thread + 256k per node. If we have this based on
     * curr_num_pages and our local additions we don't add
     * anything more, if we don't come up to this level we add to
     * reach this minimum level.
     */
    Uint32 min_pages = MIN_SEND_BUFFER_GENERAL +
      (MIN_SEND_BUFFER_PER_NODE * num_nodes) +
      (MIN_SEND_BUFFER_PER_THREAD * num_threads);

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
}

ThreadConfig::~ThreadConfig()
{
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
  num_threads = first_receiver_thread_no + num_recv_threads;
  require(num_threads <= MAX_BLOCK_THREADS);

  ndbout << "NDBMT: number of block threads=" << num_threads << endl;

  ::rep_init(&g_thr_repository, num_threads,
             globalEmulatorData.m_mem_manager);
}

static
void
setcpuaffinity(struct thr_repository* rep)
{
  THRConfigApplier & conf = globalEmulatorData.theConfiguration->m_thr_config;
  conf.create_cpusets();
  if (conf.getInfoMessage())
  {
    printf("%s", conf.getInfoMessage());
    fflush(stdout);
  }
}

/**
 * return receiver thread handling a particular node
 *   returned number is indexed from 0 and upwards to #receiver threads
 *   (or MAX_NODES is none)
 */
Uint32
mt_get_recv_thread_idx(NodeId nodeId)
{
  assert(nodeId < NDB_ARRAY_SIZE(g_node_to_recv_thr_map));
  return g_node_to_recv_thr_map[nodeId];
}

static
void
assign_receiver_threads(void)
{
  Uint32 num_recv_threads = globalData.ndbMtReceiveThreads;
  Uint32 recv_thread_idx = 0;
  for (Uint32 nodeId = 1; nodeId < MAX_NODES; nodeId++)
  {
    Transporter *node_trp =
      globalTransporterRegistry.get_transporter(nodeId);

    if (node_trp)
    {
      g_node_to_recv_thr_map[nodeId] = recv_thread_idx;
      recv_thread_idx++;
      if (recv_thread_idx == num_recv_threads)
        recv_thread_idx = 0;
    }
    else
    {
      /* Flag for no transporter */
      g_node_to_recv_thr_map[nodeId] = MAX_NODES;
    }
  }
  return;
}

void
ThreadConfig::ipControlLoop(NdbThread* pThis, Uint32 thread_index)
{
  unsigned int thr_no;
  struct thr_repository* rep = &g_thr_repository;

  /**
   * assign threads to CPU's
   */
  setcpuaffinity(rep);

  if (globalData.ndbMtSendThreads)
  {
    g_send_threads = new thr_send_threads();
  }

  /**
   * assign nodes to receiver threads
   */
  assign_receiver_threads();

  /* Start the send thread(s) */
  if (g_send_threads)
  {
    g_send_threads->start_send_threads();
  }

  /*
   * Start threads for all execution threads, except for the receiver
   * thread, which runs in the main thread.
   */
  for (thr_no = 0; thr_no < num_threads; thr_no++)
  {
    rep->m_thread[thr_no].m_thr_data.m_time = NdbTick_CurrentMillisecond();

    if (thr_no == first_receiver_thread_no)
      continue;                 // Will run in the main thread.

    /*
     * The NdbThread_Create() takes void **, but that is cast to void * when
     * passed to the thread function. Which is kind of strange ...
     */
    if (thr_no < first_receiver_thread_no)
    {
      /* Start block threads */
      rep->m_thread[thr_no].m_thr_data.m_thread =
        NdbThread_Create(mt_job_thread_main,
                         (void **)(rep->m_thread + thr_no),
                         1024*1024,
                         "execute thread", //ToDo add number
                         NDB_THREAD_PRIO_MEAN);
      require(rep->m_thread[thr_no].m_thr_data.m_thread != NULL);
    }
    else
    {
      /* Start a receiver thread, also block thread for TRPMAN */
      rep->m_thread[thr_no].m_thr_data.m_thread =
        NdbThread_Create(mt_receiver_thread_main,
                         (void **)(&rep->m_thread[thr_no].m_thr_data),
                         1024*1024,
                         "receive thread", //ToDo add number
                         NDB_THREAD_PRIO_MEAN);
      require(rep->m_thread[thr_no].m_thr_data.m_thread != NULL);
    }
  }

  /* Now run the main loop for first receiver thread directly. */
  rep->m_thread[first_receiver_thread_no].m_thr_data.m_thread = pThis;
  mt_receiver_thread_main(&(rep->m_thread[first_receiver_thread_no].m_thr_data));

  /* Wait for all threads to shutdown. */
  for (thr_no = 0; thr_no < num_threads; thr_no++)
  {
    if (thr_no == first_receiver_thread_no)
      continue;
    void *dummy_return_status;
    NdbThread_WaitFor(rep->m_thread[thr_no].m_thr_data.m_thread,
                      &dummy_return_status);
    NdbThread_Destroy(&(rep->m_thread[thr_no].m_thr_data.m_thread));
  }

  /* Delete send threads, includes waiting for threads to shutdown */
  if (g_send_threads)
  {
    delete g_send_threads;
  }
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

/*
 * Compare signal ids, taking into account overflow/wrapover.
 * Return same as strcmp().
 * Eg.
 *   wrap_compare(0x10,0x20) -> -1
 *   wrap_compare(0x10,0xffffff20) -> 1
 *   wrap_compare(0xffffff80,0xffffff20) -> 1
 *   wrap_compare(0x7fffffff, 0x80000001) -> -1
 */
static
inline
int
wrap_compare(Uint32 a, Uint32 b)
{
  /* Avoid dependencies on undefined C/C++ interger overflow semantics. */
  if (a >= 0x80000000)
    if (b >= 0x80000000)
      return (int)(a & 0x7fffffff) - (int)(b & 0x7fffffff);
    else
      return (a - b) >= 0x80000000 ? -1 : 1;
  else
    if (b >= 0x80000000)
      return (b - a) >= 0x80000000 ? 1 : -1;
    else
      return (int)a - (int)b;
}

Uint32
FastScheduler::traceDumpGetNumThreads()
{
  /* The last thread is only for receiver -> no trace file. */
  return num_threads;
}

bool
FastScheduler::traceDumpGetJam(Uint32 thr_no, Uint32 & jamBlockNumber,
                               const Uint32 * & thrdTheEmulatedJam,
                               Uint32 & thrdTheEmulatedJamIndex)
{
  if (thr_no >= num_threads)
    return false;

#ifdef NO_EMULATED_JAM
  jamBlockNumber = 0;
  thrdTheEmulatedJam = NULL;
  thrdTheEmulatedJamIndex = 0;
#else
  const EmulatedJamBuffer *jamBuffer =
    &g_thr_repository.m_thread[thr_no].m_thr_data.m_jam;
  thrdTheEmulatedJam = jamBuffer->theEmulatedJam;
  thrdTheEmulatedJamIndex = jamBuffer->theEmulatedJamIndex;
  jamBlockNumber = jamBuffer->theEmulatedJamBlockNumber;
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
  void *value= NdbThread_GetTlsKey(NDB_THREAD_TLS_THREAD);
  const thr_data *selfptr = reinterpret_cast<const thr_data *>(value);
  /* The selfptr might be NULL, or pointer to thread that crashed. */

  Uint32 waitFor_count = 0;
  NdbMutex_Lock(&g_thr_repository.stop_for_crash_mutex);
  g_thr_repository.stopped_threads = 0;

  for (Uint32 thr_no = 0; thr_no < num_threads; thr_no++)
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
  NDB_TICKS start = NdbTick_CurrentMillisecond();
  while (g_thr_repository.stopped_threads < waitFor_count)
  {
    NdbCondition_WaitTimeout(&g_thr_repository.stop_for_crash_cond,
                             &g_thr_repository.stop_for_crash_mutex,
                             10);
    NDB_TICKS now = NdbTick_CurrentMillisecond();
    if (now > start + max_wait_seconds * 1000)
      break;                    // Give up
  }
  if (g_thr_repository.stopped_threads < waitFor_count)
  {
    if (nst != NST_ErrorInsert)
    {
      nst = NST_Watchdog; // Make this abort fast
    }
    ndbout_c("Warning: %d thread(s) did not stop before starting crash dump.",
             waitFor_count - g_thr_repository.stopped_threads);
  }
  NdbMutex_Unlock(&g_thr_repository.stop_for_crash_mutex);

  /* Now we are ready (or as ready as can be) for doing crash dump. */
}

void mt_execSTOP_FOR_CRASH()
{
  void *value= NdbThread_GetTlsKey(NDB_THREAD_TLS_THREAD);
  const thr_data *selfptr = reinterpret_cast<const thr_data *>(value);
  require(selfptr != NULL);

  NdbMutex_Lock(&g_thr_repository.stop_for_crash_mutex);
  g_thr_repository.stopped_threads++;
  NdbCondition_Signal(&g_thr_repository.stop_for_crash_cond);
  NdbMutex_Unlock(&g_thr_repository.stop_for_crash_mutex);

  /* ToDo: is this correct? */
  globalEmulatorData.theWatchDog->unregisterWatchedThread(selfptr->m_thr_no);

  pthread_exit(NULL);
}

void
FastScheduler::dumpSignalMemory(Uint32 thr_no, FILE* out)
{
  void *value= NdbThread_GetTlsKey(NDB_THREAD_TLS_THREAD);
  thr_data *selfptr = reinterpret_cast<thr_data *>(value);
  const thr_repository *rep = &g_thr_repository;
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

  const struct thr_data *thr_ptr = &rep->m_thread[thr_no].m_thr_data;
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

  /* Now pick out one signal at a time, in signal id order. */
  while (num_jbs > 0)
  {
    if (watchDogCounter)
      *watchDogCounter = 4;

    /* Search out the smallest signal id remaining. */
    Uint32 idx_min = 0;
    const Uint32 *p = jbs[idx_min].m_jb->m_data + jbs[idx_min].m_pos;
    const SignalHeader *s_min = reinterpret_cast<const SignalHeader*>(p);
    Uint32 sid_min = s_min->theSignalId;

    for (Uint32 i = 1; i < num_jbs; i++)
    {
      p = jbs[i].m_jb->m_data + jbs[i].m_pos;
      const SignalHeader *s = reinterpret_cast<const SignalHeader*>(p);
      Uint32 sid = s->theSignalId;
      if (wrap_compare(sid, sid_min) < 0)
      {
        idx_min = i;
        s_min = s;
        sid_min = sid;
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
    if (siglen > 25)
      siglen = 25;              // Sanity check
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
  void *value= NdbThread_GetTlsKey(NDB_THREAD_TLS_THREAD);
  const thr_data *selfptr = reinterpret_cast<const thr_data *>(value);

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
  lock(&(g_thr_repository.m_section_lock));
}

void
mt_section_unlock()
{
  unlock(&(g_thr_repository.m_section_lock));
}

void
mt_mem_manager_init()
{
}

void
mt_mem_manager_lock()
{
  lock(&(g_thr_repository.m_mem_manager_lock));
}

void
mt_mem_manager_unlock()
{
  unlock(&(g_thr_repository.m_mem_manager_lock));
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

Uint32
mt_get_thread_references_for_blocks(const Uint32 blocks[], Uint32 threadId,
                                    Uint32 dst[], Uint32 len)
{
  Uint32 cnt = 0;
  Bitmask<(MAX_BLOCK_THREADS+31)/32> mask;
  mask.set(threadId);
  for (Uint32 i = 0; blocks[i] != 0; i++)
  {
    Uint32 block = blocks[i];
    /**
     * Find each thread that has instance of block
     */
    assert(block == blockToMain(block));
    Uint32 index = block - MIN_BLOCK_NO;
    for (Uint32 instance = 0; instance < NDB_ARRAY_SIZE(thr_map[instance]); instance++)
    {
      Uint32 thr_no = thr_map[index][instance].thr_no;
      if (thr_no == thr_map_entry::NULL_THR_NO)
        break;

      if (mask.get(thr_no))
        continue;

      mask.set(thr_no);
      require(cnt < len);
      dst[cnt++] = numberToRef(block, instance, 0);
    }
  }
  return cnt;
}

void
mt_wakeup(class SimulatedBlock* block)
{
  Uint32 thr_no = block->getThreadId();
  struct thr_data *thrptr = &g_thr_repository.m_thread[thr_no].m_thr_data;
  wakeup(&thrptr->m_waiter);
}

#ifdef VM_TRACE
void
mt_assert_own_thread(SimulatedBlock* block)
{
  Uint32 thr_no = block->getThreadId();
  struct thr_data *thrptr = &g_thr_repository.m_thread[thr_no].m_thr_data;

  if (unlikely(pthread_equal(thrptr->m_thr_id, pthread_self()) == 0))
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
  struct thr_data *thr_ptr = &g_thr_repository.m_thread[thr_no].m_thr_data;

  for (Uint32 i = 0; i < thr_ptr->m_instance_count; i++)
  {
    arr[i] = thr_ptr->m_instance_list[i];
  }

  return thr_ptr->m_instance_count;
}

void
mt_get_thr_stat(class SimulatedBlock * block, ndb_thr_stat* dst)
{
  bzero(dst, sizeof(* dst));
  Uint32 thr_no = block->getThreadId();
  struct thr_data *selfptr = &g_thr_repository.m_thread[thr_no].m_thr_data;

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

/**
 * Global data
 */
struct thr_repository g_thr_repository;

struct trp_callback g_trp_callback;

TransporterRegistry globalTransporterRegistry(&g_trp_callback, NULL,
                                              false);
