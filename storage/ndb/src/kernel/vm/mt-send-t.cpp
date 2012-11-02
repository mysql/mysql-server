#include "mt-asm.h"
#include "mt-lock.hpp"
#include <NdbTick.h>
#include <NdbMutex.h>
#include <NdbThread.h>
#include <NdbCondition.h>
#include <NdbTap.hpp>
#include <Bitmask.hpp>
#include <ndb_rand.h>

#define BUGGY_VERSION 0

/**
 * DO_SYSCALL inside critical section
 *  (the equivalent of writev(socket)
 */
#define DO_SYSCALL 1

/**
 * This is a unit test of the send code for mt.cpp
 *   specifically the code that manages which thread will send
 *   (write gathering)
 *
 * Each thread is a producer of Signals
 * Each signal has a destination remote node (transporter)
 * Each thread will after having produced a set of signals
 *   check if it should send them on socket.
 *   If it decides that it should, it consumes all the signals
 *   produced by all threads.
 *
 * In this unit test, we don't send signals, but the producing part
 *   will only be to increment a counter.
 *
 ******************************************************************
 *
 * To use this program seriously...
 *
 *   you should set BUGGY_VERSION to 1
 *   and experiment with values on cnt_*
 *   until you find a variant which crashes (abort)
 *
 * The values compiled-in makes it crash on a single socket
 *   Intel(R) Core(TM) i5-2400 CPU @ 3.10GHz release compiled!
 *   (i never managed to get it debug compiled)
 */
#define MAX_THREADS 256
#define MAX_TRANSPORTERS 256

/**
 * global variables
 */
static unsigned cnt_threads = 64;
static unsigned cnt_transporters = 8;

/**
 * outer loops
 *   start/stop threads
 */
static unsigned cnt_seconds = 180;

/**
 * no of signals produced before calling consume
 */
static unsigned cnt_signals_before_consume = 4;

/**
 * no of signals produced in one inner loop
 */
static unsigned cnt_signals_per_inner_loop = 4;

/**
 * no inner loops per outer loop
 *
 *   after each inner loop
 *   threads will be stalled and result verified
 */
static unsigned cnt_inner_loops = 5000;

/**
 * pct of do_send that are using forceSend()
 */
static unsigned pct_force = 15;

typedef Bitmask<(MAX_TRANSPORTERS+31)/32> TransporterMask;

struct Producer
{
  Producer() {
    bzero(val, sizeof(val));
    pendingcount = 0;
  }

  void init() {}

  /**
   * values produced...
   */
  unsigned val[MAX_TRANSPORTERS];

  /**
   * mask/array to keep track of which transporter we have produce values to
   */
  TransporterMask pendingmask;
  unsigned pendingcount;
  unsigned char pendinglist[MAX_TRANSPORTERS];

  /**
   * produce a value
   *
   * This is the equivalent of mt_send_remote()
   */
  void produce(unsigned D);

  /**
   * consume values (from all threads)
   *   for transporters that we have produced a value to
   *
   * This is the equivalent to do_send and if force=true
   *   this is the equivalent of forceSend()
   */
  void consume(bool force);
};

struct Thread
{
  Thread() { thread= 0; }

  void init() { p.init(); }

  NdbThread * thread;
  Producer p;
};

/**
 * This is the consumer of values for *one* transporter
 */
struct Consumer
{
  Consumer() {
    m_force_send = 0; bzero(val, sizeof(val));
  }

  void init() {}

  struct thr_spin_lock<8> m_send_lock;
  unsigned m_force_send;
  unsigned val[MAX_THREADS];

  /**
   * consume values from all threads to this transporter
   */
  void consume(unsigned D);

  /**
   * force_consume
   */
  void forceConsume(unsigned D);
};

struct Consumer_pad
{
  Consumer c;
  char pad[NDB_CL_PADSZ(sizeof(Consumer))];
};

struct Thread_pad
{
  Thread t;
  char pad[NDB_CL_PADSZ(sizeof(Thread))];
};

/**
 * Thread repository
 *   and an instance of it
 */
static
struct Rep
{
  Thread_pad t[MAX_THREADS];
  Consumer_pad c[MAX_TRANSPORTERS];

  /**
   * This menthod is called when all threads are stalled
   *   so it's safe to read values without locks
   */
  void validate() {
    for (unsigned ic = 0; ic < cnt_transporters; ic++)
    {
      for (unsigned it = 0; it < cnt_threads; it++)
      {
        if (c[ic].c.val[it] != t[it].t.p.val[ic])
        {
          printf("Detected bug!!!\n");
          printf("ic: %u it: %u c[ic].c.val[it]: %u t[it].t.p.val[ic]: %u\n",
                 ic, it, c[ic].c.val[it], t[it].t.p.val[ic]);
          abort();
        }
      }
    }
  }

  void init() {
    for (unsigned i = 0; i < cnt_threads; i++)
      t[i].t.init();

    for (unsigned i = 0; i < cnt_transporters; i++)
      c[i].c.init();
  }
} rep;

static
struct Test
{
  Test() {
    waiting_start = 0;
    waiting_stop = 0;
    mutex = 0;
    cond = 0;
  }

  void init() {
    mutex = NdbMutex_Create();
    cond = NdbCondition_Create();
  }

  unsigned waiting_start;
  unsigned waiting_stop;
  NdbMutex* mutex;
  NdbCondition* cond;

  void wait_started();
  void wait_completed();
} test;

void*
thread_main(void * _t)
{
  unsigned seed =
    (unsigned)NdbTick_CurrentNanosecond() +
    (unsigned)(unsigned long long)_t;

  Thread * self = (Thread*) _t;
  for (unsigned i = 0; i < cnt_inner_loops; i++)
  {
    test.wait_started();
    for (unsigned j = 0; j < cnt_signals_per_inner_loop;)
    {
      for (unsigned k = 0; k < cnt_signals_before_consume; k++)
      {
        /**
         * Produce a signal to destination D
         */
        unsigned D = unsigned(ndb_rand_r(&seed)) % cnt_transporters;
        self->p.produce(D);
      }

      j += cnt_signals_before_consume;

      /**
       * This is the equivalent of do_send()
       */
      bool force = unsigned(ndb_rand_r(&seed) % 100) < pct_force;
      self->p.consume(force);
    }
    test.wait_completed();
  }
  return 0;
}

static
bool
match(const char * arg, const char * val, unsigned * valptr)
{
  if (strncmp(arg, val, strlen(val)) == 0)
  {
    * valptr = atoi(arg + strlen(val));
    return true;
  }
  return false;
}

int
main(int argc, char ** argv)
{
  plan(1);
  ndb_init();
  test.init();
  rep.init();

  if (argc == 1)
  {
    printf("No arguments supplied...\n"
           "assuming we're being run from MTR or similar.\n"
           "decreasing loop counts to ridiculously small values...\n");
    cnt_seconds = 10;
    cnt_inner_loops = 3000;
    cnt_threads = 4;
  }
  else
  {
    printf("Arguments supplied...\n");
    for (int i = 1; i < argc; i++)
    {
      if (match(argv[i], "cnt_seconds=", &cnt_seconds))
        continue;
      else if (match(argv[i], "cnt_threads=", &cnt_threads))
        continue;
      else if (match(argv[i], "cnt_transporters=", &cnt_transporters))
        continue;
      else if (match(argv[i], "cnt_inner_loops=", &cnt_inner_loops))
        continue;
      else if (match(argv[i], "cnt_signals_before_consume=",
                     &cnt_signals_before_consume))
        continue;
      else if (match(argv[i], "cnt_signals_per_inner_loop=",
                     &cnt_signals_per_inner_loop))
        continue;
      else if (match(argv[i], "pct_force=",
                     &pct_force))
        continue;
      else
      {
        printf("ignoreing unknown argument: %s\n", argv[i]);
      }
    }
  }

  printf("%s"
         " cnt_seconds=%u"
         " cnt_threads=%u"
         " cnt_transporters=%u"
         " cnt_inner_loops=%u"
         " cnt_signals_before_consume=%u"
         " cnt_signals_per_inner_loop=%u"
         " pct_force=%u"
         "\n",
         argv[0],
         cnt_seconds,
         cnt_threads,
         cnt_transporters,
         cnt_inner_loops,
         cnt_signals_before_consume,
         cnt_signals_per_inner_loop,
         pct_force);

  Uint32 loop = 0;
  Uint64 start = NdbTick_CurrentMillisecond() / 1000;
  while (start + cnt_seconds > (NdbTick_CurrentMillisecond() / 1000))
  {
    printf("%u ", loop++); fflush(stdout);
    if ((loop < 100 && (loop % 25) == 0) ||
        (loop >= 100 && (loop % 20) == 0))
      printf("\n");

    for (unsigned t = 0; t < cnt_threads; t++)
    {
      rep.t[t].t.thread = NdbThread_Create(thread_main,
                                           (void**)&rep.t[t].t,
                                           1024*1024,
                                           "execute thread",
                                           NDB_THREAD_PRIO_MEAN);
    }

    for (unsigned t = 0; t < cnt_threads; t++)
    {
      void * ret;
      NdbThread_WaitFor(rep.t[t].t.thread, &ret);
    }
  }
  printf("\n"); fflush(stdout);

  ok(true, "ok");
  return 0;
}

inline
void
Producer::produce(unsigned D)
{
  if (!pendingmask.get(D))
  {
    pendingmask.set(D);
    pendinglist[pendingcount] = D;
    pendingcount++;
  }
  val[D]++;
}

inline
void
Producer::consume(bool force)
{
  unsigned count = pendingcount;
  pendingmask.clear();
  pendingcount = 0;

  for (unsigned i = 0; i < count; i++)
  {
    unsigned D = pendinglist[i];
    if (force)
      rep.c[D].c.forceConsume(D);
    else
      rep.c[D].c.consume(D);
  }
}

inline
void
Consumer::consume(unsigned D)
{
  /**
   * This is the equivalent of do_send(must_send = 1)
   */
  m_force_send = 1;

  do
  {
    if (trylock(&m_send_lock) != 0)
    {
      /* Other thread will send for us as we set m_force_send. */
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
    m_force_send = 0;
    mb();

    /**
     * This is the equivalent of link_thread_send_buffers
     */
    for (unsigned i = 0; i < cnt_threads; i++)
    {
      val[i] = rep.t[i].t.p.val[D];
    }

    /**
     * Do a syscall...which could have affect on barriers...etc
     */
    if (DO_SYSCALL)
    {
      NdbTick_CurrentMillisecond();
    }

    unlock(&m_send_lock);

#if BUGGY_VERSION
#else
    mb();
#endif
  }
  while (m_force_send != 0);
}

inline
void
Consumer::forceConsume(unsigned D)
{
  /**
   * This is the equivalent of forceSend()
   */

  do
  {
    /**
     * NOTE: since we unconditionally lock m_send_lock
     *   we don't need a mb() after the clearing of m_force_send here.
     */
    m_force_send = 0;

    lock(&m_send_lock);

    /**
     * This is the equivalent of link_thread_send_buffers
     */
    for (unsigned i = 0; i < cnt_threads; i++)
    {
      val[i] = rep.t[i].t.p.val[D];
    }

    /**
     * Do a syscall...which could have affect on barriers...etc
     */
    if (DO_SYSCALL)
    {
      NdbTick_CurrentMillisecond();
    }

    unlock(&m_send_lock);

#if BUGGY_VERSION
#else
    mb();
#endif
  }
  while (m_force_send != 0);
}

void
Test::wait_started()
{
  NdbMutex_Lock(mutex);
  if (waiting_start + 1 == cnt_threads)
  {
    waiting_stop = 0;
  }
  waiting_start++;
  assert(waiting_start <= cnt_threads);
  while (waiting_start < cnt_threads)
    NdbCondition_Wait(cond, mutex);

  NdbCondition_Broadcast(cond);
  NdbMutex_Unlock(mutex);
}

void
Test::wait_completed()
{
  NdbMutex_Lock(mutex);
  if (waiting_stop + 1 == cnt_threads)
  {
    rep.validate();
    waiting_start = 0;
  }
  waiting_stop++;
  assert(waiting_stop <= cnt_threads);
  while (waiting_stop < cnt_threads)
    NdbCondition_Wait(cond, mutex);

  NdbCondition_Broadcast(cond);
  NdbMutex_Unlock(mutex);
}

static
void
register_lock(const void * ptr, const char * name)
{
  return;
}

static
mt_lock_stat *
lookup_lock(const void * ptr)
{
  return 0;
}
