/* Copyright (c) 2012, 2022, Oracle and/or its affiliates.

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

/**
  \file task.cc
  Rudimentary, non-preemptive task system in portable C,
  based on Tom Duff's switch-based coroutine trick
  and a stack of environment structs. (continuations?)
  Nonblocking IO and event handling need to be rewritten for each new OS.
  The code is not MT-safe, but could be made safe by moving all global
  variables into a context struct which could be the first parameter
  to all the functions.
*/
#if defined(linux) && !defined(_XOPEN_SOURCE)
#define _XOPEN_SOURCE
#endif

#if defined(linux) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#ifdef _MSC_VER
#include <stdint.h>
#endif
#include "xcom/x_platform.h"
#include "xcom/xcom_memory.h"
#include "xcom/xcom_profile.h"

#ifndef XCOM_WITHOUT_OPENSSL
#ifdef _WIN32
/* In OpenSSL before 1.1.0, we need this first. */
#include <winsock2.h>
#endif /* _WIN32 */

#include <openssl/err.h>
#include <openssl/ssl.h>

#endif

#include <limits.h>

#ifndef _WIN32
#include <arpa/inet.h>
#include <netdb.h>
#endif

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>

#include <memory>

#include "xcom/node_connection.h"
#include "xdr_gen/xcom_vp.h"

#include "xcom/simset.h"
#include "xcom/site_def.h"
#include "xcom/task.h"
#include "xcom/task_debug.h"
#include "xcom/task_net.h"
#include "xcom/task_os.h"
#include "xcom/xcom_base.h"
#include "xcom/xcom_cfg.h"
#include "xcom/xcom_transport.h"

#ifndef _WIN32
#include <poll.h>
#endif

#include "xcom/retry.h"
#include "xcom/xdr_utils.h"

#ifdef _WIN32
#define xcom_buf char
#else
#define xcom_buf void
#endif

extern const char *pax_op_to_str(int x);
extern uint32_t get_my_xcom_id();

task_arg null_arg = {a_end, {0}};

struct iotasks;
typedef struct iotasks iotasks;

typedef struct {
  u_int pollfd_array_len;
  pollfd *pollfd_array_val;
} pollfd_array;

typedef task_env *task_env_p;

typedef struct {
  u_int task_env_p_array_len;
  task_env_p *task_env_p_array_val;
} task_env_p_array;

init_xdr_array(pollfd) free_xdr_array(pollfd) set_xdr_array(pollfd)
    get_xdr_array(pollfd) init_xdr_array(task_env_p) free_xdr_array(task_env_p)
        set_xdr_array(task_env_p) get_xdr_array(task_env_p)

            struct iotasks {
  u_int nwait;
  pollfd_array fd;
  task_env_p_array tasks;
};

int task_errno = 0;
static task_env *extract_first_delayed();
static task_env *task_ref(task_env *t);
static task_env *task_unref(task_env *t);
static void wake_all_io();
static void task_sys_deinit();

/*
xcom_monotonic_seconds returns monotonically increasing time as seconds since
January 1, 1970.
The struct xcom_clock contains the system time when the clock was initialized,
and the value of the monotonic system timer at approximately the same time.
The time that is returned is the delta between the current monotonic system
timer and the monotonic start time plus the system start time.
This ensures a monotonically increasing time that is reasonably correlated
with the system time.

Windows version and Posix version differ slightly, but use the same general
approach.
*/
#define crash(x) g_critical("%s\n", x)

#ifdef _WIN32

/*
All reasonably recent versions of Windows have QueryPerformanceCounter,
which is guaranteed to be monotonic, so we use that, combined with
GetSystemTimeAsFileTime to get the real start time.
*/

struct xcom_clock {
  double real_start;        /* System time at init, in seconds */
  uint64_t monotonic_start; /* Performance counter ticks at init */
  uint64_t freq; /* Performance counter frequency, in counts per second */
  double now;    /* The last computed clock */
  int done;
};
typedef struct xcom_clock xcom_clock;

/* Return number of 100-ns intervals since January 1, 1970 */
static inline uint64_t systime(void) {
  /* Offset of January 1, 1970 from year 1601 */
  static uint64_t const EPOCH = ((uint64_t)116444736000000000ULL);
  FILETIME file_time;
  uint64_t time;

  /* GetSystemTimeAsFileTime is based on year 1601 as zero */
  GetSystemTimeAsFileTime(&file_time);
  time = ((uint64_t)file_time.dwLowDateTime);
  time += ((uint64_t)file_time.dwHighDateTime) << 32;
  /* We now have number of 100-ns intervals since January 1, 1601 */
  time -= EPOCH; /* Rebase to 1970 */
  return time;
}

/* Return the performance counter */
static inline uint64_t get_pc(void) {
  LARGE_INTEGER count;

  /* On systems that run Windows XP or later, the function will always succeed
   * and will thus never return zero. */
  if (!QueryPerformanceCounter(&count)) {
    crash("Need performance counter");
  }
  return (uint64_t)(count.QuadPart);
}

/* Return the frequency of the performance counter. It is guaranteed to not
 * change during execution. */
static inline uint64_t query_freq(void) {
  LARGE_INTEGER freq;

  /* On systems that run Windows XP or later, the function will always succeed
   * and will thus never return zero. */
  if (!QueryPerformanceFrequency(&freq) || !freq.QuadPart) {
    crash("Need performance counter frequency");
  }
  return (uint64_t)(freq.QuadPart);
}

/* Compute monotonic clock by ading elapsed monotonic time to the time when the
 * clock was initialized */
static double xcom_monotonic_seconds(xcom_clock *clock) {
  clock->now =
      clock->real_start +
      ((double)(get_pc() - clock->monotonic_start) / (double)clock->freq);
  return clock->now;
}

/* Initialize the clock by setting the performance counter frequency, the
 * current value of the performance counter, and the current value of the system
 * time. */
static void xcom_init_clock(xcom_clock *clock) {
  clock->freq = query_freq();
  clock->monotonic_start = get_pc();
  clock->real_start = ((double)systime()) / 10000000.0;
  xcom_monotonic_seconds(clock);
  clock->done = 1;
}

#else

/* POSIX 1003.1 defines clock_gettime(), which is required to support
 * CLOCK_REALTIME: "All implementations shall support a clock_id of
 * CLOCK_REALTIME as defined in <time.h>.  This  clock represents the realtime
 * clock for the system."  An implementation does not need to support
 * CLOCK_MONOTONIC, but: "If the Monotonic Clock option is supported, all
 * implementations shall support a clock_id  of CLOCK_MONOTONIC defined in
 * <time.h>. This clock represents the monotonic clock for the  system. For this
 * clock, the value returned by clock_gettime() represents the amount of  time
 * (in seconds and nanoseconds) since an unspecified point in the past (for
 * example,  system start-up time, or the Epoch)" Even though POSIX does not
 * specifically require it, all supported platforms have it, and on non-Windows
 * systems this implementation of a monotonic clock requires it.*/

#ifndef CLOCK_MONOTONIC
/* Fail here if we have no monotonic clock on Posix */
#error "Need CLOCK_MONOTONIC"
#endif

struct xcom_clock {
  double real_start;      /* System time at init, in seconds */
  double monotonic_start; /* Monotonic time at init, in seconds */
  double offset; /* Delta between system time and monotonic time, at init */
  double now;    /* The last computed clock */
  int done;
};
typedef struct xcom_clock xcom_clock;

/* Convert time in timespec to double. */
static inline double ts_to_sec(struct timespec *ts) {
  /* tv_nsec is number of nanoseconds */
  return (double)ts->tv_sec + (double)ts->tv_nsec / 1000000000.0;
}

/* Returns elapsed time since some arbitrary time in the past */
static inline double get_monotonic_time() {
  struct timespec t;

  clock_gettime(CLOCK_MONOTONIC, &t);
  return ts_to_sec(&t);
}

/* Returns current system time */
static inline double get_real_time() {
  struct timespec t;

  clock_gettime(CLOCK_REALTIME, &t);
  return ts_to_sec(&t);
}

/* Compute the monotonic clock as the sum of the current monotonic time and its
 * offset relative to the system time */
static double xcom_monotonic_seconds(xcom_clock *clock) {
  clock->now = get_monotonic_time() + clock->offset;
  return clock->now;
}

/* Initialize clock by capturing the current values of monotonic time and system
 * time and computing the offset. */
static void xcom_init_clock(xcom_clock *clock) {
  clock->monotonic_start = get_monotonic_time();
  clock->real_start = get_real_time();
  /* Capture the difference between real time and monotonic time */
  clock->offset = clock->real_start - clock->monotonic_start;
  xcom_monotonic_seconds(clock);
  clock->done = 1;
}

#endif

/* The monotonic clock used by xcom */
static xcom_clock task_timer;

/* Calls xcom_monotonic_seconds after having initialized the clock if necessary
 */
double seconds() {
  if (!task_timer.done) {
    xcom_init_clock(&task_timer);
  }
  return xcom_monotonic_seconds(&task_timer);
}

/* Return the value of the last call to seconds() */
double task_now() {
  if (!task_timer.done) {
    xcom_init_clock(&task_timer);
  }
  return task_timer.now;
}

#ifdef _WIN32
int gettimeofday(struct timeval *tp, struct timezone *) {
  static uint64_t const EPOCH = ((uint64_t)116444736000000000ULL);

  SYSTEMTIME system_time;
  FILETIME file_time;
  uint64_t time;

  GetSystemTime(&system_time);
  SystemTimeToFileTime(&system_time, &file_time);
  time = ((uint64_t)file_time.dwLowDateTime);
  time += ((uint64_t)file_time.dwHighDateTime) << 32;

  tp->tv_sec = (long)((time - EPOCH) / 10000000L);
  tp->tv_usec = (long)(system_time.wMilliseconds * 1000);
  return 0;
}
#endif

#ifdef NOTDEF
static void task_queue_init(task_queue *q) { q->curn = 0; }

static void task_queue_debug(task_queue *q) {
  int i;
  GET_GOUT;
  if (!IS_XCOM_DEBUG_WITH(XCOM_DEBUG_TRACE)) return;
  STRLIT("task_queue_debug ");
  for (i = 1; i <= q->curn; i++) {
    NDBG(i, d);
    PTREXP(q->x[i]);
    STREXP(q->x[i]->name);
    NDBG(q->x[i]->heap_pos, d);
    NDBG(q->x[i]->terminate, d);
    NDBG(q->x[i]->time, f);
  }
  PRINT_GOUT;
  FREE_GOUT;
}

static int is_heap(task_queue *q) {
  if (q->curn) {
    int i;
    for (i = q->curn; i > 1; i--) {
      if ((q->x[i]->time < q->x[i / 2]->time) || (q->x[i]->heap_pos != i)) {
        task_queue_debug(q);
        return 0;
      }
    }
    if (q->x[1]->heap_pos != 1) {
      task_queue_debug(q);
      return 0;
    }
  }
  return 1;
}

static int task_queue_full(task_queue *q) { return q->curn >= MAXTASKS; }

#endif

#define FIX_POS(i) q->x[i]->heap_pos = (i)
/* #define TASK_SWAP(x,y) { task_env *tmp = (x); (x) = (y); (y) = (tmp); } */
#define TASK_SWAP(i, j)      \
  {                          \
    task_env *tmp = q->x[i]; \
    q->x[i] = q->x[j];       \
    q->x[j] = tmp;           \
    FIX_POS(i);              \
    FIX_POS(j);              \
  }
#define TASK_MOVE(i, j) \
  {                     \
    q->x[i] = q->x[j];  \
    FIX_POS(i);         \
  }
/* Put the task_env* at index n in its right place when Heap(1,n-1) */
static void task_queue_siftup(task_queue *q, int n) {
  int i = n;
  int p;
  assert(n >= 0);
  /* Heap(1,n-1) */
  for (;;) {
    if (i == 1) break; /* Reached root */
    p = i / 2;         /* Find parent */
    if (q->x[p]->time <= q->x[i]->time)
      break; /* We have reached correct place */
    TASK_SWAP(p, i);
    i = p;
  }
  /* Heap(1,n) */
}

/* Put the task_env* at index l in its right place when Heap(l+1,n) */
static void task_queue_siftdown(task_queue *q, int l, int n) {
  int i = l;
  int c;
  assert(n >= 0);
  /* Heap(l+1..n) */
  for (;;) {
    c = 2 * i;                               /* First child */
    if (c > n) break;                        /* Outside heap */
    if (c + 1 <= n)                          /* We have second child */
      if (q->x[c + 1]->time < q->x[c]->time) /* Select lesser child */
        c++;
    if (q->x[i]->time <= q->x[c]->time)
      break; /* We have reached correct place */
    TASK_SWAP(c, i);
    i = c;
  }
  /* Heap(l,n) */
}

/* Remove any element from the heap */
static task_env *task_queue_remove(task_queue *q, int i) {
  task_env *tmp = q->x[i]; /* Will return this */
  /* The element at index 0 is never part of the queue */
  if (0 == i) return nullptr;
  assert(q->curn);
  IFDBG(D_NONE, FN; STRLIT("task_queue_remove "); NDBG(i, d));
  TASK_MOVE(i, q->curn); /* Fill the hole */
  q->curn--;             /* Heap is now smaller */
                         /* Re-establish heap property */
  /* Avoid special case of empty queue and that we just removed the last element
   */
  if (q->curn && i <= q->curn) {
    int p = i / 2;
    if (p && q->x[p]->time > q->x[i]->time)
      task_queue_siftup(q, i);
    else
      task_queue_siftdown(q, i, q->curn);
  }
  tmp->heap_pos = 0;
  return task_unref(tmp);
}

/* Insert task_env * in queue */
static void task_queue_insert(task_queue *q, task_env *t) {
  assert(t->heap_pos == 0);
  assert(q->curn < MAXTASKS);
  q->curn++;
  q->x[q->curn] = t;
  FIX_POS(q->curn);
  /* Heap(1,n-1) */
  task_queue_siftup(q, q->curn);
  /* Heap(1,n) */
}

static int task_queue_empty(task_queue *q) {
#ifdef DEBUG_TASKS
  IFDBG(D_NONE, FN; PTREXP(q));
#endif
  return q->curn < 1;
}

static task_env *task_queue_min(task_queue *q) {
  assert(q->curn >= 1);
  return q->x[1];
}

/* Extract first task_env * from queue */
static task_env *task_queue_extractmin(task_queue *q) {
  task_env *tmp;
  assert(q);
  assert(q->curn >= 1);
  tmp = q->x[1];
  TASK_MOVE(1, q->curn);
  q->x[q->curn] = nullptr;
  q->curn--;
  /* Heap(2,n) */
  if (q->curn) task_queue_siftdown(q, 1, q->curn);
  /* Heap(1,n) */
  tmp->heap_pos = 0;
  return tmp;
}

static linkage ash_nazg_gimbatul = {
    0, &ash_nazg_gimbatul, &ash_nazg_gimbatul}; /* One ring to bind them all */

static void task_init(task_env *p);
void *task_allocate(task_env *p, unsigned int bytes);
/**
   Initialize task memory
*/
static void task_init(task_env *t) {
  link_init(&t->l, TYPE_HASH("task_env"));
  link_init(&t->all, TYPE_HASH("task_env"));
  t->heap_pos = 0;
  assert(ash_nazg_gimbatul.type == TYPE_HASH("task_env"));
  link_into(&t->all, &ash_nazg_gimbatul); /* Put it in the list of all tasks */
  assert(ash_nazg_gimbatul.type == TYPE_HASH("task_env"));
  t->terminate = RUN;
  t->refcnt = 0;
  t->taskret = 0;
  t->time = 0.0;
  t->arg = null_arg;
  t->where = t->buf;
  t->stack_top = &t->buf[TASK_POOL_ELEMS - 1];
  t->sp = t->stack_top;
  memset(t->buf, 0, TASK_POOL_ELEMS * sizeof(TaskAlign));
}

static linkage tasks = {0, &tasks, &tasks};
static task_queue task_time_q;
static linkage free_tasks = {0, &free_tasks, &free_tasks};
/* Basic operations on tasks */
static task_env *activate(task_env *t) {
  if (t) {
    IFDBG(D_NONE, FN; STRLIT("activating task "); PTREXP(t); STREXP(t->name);
          NDBG(t->heap_pos, d); NDBG(t->time, f););
    assert(ash_nazg_gimbatul.type == TYPE_HASH("task_env"));
    if (t->heap_pos) task_queue_remove(&task_time_q, t->heap_pos);
    link_into(&t->l, &tasks);
    t->time = 0.0;
    t->heap_pos = 0;
    assert(ash_nazg_gimbatul.type == TYPE_HASH("task_env"));
  }
  return t;
}

static task_env *deactivate(task_env *t) {
  if (t) {
    assert(ash_nazg_gimbatul.type == TYPE_HASH("task_env"));
    link_out(&t->l);
    assert(ash_nazg_gimbatul.type == TYPE_HASH("task_env"));
  }
  return t;
}

void task_delay_until(double time) {
  if (stack) {
    stack->time = time;
    task_queue_insert(&task_time_q, task_ref(deactivate(stack)));
  }
}

/* Wait queues */
void task_wait(task_env *t, linkage *queue) {
  if (t) {
    TASK_DEBUG("task_wait");
    deactivate(t);
    link_into(&t->l, queue);
  }
}

void task_wakeup(linkage *queue) {
  assert(queue);
  assert(queue != &tasks);
  while (!link_empty(queue)) {
    activate(container_of(link_extract_first(queue), task_env, l));
    TASK_DEBUG("task_wakeup");
  }
}

static void task_wakeup_first(linkage *queue) {
  assert(queue);
  assert(queue != &tasks);
  if (!link_empty(queue)) {
    activate(container_of(link_extract_first(queue), task_env, l));
    TASK_DEBUG("task_wakeup_first");
  }
}

/* Channels */
channel *channel_init(channel *c, unsigned int type) {
  link_init(&c->data, type);
  link_init(&c->queue, TYPE_HASH("task_env"));
  return c;
}

void channel_put(channel *c, linkage *data) {
  IFDBG(D_NONE, FN; PTREXP(data); PTREXP(&c->data));
  link_into(data, &c->data);
  task_wakeup_first(&c->queue);
}

void channel_put_front(channel *c, linkage *data) {
  link_follow(data, &c->data);
  task_wakeup_first(&c->queue);
}

static int active_tasks = 0;
task_env *task_new(task_func func, task_arg arg, const char *name, int debug) {
  task_env *t;
  if (link_empty(&free_tasks))
    t = (task_env *)xcom_malloc(sizeof(task_env));
  else
    t = container_of(link_extract_first(&free_tasks), task_env, l);
  IFDBG(D_NONE, FN; PTREXP(t); STREXP(name); NDBG(active_tasks, d););
  task_init(t);
  t->func = func;
  t->arg = arg;
  t->name = name;
  t->debug = debug;
  t->waitfd = -1;
  t->interrupt = 0;
  activate(t);
  task_ref(t);
  active_tasks++;
  return t;
}

/**Allocate bytes from pool, initialized to zero */
void *task_allocate(task_env *p, unsigned int bytes) {
  /* TaskAlign to boundary  */
  unsigned int alloc_units =
      (unsigned int)((bytes + sizeof(TaskAlign) - 1) / sizeof(TaskAlign));
  TaskAlign *ret;
  /* Check if there is space */
  TASK_DEBUG("task_allocate");
  if ((p->where + alloc_units) <= (p->stack_top)) {
    ret = p->where;
    p->where += alloc_units;
    memset(ret, 0, alloc_units * sizeof(TaskAlign));
  } else {
    ret = nullptr;
    abort();
  }
  return ret;
}

void reset_state(task_env *p) {
  if ((p->where) <= (p->stack_top - 1)) {
    TASK_DEBUG("reset_state");
    p->stack_top[-1].state = 0;
  } else {
    abort();
  }
}

void pushp(task_env *p, void *ptr) {
  assert(ptr);
  if ((p->where) <= (p->stack_top - 1)) {
    p->stack_top->ptr = ptr;
    p->stack_top--;
    TASK_DEBUG("pushp");
  } else {
    abort();
  }
}

void popp(task_env *p) {
  if (p->stack_top < &p->buf[TASK_POOL_ELEMS]) {
    TASK_DEBUG("popp");
    p->stack_top++;
  } else {
    abort();
  }
}

static int runnable_tasks() { return !link_empty(&tasks); }

static int delayed_tasks() {
#ifdef DEBUG_TASKS
  IFDBG(D_NONE, FN; PTREXP(&task_time_q));
#endif
  return !task_queue_empty(&task_time_q);
}

static void task_delete(task_env *t) {
  IFDBG(D_NONE, FN; PTREXP(t); STREXP(t->name); NDBG(t->refcnt, d);
        NDBG(active_tasks, d););
  link_out(&t->all); /* Remove task from list of all tasks */
#if 1
  free(deactivate(t)); /* Deactivate and free task */
#else
  deactivate(t);
  link_into(&t->l, &free_tasks);
#endif
  active_tasks--;
}

static task_env *task_ref(task_env *t) {
  if (t) {
    t->refcnt++;
  }
  return t;
}

static task_env *task_unref(task_env *t) {
  if (t) {
    t->refcnt--;
    if (t->refcnt == 0) {
      task_delete(t);
      return nullptr;
    }
  }
  return t;
}

task_env *task_activate(task_env *t) { return activate(t); }

task_env *task_deactivate(task_env *t) { return deactivate(t); }

/* Set terminate flag and activate task */
task_env *task_terminate(task_env *t) {
  if (t) {
    IFDBG(D_NONE, FN; STRLIT("terminating "); PTREXP(t); STREXP(t->name);
          NDBG(t->refcnt, d));
    t->terminate = KILL; /* Set terminate flag */
    activate(t);         /* and get it running */
    IFDBG(D_NONE, FN; STRLIT("terminated "); PTREXP(t); STREXP(t->name);
          NDBG(t->refcnt, d));
  }
  return t;
}

/* Call task_terminate on all tasks */
void task_terminate_all() {
  /* First, activate all tasks which wait for timeout */
  while (delayed_tasks()) {
    task_env *t = extract_first_delayed(); /* May be NULL */
    if (t) activate(t);                    /* Make it runnable */
  }
  /* Then wake all tasks waiting for IO */
  wake_all_io();
  /* At last, terminate everything */
  FWD_ITER(&ash_nazg_gimbatul, task_env,
           task_terminate(container_of(link_iter, task_env, all)););
}

static task_env *first_delayed() { return task_queue_min(&task_time_q); }

static task_env *extract_first_delayed() {
  task_env *ret = task_queue_extractmin(&task_time_q);
  ret->time = 0.0;
  return task_unref(ret);
}

static iotasks iot;

static void iotasks_init(iotasks *iot_to_init) {
  IFDBG(D_NONE, FN);
  iot_to_init->nwait = 0;
  init_pollfd_array(&iot_to_init->fd);
  init_task_env_p_array(&iot_to_init->tasks);
}

static void iotasks_deinit(iotasks *iot_to_deinit) {
  IFDBG(D_NONE, FN);
  iot_to_deinit->nwait = 0;
  free_pollfd_array(&iot_to_deinit->fd);
  free_task_env_p_array(&iot_to_deinit->tasks);
}

static void poll_wakeup(u_int i) {
  activate(task_unref(get_task_env_p(&iot.tasks, i)));
  set_task_env_p(&iot.tasks, nullptr, i);
  iot.nwait--; /* Shrink array of pollfds */
  set_pollfd(&iot.fd, get_pollfd(&iot.fd, iot.nwait), i);
  set_task_env_p(&iot.tasks, get_task_env_p(&iot.tasks, iot.nwait), i);
}

static int poll_wait(int ms) {
  result nfds = {0, 0};
  int wake = 0;

  /* Wait at most ms milliseconds */
  IFDBG(D_NONE, FN; NDBG(ms, d));
  if (ms < 0 || ms > 1000) ms = 1000; /* Wait at most 1000 ms */
  SET_OS_ERR(0);
  while ((nfds.val = poll(iot.fd.pollfd_array_val, iot.nwait, ms)) == -1) {
    /* purecov: begin inspected */
    nfds.funerr = to_errno(GET_OS_ERR);
    if (!can_retry(nfds.funerr)) {
      task_dump_err(nfds.funerr);
      DBGOUT(FN; STRLIT("poll failed "); NUMEXP(nfds.val); NUMEXP(nfds.funerr);
             NUMEXP(iot.nwait));
      break;
      /* abort(); */
    }
    SET_OS_ERR(0);
    /* purecov: end */
  }
  /* Wake up ready tasks */
  {
    u_int i = 0;
    int interrupt = 0;
    while (i < iot.nwait) {
      interrupt = (get_task_env_p(&iot.tasks, i)->time != 0.0 &&
                   get_task_env_p(&iot.tasks, i)->time < task_now());
      if (interrupt || /* timeout ? */
          get_pollfd(&iot.fd, i).revents) {
        if (get_pollfd(&iot.fd, i).revents & POLLERR) {
          DBGOUT(FN; STRLIT("IO failed POLLERR "); NUMEXP(i);
                 NUMEXP(get_pollfd(&iot.fd, i).fd);
                 NUMEXP(get_pollfd(&iot.fd, i).events);
                 NUMEXP(get_pollfd(&iot.fd, i).revents); NUMEXP(i);
                 NUMEXP(iot.nwait););
        }
        get_task_env_p(&iot.tasks, i)->interrupt = interrupt;
        poll_wakeup(i);
        wake = 1;
      } else {
        i++;
      }
    }
  }
  return wake;
}

static void add_fd(task_env *t, int fd, int op) {
  short events = 'r' == op ? POLLIN | POLLRDNORM : POLLOUT;
  IFDBG(D_NONE, FN; PTREXP(t); NDBG(fd, d); NDBG(op, d));
  assert(fd >= 0);
  t->waitfd = fd;
  deactivate(t);
  task_ref(t);
  set_task_env_p(&iot.tasks, t, iot.nwait);
  {
    pollfd x;
    x.fd = fd;
    x.events = events;
    x.revents = 0;
    set_pollfd(&iot.fd, x, iot.nwait);
  }
  iot.nwait++;
}

static void unpoll(u_int i) {
  task_unref(get_task_env_p(&iot.tasks, i));
  set_task_env_p(&iot.tasks, nullptr, i);
  {
    pollfd x;
    x.fd = -1;
    x.events = 0;
    x.revents = 0;
    set_pollfd(&iot.fd, x, i);
  }
}

static void wake_all_io() {
  u_int i;
  for (i = 0; i < iot.nwait; i++) {
    activate(get_task_env_p(&iot.tasks, i));
    unpoll(i);
  }
  iot.nwait = 0;
}

void remove_and_wakeup(int fd) {
  u_int i = 0;
  IFDBG(D_NONE, FN; NDBG(fd, d));
  while (i < iot.nwait) {
    if (static_cast<int>(get_pollfd(&iot.fd, i).fd) == fd) {
      poll_wakeup(i);
    } else {
      i++;
    }
  }
}

task_env *stack = nullptr;

task_env *wait_io(task_env *t, int fd, int op) {
  t->time = 0.0;
  t->interrupt = 0;
  add_fd(deactivate(t), fd, op);
  return t;
}

static uint64_t send_count;
static uint64_t receive_count;
static uint64_t send_bytes;
static uint64_t receive_bytes;

#ifndef XCOM_WITHOUT_OPENSSL
result con_read(connection_descriptor const *rfd, void *buf, int n) {
  result ret = {0, 0};

  if (rfd->ssl_fd) {
    ERR_clear_error();
    ret.val = SSL_read(rfd->ssl_fd, buf, n);
    ret.funerr = to_ssl_err(SSL_get_error(rfd->ssl_fd, ret.val));
  } else {
    SET_OS_ERR(0);
    ret.val = (int)recv(rfd->fd, (xcom_buf *)buf, (size_t)n, 0);
    ret.funerr = to_errno(GET_OS_ERR);
  }
  return ret;
}
#else
result con_read(connection_descriptor const *rfd, void *buf, int n) {
  result ret = {0, 0};

  SET_OS_ERR(0);
  ret.val = (int)recv(rfd->fd, (xcom_buf *)buf, (size_t)n, 0);
  ret.funerr = to_errno(GET_OS_ERR);

  return ret;
}
#endif

result con_pipe_read(connection_descriptor const *rfd, void *buf, int n) {
  result ret = {0, 0};

  SET_OS_ERR(0);
  ret.val = (int)read(rfd->fd, (xcom_buf *)buf, (size_t)n);
  ret.funerr = to_errno(GET_OS_ERR);

  return ret;
}

/*
  It just reads no more than INT_MAX bytes. Caller should call it again for
  read more than INT_MAX bytes.

  Either the bytes written or an error number is returned to the caller through
  'ret' argument. Error number is always negative integers.
*/
int task_read(connection_descriptor const *con, void *buf, int n, int64_t *ret,
              connnection_read_method read_function) {
  DECL_ENV
  int dummy;
  ENV_INIT
  END_ENV_INIT
  END_ENV;

  result sock_ret = {0, 0};
  *ret = 0;

  assert(n >= 0);

  TASK_BEGIN
  IFDBG(D_NONE, FN; PTREXP(stack); NDBG(con->fd, d); PTREXP(buf); NDBG(n, d));

  for (;;) {
    if (con->fd <= 0) TASK_FAIL;

    sock_ret = read_function(con, buf, n);

    *ret = sock_ret.val;
    IFDBG(D_TRANSPORT, FN; NDBG(con->fd, d); NDBG(n, d); NDBG(sock_ret.val, d);
          NDBG(sock_ret.funerr, d););
    if (sock_ret.val >= 0) /* OK */
      break;
    /* If we get here, we have an error, see if we can retry, and fail if not */
    if (!can_retry_read(sock_ret.funerr)) {
      IFDBG(D_BUG, FN; PTREXP(stack); NDBG(con->fd, d); PTREXP(buf);
            NDBG(n, d));
      TASK_FAIL;
    }
    wait_io(stack, con->fd, 'r');
    TASK_YIELD;
    IFDBG(D_NONE, FN; PTREXP(stack); NDBG(con->fd, d); PTREXP(buf); NDBG(n, d));
  }

  FINALLY
  receive_count++;
  if (*ret > 0) receive_bytes += (uint64_t)(*ret);
  TASK_END;
}

#ifndef XCOM_WITHOUT_OPENSSL
result con_write(connection_descriptor const *wfd, void *buf, int n) {
  result ret = {0, 0};

  assert(n > 0);

  if (wfd->ssl_fd) {
    ERR_clear_error();
    ret.val = SSL_write(wfd->ssl_fd, buf, n);
    ret.funerr = to_ssl_err(SSL_get_error(wfd->ssl_fd, ret.val));
  } else {
    SET_OS_ERR(0);
    ret.val = (int)send(wfd->fd, (xcom_buf *)buf, (size_t)n, 0);
    ret.funerr = to_errno(GET_OS_ERR);
  }
  return ret;
}
#else
result con_write(connection_descriptor const *wfd, void *buf, int n) {
  result ret = {0, 0};

  assert(n > 0);

  SET_OS_ERR(0);
  ret.val = (int)send(wfd->fd, (xcom_buf *)buf, (size_t)n, 0);
  ret.funerr = to_errno(GET_OS_ERR);
  return ret;
}
#endif
result con_pipe_write(connection_descriptor const *wfd, void *buf, int n) {
  result ret = {0, 0};

  assert(n > 0);

  SET_OS_ERR(0);
  ret.val = (int)write(wfd->fd, (xcom_buf *)buf, (size_t)n);
  ret.funerr = to_errno(GET_OS_ERR);
  return ret;
}

/*
  It writes no more than UINT_MAX bytes which is the biggest size of
  paxos message.

  Either the bytes written or an error number is returned to the caller through
  'ret' argument. Error number is always negative integers.
*/
int task_write(connection_descriptor const *con, void *_buf, uint32_t n,
               int64_t *ret) {
  char *buf = (char *)_buf;
  DECL_ENV
  uint32_t total; /* Keeps track of number of bytes written so far */
  ENV_INIT
  END_ENV_INIT
  END_ENV;
  result sock_ret = {0, 0};

  TASK_BEGIN
  ep->total = 0;
  *ret = 0;
  while (ep->total < n) {
    IFDBG(D_NONE, FN; PTREXP(stack); NDBG(con->fd, d); NDBG(n - ep->total, u));
    for (;;) {
      if (con->fd <= 0) TASK_FAIL;
      /*
        con_write can only write messages that their sizes don't exceed
        INT_MAX bytes. We should never pass a length bigger than INT_MAX
        to con_write.
      */
      sock_ret =
          con_write(con, buf + ep->total,
                    n - ep->total >= INT_MAX ? INT_MAX : (int)(n - ep->total));
      if (sock_ret.val >= 0) /* OK */
        break;
      /* If we get here, we have an error, see if we can retry, and fail if not
       */
      if (!can_retry_write(sock_ret.funerr)) {
        IFDBG(D_NONE, FN; PTREXP(stack); NDBG(con->fd, d);
              NDBG(sock_ret.val, d));
        TASK_FAIL;
      }
      wait_io(stack, con->fd, 'w');
      IFDBG(D_NONE, FN; PTREXP(stack); NDBG(con->fd, d);
            NDBG(n - ep->total, u));
      TASK_YIELD;
    }
    if (0 == sock_ret.val) { /* We have successfully written n bytes */
      TERMINATE;
    } else if (sock_ret.val < 0) { /* Something went wrong */
      TASK_FAIL;
    } else {
      /* Add number of bytes written to total */
      ep->total += (uint32_t)sock_ret.val;
    }
  }
  assert(ep->total == n);
  TASK_RETURN(ep->total);
  FINALLY
  send_count++;
  send_bytes += ep->total;
  TASK_END;
}

int unblock_fd(int fd) {
#if !defined(_WIN32)
  int x = fcntl(fd, F_GETFL, 0);
  x = fcntl(fd, F_SETFL, x | O_NONBLOCK);
#else
  /**
   * On windows we toggle the FIONBIO flag directly
   *
   * Undocumented in MSDN:
   * Calling ioctlsocket(FIONBIO) to an already set state
   * seems to return -1 and WSAGetLastError() == 0.
   */
  u_long nonblocking = 1; /** !0 == non-blocking */
  int x = ioctlsocket(fd, FIONBIO, &nonblocking);
#endif
  return x;
}

int block_fd(int fd) {
#if !defined(_WIN32)
  int x = fcntl(fd, F_GETFL, 0);
  x = fcntl(fd, F_SETFL, x & ~O_NONBLOCK);
#else
  /**
   * On windows we toggle the FIONBIO flag directly.
   *
   * Undocumented in MSDN:
   * Calling ioctlsocket(FIONBIO) to an already set state seems to
   * return -1 and WSAGetLastError() == 0.
   */
  u_long nonblocking = 0; /** 0 == blocking */
  int x = ioctlsocket(fd, FIONBIO, &nonblocking);
#endif
  return x;
}

#ifndef AGGRESSIVE_SWEEP
/* purecov: begin deadcode */
int is_only_task() { return link_first(&tasks) == link_last(&tasks); }
/* purecov: end */
#endif

static task_env *first_runnable() { return (task_env *)link_first(&tasks); }

static task_env *next_task(task_env *t) {
  return (task_env *)link_first(&t->l);
}

static int is_task_head(task_env *t) { return &t->l == &tasks; }

static int msdiff(double time) {
  return (int)(1000.5 * (first_delayed()->time - time));
}

static should_exit_getter get_should_exit;

void set_should_exit_getter(should_exit_getter x) { get_should_exit = x; }

static double idle_time = 0.0;
void task_loop() {
  task_env *t = nullptr;
  /* While there are tasks */
  for (;;) {
    /* check forced exit callback */
    if (get_should_exit()) {
      terminate_and_exit();
    }

    t = first_runnable();
    /* While runnable tasks */
    while (runnable_tasks()) {
      task_env *next = next_task(t);
      if (!is_task_head(t)) {
        /* IFDBG(D_NONE, FN; PTREXP(t); STRLIT(t->name ? t->name : "TASK WITH NO
         * NAME")); */
        stack = t;
        assert(stack);
        assert(t->terminate != TERMINATED);
        {
          /* double when = seconds(); */
          int val = 0;
          assert(t->func);
          assert(stack == t);
          val = t->func(t->arg);
          assert(ash_nazg_gimbatul.type == TYPE_HASH("task_env"));
          if (!val) { /* Is task finished? */
            deactivate(t);
            t->terminate = TERMINATED;
            task_unref(t);
            stack = nullptr;
          }
        }
      }
      t = next;
    }
    if (active_tasks <= 0) break;
/* When we get here, there are no runnable tasks left.
       Wait until something happens.
    */
#ifdef DEBUG_TASKS
    IFDBG(D_NONE, FN; STRLIT("waiting tasks time "); NDBG(seconds(), f);
          NDBG(iot.nwait, d); NDBG(task_time_q.curn));
#endif
    {
      double time = seconds();
      if (delayed_tasks()) {
        int ms = msdiff(time);
        if (ms > 0) {
          if (the_app_xcom_cfg != nullptr &&
              the_app_xcom_cfg->m_poll_spin_loops) {
            u_int busyloop;
            for (busyloop = 0; busyloop < the_app_xcom_cfg->m_poll_spin_loops;
                 busyloop++) {
              ADD_WAIT_EV(task_now(), __FILE__, __LINE__, "poll_wait(ms)", 0);
              if (poll_wait(0)) /*Just poll */
                goto done_wait;
              ADD_WAIT_EV(task_now(), __FILE__, __LINE__, "poll_wait(ms) end",
                          0);
              thread_yield();
            }
          }
          ADD_WAIT_EV(task_now(), __FILE__, __LINE__, "poll_wait(ms)", ms);
          poll_wait(ms); /* Wait at most ms milliseconds and poll for IO */
          ADD_WAIT_EV(task_now(), __FILE__, __LINE__, "poll_wait(ms) end", ms);
        }
      done_wait:
        /* While tasks with expired timers */
        while (delayed_tasks() && msdiff(time) <= 0) {
          task_env *delayed_task = extract_first_delayed(); /* May be NULL */
          if (delayed_task) activate(delayed_task); /* Make it runnable */
        }
      } else {
        ADD_T_EV(task_now(), __FILE__, __LINE__, "poll_wait(-1)");
        poll_wait(-1); /* Wait and poll for IO */
        ADD_T_EV(seconds(), __FILE__, __LINE__, "poll_wait(-1) end");
      }
      idle_time += seconds() - time;
    }
  }
  task_sys_deinit();
}

#define STAT_INTERVAL 1.0

#if 0

/*
  This was disabled to prevent unnecessary build warnings.

  TODO:
  Needs to be assessed whether it should be removed altogether.
 */

static int statistics_task(task_arg arg) {
  DECL_ENV
  double next;
  END_ENV;
  TASK_BEGIN(void) arg;
  idle_time = 0.0;
  send_count = 0;
  receive_count = 0;
  send_bytes = 0;
  receive_bytes = 0;
  ep->next = seconds() + STAT_INTERVAL;
  TASK_DELAY_UNTIL(ep->next);
  for (;;) {
    G_DEBUG(
        "task system idle %f send/s %f receive/s %f send b/s %f receive b/s %f",
        (idle_time / STAT_INTERVAL) * 100.0, send_count / STAT_INTERVAL,
        receive_count / STAT_INTERVAL, send_bytes / STAT_INTERVAL,
        receive_bytes / STAT_INTERVAL);
    idle_time = 0.0;
    send_count = 0;
    receive_count = 0;
    send_bytes = 0;
    receive_bytes = 0;
    ep->next += STAT_INTERVAL;
    TASK_DELAY_UNTIL(ep->next);
  }
  FINALLY
  IFDBG(D_BUG, FN; STRLIT(" shutdown "));
  TASK_END;
}
#endif

static void init_task_vars() {
  stack = nullptr;
  task_errno = 0;
}

void task_sys_init() {
  xcom_init_clock(&task_timer);
  IFDBG(D_NONE, FN; NDBG(FD_SETSIZE, d));
  init_task_vars();
  link_init(&tasks, TYPE_HASH("task_env"));
  link_init(&free_tasks, TYPE_HASH("task_env"));
  link_init(&ash_nazg_gimbatul, TYPE_HASH("task_env"));
  iotasks_init(&iot);
  /* task_new(statistics_task, null_arg, "statistics_task", 1); */
}

static void task_sys_deinit() {
  IFDBG(D_NONE, FN);
  iotasks_deinit(&iot);
}

void set_task(task_env **p, task_env *t) {
  if (t) task_ref(t);
  if (*p) task_unref(*p);
  *p = t;
}

const char *task_name() { return stack ? stack->name : "idle"; }

#ifdef TASK_EVENT_TRACE
static struct {
  int n;
  int front;
  int rear;
  task_event q[MAX_TASK_EVENT];
} task_events;

static inline int addone(int i) { return ((i + 1) % MAX_TASK_EVENT); }

/* Is queue empty?  */
static inline int event_empty() { return task_events.n <= 0; }

/* Is queue full?  */
static inline int event_full() { return task_events.n >= MAX_TASK_EVENT; }

/* Insert in queue  */
static inline void event_insert(task_event s) {
  if (event_full()) {
    task_events.front = addone(task_events.front);
  } else {
    task_events.n++;
  }
  task_events.q[task_events.rear] = s;
  task_events.rear = addone(task_events.rear);
}

static const task_event null_event = {{(arg_type)0, {0}}, 0};

/* Extract first from queue  */
static inline task_event event_extract() {
  if (!event_empty()) {
    task_event ret = task_events.q[task_events.front];
    task_events.front = addone(task_events.front);
    task_events.n--;
    return ret;
  } else {
    return null_event;
  }
}

#ifdef _WIN32
#define snprintf(...) _snprintf(__VA_ARGS__)
#endif

/* purecov: begin deadcode */
void ev_print(task_event te) {
  enum { bufsize = 10000 };
  static char buf[bufsize];
  static size_t pos = 0;

  switch (te.arg.type) {
    case a_int:
      pos += (size_t)snprintf(&buf[pos], bufsize - pos, "%d", te.arg.val.i);
      break;
    case a_long:
      pos += (size_t)snprintf(&buf[pos], bufsize - pos, "%ld", te.arg.val.l);
      break;
    case a_uint:
      if (te.flag & EVENT_DUMP_HEX)
        pos += (size_t)snprintf(&buf[pos], bufsize - pos, "%x", te.arg.val.u_i);
      else
        pos += (size_t)snprintf(&buf[pos], bufsize - pos, "%u", te.arg.val.u_i);
      break;
    case a_ulong:
      if (te.flag & EVENT_DUMP_HEX)
        pos +=
            (size_t)snprintf(&buf[pos], bufsize - pos, "%lx", te.arg.val.u_l);
      else
        pos +=
            (size_t)snprintf(&buf[pos], bufsize - pos, "%lu", te.arg.val.u_l);
      break;
    case a_ulong_long:
      if (te.flag & EVENT_DUMP_HEX)
        pos +=
            (size_t)snprintf(&buf[pos], bufsize - pos, "%llx", te.arg.val.u_ll);
      else
        pos +=
            (size_t)snprintf(&buf[pos], bufsize - pos, "%llu", te.arg.val.u_ll);
      break;
    case a_float:
      pos += (size_t)snprintf(&buf[pos], bufsize - pos, "%f", te.arg.val.f);
      break;
    case a_double:
      pos += (size_t)snprintf(&buf[pos], bufsize - pos, "%f", te.arg.val.d);
      break;
    case a_void:
      pos += (size_t)snprintf(&buf[pos], bufsize - pos, "%p", te.arg.val.v);
      break;
    case a_string:
      pos += (size_t)snprintf(&buf[pos], bufsize - pos, "%s", te.arg.val.s);
      break;
    case a_end:
      if (pos) G_TRACE("pid %d xcom_id %x %s", xpid(), get_my_xcom_id(), buf);
      pos = 0;
      break;
    default:
      pos += (size_t)snprintf(&buf[pos], bufsize - pos, "???");
  }
  if (te.flag & EVENT_DUMP_PAD && bufsize > pos) {
    buf[pos++] = ' ';
  }
  buf[pos] = 0;
}

void add_event(int flag, task_arg te) {
  task_event t;
  t.arg = te;
  t.flag = flag;
  event_insert(t);
}

#if defined(WIN32)
static inline int pathsep(char const **x) {
  int ret = ('\\' == **x);
  (*x)++;
  if (ret) {
    ret = ('\\' == **x);
    if (ret) {
      (*x)++;
    }
  }
  return ret;
}
#else
static inline int pathsep(char const **x) {
  int ret = ('/' == **x);
  (*x)++;
  return ret;
}
#endif

/* Return last part of path by scanning from beginning */
static char const *ev_filename(char const *path) {
  char const *ret = path;
  char const *p = path;
  while (*p) {
    if (pathsep(&p)) ret = p;
  }
  return ret;
}

void add_base_event(double when, char const *file, int state) {
  static double t = 0.0;
  char const *fn = ev_filename(file);

  add_event(EVENT_DUMP_PAD, double_arg(when));
  add_event(EVENT_DUMP_PAD, double_arg(when - t));
  t = when;
  add_event(0, string_arg(fn));
  add_event(0, string_arg(":"));
  add_event(EVENT_DUMP_PAD, int_arg(state));
}

void add_task_event(double when, char const *file, int state,
                    char const *what) {
  add_base_event(when, file, state);
  add_event(EVENT_DUMP_PAD, string_arg(what));
  add_event(EVENT_DUMP_PAD, end_arg());
}

void add_wait_event(double when, char const *file, int state, char const *what,
                    int milli) {
  add_base_event(when, file, state);
  add_event(EVENT_DUMP_PAD, string_arg(what));

  add_event(EVENT_DUMP_PAD, string_arg("milli"));
  add_event(EVENT_DUMP_PAD, int_arg(milli));
  add_event(EVENT_DUMP_PAD, end_arg());
}

void dump_task_events() {
  if (!event_empty()) {
    G_DEBUG(
        "before dump task_events.front %d task_events.rear %d task_events.n %d",
        task_events.front, task_events.rear, task_events.n);
    while (!event_empty()) {
      ev_print(event_extract());
    }
    G_DEBUG(
        "after dump task_events.front %d task_events.rear %d task_events.n %d",
        task_events.front, task_events.rear, task_events.n);
  }
}

void reset_task_events() {
  task_events.front = task_events.rear = task_events.n = 0;
}
/* purecov: end */

#endif
