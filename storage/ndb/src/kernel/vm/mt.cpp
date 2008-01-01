#include <sys/uio.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include <errno.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <VMSignal.hpp>
#include <kernel_types.h>
#include <Prio.hpp>
#include <SignalLoggerManager.hpp>
#include <SimulatedBlock.hpp>
#include <ErrorHandlingMacros.hpp>
#include <GlobalData.hpp>
#include <WatchDog.hpp>
#include <TransporterDefinitions.hpp>
#include "FastScheduler.hpp"
#include "mt.hpp"
#include <DebuggerNames.hpp>
#include <signaldata/StopForCrash.hpp>

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

//#define NDB_MT_LOCK_TO_CPU

static const Uint32 NUM_THREADS = 3;
static const Uint32 RECEIVER_THREAD_NO = 2;
#define MAX_THREADS 4

#ifdef NDBMTD_X86
static inline
int
xcng(volatile unsigned * addr, int val)
{
  asm volatile ("xchg %0, %1;" : "+r" (val) , "+m" (*addr));
  return val;
}

/**
 * from ?md/?ntel manual "spinlock howto"
 */
static
inline
void
cpu_pause()
{
  asm volatile ("rep;nop");
}

/* Memory barriers, these definitions are for x64_64. */
#define mb() 	asm volatile("mfence":::"memory")
/* According to Intel docs, it does not reorder loads. */
//#define rmb()	asm volatile("lfence":::"memory")
#define rmb()	asm volatile("" ::: "memory")
#define wmb()	asm volatile("" ::: "memory")
#define read_barrier_depends()	do {} while(0)
#else
#error "Unsupported architecture"
#endif

#ifdef HAVE_LINUX_FUTEX
#define USE_FUTEX
#endif

#ifdef USE_FUTEX
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <linux/futex.h>

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
  enum {
    FS_RUNNING = 0,
    FS_SLEEPING = 1,
  };
  thr_wait() { xcng(&m_futex_state, FS_RUNNING);}
};

/**
 * Sleep until woken up or timeout occurs.
 *
 * Will call check_callback(check_arg) after proper synchronisation, and only
 * if that returns true will it actually sleep, else it will return
 * immediately. This is needed to avoid races with wakeup.
 */
static
void
yield(struct thr_wait* wait, const struct timespec *timeout,
      bool (*check_callback)(void *), void *check_arg)
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

  if ((*check_callback)(check_arg))
    futex_wait(val, thr_wait::FS_SLEEPING, timeout);
  xcng(val, thr_wait::FS_RUNNING);
}

static
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
  thr_wait() {
    m_mutex = NdbMutex_Create();
    m_cond = NdbCondition_Create();
  }
};

static
void
yield(struct thr_wait* wait, const struct timespec *timeout,
      bool (*check_callback)(void *), void *check_arg)
{
  Uint32 msec = 
    (1000 * timeout->tv_sec) + 
    (timeout->tv_nsec / 1000000);
  NdbMutex_Lock(wait->m_mutex);
  if ((*check_callback)(check_arg))
    NdbCondition_WaitTimeout(wait->m_cond, wait->m_mutex, msec);
  NdbMutex_Unlock(wait->m_mutex);
}

static
int
wakeup(struct thr_wait* wait)
{
  NdbMutex_Lock(wait->m_mutex);
  NdbCondition_Signal(wait->m_cond);
  NdbMutex_Unlock(wait->m_mutex);
  return 0;
}
#endif

inline void require(bool x)
{
  if (unlikely(!(x)))
    abort();
}

struct thr_spin_lock
{
  thr_spin_lock(const char * name = 0)
  {
    m_lock = 0;
    m_name = name;
    m_contended_count = 0;
  }

  const char * m_name;
  Uint32 m_contended_count;
  volatile Uint32 m_lock;
};

struct thr_mutex
{
  thr_mutex(const char * name) {
    m_mutex = NdbMutex_Create();
    m_name = name;
  }

  const char * m_name;
  NdbMutex * m_mutex;
};

static
inline
void
lock(struct thr_spin_lock* sl)
{
  volatile unsigned* val = &sl->m_lock;
test:
  if (likely(xcng(val, 1) == 0))
    return;

  /*
   * There is a race conditions here on m_contended_count. But it doesn't
   * really matter if the counts are not 100% accurate
   */
  Uint32 count = sl->m_contended_count++;
  Uint32 freq = (count > 10000 ? 5000 : (count > 20 ? 200 : 1));
  if ((count % freq) == 0)
    printf("%s waiting for lock, contentions~=%u\n", sl->m_name, count);

  do {
    cpu_pause();
  } while (* val == 1);

  goto test;
}

static
inline
void
unlock(struct thr_spin_lock* sl)
{
  /**
   * Memory barrier here, to make sure all of our stores are visible before
   * the lock release is.
   */
  mb();
  sl->m_lock = 0;
}

static
inline
int
trylock(struct thr_spin_lock* sl)
{
  volatile unsigned* val = &sl->m_lock;
  return xcng(val, 1);
}

static
inline
void
lock(struct thr_mutex* sl)
{
  NdbMutex_Lock(sl->m_mutex);
}

static
inline
void
unlock(struct thr_mutex* sl)
{
  NdbMutex_Unlock(sl->m_mutex);
}

static
inline
int
trylock(struct thr_mutex * sl)
{
  return NdbMutex_Trylock(sl->m_mutex);
}

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
  Uint32 m_data[SIZE];
};  

struct thr_job_queue
{
  static const unsigned SIZE = 30;

  unsigned m_read_index; // Read/written by consumer, read by producer
  unsigned m_write_index; // Read/written by producer, read by consumer
  struct thr_job_buffer* m_buffers[SIZE];
};

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
  Uint32 m_write_index;
  Uint32 m_write_pos;
};

struct thr_tq
{
  static const unsigned SQ_SIZE = 512;
  static const unsigned LQ_SIZE = 512;
  static const unsigned PAGES = 32 * (SQ_SIZE + LQ_SIZE) / 8192;
  
  Uint32 m_next_timer;
  Uint32 m_current_time;
  Uint32 m_next_free;
  Uint32 m_cnt[2];
  Uint32 * m_delayed_signals[PAGES];
  Uint32 m_short_queue[SQ_SIZE];
  Uint32 m_long_queue[LQ_SIZE];
};

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

struct thr_data
{
  thr_data() : m_jba_write_lock("jbalock") {}

  thr_wait m_waiter;
  unsigned m_thr_no;
  struct SimulatedBlock* m_blocks[NO_OF_BLOCKS];
  
  Uint64 m_time;
  struct thr_tq m_tq;

  /* Prio A signal incoming queue. */
  struct thr_job_queue m_jba;
  struct thr_spin_lock m_jba_write_lock;
  /*
   * In m_next_buffer we keep a free buffer at all times, so that when
   * we hold the lock and find we need a new buffer, we can use this and this
   * way defer allocation to after releasing the lock.
   */
  struct thr_job_buffer* m_next_buffer;
  /* Thread-local read state of prio A buffer. */
  struct thr_jb_read_state m_jba_read_state;
  /*
   * There is no m_jba_write_state, as we have multiple writers to the prio A
   * queue, so local state becomes invalid as soon as we release the lock.
   */

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

  /*
   * These are the thread input queues, where other threads deliver signals
   * into.
   */
  struct thr_job_queue m_in_queue[MAX_THREADS];
  /* These are the write states of m_in_queue[self] in each thread. */
  struct thr_jb_write_state m_write_states[MAX_THREADS];
  /* These are the read states of all of our own m_in_queue[]. */
  struct thr_jb_read_state m_read_states[MAX_THREADS];

  /* Jam buffers for making trace files at crashes. */
  EmulatedJamBuffer *m_jam;
  /* Watchdog counter for this thread. */
  Uint32 *m_watchdog_counter;
  /* Signal delivery statistics. */
  Uint32 m_prioa_count;
  Uint32 m_prioa_size;
  Uint32 m_priob_count;
  Uint32 m_priob_size;
  /* Array of node ids with pending remote send data. */
  Uint8 m_pending_send_nodes[MAX_NTRANSPORTERS];
  /* Number of node ids in m_pending_send_nodes. */
  Uint32 m_pending_send_count;
  /**
   * Bitmap of pending node ids with send data.
   * Used to quickly check if a node id is already in m_pending_send_nodes.
   */
  Bitmask<(MAX_NTRANSPORTERS+31)/32> m_pending_send_mask;
};

template<typename T>
struct thr_safe_pool
{
  thr_safe_pool() : m_lock("mempool"), m_free_list(0) {}

  thr_spin_lock m_lock;
  T* m_free_list;
  Ndbd_mem_manager *m_mm;

  T* seize() {
    T* ret = 0;
    lock(&m_lock);
    if (m_free_list)
    {
      ret = m_free_list;
      m_free_list = *reinterpret_cast<T**>(m_free_list);
      unlock(&m_lock);
    }
    else
    {
      Uint32 dummy;
      unlock(&m_lock);
      ret = reinterpret_cast<T*>(m_mm->alloc_page(RT_JOB_BUFFER, &dummy));
      // ToDo: How to deal with failed allocation?!?
      // I think in this case we need to start grabbing buffers kept for signal
      // trace.
    }
    return ret;
  }

  void release(T* t){
    lock(&m_lock);
    T** nextptr = reinterpret_cast<T**>(t);
    * nextptr = m_free_list;
    m_free_list = t;
    unlock(&m_lock);
  }
};

struct thr_repository
{
  thr_repository()
    : m_receive_lock("recvlock"),
      m_section_lock("sectionlock"),
      m_mem_manager_lock("memmanagerlock") {}

  unsigned m_thread_count;
  struct thr_spin_lock m_receive_lock;
  struct thr_spin_lock m_section_lock;
  struct thr_spin_lock m_mem_manager_lock;
  struct thr_data m_thread[MAX_THREADS];
  struct thr_safe_pool<thr_job_buffer> m_free_list;

  /*
   * These are used to synchronize during crash / trace dumps.
   *
   * ToDo: Replace pthread stuff with portable wrappers in portlib.
   */
  pthread_mutex_t stop_for_crash_mutex;
  pthread_cond_t stop_for_crash_cond;
  Uint32 stopped_threads;

  /**
   * Send locks for the transporters, one per possible remote node.
   */
  thr_spin_lock m_send_locks[MAX_NTRANSPORTERS];
};

static
thr_job_buffer*
seize_buffer(struct thr_repository* rep, int thr_no, bool prioa)
{
  thr_job_buffer* jb;
  thr_data* selfptr = rep->m_thread + thr_no;
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
    Uint32 batch = THR_FREE_BUF_MAX / THR_FREE_BUF_BATCH;
    assert(batch > 0);
    assert(batch + THR_FREE_BUF_MIN < THR_FREE_BUF_MAX);
    do {
      jb = rep->m_free_list.seize();
      jb->m_len = 0;
      jb->m_prioa = false;
      first_free = (first_free ? first_free : THR_FREE_BUF_MAX) - 1;
      selfptr->m_free_fifo[first_free] = jb;
      batch--;
    } while (batch > 0);
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
  struct thr_data* selfptr = rep->m_thread + thr_no;
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
      rep->m_free_list.release(selfptr->m_free_fifo[first_free]);
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
      sendprioa(thr_no, s->theReceiversBlockNumber, s, data,
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
scan_time_queues(struct thr_data* selfptr)
{
  struct thr_tq * tq = &selfptr->m_tq;
  NDB_TICKS now = NdbTick_CurrentMillisecond();
  NDB_TICKS last = selfptr->m_time;

  Uint32 curr = tq->m_current_time;
  Uint32 cnt0 = tq->m_cnt[0];
  Uint32 cnt1 = tq->m_cnt[1];

  Uint64 diff = now - last;
  if (diff == 0)
  {
    return;
  }
  else if (diff > 0)
  {
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
    
    return;
  }
  else if (diff == 0)
  {
    return;
  }
  abort();
}

/*
 * Flush the write state to the job queue, making any new signals available to
 * receiving threads.
 */
static inline
void
flush_write_state(Uint32 dst, thr_job_queue *q, thr_jb_write_state *w)
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
  q->m_write_index = w->m_write_index;
  w->m_pending_signals = 0;

  wakeup(&(g_thr_repository.m_thread[dst].m_waiter));
}

static
void
flush_jbb_write_state(thr_data *selfptr)
{
  Uint32 thr_count = g_thr_repository.m_thread_count;
  Uint32 self = selfptr->m_thr_no;

  for (Uint32 thr_no = 0; thr_no < thr_count; thr_no ++)
  {
    thr_jb_write_state *w = selfptr->m_write_states + thr_no;
    if (w->m_pending_signals > 0)
    {
      thr_job_queue *q = g_thr_repository.m_thread[thr_no].m_in_queue + self;
      flush_write_state(thr_no, q, w);
    }
  }
}

Uint32 senderThreadId;

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

/**
 * Send any pending data to remote nodes.
 *
 * If MUST_SEND is false, will only try to lock the send lock, but if it would
 * block, that node is skipped, to be tried again next time round.
 *
 * If MUST_SEND is true, will always take the lock, waiting on it if needed.
 *
 * Currently, the list of pending nodes to send to is thread-local, but the
 * per-node send buffer is shared by all threads. Thus we might skip a node
 * for which another thread has pending send data, and we might send pending
 * data also for another thread without clearing the node from the pending
 * list of that other thread (but we will never loose signals due to this).
 *
 * Later, when we might move to per-thread-per-node send buffers, eliminating
 * these issues and significantly reducing the send lock contentions.
 */
static
void
do_send(struct thr_repository* rep, struct thr_data* selfptr,
        Uint32 *watchDogCounter, bool must_send)
{
  Uint32 i;
  Uint32 count = selfptr->m_pending_send_count;
  Uint8 *nodes = selfptr->m_pending_send_nodes;

  if (count == 0)
    return;
  /* Clear the pending list. */
  selfptr->m_pending_send_mask.clear();
  selfptr->m_pending_send_count = 0;

  for (i = 0; i < count; i++)
  {
    NodeId nodeId = nodes[i];
    *watchDogCounter = 6;
    if (must_send)
      lock(&rep->m_send_locks[nodeId]);
    else if (trylock(&rep->m_send_locks[nodeId]) != 0)
    {
      /**
       * Not doing this node now, re-add to pending list.
       *
       * As we only add from the start of an empty list, we are safe from
       * overwriting the list while we are iterating over it.
       */
      register_pending_send(selfptr, nodeId);
      continue;
    }

    /**
     * The senderThreadId global is set so that TransporterCallback can know
     * which thread queue to use to deliver local signals for transporter
     * state change signals.
     */
    senderThreadId = selfptr->m_thr_no;
    globalTransporterRegistry.performSend(nodeId);
    unlock(&rep->m_send_locks[nodeId]);
  }
}

static
inline
void
sendpacked(struct thr_data* selfptr, Signal* signal, Uint32 thr_no)
{
  SimulatedBlock** blockptr = selfptr->m_blocks - MIN_BLOCK_NO;
  SimulatedBlock* b_lqh = * (blockptr + DBLQH);
  SimulatedBlock* b_tc = * (blockptr + DBTC);
  SimulatedBlock* b_tup = * (blockptr + DBTUP);
  if (b_lqh)
    b_lqh->executeFunction(GSN_SEND_PACKED, signal);
  if (b_tc)
    b_tc->executeFunction(GSN_SEND_PACKED, signal);
  if (b_tup)
    b_tup->executeFunction(GSN_SEND_PACKED, signal);
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
insert_signal(thr_job_queue *q, thr_jb_write_state *w, Uint32 prioa,
              const SignalHeader* sh, const Uint32 *data,
              const Uint32 secPtr[3], thr_job_buffer *new_buffer)
{
  Uint32 write_pos = w->m_write_pos;
  Uint32 datalen = sh->theLength;
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
    /*
     * Full job buffer is fatal.
     *
     * ToDo: should we wait for it to become non-full? There is no guarantee
     * that this will actually happen...
     *
     * Or alternatively, ndbrequire() ?
     */
    assert(write_index != q->m_read_index);
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
  for (Uint32 i = 0; i < count; i++)
  {
    thr_jb_read_state *r = selfptr->m_read_states + i;
    const thr_job_queue *q = selfptr->m_in_queue +i;
    Uint32 index = q->m_write_index;
    r->m_write_index = index;
    read_barrier_depends();
    r->m_write_pos = q->m_buffers[index]->m_len;
  }
}

static
void
read_jba_state(thr_data *selfptr)
{
  const thr_job_queue *jba = &(selfptr->m_jba);
  Uint32 index = jba->m_write_index;
  selfptr->m_jba_read_state.m_write_index = index;
  read_barrier_depends();
  selfptr->m_jba_read_state.m_write_pos = jba->m_buffers[index]->m_len;
}

/* Check all job queues, return true only if all are empty. */
static bool
check_queues_empty(void *data)
{
  Uint32 thr_count = g_thr_repository.m_thread_count;
  thr_data *selfptr = reinterpret_cast<thr_data *>(data);

  read_jbb_state(selfptr, thr_count);
  read_jba_state(selfptr);
  const thr_jb_read_state *r = &(selfptr->m_jba_read_state);
  if (r->m_read_index < r->m_write_index || r->m_read_pos < r->m_write_pos)
    return false;
  for (Uint32 i = 0; i < thr_count; i++)
  {
    r = selfptr->m_read_states + i;;
    if (r->m_read_index < r->m_write_index || r->m_read_pos < r->m_write_pos)
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
execute_signals(thr_data *selfptr, thr_job_queue *q, thr_jb_read_state *r,
                Signal *sig, Uint32 max_signals,
                Uint32 *watchDogCounter, Uint32 *signalIdCounter)
{
  Uint32 num_signals = 0;

  Uint32 read_index = r->m_read_index;
  Uint32 write_index = r->m_write_index;
  Uint32 read_pos = r->m_read_pos;
  Uint32 write_pos = (read_index == write_index ?
                      r->m_write_pos :
                      q->m_buffers[read_index]->m_len);
  thr_job_buffer *read_buffer = r->m_read_buffer;
  SimulatedBlock** blockptr = selfptr->m_blocks - MIN_BLOCK_NO;

  while (num_signals < max_signals)
  {
    while (read_pos >= write_pos)
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
        write_pos = (read_index == write_index ?
                     r->m_write_pos :
                     q->m_buffers[read_index]->m_len);
        /* Update thread-local read state. */
        r->m_read_index = q->m_read_index = read_index;
        r->m_read_buffer = read_buffer;
        r->m_read_pos = read_pos;
      }
    }

    /*
     * These pre-fetching were found using OProfile to reduce cache misses.
     * (Though on Intel Core 2, they do not give much speedup, as apparently
     * the hardware prefetcher is already doing a fairly good job).
     */
    __builtin_prefetch (read_buffer->m_data + read_pos + 16, 0, 3);
    __builtin_prefetch ((Uint32 *)&sig->header + 16, 1, 3);

    /* Now execute the signal. */
    SignalHeader* s =
      reinterpret_cast<SignalHeader*>(read_buffer->m_data + read_pos);
    Uint32 seccnt = s->m_noOfSections;
    Uint32 siglen = (sizeof(*s)>>2) + s->theLength;
    if(siglen>16)
      __builtin_prefetch (read_buffer->m_data + read_pos + 32, 0, 3);
    Uint32 bno = s->theReceiversBlockNumber;
    Uint32 gsn = s->theVerId_signalNumber;
    SimulatedBlock * block = blockptr[bno];
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

    block->executeFunction(gsn, sig);

    num_signals++;
  }

  return num_signals;
}

static
Uint32
run_job_buffers(thr_data *selfptr, Signal *sig,
                Uint32 *watchDogCounter, Uint32 *signalIdCounter)
{
  Uint32 thr_count = g_thr_repository.m_thread_count;
  Uint32 signal_count = 0;

  read_jbb_state(selfptr, thr_count);
  /*
   * A load memory barrier to ensure that we see any prio A signal sent later
   * than loaded prio B signals.
   */
  rmb();

  for (Uint32 send_thr_no = 0; send_thr_no < thr_count; send_thr_no++)
  {
    /* Read the prio A state often, to avoid starvation of prio A. */
    read_jba_state(selfptr);
    static Uint32 max_prioA = thr_job_queue::SIZE * thr_job_buffer::SIZE;
    signal_count += execute_signals(selfptr, &(selfptr->m_jba),
                                    &(selfptr->m_jba_read_state), sig,
                                    max_prioA, watchDogCounter,
                                    signalIdCounter);

    /* Now execute prio B signals from one thread. */
    thr_job_queue *queue = selfptr->m_in_queue + send_thr_no;
    thr_jb_read_state *read_state = selfptr->m_read_states + send_thr_no;
    signal_count += execute_signals(selfptr, queue, read_state,
                                    sig, MAX_SIGNALS_PER_JB,
                                    watchDogCounter, signalIdCounter);
  }

  return signal_count;
}

static inline Uint32
block2ThreadId(Uint32 block)
{
  /*
   * Block assignment:
   *    0 BACKUP            LOCAL
   *    1 DBTC      GLOBAL
   *    2 DBDIH     GLOBAL
   *    3 DBLQH             LOCAL
   *    4 DBACC             LOCAL
   *    5 DBTUP             LOCAL
   *    6 DBDICT    GLOBAL
   *    7 NDBCNTR   GLOBAL
   *    8 QMGR      GLOBAL
   *    9 NDBFS     GLOBAL
   *   10 CMVMI     GLOBAL
   *   11 TRIX      GLOBAL
   *   12 DBUTIL    GLOBAL
   *   13 SUMA              LOCAL
   *   14 DBTUX             LOCAL
   *   15 TSMAN             LOCAL
   *   16 LGMAN             LOCAL
   *   17 PGMAN             LOCAL
   *   18 RESTORE           LOCAL
   *
   * Thread 0 is for global, thread 1 for locals.
   */
  static const Uint32 locals = 0x7e039;
  assert(block >= MIN_BLOCK_NO && block <= MAX_BLOCK_NO);
  return (locals >> (block - MIN_BLOCK_NO)) & 1;
}

static void reportSignalStats(Uint32 self, Uint32 a_count, Uint32 a_size,
                              Uint32 b_count, Uint32 b_size)
{
  SignalT<6> sT;
  Signal *s= (Signal *)&sT;

  memset(&s->header, 0, sizeof(s->header));
  s->header.theLength = 6;
  s->header.theSendersSignalId = 0;
  s->header.theSendersBlockRef = numberToRef(0, 0);
  s->header.theVerId_signalNumber = GSN_EVENT_REP;
  s->header.theReceiversBlockNumber = CMVMI;
  s->theData[0] = NDB_LE_MTSignalStatistics;
  s->theData[1] = self;
  s->theData[2] = a_count;
  s->theData[3] = a_size;
  s->theData[4] = b_count;
  s->theData[5] = b_size;
  /* ToDo: need this really be prio A like in old code? */
  sendlocal(self, s->header.theReceiversBlockNumber, &s->header, s->theData,
            NULL);
}

static inline void
update_sched_stats(thr_data *selfptr)
{
  if(selfptr->m_prioa_count + selfptr->m_priob_count >= 2000000)
  {
    reportSignalStats(selfptr->m_thr_no,
                      selfptr->m_prioa_count,
                      selfptr->m_prioa_size,
                      selfptr->m_priob_count,
                      selfptr->m_priob_size);
    selfptr->m_prioa_count = 0;
    selfptr->m_prioa_size = 0;
    selfptr->m_priob_count = 0;
    selfptr->m_priob_size = 0;
  }
}

static void
init_thread(thr_data *selfptr, EmulatedJamBuffer *jam, Uint32 *watchDogCounter)
{
  jam->theEmulatedJamIndex = 0;
  jam->theEmulatedJamBlockNumber = 0;
  memset(jam->theEmulatedJam, 0, sizeof(jam->theEmulatedJam));
  NdbThread_SetTlsKey(NDB_THREAD_TLS_JAM, jam);
  selfptr->m_jam = jam;

  selfptr->m_watchdog_counter = watchDogCounter;
  unsigned thr_no = selfptr->m_thr_no;
  globalEmulatorData.theWatchDog->registerWatchedThread(watchDogCounter,
                                                        thr_no);

  NdbThread_SetTlsKey(NDB_THREAD_TLS_THREAD, selfptr);

#ifdef NDB_MT_LOCK_TO_CPU
  pid_t tid = (unsigned)syscall(SYS_gettid);
  ndbout_c("Tread %u started, tid=%u", thr_no, tid);
  uint cpu_no = 1 + (thr_no % 3);
  cpu_no = (cpu_no >= 2 ? 5 - cpu_no : cpu_no);
  ndbout_c("lock to cpu %u", cpu_no);
  {
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(cpu_no, &mask);
    sched_setaffinity(tid, sizeof(mask), &mask);
  }
#endif
}

Uint32 receiverThreadId;

/*
 * We only do receive in thread 2, which _only_ does receive.
 *
 * Otherwise we have a problem waking up a thread that is sleeping in
 * the transporter
 */
extern "C"
void *
mt_receiver_thread_main(void *thr_arg)
{
  struct thr_repository* rep = &g_thr_repository;
  struct thr_data* selfptr = (struct thr_data *)thr_arg;
  unsigned thr_no = selfptr->m_thr_no;
  EmulatedJamBuffer thread_jam;
  Uint32 watchDogCounter;

  init_thread(selfptr, &thread_jam, &watchDogCounter);
  receiverThreadId = thr_no;

  while (globalData.theRestartFlag != perform_stop)
  { 
    static int cnt = 0;

    update_sched_stats(selfptr);

    if (cnt == 0)
    {
      watchDogCounter = 5;
      globalTransporterRegistry.update_connections();
    }
    cnt = (cnt + 1) & 15;

    watchDogCounter = 7;

    if (globalTransporterRegistry.pollReceive(1))
    {
      watchDogCounter = 8;
      lock(&rep->m_receive_lock);
      globalTransporterRegistry.performReceive();
      unlock(&rep->m_receive_lock);
    }

    flush_jbb_write_state(selfptr);
  }

  globalEmulatorData.theWatchDog->unregisterWatchedThread(thr_no);
  return NULL;                  // Return value not currently used
}

extern "C"
void *
mt_job_thread_main(void *thr_arg)
{
  unsigned char signal_buf[sizeof(Signal) + 63 + 256 * MAX_THREADS];
  Signal *signal;
  struct timespec nowait;
  nowait.tv_sec = 0;
  nowait.tv_nsec = 10 * 1000000;
  EmulatedJamBuffer thread_jam;
  Uint32 watchDogCounter;
  Uint32 thrSignalId = 0;

  struct thr_repository* rep = &g_thr_repository;
  struct thr_data* selfptr = (struct thr_data *)thr_arg;
  init_thread(selfptr, &thread_jam, &watchDogCounter);

  unsigned thr_no = selfptr->m_thr_no;
  /*
   * Align signal buffer for better cache performance.
   * Also skew it a litte for each thread to avoid cache pollution.
   */
  UintPtr sigtmp= (UintPtr)signal_buf;
  sigtmp= (sigtmp+63) & (~(UintPtr)63);
  sigtmp+= thr_no*256;
  signal = (Signal *)sigtmp;

  /*
   * Now we need to somehow assign to this thread all blocks that will run in
   * this thread.
   */
  for (Uint32 i = 0; i < NO_OF_BLOCKS; i++)
  {
    if (block2ThreadId(i + MIN_BLOCK_NO) == thr_no)
    {
      require(selfptr->m_blocks[i] != 0);
      selfptr->m_blocks[i]->assignToThread(thr_no, &thread_jam,
                                           &watchDogCounter);
    }
  }

  /* Avoid false watchdog alarms caused by race condition. */
  watchDogCounter = 1;

  Uint32 send_sum = 0;
  while (globalData.theRestartFlag != perform_stop)
  { 
    update_sched_stats(selfptr);

    watchDogCounter = 2;
    scan_time_queues(selfptr);

    Uint32 sum = run_job_buffers(selfptr, signal,
                                 &watchDogCounter, &thrSignalId);
    
    watchDogCounter = 1;
    sendpacked(selfptr, signal, thr_no);
    
    if (sum)
    {
      watchDogCounter = 6;
      flush_jbb_write_state(selfptr);
    }

    send_sum += sum;

    if (send_sum > 0)
    {
      if (sum == 0)
      {
        /* About to sleep, _must_ send now. */
        do_send(rep, selfptr, &watchDogCounter, TRUE);
        send_sum = 0;
      }
      else if (send_sum > MAX_SIGNALS_BEFORE_SEND)
      {
        /* Try to send, but skip for now in case of lock contention. */
        do_send(rep, selfptr, &watchDogCounter, FALSE);
        send_sum = 0;
      }
    }
    
    if (sum == 0)
    {
      yield(&selfptr->m_waiter, &nowait, check_queues_empty, selfptr);
    }
  }

  globalEmulatorData.theWatchDog->unregisterWatchedThread(thr_no);
  return NULL;                  // Return value not currently used
}

void
sendlocal(Uint32 self, Uint32 block, const SignalHeader *s, const Uint32 *data,
          const Uint32 secPtr[3])
{
  /*
   * Max number of signals to put into job buffer before flushing the buffer
   * to the other thread.
   * This parameter found to be reasonable by benchmarking.
   */
  Uint32 MAX_SIGNALS_BEFORE_FLUSH = (self == RECEIVER_THREAD_NO ? 2 : 20);
  Uint32 dst = block2ThreadId(block);
  struct thr_repository* rep = &g_thr_repository;
  struct thr_data * selfptr = rep->m_thread + self;

  selfptr->m_priob_count++;
  Uint32 siglen = (sizeof(*s) >> 2) + s->theLength + s->m_noOfSections;
  selfptr->m_priob_size += siglen;

  thr_job_queue *q = rep->m_thread[dst].m_in_queue + self;
  thr_jb_write_state *w = selfptr->m_write_states + dst;
  if (insert_signal(q, w, false, s, data, secPtr, selfptr->m_next_buffer))
  {
    selfptr->m_next_buffer = seize_buffer(rep, self, false);
  }

  if (w->m_pending_signals >= MAX_SIGNALS_BEFORE_FLUSH)
    flush_write_state(dst, q, w);
}

void
sendprioa(Uint32 self, Uint32 block, const SignalHeader *s, const uint32 *data,
          const Uint32 secPtr[3])
{
  Uint32 dst = block2ThreadId(block);
  struct thr_repository* rep = &g_thr_repository;
  struct thr_data * selfptr = rep->m_thread + self;
  struct thr_data *dstptr = rep->m_thread + dst;

  selfptr->m_prioa_count++;
  Uint32 siglen = (sizeof(*s) >> 2) + s->theLength + s->m_noOfSections;
  selfptr->m_prioa_size += siglen;  

  thr_job_queue *q = &(dstptr->m_jba);
  thr_jb_write_state w;

  lock(&dstptr->m_jba_write_lock);

  Uint32 index = q->m_write_index;
  w.m_write_index = index;
  thr_job_buffer *buffer = q->m_buffers[index];
  w.m_write_buffer = buffer;
  w.m_write_pos = buffer->m_len;
  w.m_pending_signals = 0;
  bool buf_used = insert_signal(q, &w, true, s, data, secPtr,
                                selfptr->m_next_buffer);
  flush_write_state(dst, q, &w);

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
  thr_data *selfptr = rep->m_thread + self;
  SendStatus ss;

  register_pending_send(selfptr, nodeId);
  lock(&rep->m_send_locks[nodeId]);
  ss = globalTransporterRegistry.prepareSend(sh, prio, data, nodeId, ptr);
  unlock(&rep->m_send_locks[nodeId]);
  return ss;
}

SendStatus
mt_send_remote(Uint32 self, const SignalHeader *sh, Uint8 prio,
               const Uint32 *data, NodeId nodeId,
               class SectionSegmentPool *thePool,
               const SegmentedSectionPtr ptr[3])
{
  thr_repository *rep = &g_thr_repository;
  thr_data *selfptr = rep->m_thread + self;
  SendStatus ss;

  register_pending_send(selfptr, nodeId);
  lock(&rep->m_send_locks[nodeId]);
  ss = globalTransporterRegistry.prepareSend(sh, prio, data, nodeId,
                                             *thePool, ptr);
  unlock(&rep->m_send_locks[nodeId]);
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
sendprioa_STOP_FOR_CRASH(Uint32 dst)
{
  SignalT<StopForCrash::SignalLength> signalT;
  struct thr_repository* rep = &g_thr_repository;
  /* As this signal will be the last one executed by the other thread, it does
     not matter which buffer we use in case the current buffer is filled up by
     the STOP_FOR_CRASH signal; the data in it will never be read.
  */
  static thr_job_buffer dummy_buffer;

  /*
   * Currently we have two threads with fixed block assignment.
   * So we send STOP_FOR_CRASH to CMVMI for thread 0 (global) and to
   * DBLQH for thread 1 (local).
   */
  assert(dst == 0 || dst == 1); // ToDo when/if more threads.
  Uint32 bno = 0;
  if (dst == 0)
    bno = CMVMI;
  else if (dst == 1)
    bno = DBLQH;
  assert(block2ThreadId(bno) == dst);
  struct thr_data * dstptr = rep->m_thread + dst;

  memset(&signalT.header, 0, sizeof(SignalHeader));
  signalT.header.theVerId_signalNumber   = GSN_STOP_FOR_CRASH;
  signalT.header.theReceiversBlockNumber = bno;
  signalT.header.theSendersBlockRef      = 0;
  signalT.header.theTrace                = 0;
  signalT.header.theSendersSignalId      = 0;
  signalT.header.theSignalId             = 0;
  signalT.header.theLength               = StopForCrash::SignalLength;
  StopForCrash * const stopForCrash = (StopForCrash *)&signalT.theData[0];
  stopForCrash->flags = 0;

  thr_job_queue *q = &(dstptr->m_jba);
  thr_jb_write_state w;

  lock(&dstptr->m_jba_write_lock);

  Uint32 index = q->m_write_index;
  w.m_write_index = index;
  thr_job_buffer *buffer = q->m_buffers[index];
  w.m_write_buffer = buffer;
  w.m_write_pos = buffer->m_len;
  w.m_pending_signals = 0;
  insert_signal(q, &w, true, &signalT.header, signalT.theData, NULL,
                &dummy_buffer);
  flush_write_state(dst, q, &w);

  unlock(&dstptr->m_jba_write_lock);
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
}

void
senddelay(Uint32 thr_no, const SignalHeader* s, Uint32 delay)
{
  struct thr_repository* rep = &g_thr_repository;
  struct thr_data * selfptr = rep->m_thread + thr_no;
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
  selfptr->m_first_free = 0;
  selfptr->m_first_unused = 0;
  
  selfptr->m_jba.m_read_index = 0;
  selfptr->m_jba.m_write_index = 0;
  thr_job_buffer *buffer = seize_buffer(rep, thr_no, true);
  selfptr->m_jba.m_buffers[0] = buffer;
  selfptr->m_jba_read_state.m_read_index = 0;
  selfptr->m_jba_read_state.m_read_buffer = buffer;
  selfptr->m_jba_read_state.m_read_pos = 0;
  selfptr->m_jba_read_state.m_write_index = 0;
  selfptr->m_jba_read_state.m_write_pos = 0;
  selfptr->m_next_buffer = seize_buffer(rep, thr_no, false);
  
  for (i = 0; i<cnt; i++)
  {
    selfptr->m_in_queue[i].m_read_index = 0;
    selfptr->m_in_queue[i].m_write_index = 0;
    buffer = seize_buffer(rep, thr_no, false);
    selfptr->m_in_queue[i].m_buffers[0] = buffer;
    selfptr->m_read_states[i].m_read_index = 0;
    selfptr->m_read_states[i].m_read_buffer = buffer;
    selfptr->m_read_states[i].m_read_pos = 0;
    selfptr->m_read_states[i].m_write_index = 0;
    selfptr->m_read_states[i].m_write_pos = 0;
  }

  queue_init(&selfptr->m_tq);

  selfptr->m_jam = NULL;

  selfptr->m_prioa_count = 0;
  selfptr->m_prioa_size = 0;
  selfptr->m_priob_count = 0;
  selfptr->m_priob_size = 0;

  selfptr->m_pending_send_count = 0;
  selfptr->m_pending_send_mask.clear();
}

/* Have to do this after init of all m_in_queues is done. */
static
void
thr_init2(struct thr_repository* rep, struct thr_data *selfptr, unsigned int cnt,
          unsigned thr_no)
{
  for (Uint32 i = 0; i<cnt; i++)
  {
    selfptr->m_write_states[i].m_write_index = 0;
    selfptr->m_write_states[i].m_write_pos = 0;
    selfptr->m_write_states[i].m_write_buffer =
      rep->m_thread[i].m_in_queue[thr_no].m_buffers[0];
    selfptr->m_write_states[i].m_pending_signals = 0;
  }    
}

static
void
rep_init(struct thr_repository* rep, unsigned int cnt, Ndbd_mem_manager *mm)
{
  rep->m_free_list.m_mm = mm;

  rep->m_thread_count = cnt;
  for (unsigned int i = 0; i<cnt; i++)
  {
    thr_init(rep, rep->m_thread + i, cnt, i);
  }
  for (unsigned int i = 0; i<cnt; i++)
  {
    thr_init2(rep, rep->m_thread + i, cnt, i);
  }

  rep->stopped_threads = 0;
  pthread_mutex_init(&rep->stop_for_crash_mutex, NULL);
  pthread_cond_init(&rep->stop_for_crash_cond, NULL);

  for (int i = 0 ; i < MAX_NTRANSPORTERS; i++)
  {
    char buf[100];
    snprintf(buf, sizeof(buf), "send lock node %d", i);
    rep->m_send_locks[i].m_name = strdup(buf);
  }
}


/**
 * Thread Config
 */

#include "ThreadConfig.hpp"
#include <signaldata/StartOrd.hpp>

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
ThreadConfig::init(EmulatorData *emulatorData)
{
  ::rep_init(&g_thr_repository, NUM_THREADS, emulatorData->m_mem_manager);
}

void ThreadConfig::ipControlLoop(Uint32 thread_index)
{
  unsigned int i;
  unsigned int thr_no;
  struct thr_repository* rep = &g_thr_repository;
  NdbThread *threads[NUM_THREADS];

  /*
   * Start threads for all execution threads, except for the receiver
   * thread, which runs in the main thread.
   */
  for (thr_no = 0; thr_no < NUM_THREADS; thr_no++)
  {
    for (i = 0; i<NO_OF_BLOCKS; i++)
    {
      if (block2ThreadId(i + MIN_BLOCK_NO) == thr_no)
      {
        rep->m_thread[thr_no].m_blocks[i] =
          globalData.getBlock(i + MIN_BLOCK_NO);
      }
    }
    rep->m_thread[thr_no].m_time = NdbTick_CurrentMillisecond();

    if (thr_no == RECEIVER_THREAD_NO)
      continue;                 // Will run in the main thread.
    /*
     * The NdbThread_Create() takes void **, but that is cast to void * when
     * passed to the thread function. Which is kind of strange ...
     */
    threads[thr_no] = NdbThread_Create(mt_job_thread_main,
                                           (void **)(rep->m_thread + thr_no),
                                           1024*1024,
                                           "execute thread", //ToDo add number
                                           NDB_THREAD_PRIO_MEAN);
    assert(threads[thr_no] != NULL);
  }

  /* Now run the main loop for thread 0 directly. */
  mt_receiver_thread_main(&(rep->m_thread[RECEIVER_THREAD_NO]));

  /* Wait for all threads to shutdown. */
  for (thr_no = 0; thr_no < NUM_THREADS; thr_no++)
  {
    if (thr_no == RECEIVER_THREAD_NO)
      continue;
    void *dummy_return_status;
    NdbThread_WaitFor(threads[thr_no], &dummy_return_status);
    NdbThread_Destroy(&(threads[thr_no]));
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
  
  StartOrd * const  startOrd = (StartOrd *)&signalT.theData[0];
  startOrd->restartInfo = 0;
  
  senddelay(0, &signalT.header, 1);
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
  return NUM_THREADS - 1;
}

bool
FastScheduler::traceDumpGetJam(Uint32 thr_no, Uint32 & jamBlockNumber,
                               const Uint32 * & thrdTheEmulatedJam,
                               Uint32 & thrdTheEmulatedJamIndex)
{
  if (thr_no >= (NUM_THREADS - 1))
    return false;

#ifdef NO_EMULATED_JAM
  jamBlockNumber = 0;
  thrdTheEmulatedJam = NULL;
  thrdTheEmulatedJamIndex = 0;
#else
  const EmulatedJamBuffer *jamBuffer = g_thr_repository.m_thread[thr_no].m_jam;
  thrdTheEmulatedJam = jamBuffer->theEmulatedJam;
  thrdTheEmulatedJamIndex = jamBuffer->theEmulatedJamIndex;
  jamBlockNumber = jamBuffer->theEmulatedJamBlockNumber;
#endif
  return true;
}

void
FastScheduler::traceDumpPrepare()
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
  pthread_mutex_lock(&g_thr_repository.stop_for_crash_mutex);
  g_thr_repository.stopped_threads = 0;

  for (Uint32 thr_no = 0; thr_no < (NUM_THREADS - 1); thr_no++)
  {
    if (selfptr != NULL && selfptr->m_thr_no == thr_no)
    {
      /* This is own thread; we have already stopped processing. */
      continue;
    }

    sendprioa_STOP_FOR_CRASH(thr_no);

    waitFor_count++;
  }

  static const Uint32 max_wait_seconds = 2;
  NDB_TICKS start = NdbTick_CurrentMillisecond();
  struct timespec waittime;
  waittime.tv_sec = 0;
  waittime.tv_nsec = 10*1000*1000;
  while (g_thr_repository.stopped_threads < waitFor_count)
  {
    pthread_cond_timedwait(&g_thr_repository.stop_for_crash_cond,
                           &g_thr_repository.stop_for_crash_mutex,
                           &waittime);
    NDB_TICKS now = NdbTick_CurrentMillisecond();
    if (now > start + max_wait_seconds * 1000)
      break;                    // Give up
  }
  if (g_thr_repository.stopped_threads < waitFor_count)
  {
    ndbout_c("Warning: %d thread(s) did not stop before starting crash dump.",
             waitFor_count - g_thr_repository.stopped_threads);
  }
  pthread_mutex_unlock(&g_thr_repository.stop_for_crash_mutex);

  /* Now we are ready (or as ready as can be) for doing crash dump. */
}

void mt_execSTOP_FOR_CRASH()
{
  void *value= NdbThread_GetTlsKey(NDB_THREAD_TLS_THREAD);
  const thr_data *selfptr = reinterpret_cast<const thr_data *>(value);
  assert(selfptr != NULL);

  pthread_mutex_lock(&g_thr_repository.stop_for_crash_mutex);
  g_thr_repository.stopped_threads++;
  pthread_cond_signal(&g_thr_repository.stop_for_crash_cond);
  pthread_mutex_unlock(&g_thr_repository.stop_for_crash_mutex);

  /* ToDo: is this correct? */
  globalEmulatorData.theWatchDog->unregisterWatchedThread(selfptr->m_thr_no);

  pthread_exit(NULL);
}

void
FastScheduler::dumpSignalMemory(Uint32 thr_no, FILE* out)
{
  void *value= NdbThread_GetTlsKey(NDB_THREAD_TLS_THREAD);
  const thr_data *selfptr = reinterpret_cast<const thr_data *>(value);
  const thr_repository *rep = &g_thr_repository;
  /*
   * The selfptr might be NULL, or pointer to thread that is doing the crash
   * jump.
   * If non-null, we should update the watchdog counter while dumping.
   */
  Uint32 *watchDogCounter;
  if (selfptr)
    watchDogCounter = selfptr->m_watchdog_counter;
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

  const thr_data *thr_ptr = &rep->m_thread[thr_no];
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
   * (and freed) buffers, plus MAX_THREADS buffers for currently active
   * prio B buffers, plus one active prio A buffer.
   */
  struct {
    const thr_job_buffer *m_jb;
    Uint32 m_pos;
    Uint32 m_max;
  } jbs[THR_FREE_BUF_MAX + MAX_THREADS + 1];

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
    if (read_pos > 0)
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
    Signal signal;
    const SignalHeader *s = signalSequence[seq_end].ptr;
    unsigned siglen = (sizeof(*s)>>2) + s->theLength;
    if (siglen > 25)
      siglen = 25;              // Sanity check
    memcpy(&signal.header, s, 4*siglen);

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
mt_mem_manager_lock()
{
  lock(&(g_thr_repository.m_mem_manager_lock));
}

void
mt_mem_manager_unlock()
{
  unlock(&(g_thr_repository.m_mem_manager_lock));
}

void
mt_receive_lock()
{
  lock(&(g_thr_repository.m_receive_lock));
}

void
mt_receive_unlock()
{
  unlock(&(g_thr_repository.m_receive_lock));
}

void
mt_send_lock(void *dummy, NodeId nodeId)
{
  lock(&(g_thr_repository.m_send_locks[nodeId]));
}

void
mt_send_unlock(void *dummy, NodeId nodeId)
{
  unlock(&(g_thr_repository.m_send_locks[nodeId]));
}

/**
 * Global data
 */
struct thr_repository g_thr_repository;
