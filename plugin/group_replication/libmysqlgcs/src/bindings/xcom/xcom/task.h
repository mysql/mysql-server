/* Copyright (c) 2012, 2023, Oracle and/or its affiliates.

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

#ifndef TASK_H
#define TASK_H

#include "xcom/xcom_profile.h"

#include <assert.h>
#include <errno.h>
#ifndef XCOM_STANDALONE
#include "my_compiler.h"
#endif
#include "xcom/simset.h"
#include "xcom/task_arg.h"
#include "xcom/x_platform.h"
#include "xcom/xcom_common.h"

#include "xcom/node_connection.h"
#include "xcom/result.h"

/** \file
        Rudimentary task system in portable C, based on Tom Duff's switch-based
   coroutine trick
        and a stack of environment structs. (continuations?)
        Nonblocking IO and event handling need to be rewritten for each new OS.
*/

#ifdef TASK_EVENT_TRACE
void add_base_event(double when, char const *file, int state);
#define ADD_BASE_EVENT                               \
  {                                                  \
    add_base_event(seconds(), __FILE__, __LINE__);   \
    add_event(EVENT_DUMP_PAD, string_arg(__func__)); \
  }

#define ADD_DBG(d, x)                     \
  if (do_dbg(d)) {                        \
    ADD_BASE_EVENT x;                     \
    add_event(EVENT_DUMP_PAD, end_arg()); \
  }

#define ADD_EVENTS(x) ADD_DBG(D_BUG, x)

#define ADD_T_EV(when, file, state, what)
#define ADD_WAIT_EV(when, file, state, what, milli)

#else
#define ADD_EVENTS(x)
#define ADD_DBG(d, x)
#define ADD_T_EV(when, file, state, what)
#define ADD_WAIT_EV(when, file, state, what, milli)
#endif

static inline void set_int_arg(task_arg *arg, int value) {
  arg->type = a_int;
  arg->val.i = value;
}

static inline int get_int_arg(task_arg arg) {
  assert(arg.type == a_int);
  return arg.val.i;
}

static inline void set_long_arg(task_arg *arg, long value) {
  arg->type = a_long;
  arg->val.l = value;
}

static inline long get_long_arg(task_arg arg) {
  assert(arg.type == a_long);
  return arg.val.l;
}

static inline void set_uint_arg(task_arg *arg, unsigned int value) {
  arg->type = a_uint;
  arg->val.u_i = value;
}

static inline unsigned int get_uint_arg(task_arg arg) {
  assert(arg.type == a_uint);
  return arg.val.u_i;
}

static inline void set_ulong_arg(task_arg *arg, unsigned long value) {
  arg->type = a_ulong;
  arg->val.u_l = value;
}

static inline void set_ulong_long_arg(task_arg *arg, unsigned long long value) {
  arg->type = a_ulong_long;
  arg->val.u_ll = value;
}

static inline unsigned long get_ulong_arg(task_arg arg) {
  assert(arg.type == a_ulong);
  return arg.val.u_l;
}

static inline unsigned long long get_ulong_long_arg(task_arg arg) {
  assert(arg.type == a_ulong_long);
  return arg.val.u_l;
}

static inline void set_float_arg(task_arg *arg, float value) {
  arg->type = a_float;
  arg->val.f = value;
}

static inline float get_float_arg(task_arg arg) {
  assert(arg.type == a_float);
  return arg.val.f;
}

static inline void set_double_arg(task_arg *arg, double value) {
  arg->type = a_double;
  arg->val.d = value;
}

static inline double get_double_arg(task_arg arg) {
  assert(arg.type == a_double);
  return arg.val.d;
}

static inline void set_string_arg(task_arg *arg, char const *value) {
  arg->type = a_string;
  arg->val.s = value;
}

static inline void set_void_arg(task_arg *arg, void *value) {
  arg->type = a_void;
  arg->val.v = value;
}

static inline char const *get_string_arg(task_arg arg) {
  assert(arg.type == a_string);
  return (char const *)arg.val.v;
}

static inline void *get_void_arg(task_arg arg) {
  assert(arg.type == a_void);
  return arg.val.v;
}

static inline task_arg int_arg(int i) {
  task_arg retval;
  set_int_arg(&retval, i);
  return retval;
}

static inline task_arg uint_arg(unsigned int i) {
  task_arg retval;
  set_uint_arg(&retval, i);
  return retval;
}

static inline task_arg ulong_arg(unsigned long l) {
  task_arg retval;
  set_ulong_arg(&retval, l);
  return retval;
}

static inline task_arg ulong_long_arg(unsigned long long ll) {
  task_arg retval;
  set_ulong_long_arg(&retval, ll);
  return retval;
}

static inline task_arg double_arg(double i) {
  task_arg retval;
  set_double_arg(&retval, i);
  return retval;
}

static inline task_arg string_arg(char const *v) {
  task_arg retval;
  set_string_arg(&retval, v);
  return retval;
}

static inline task_arg void_arg(void *v) {
  task_arg retval;
  set_void_arg(&retval, v);
  return retval;
}

static inline task_arg end_arg() {
  task_arg retval;
  retval.type = a_end;
  return retval;
}

/* Combined environment pointer and state variable */
struct task_ptr {
  int state;
  void *ptr;
};

typedef struct task_ptr TaskAlign;

struct task_env;

/* All task functions  have this signature */
typedef int (*task_func)(task_arg arg);

/* Increase this if tasks need bigger stacks, or use a linked list for the stack
 */
#define TASK_POOL_ELEMS 1000

typedef enum terminate_enum {
  RUN = 0,
  KILL = 1,
  TERMINATED = 2
} terminate_enum;

/* A complete task.
   The buffer buf is used for a heap which grows upwards and a stack which grows
   downwards.
   The stack contains pointers to environment structs, which are allocated from
   the heap.
 */
struct task_env {
  linkage l;    /* Used for runnable tasks and wait queues */
  linkage all;  /* Links all tasks */
  int heap_pos; /* Index in time priority queue, necessary for efficient removal
                 */
  terminate_enum
      terminate;        /* Set this and activate task to make it terminate */
  int refcnt;           /* Number of references to task */
  int taskret;          /* Return value from task function */
  task_func func;       /* The task function */
  task_arg arg;         /* Argument passed to the task */
  const char *name;     /* The task name */
  TaskAlign *where;     /* High water mark in heap */
  TaskAlign *stack_top; /* The stack top */
  TaskAlign *sp;        /* The current stack pointer */
  double time;          /* Time when the task should be activated */
  TaskAlign buf[TASK_POOL_ELEMS]; /* Heap and stack */
  int debug;
  int waitfd;
  int interrupt; /* Set if timeout while waiting */
};

typedef struct task_env task_env;

#define MAXTASKS 1000

/* Priority queue of task_env */
struct task_queue {
  int curn;
  task_env *x[MAXTASKS + 1];
};
typedef struct task_queue task_queue;

#define _ep ((struct env *)(stack->sp->ptr))

#define TASK_ALLOC(pool, type) (task_allocate(pool, (unsigned int)sizeof(type)))

#if 0
#define TASK_DEBUG(x)                                            \
  if (stack->debug) {                                            \
    IFDBG(D_NONE, FN; STRLIT(x " task "); PTREXP((void *)stack); \
          STRLIT(stack->name); NDBG(stack->sp->state, d));       \
  }
#else
#define TASK_DEBUG(x)
#endif

/* Place cleanup code after this label */
#define FINALLY \
  task_cleanup:

/* We have reached the top of the stack when sp == stack_top + 1 since the stack
 * grows downwards */
#define ON_STACK_TOP (stack->sp == stack->stack_top + 1)

/* If we have climbed the stack to the top, check the terminate flag.
   Execute cleanup code and exit this stack frame if terminate is set.
 */
#define TERM_CHECK \
  if (ON_STACK_TOP && stack->terminate) goto task_cleanup

#define TERMINATE goto task_cleanup

/* Switch on task state. The first time, allocate a new stack frame and check
 * for termination */
#define TASK_BEGIN                                            \
  /* assert(ep);   */                                         \
  ADD_DBG(                                                    \
      D_TASK,                                                 \
      if (stack->sp->state) {                                 \
        add_event(EVENT_DUMP_PAD, string_arg("state"));       \
        add_event(EVENT_DUMP_PAD, int_arg(stack->sp->state)); \
      } add_event(EVENT_DUMP_PAD, string_arg("TASK_BEGIN"));  \
      add_event(EVENT_DUMP_PAD, void_arg(stack)););           \
  TASK_DEBUG("TASK_BEGIN");                                   \
  switch (stack->sp->state) {                                 \
    case 0:                                                   \
      pushp(stack, TASK_ALLOC(stack, struct env));            \
      ep = _ep;                                               \
      assert(ep);                                             \
      ep->init();                                             \
      TERM_CHECK;

/* This stack frame is finished, deallocate it and return 0 to signal exit */
#define TASK_END                                              \
  ADD_DBG(                                                    \
      D_TASK,                                                 \
      if (stack->sp->state) {                                 \
        add_event(EVENT_DUMP_PAD, string_arg("state"));       \
        add_event(EVENT_DUMP_PAD, int_arg(stack->sp->state)); \
      } add_event(EVENT_DUMP_PAD, string_arg("TASK_END"));    \
      add_event(EVENT_DUMP_PAD, void_arg(stack)););           \
  TASK_DEBUG("TASK_END");                                     \
  stack->sp->state = 0;                                       \
  stack->where = (TaskAlign *)stack->sp->ptr;                 \
  assert(stack->where);                                       \
  popp(stack);                                                \
  return 0;                                                   \
  }                                                           \
  return 0

/* Assign a return value, execute cleanup code, and exit this stack frame */
#define TASK_RETURN(x) \
  {                    \
    *ret = (x);        \
    goto task_cleanup; \
  }

#define TASK_DUMP_ERR                                          \
  if (errno || SOCK_ERRNO || task_errno) {                     \
    IFDBG(D_NONE, FN; NDBG(errno, d); STREXP(strerror(errno)); \
          NDBG(SOCK_ERRNO, d); STREXP(strerror(SOCK_ERRNO));   \
          NDBG(task_errno, d); STREXP(strerror(task_errno)));  \
  }

/* Assign -1 as exit code, execute cleanup code, and exit this stack frame */
#define TASK_FAIL                                                          \
  {                                                                        \
    *ret = (-1);                                                           \
    TASK_DUMP_ERR;                                                         \
    IFDBG(D_NONE, FN; STRLIT("TASK_FAIL"));                                \
    ADD_DBG(D_TASK, add_event(EVENT_DUMP_PAD, string_arg("task failed"))); \
    goto task_cleanup;                                                     \
  }

/* Capture the line number in the state variable, and return.
   When called again (after the switch label), check for termination.
*/
#define TASK_YIELD                     \
  {                                    \
    TASK_DEBUG("TASK_YIELD");          \
    stack->sp->state = __LINE__;       \
    return 1;                          \
    case __LINE__:                     \
      TASK_DEBUG("RETURN FROM YIELD"); \
      ep = _ep;                        \
      assert(ep);                      \
      TERM_CHECK;                      \
  }

#define TASK_DEACTIVATE            \
  {                                \
    TASK_DEBUG("TASK_DEACTIVATE"); \
    task_deactivate(stack);        \
    TASK_YIELD;                    \
  }

/* Put the task in the queue of tasks waiting for timeout, then yield.
   Wait until current time + t seconds.
 */
#define TASK_DELAY(t)                \
  {                                  \
    TASK_DEBUG("TASK_DELAY");        \
    task_delay_until(seconds() + t); \
    TASK_YIELD;                      \
  }

/* Put the task in the queue of tasks waiting for timeout, then yield.
   Wait until t.
 */
#define TASK_DELAY_UNTIL(t)         \
  {                                 \
    TASK_DEBUG("TASK_DELAY_UNTIL"); \
    task_delay_until(t);            \
    TASK_YIELD;                     \
  }

/* Put the task in a wait queue, then yield */
#define TASK_WAIT(queue)     \
  {                          \
    TASK_DEBUG("TASK_WAIT"); \
    task_wait(stack, queue); \
    TASK_YIELD;              \
  }

/* Put the task in a wait queue with timeout, then yield */
#define TIMED_TASK_WAIT(queue, t)      \
  {                                    \
    TASK_DEBUG("TIMED_TASK_WAIT");     \
    task_delay_until(seconds() + (t)); \
    task_wait(stack, queue);           \
    TASK_YIELD;                        \
  }

/* A channel has a queue of data elements and a queue of waiting tasks */
struct channel {
  linkage data;
  linkage queue;
};

typedef struct channel channel;

/* Channel construction */
channel *channel_init(channel *c, unsigned int type);

/* Put data in channel. No wait, since channels can never be full */
void channel_put(channel *c, linkage *data); /* Append to queue */
void channel_put_front(channel *c,
                       linkage *data); /* Insert in front of queue */

/* Wait until there is data in the channel, then extract and cast to type */
#define CHANNEL_GET(channel, ptr, type)                            \
  {                                                                \
    while (link_empty(&(channel)->data)) {                         \
      TASK_WAIT(&(channel)->queue);                                \
    }                                                              \
    *(ptr) = (type *)link_extract_first(&(channel)->data);         \
    IFDBG(D_TRANSPORT, FN; STRLIT("CHANNEL_GET "); PTREXP(*(ptr)); \
          PTREXP(&((channel)->data)));                             \
  }

#define CHANNEL_PEEK(channel, ptr, type)           \
  {                                                \
    while (link_empty(&(channel)->data)) {         \
      TASK_WAIT(&(channel)->queue);                \
    }                                              \
    *(ptr) = (type *)link_first(&(channel)->data); \
  }

#define CHANNEL_GET_REVERSE(channel, ptr, type)           \
  {                                                       \
    while (link_empty(&(channel)->data)) {                \
      TASK_WAIT(&(channel)->queue);                       \
    }                                                     \
    *(ptr) = (type *)link_extract_last(&(channel)->data); \
  }

/*
   The first time, reset the state of the stack frame of the called function.
   Keep calling the function until it returns 0, which signals exit.
   Yield after each call.
 */
#define TASK_CALL(funcall)            \
  {                                   \
    reset_state(stack);               \
    TASK_DEBUG("BEFORE CALL");        \
    do {                              \
      stack->sp--;                    \
      stack->taskret = funcall;       \
      stack->sp++;                    \
      TERM_CHECK;                     \
      if (stack->taskret) TASK_YIELD; \
    } while (stack->taskret);         \
    TASK_DEBUG("AFTER CALL");         \
  }

/* Define the typeless struct which is the container for all variables in the
 * stack frame */
#define DECL_ENV struct env {
/*Define a code block where we can init ENV variables. It only works nested
  within DECL_ENV
*/
#define ENV_INIT void init() {
/*Ends the default initialization block*/
#define END_ENV_INIT }

/* Define a pointer to the environment struct */
#define END_ENV \
  }             \
  ;             \
  [[maybe_unused]] struct env *ep

/* Try to lock a fd for read or write.
   Yield and spin until it succeeds.
*/
#define LOCK_FD(fd, op)                                                        \
  {                                                                            \
    while (!lock_fd(                                                           \
        fd, stack,                                                             \
        op)) { /* Effectively a spin lock, but should not happen very often */ \
      wait_io(stack, fd, op);                                                  \
      TASK_YIELD;                                                              \
      /* TASK_DELAY(1.0);     */                                               \
    }                                                                          \
  }

/* Unlock a fd */
#define UNLOCK_FD(fd, op) unlock_fd(fd, stack, op)

#ifdef TASK_EVENT_TRACE
enum { EVENT_DUMP_PAD = 1, EVENT_DUMP_HEX = 2 };

struct task_event {
  task_arg arg;
  int flag;
};

typedef struct task_event task_event;

void add_event(int flag, task_arg te);
void add_task_event(double when, char const *file, int state, char const *what);
void add_wait_event(double when, char const *file, int state, char const *what,
                    int milli);
void dump_task_events();
void reset_task_events();
#endif

/* The current task */
extern task_env *stack;

extern void *task_allocate(task_env *p, unsigned int bytes);
extern void reset_state(task_env *p);
extern void pushp(task_env *p, void *ptr);
extern void popp(task_env *p);

extern double seconds();  /* Return time as double */
extern double task_now(); /* Return result of last call to seconds() */
extern void task_delay_until(double time);

extern int unblock_fd(int fd);
extern int block_fd(int fd);

typedef result (*connnection_read_method)(connection_descriptor const *rfd,
                                          void *buf, int n);

extern result con_read(connection_descriptor const *rfd, void *buf, int n);
extern result con_pipe_read(connection_descriptor const *rfd, void *buf, int n);

extern int task_read(connection_descriptor const *con, void *buf, int n,
                     int64_t *ret,
                     connnection_read_method read_function = con_read);

typedef result (*connnection_write_method)(connection_descriptor const *rfd,
                                           void *buf, int n);
result con_write(connection_descriptor const *wfd, void *buf, int n);
result con_pipe_write(connection_descriptor const *wfd, void *buf, int n);

extern int task_write(connection_descriptor const *con, void *buf, uint32_t n,
                      int64_t *ret);
extern int is_locked(int fd);
extern int lock_fd(int fd, task_env *t, int lock);
extern int unlock_fd(int fd, task_env *t, int lock);

extern void task_sys_init();
extern task_env *task_new(task_func func, task_arg arg, const char *name,
                          int debug);
#define xstr(s) #s
/* #define task_new(func, arg) _task_new(func, arg, __FILE__":" xstr(__LINE__))
 */
extern void task_loop();
extern void task_wait(task_env *t, linkage *queue);
extern void task_wakeup(linkage *queue);
extern task_env *task_terminate(task_env *t);
extern void set_task(task_env **p, task_env *t);
extern void task_terminate_all();
extern void remove_and_wakeup(int i);
extern int task_errno;
extern int is_only_task();
extern task_env *task_activate(task_env *t);
extern task_env *task_deactivate(task_env *t);
extern const char *task_name();
extern task_env *wait_io(task_env *t, int fd, int op);

extern result con_write(connection_descriptor const *wfd, void *buf, int n);
extern result set_nodelay(int fd);

extern task_env *timed_wait_io(task_env *t, int fd, int op, double timeout);

extern xcom_proto const my_min_xcom_version; /* The minimum protocol version I
                                                am able to understand */
extern xcom_proto const
    my_xcom_version; /* The maximum protocol version I am able to understand */

#endif
