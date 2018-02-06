/* Copyright (c) 2012, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @file task.c
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

#include "x_platform.h"

#ifdef XCOM_HAVE_OPENSSL
#include "openssl/ssl.h"
#include "openssl/err.h"
#endif

#include <limits.h>
#include <stdlib.h>
#include "xcom_vp.h"
#include "node_connection.h"

#ifndef WIN
#include <arpa/inet.h>
#endif

#include <sys/types.h>
#include <sys/time.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

#include "task_debug.h"
#include "task_net.h"
#include "simset.h"
#include "task_os.h"
#include "task.h"
#include "xcom_cfg.h"
#ifndef _WIN32
#include <sys/poll.h>
#endif

#include "retry.h"
#include "xdr_utils.h"

extern char *pax_op_to_str(int x);

task_arg null_arg = {a_end,{0}};

struct iotasks ;
typedef struct iotasks iotasks;

typedef struct {
  u_int pollfd_array_len;
  pollfd *pollfd_array_val;
} pollfd_array;

typedef task_env* task_env_p;

typedef struct {
  u_int task_env_p_array_len;
  task_env_p *task_env_p_array_val;
} task_env_p_array;

define_xdr_funcs(pollfd)
define_xdr_funcs(task_env_p)

struct iotasks {
  int nwait;
  pollfd_array fd;
  task_env_p_array tasks;
};

int	task_errno = 0;
static task_env *extract_first_delayed();
static task_env *task_ref(task_env *t);
static task_env *task_unref(task_env *t);
static void wake_all_io();
static void task_sys_deinit();

/* Return time as seconds */
static double	_now = 0.0;

double	task_now()
{
	return _now;
}


#ifdef WIN
int gettimeofday(struct timeval * tp, struct timezone * tzp)
{
    static uint64_t const EPOCH = ((uint64_t) 116444736000000000ULL);

    SYSTEMTIME  system_time;
    FILETIME    file_time;
    uint64_t    time;

    GetSystemTime( &system_time );
    SystemTimeToFileTime( &system_time, &file_time );
    time =  ((uint64_t)file_time.dwLowDateTime )      ;
    time += ((uint64_t)file_time.dwHighDateTime) << 32;

    tp->tv_sec  = (long) ((time - EPOCH) / 10000000L);
    tp->tv_usec = (long) (system_time.wMilliseconds * 1000);
    return 0;
}
#endif

double	seconds(void)
{
	struct timeval tv;
	if (gettimeofday(&tv, 0) < 0)
		return - 1.0;
	return _now = (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
}


#ifdef NOTDEF
static void task_queue_init(task_queue *q)
{
       q->curn = 0;
}

static void task_queue_debug(task_queue *q)
{
	int	i;
	GET_GOUT;
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


static int is_heap(task_queue *q)
{
	if (q->curn) {
		int	i;
		for (i = q->curn; i > 1; i--) {
			if ((q->x[i]->time < q->x[i/2]->time) ||
			    (q->x[i]->heap_pos != i)) {
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

static int     task_queue_full(task_queue *q)
{
       /* assert(is_heap(q)); */
       return q->curn >= MAXTASKS;
}


#endif

#define FIX_POS(i) q->x[i]->heap_pos = (i)
/* #define TASK_SWAP(x,y) { task_env *tmp = (x); (x) = (y); (y) = (tmp); } */
#define TASK_SWAP(i,j) { task_env *tmp = q->x[i]; q->x[i] = q->x[j]; q->x[j] = tmp; FIX_POS(i); FIX_POS(j); }
#define TASK_MOVE(i,j) { q->x[i] = q->x[j]; FIX_POS(i); }
/* Put the task_env* at index n in its right place when Heap(1,n-1) */
static void task_queue_siftup(task_queue *q, int n)
{
	int	i = n;
	int	p;
	assert(n >= 0);
	/* Heap(1,n-1) */
	for (; ; ) {
		if (i == 1)
			break; /* Reached root */
		p = i / 2; /* Find parent */
		if (q->x[p]->time <= q->x[i]->time)
			break; /* We have reached correct place */
		TASK_SWAP(p, i);
		i = p;
	}
	/* Heap(1,n) */
}


/*   Put the task_env* at index l in its right place when Heap(l+1,n) */
static void task_queue_siftdown(task_queue *q, int l, int n)
{
	int	i = l;
	int	c;
	assert(n >= 0);
	/* Heap(l+1,,n) */
	for (; ; ) {
		c = 2 * i; /* First child */
		if (c > n)
			break; /* Outside heap */
		if (c + 1 <= n) /* We have second child */
			if (q->x[c+1]->time < q->x[c]->time) /* Select lesser child */
				c++;
		if (q->x[i]->time <= q->x[c]->time)
			break; /* We have reached correct place */
		TASK_SWAP(c, i);
		i = c;
	}
	/* Heap(l,n) */
}


/* Remove any element from the heap */
static task_env *task_queue_remove(task_queue *q, int i)
{
	task_env * tmp = q->x[i]; /* Will return this */
	assert(q->curn);
	/* assert(is_heap(q)); */
	MAY_DBG(FN; STRLIT("task_queue_remove "); NDBG(i, d));
	/* task_queue_debug(q); */
	TASK_MOVE(i, q->curn); /* Fill the hole */
	q->curn--; /* Heap is now smaller */
	/* Re-establish heap property */
	if (q->curn) {
		int	p = i / 2;
		if (p && q->x[p]->time > q->x[i]->time)
			task_queue_siftup(q, i);
		else
			task_queue_siftdown(q, i, q->curn);
	}
	/* task_queue_debug(q); */
	/* assert(is_heap(q)); */
	tmp->heap_pos = 0;
	return task_unref(tmp);
}


/* Insert task_env * in queue */
static void task_queue_insert(task_queue *q, task_env *t)
{
	assert(t->heap_pos == 0);
	assert(q->curn < MAXTASKS);
	/* assert(is_heap(q)); */
	q->curn++;
	q->x[q->curn] = t;
	FIX_POS(q->curn);
	/* Heap(1,n-1) */
	task_queue_siftup(q, q->curn);
	/* Heap(1,n) */
	/* assert(is_heap(q)); */
}


static int	task_queue_empty(task_queue *q)
{
	/* assert(is_heap(q)); */
	return q->curn < 1;
}


static task_env *task_queue_min(task_queue *q)
{
	/* assert(is_heap(q)); */
	assert(q->curn >= 1);
	return q->x[1];
}


/* Extract first task_env * from queue */
static task_env *task_queue_extractmin(task_queue *q)
{
	task_env * tmp;
	assert(q);
	assert(q->curn >= 1);
	/* assert(is_heap(q)); */
	/* task_queue_debug(q); */
	tmp = q->x[1];
	TASK_MOVE(1, q->curn);
	q->x[q->curn] = 0;
	q->curn--;
	/* Heap(2,n) */
	if (q->curn)
		task_queue_siftdown(q, 1, q->curn);
	/* Heap(1,n) */
	/* task_queue_debug(q); */
	/* assert(is_heap(q)); */
	tmp->heap_pos = 0;
	return tmp;
}


static linkage ash_nazg_gimbatul = {0,&ash_nazg_gimbatul,&ash_nazg_gimbatul}; /* One ring to bind them all */

static void task_init(task_env *p);
void *task_allocate(task_env *p, unsigned int bytes);
/**
   Initialize task memory
*/
static void task_init(task_env *t)
{
	link_init(&t->l, type_hash("task_env"));
	link_init(&t->all, type_hash("task_env"));
	t->heap_pos = 0;
	/* assert(ash_nazg_gimbatul.suc > (linkage*)0x8000000); */
	/* assert(ash_nazg_gimbatul.pred > (linkage*)0x8000000); */
	assert(ash_nazg_gimbatul.type == type_hash("task_env"));
	/* #ifdef __sun */
	/*   mem_watch(&ash_nazg_gimbatul,sizeof(&ash_nazg_gimbatul), 0); */
	/* #endif */
	link_into(&t->all, &ash_nazg_gimbatul); /* Put it in the list of all tasks */
	/* #ifdef __sun */
	/*   mem_watch(&ash_nazg_gimbatul,sizeof(&ash_nazg_gimbatul), WA_WRITE); */
	/* #endif */
	assert(ash_nazg_gimbatul.type == type_hash("task_env"));
	/* assert(ash_nazg_gimbatul.suc > (linkage*)0x8000000); */
	/* assert(ash_nazg_gimbatul.pred > (linkage*)0x8000000); */
	t->terminate = RUN;
	t->refcnt = 0;
	t->taskret = 0;
	t->time = 0.0;
	t->arg = null_arg;
	t->where = t->buf;
	t->stack_top = &t->buf[TASK_POOL_ELEMS-1];
	t->sp = t->stack_top;
	memset(t->buf, 0, TASK_POOL_ELEMS * sizeof(TaskAlign));
}


static linkage tasks = {0,&tasks,&tasks};
static task_queue task_time_q;
static linkage free_tasks = {0,&free_tasks,&free_tasks};
/* Basic operations on tasks */
static task_env *activate(task_env *t)
{
	if (t) {
		MAY_DBG(FN;
		    STRLIT("activating task ");
		    PTREXP(t);
		    STREXP(t->name);
		    NDBG(t->heap_pos, d);
		    NDBG(t->time, f);
		    );
		assert(ash_nazg_gimbatul.type == type_hash("task_env"));
		if (t->heap_pos)
			task_queue_remove(&task_time_q, t->heap_pos);
		link_into(&t->l, &tasks);
		t->time = 0.0;
		t->heap_pos = 0;
		assert(ash_nazg_gimbatul.type == type_hash("task_env"));
	}
	return t;
}


static task_env *deactivate(task_env *t)
{
	if (t) {
		assert(ash_nazg_gimbatul.type == type_hash("task_env"));
		link_out(&t->l);
		assert(ash_nazg_gimbatul.type == type_hash("task_env"));
	}
	return t;
}


void task_delay_until(double time)
{
	if (stack) {
		stack->time = time;
		task_queue_insert(&task_time_q, task_ref(deactivate(stack)));
	}
}


/* Wait queues */
void task_wait(task_env *t, linkage *queue)
{
	if (t) {
		TASK_DEBUG("task_wait");
		deactivate(t);
		link_into(&t->l, queue);
	}
}


void task_wakeup(linkage *queue)
{
	assert(queue);
	assert(queue != &tasks);
	while (!link_empty(queue)) {
		activate(container_of(link_extract_first(queue), task_env, l));
		TASK_DEBUG("task_wakeup");
	}
}


static void task_wakeup_first(linkage *queue)
{
	assert(queue);
	assert(queue != &tasks);
	if (!link_empty(queue)) {
		activate(container_of(link_extract_first(queue), task_env, l));
		TASK_DEBUG("task_wakeup_first");
	}
}


/* Channels */
channel *channel_init(channel *c, unsigned int type)
{
	link_init(&c->data, type);
	link_init(&c->queue, type_hash("task_env"));
	return c;
}

/* purecov: begin deadcode */
channel *channel_new()
{
	channel * c = malloc(sizeof(channel));
	channel_init(c, NULL_TYPE);
	return c;
}
/* purecov: end */

void channel_put(channel *c, linkage *data)
{
	MAY_DBG(FN; PTREXP(data); PTREXP(&c->data));
	link_into(data, &c->data);
	task_wakeup_first(&c->queue);
}


void channel_put_front(channel *c, linkage *data)
{
	link_follow(data, &c->data);
	task_wakeup_first(&c->queue);
}


static int	active_tasks = 0;
task_env *task_new(task_func func, task_arg arg, const char *name, int debug)
{
	task_env * t;
	if (link_empty(&free_tasks))
		t = malloc(sizeof(task_env));
	else
		t = container_of(link_extract_first(&free_tasks), task_env, l);
	DBGOUT(FN; PTREXP(t); STREXP(name));
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
void *task_allocate(task_env *p, unsigned int bytes)
{
	/*  TaskAlign to boundary  */
	unsigned int	alloc_units = (unsigned int)
		((bytes + sizeof(TaskAlign) -1) / sizeof(TaskAlign));
	TaskAlign * ret;
	/*  Check if there is space */
	TASK_DEBUG("task_allocate");
	if ((p->where + alloc_units) <= (p->stack_top)) {
		ret = p->where;
		p->where += alloc_units;
		memset(ret, 0, alloc_units * sizeof(TaskAlign));
	} else {
		ret = 0;
		abort();
	}
	return ret;
}


void reset_state(task_env *p)
{
	if ((p->where) <= (p->stack_top - 1)) {
		TASK_DEBUG("reset_state");
		p->stack_top[-1].state = 0;
	} else {
		abort();
	}
}


void pushp(task_env *p, void *ptr)
{
	assert(ptr);
	if ((p->where) <= (p->stack_top - 1)) {
		p->stack_top->ptr = ptr;
		p->stack_top--;
		TASK_DEBUG("pushp");
	} else {
		abort();
	}
}


void popp(task_env *p)
{
	if (p->stack_top < &p->buf[TASK_POOL_ELEMS]) {
		TASK_DEBUG("popp");
		p->stack_top++;
	} else {
		abort();
	}
}


static int	runnable_tasks()
{
	return !link_empty(&tasks);
}


static int	delayed_tasks()
{
	return !task_queue_empty(&task_time_q);
}


static void task_delete(task_env *t)
{
	DBGOUT(FN; PTREXP(t); STREXP(t->name); NDBG(t->refcnt, d));
	link_out(&t->all); /* Remove task from list of all tasks */
#if 1
	free(deactivate(t)); /* Deactivate and free task */
#else
  deactivate(t);
	link_into(&t->l, &free_tasks);
#endif
	active_tasks--;
}


static task_env *task_ref(task_env *t)
{
	if (t) {
		t->refcnt++;
	}
	return t;
}


static task_env *task_unref(task_env *t)
{
	if (t) {
		t->refcnt--;
		if (t->refcnt == 0) {
			task_delete(t);
			return NULL;
		}
	}
	return t;
}


task_env *task_activate(task_env *t)
{
	return activate(t);
}


task_env *task_deactivate(task_env *t)
{
	return deactivate(t);
}


/* Set terminate flag and activate task */
task_env *task_terminate(task_env *t) {
  if (t) {
    DBGOUT(FN; PTREXP(t); STREXP(t->name); NDBG(t->refcnt, d));
    t->terminate = KILL; /* Set terminate flag */
    activate(t);         /* and get it running */
  }
  return t;
}


/* Call task_terminate on all tasks */
void task_terminate_all()
{
	/* First, activate all tasks which wait for timeout */
	while (delayed_tasks()) {
		task_env * t = extract_first_delayed(); /* May be NULL */
		if (t)
			activate(t); /* Make it runnable */
	}
	/* Then wake all tasks waiting for IO */
	wake_all_io();
	/* At last, terminate everything */
	/* assert(ash_nazg_gimbatul.suc > (linkage*)0x8000000); */
	/* assert(ash_nazg_gimbatul.pred > (linkage*)0x8000000); */
	FWD_ITER(&ash_nazg_gimbatul, task_env,
	    task_terminate(container_of(link_iter, task_env, all));
	    );
}


static task_env *first_delayed()
{
	return task_queue_min(&task_time_q);
}


static task_env *extract_first_delayed()
{
	task_env * ret = task_queue_extractmin(&task_time_q);
	ret->time = 0.0;
	return task_unref(ret);
}


static iotasks iot;

static void iotasks_init(iotasks *iot)
 {
  DBGOUT(FN);
  iot->nwait = 0;
  init_pollfd_array(&iot->fd);
  init_task_env_p_array(&iot->tasks);
 }

static void iotasks_deinit(iotasks *iot)
{
  DBGOUT(FN);
  iot->nwait = 0;
  free_pollfd_array(&iot->fd);
  free_task_env_p_array(&iot->tasks);
}

#if TASK_DBUG_ON
static void poll_debug()
{
	int	i = 0;
	for (i = 0; i < iot.nwait; i++) {
		NDBG(i,d); PTREXP(iot.tasks[i]); NDBG(iot.fd[i].fd,d); NDBG(iot.fd[i].events,d); NDBG(iot.fd[i].revents,d);
	}
}
#endif

static void poll_wakeup(int i)
{
  activate(task_unref(get_task_env_p(&iot.tasks,i)));
  set_task_env_p(&iot.tasks, NULL,i);
  iot.nwait--; /* Shrink array of pollfds */
  set_pollfd(&iot.fd, get_pollfd(&iot.fd,iot.nwait),i);
  set_task_env_p(&iot.tasks, get_task_env_p(&iot.tasks,iot.nwait),i);
}

static int poll_wait(int ms) {
  result nfds = {0, 0};
  int wake = 0;

  /* Wait at most ms milliseconds */
  MAY_DBG(FN; NDBG(ms, d));
  if (ms < 0 || ms > 1000) ms = 1000; /* Wait at most 1000 ms */
  SET_OS_ERR(0);
  while ((nfds.val = poll(iot.fd.pollfd_array_val, iot.nwait, ms)) == -1) {
    nfds.funerr = to_errno(GET_OS_ERR);
    if (nfds.funerr != SOCK_EINTR) {
      task_dump_err(nfds.funerr);
      MAY_DBG(FN; STRLIT("poll failed"));
      abort();
    }
    SET_OS_ERR(0);
  }
  /* Wake up ready tasks */
  {
    int i = 0;
    int interrupt = 0;
    while (i < iot.nwait) {
      interrupt =
        (get_task_env_p(&iot.tasks,i)->time != 0.0 &&
        get_task_env_p(&iot.tasks,i)->time < task_now());
      if (interrupt || /* timeout ? */
        get_pollfd(&iot.fd,i).revents) {
        /* if(iot.fd[i].revents & POLLERR) abort(); */
        get_task_env_p(&iot.tasks,i)->interrupt = interrupt;
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
  int events = 'r' == op ? POLLIN | POLLRDNORM : POLLOUT;
  MAY_DBG(FN; PTREXP(t); NDBG(fd, d); NDBG(op, d));
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

void unpoll(int i) {
  task_unref(get_task_env_p(&iot.tasks, i));
  set_task_env_p(&iot.tasks, NULL,i);
  {
    pollfd x;
    x.fd = -1;
    x.events = 0;
    x.revents = 0;
    set_pollfd(&iot.fd, x, i);
  }
}

static void wake_all_io() {
  int i;
  for (i = 0; i < iot.nwait; i++) {
    activate(get_task_env_p(&iot.tasks,i));
    unpoll(i);
  }
  iot.nwait = 0;
}

void remove_and_wakeup(int fd) {
  int i = 0;
  MAY_DBG(FN; NDBG(fd, d));
  while (i < iot.nwait) {
    if (get_pollfd(&iot.fd,i).fd == fd) {
      poll_wakeup(i);
    } else {
      i++;
    }
  }
}

task_env *stack = NULL;

task_env *wait_io(task_env *t, int fd, int op)
{
	t->time = 0.0;
	t->interrupt = 0;
	add_fd(deactivate(t), fd, op);
	return t;
}


static task_env *timed_wait_io(task_env *t, int fd, int op, double timeout)
{
	t->time = task_now() + timeout;
	t->interrupt = 0;
	add_fd(deactivate(t), fd, op);
	return t;
}


static uint64_t send_count;
static uint64_t receive_count;
static uint64_t send_bytes;
static uint64_t receive_bytes;

#ifdef XCOM_HAVE_OPENSSL
result	con_read(connection_descriptor const *rfd, void *buf, int n)
{
  result	ret = {0,0};

  if (rfd->ssl_fd) {
    ERR_clear_error();
    ret.val = SSL_read(rfd->ssl_fd, buf, n);
    ret.funerr = to_ssl_err(SSL_get_error(rfd->ssl_fd, ret.val));
  } else {
    SET_OS_ERR(0);
    ret.val = (int)recv(rfd->fd, buf, (size_t)n, 0);
    ret.funerr = to_errno(GET_OS_ERR);
  }
  return ret;
}
#else
result	con_read(connection_descriptor const *rfd, void *buf, int n)
{
  result	ret = {0,0};

  SET_OS_ERR(0);
  ret.val = recv(rfd->fd, buf, (size_t)n, 0);
  ret.funerr = to_errno(GET_OS_ERR);

  return ret;
}
#endif

/*
  It just reads no more than INT_MAX bytes. Caller should call it again for
  read more than INT_MAX bytes.

  Either the bytes written or an error number is returned to the caller through
  'ret' argument. Error number is always negative integers.
*/
int	task_read(connection_descriptor const* con, void *buf, int n, int64_t *ret)
{
	DECL_ENV
		int dummy;
	END_ENV;

	result 	sock_ret = {0,0};
        *ret= 0;

	assert(n >= 0);

	TASK_BEGIN
		MAY_DBG(FN; PTREXP(stack); NDBG(con->fd,d); PTREXP(buf); NDBG(n,d));

	for(;;){
		if(con->fd <= 0)
			TASK_FAIL;
		sock_ret = con_read(con, buf, n);
		*ret = sock_ret.val;
		task_dump_err(sock_ret.funerr);
		MAY_DBG(FN; PTREXP(stack); NDBG(con->fd,d); PTREXP(buf); NDBG(n,d));
		if(sock_ret.val >= 0 || (! can_retry_read(sock_ret.funerr)))
			break;
		wait_io(stack, con->fd, 'r');
		TASK_YIELD;
		MAY_DBG(FN; PTREXP(stack); NDBG(con->fd,d); PTREXP(buf); NDBG(n,d));
	}

	assert(!can_retry_read(sock_ret.funerr));
	FINALLY
	    receive_count++;
	if(*ret > 0)
		receive_bytes += (uint64_t)(*ret);
	TASK_END;
}

#ifdef XCOM_HAVE_OPENSSL
result	con_write(connection_descriptor const *wfd, void *buf, int n)
{
  result	ret = {0,0};

  assert(n >0);

  if (wfd->ssl_fd) {
    ERR_clear_error();
    ret.val = SSL_write(wfd->ssl_fd, buf, n);
    ret.funerr = to_ssl_err(SSL_get_error(wfd->ssl_fd, ret.val));
  } else {
    SET_OS_ERR(0);
    ret.val = (int)send(wfd->fd, buf, (size_t)n, 0);
    ret.funerr = to_errno(GET_OS_ERR);
  }
  return ret;
}
#else
result	con_write(connection_descriptor const *wfd, void *buf, int n)
{
  result	ret = {0,0};

  assert(n >0);

  SET_OS_ERR(0);
  ret.val = send(wfd->fd, buf, (size_t)n, 0);
  ret.funerr = to_errno(GET_OS_ERR);
  return ret;
}
#endif

/*
  It writes no more than UINT_MAX bytes which is the biggest size of
  paxos message.

  Either the bytes written or an error number is returned to the caller through
  'ret' argument. Error number is always negative integers.
*/
int task_write(connection_descriptor const *con, void *_buf, uint32_t n,
               int64_t *ret)
{
	char	*buf = (char *) _buf;
	DECL_ENV
	   uint32_t	total; /* Keeps track of number of bytes written so far */
	END_ENV;
	result 	sock_ret = {0,0};

	TASK_BEGIN
	    ep->total = 0;
	*ret= 0;
	while (ep->total < n) {
		MAY_DBG(FN; PTREXP(stack); NDBG(con->fd,d); NDBG(n - ep->total,u));
		for(;;){
			if(con->fd <= 0)
				TASK_FAIL;
			/*
			  con_write can only write messages that their sizes don't exceed
			  INT_MAX bytes. We should never pass a length bigger than INT_MAX
			  to con_write.
			*/
			sock_ret = con_write(con, buf + ep->total, n-ep->total >= INT_MAX ?
                           INT_MAX : (int)(n-ep->total));
			task_dump_err(sock_ret.funerr);
			MAY_DBG(FN; PTREXP(stack); NDBG(con->fd,d); NDBG(sock_ret.val, d));
			if(sock_ret.val >= 0 || (! can_retry_write(sock_ret.funerr)))
				break;
			wait_io(stack, con->fd, 'w');
			MAY_DBG(FN; PTREXP(stack); NDBG(con->fd,d); NDBG(n - ep->total,u));
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


int unblock_fd(int fd)
{
#if !defined(WIN32) && !defined(WIN64)
	int	x = fcntl(fd, F_GETFL, 0);
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
	int	x = ioctlsocket(fd, FIONBIO, &nonblocking);
#endif
  return x;
}


int block_fd(int fd)
{
#if !defined(WIN32) && !defined(WIN64)
	int	x = fcntl(fd, F_GETFL, 0);
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
	int	x = ioctlsocket(fd, FIONBIO, &nonblocking);
#endif
  return x;
}

/* purecov: begin deadcode */
int	is_only_task()
{
	return link_first(&tasks) == link_last(&tasks);
}
/* purecov: end */

static task_env *first_runnable()
{
	return (task_env * )link_first(&tasks);
}


static task_env *next_task(task_env *t)
{
	return (task_env * )link_first(&t->l);
}


static int	is_task_head(task_env *t)
{
	return & t->l == &tasks;
}


static int	msdiff(double time)
{
	return (int)(1000.5 * (first_delayed()->time - time));
}


static double	idle_time = 0.0;
void task_loop()
{
	/* While there are tasks */
	for(;;) {
		task_env * t = first_runnable();
		/* While runnable tasks */
		while (runnable_tasks()) {
			task_env * next = next_task(t);
			if (!is_task_head(t)) {
				/* DBGOUT(FN; PTREXP(t); STRLIT(t->name ? t->name : "TASK WITH NO NAME")); */
				stack = t;
				assert(stack);
				assert(t->terminate != TERMINATED);
				if (stack->debug)
					/* assert(ash_nazg_gimbatul.suc > (linkage*)0x8000000); */
					/* assert(ash_nazg_gimbatul.pred > (linkage*)0x8000000); */
					assert(ash_nazg_gimbatul.type == type_hash("task_env"));
				 {
					/*           double when = seconds(); */
					int	val = 0;
					assert(t->func);
					assert(stack == t);
					val = t->func(t->arg);
					/* assert(ash_nazg_gimbatul.suc > (linkage*)0x8000000); */
					/* assert(ash_nazg_gimbatul.pred > (linkage*)0x8000000); */
					assert(ash_nazg_gimbatul.type == type_hash("task_env"));
					if (!val) { /* Is task finished? */
						deactivate(t);
						t->terminate = TERMINATED;
						task_unref(t);
						stack = NULL;
					}
				}
			}
			t = next;
		}
		if (active_tasks <= 0)
			break;
		/* When we get here, there are no runnable tasks left.
		       Wait until something happens.
		    */
		 {
			double	time = seconds();
			if (delayed_tasks()) {
				int	ms = msdiff(time);
				if (ms > 0) {
                                       if (the_app_xcom_cfg != NULL && the_app_xcom_cfg->m_poll_spin_loops)
                                       {
                                         u_int busyloop;
                                         for(busyloop = 0; busyloop < the_app_xcom_cfg->m_poll_spin_loops; busyloop++){
                                                 ADD_WAIT_EV(task_now(), __FILE__, __LINE__, "poll_wait(ms)", 0);
                                                 if(poll_wait(0)) /*Just poll */
                                                         goto done_wait;
                                                 ADD_WAIT_EV(task_now(), __FILE__, __LINE__, "poll_wait(ms) end", 0);
                                                 thread_yield();
                                          }
                                        }
					ADD_WAIT_EV(task_now(), __FILE__, __LINE__, "poll_wait(ms)", ms);
					poll_wait(ms); /* Wait at most ms milliseconds and poll for IO */
					ADD_WAIT_EV(task_now(), __FILE__, __LINE__, "poll_wait(ms) end", ms);
				}
done_wait:
				/* While tasks with expired timers */
				while (delayed_tasks() &&
				    msdiff(time) <= 0) {
					task_env * t = extract_first_delayed(); /* May be NULL */
					if (t)
						activate(t); /* Make it runnable */
				}
			} else {
				ADD_T_EV(task_now(), __FILE__, __LINE__, "poll_wait(-1)");
				poll_wait(-1); /* Wait and poll for IO */
				ADD_T_EV(seconds(), __FILE__, __LINE__, "poll_wait(-1) end");
				/*       } */
			}
			idle_time += seconds() - time;
		}
	}
  task_sys_deinit();
}


static int	init_sockaddr(char *server, struct sockaddr_in *sock_addr,
                          socklen_t *sock_size, xcom_port port)
{
	/* Get address of server */
	struct addrinfo *addr = 0;

	checked_getaddrinfo(server, 0, 0, &addr);

	if (!addr)
		return 0;

	/* Copy first address */
	memcpy(sock_addr, addr->ai_addr, addr->ai_addrlen);
	*sock_size = addr->ai_addrlen;
	sock_addr->sin_port = htons(port);

	/* Clean up allocated memory by getaddrinfo */
	freeaddrinfo(addr);

	return 1;
}


#if TASK_DBUG_ON
static void print_sockaddr(struct sockaddr *a)
{
	u_int	i;
	GET_GOUT;
	NDBG(a->sa_family,u); NDBG(a->sa_family,d);
	STRLIT(" data ");
	for (i = 0; i < sizeof(a->sa_data); i++) {
		NPUT((unsigned char)a->sa_data[i],d);
	}
  PRINT_GOUT;
  FREE_GOUT;
}
#endif


int	connect_tcp(char *server, xcom_port port, int *ret)
{
	DECL_ENV
	    int	fd;
	struct sockaddr sock_addr;
	socklen_t sock_size;
	END_ENV;
	TASK_BEGIN;
	DBGOUT(FN; STREXP(server); NDBG(port,d));
	/* Create socket */
	if ((ep->fd = xcom_checked_socket(AF_INET, SOCK_STREAM, 0).val) < 0) {
		DBGOUT(FN; NDBG(ep->fd, d));
		TASK_FAIL;
	}
	/* Make it non-blocking */
	unblock_fd(ep->fd);
	/* Get address of server */
	/* OHKFIX Move this before call to xcom_checked_socket and use addrinfo fields as params to socket call */
	if (!init_sockaddr(server, (struct sockaddr_in *)&ep->sock_addr,
	                   &ep->sock_size, port)) {
		DBGOUT(FN; NDBG(ep->fd, d); NDBG(ep->sock_size, d));
		TASK_FAIL;
	}
#if TASK_DBUG_ON
	DBGOUT(FN; print_sockaddr(&ep->sock_addr));
#endif
	/* Connect socket to address */
	 {
		result sock = {0,0};
		SET_OS_ERR(0);
		sock.val = connect(ep->fd, &ep->sock_addr, ep->sock_size);
		sock.funerr = to_errno(GET_OS_ERR);
		if (sock.val < 0) {
			if (hard_connect_err(sock.funerr)) {
			  task_dump_err(sock.funerr);
				MAY_DBG(FN;
				    NDBG(ep->fd, d);
				    NDBG(ep->sock_size,d));
#if TASK_DBUG_ON
				DBGOUT(FN; print_sockaddr(&ep->sock_addr));
#endif
				DBGOUT(FN; NDBG(ep->fd, d); NDBG(ep->sock_size, d));
				close_socket(&ep->fd);
				TASK_FAIL;
			}
		}
		/* Wait until connect has finished */
retry:
		timed_wait_io(stack, ep->fd, 'w', 10.0);
		TASK_YIELD;
		/* See if we timed out here. If we did, connect may or may not be active.
		       If closing fails with EINPROGRESS, we need to retry the select.
		       If close does not fail, we know that connect has indeed failed, and we
		       exit from here and return -1 as socket fd */
		if(stack->interrupt){
			result shut = {0,0};
			stack->interrupt = 0;

			/* Try to close socket on timeout */
			shut = shut_close_socket(&ep->fd);
			DBGOUT(FN; NDBG(ep->fd, d); NDBG(ep->sock_size, d));
			task_dump_err(shut.funerr);
			if (from_errno(shut.funerr) == SOCK_EINPROGRESS)
				goto retry; /* Connect is still active */
			TASK_FAIL; /* Connect has failed */
		}


		{
			int	peer = 0;
			/* Sanity check before return */
			SET_OS_ERR(0);
			sock.val = peer = getpeername(ep->fd, &ep->sock_addr, &ep->sock_size);
			sock.funerr = to_errno(GET_OS_ERR);
			if (peer >= 0) {
				TASK_RETURN(ep->fd);
			} else {
				/* Something is wrong */
				socklen_t errlen = sizeof(sock.funerr);

				getsockopt(ep->fd, SOL_SOCKET, SO_ERROR, (void * ) & sock.funerr, &errlen);
				if (sock.funerr == 0) {
					sock.funerr = to_errno(SOCK_ECONNREFUSED);
				}

				shut_close_socket(&ep->fd);
				if (sock.funerr == 0)
					sock.funerr = to_errno(SOCK_ECONNREFUSED);
				TASK_FAIL;
			}
		}
	}
	FINALLY
	    TASK_END;
}

result set_nodelay(int fd)
{
	int	n = 1;
	result ret = {0,0};

	do{
		SET_OS_ERR(0);
		ret.val = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *) & n, sizeof n);
		ret.funerr = to_errno(GET_OS_ERR);
		DBGOUT(FN; NDBG(from_errno(ret.funerr),d));
	}while(ret.val < 0 && can_retry(ret.funerr));
	return ret;
}

static result	create_server_socket()
{
	result	fd ={0,0};
	/* Create socket */
	if ((fd = xcom_checked_socket(PF_INET, SOCK_STREAM, 0)).val < 0) {
                G_MESSAGE("Unable to create socket "
                          "(socket=%d, errno=%d)!",
                          fd.val, to_errno(GET_OS_ERR));
		return fd;
	}
	{
		int	reuse = 1;
		SET_OS_ERR(0);
		if (setsockopt(fd.val, SOL_SOCKET, SOCK_OPT_REUSEADDR, (void * ) & reuse, sizeof(reuse)) < 0) {
			fd.funerr = to_errno(GET_OS_ERR);
                        G_MESSAGE("Unable to set socket options "
                                  "(socket=%d, errno=%d)!",
                                  fd.val, to_errno(GET_OS_ERR));
			close_socket(&fd.val);
			return fd;
		}
	}
	return fd;
}


static void init_server_addr(struct sockaddr_in *sock_addr, xcom_port port)
{
	memset(sock_addr, 0, sizeof(*sock_addr));
	sock_addr->sin_family = PF_INET;
	sock_addr->sin_port = htons(port);
}


result	announce_tcp(xcom_port port)
{
	result	fd;
	struct sockaddr_in sock_addr;

	fd = create_server_socket();
	if (fd.val < 0) {
		return fd;
	}
	init_server_addr(&sock_addr, port);
	if (bind(fd.val, (struct sockaddr *)&sock_addr, sizeof(sock_addr)) < 0) {
          int err = to_errno(GET_OS_ERR);
          G_MESSAGE("Unable to bind to %s:%d (socket=%d, errno=%d)!",
                    "0.0.0.0", port, fd.val, err);
		goto err;
	}
        G_DEBUG("Successfully bound to %s:%d (socket=%d).",
                  "0.0.0.0", port, fd.val);
	if (listen(fd.val, 32) < 0) {
          int err = to_errno(GET_OS_ERR);
          G_MESSAGE("Unable to listen backlog to 32. "
                    "(socket=%d, errno=%d)!", fd.val, err);
		goto err;
	}
        G_DEBUG("Successfully set listen backlog to 32 "
                  "(socket=%d)!", fd.val);
	/* Make socket non-blocking */
	unblock_fd(fd.val);
        if (fd.val < 0)
        {
          int err = to_errno(GET_OS_ERR);
          G_MESSAGE("Unable to unblock socket (socket=%d, errno=%d)!",
                    fd.val, err);
        }
        else
        {
          G_DEBUG("Successfully unblocked socket (socket=%d)!", fd.val);
        }
	return fd;

 err:
	fd.funerr = to_errno(GET_OS_ERR);
	task_dump_err(fd.funerr);
	close_socket(&fd.val);
	return fd;
}


int	accept_tcp(int fd, int *ret)
{
	struct sockaddr sock_addr;
	DECL_ENV
	    int	connection;
	END_ENV;
	TASK_BEGIN;
	/* Wait for connection attempt */
	wait_io(stack, fd, 'r');
	TASK_YIELD;
	/* Spin on benign error code */
	{
		socklen_t size = sizeof sock_addr;
		result res = {0,0};
		do{
			SET_OS_ERR(0);
			res.val = ep->connection = accept(fd, (void * ) & sock_addr, &size);
			res.funerr = to_errno(GET_OS_ERR);
		}while(res.val < 0 && from_errno(res.funerr) == SOCK_EINTR);

		if (ep->connection < 0) {
			TASK_FAIL;
		}
	}
#if TASK_DBUG_ON
	DBGOUT(FN; print_sockaddr(&sock_addr));
#endif
	TASK_RETURN(ep->connection);
	FINALLY
	    TASK_END;
}


#define STAT_INTERVAL 1.0

#if 0

/*
  This was disabled to prevent unecessary build warnings.

  TODO:
  Needs to be assessed whether it should be removed altogether.
 */

static int	statistics_task(task_arg arg)
{
	DECL_ENV
	    double	next;
	END_ENV;
	TASK_BEGIN
	    idle_time = 0.0;
	send_count = 0;
	receive_count = 0;
	send_bytes = 0;
	receive_bytes = 0;
	ep->next = seconds() + STAT_INTERVAL;
	TASK_DELAY_UNTIL(ep->next);
	for(;;) {
		G_DEBUG("task system idle %f send/s %f receive/s %f send b/s %f receive b/s %f",
		    (idle_time / STAT_INTERVAL) * 100.0, send_count / STAT_INTERVAL, receive_count / STAT_INTERVAL,
		    send_bytes / STAT_INTERVAL, receive_bytes / STAT_INTERVAL);
		idle_time = 0.0;
		send_count = 0;
		receive_count = 0;
		send_bytes = 0;
		receive_bytes = 0;
		ep->next += STAT_INTERVAL;
		TASK_DELAY_UNTIL(ep->next);
	}
	FINALLY
	    TASK_END;
}
#endif

static void init_task_vars()
{
	stack = 0;
	task_errno = 0;
}

void task_sys_init()
{
	DBGOUT(FN; NDBG(FD_SETSIZE,d));
	init_task_vars();
	link_init(&tasks, type_hash("task_env"));
	link_init(&free_tasks, type_hash("task_env"));
	link_init(&ash_nazg_gimbatul, type_hash("task_env"));
	/* assert(ash_nazg_gimbatul.suc > (linkage*)0x8000000); */
	/* assert(ash_nazg_gimbatul.pred > (linkage*)0x8000000); */
	iotasks_init(&iot);
	seconds(); /* Needed to initialize _now */
	/* task_new(statistics_task, null_arg, "statistics_task", 1);  */
}


static void task_sys_deinit()
{
  DBGOUT(FN);
  iotasks_deinit(&iot);
}

/* purecov: begin deadcode */
int	is_running(task_env *t)
{
	return t && t->terminate == RUN;
}
/* purecov: end */

void set_task(task_env**p, task_env *t)
{
	if (t)
		task_ref(t);
	if (*p)
		task_unref(*p);
	*p = t;
}


const char	*task_name()
{
	return stack ? stack->name : "idle";
}


task_event task_events[MAX_TASK_EVENT];
int cur_task_event;
int max_task_event;

#ifdef WIN
#define snprintf(...) _snprintf(__VA_ARGS__)
#endif

/* purecov: begin deadcode */
void	ev_print(task_event te)
{
	enum {
		bufsize = 10000
	};
	static char	buf[bufsize];
	static size_t pos = 0;

	if (te.pad) {
		switch (te.arg.type) {
		case a_int:
			pos += (size_t)snprintf(&buf[pos], bufsize - pos, "%d ", te.arg.val.i);
			break;
		case a_long:
			pos += (size_t)snprintf(&buf[pos], bufsize - pos, "%ld ", te.arg.val.l);
			break;
		case a_uint:
			pos += (size_t)snprintf(&buf[pos], bufsize - pos, "%u ", te.arg.val.u_i);
			break;
		case a_ulong:
			pos += (size_t)snprintf(&buf[pos], bufsize - pos, "%lu ", te.arg.val.u_l);
			break;
		case a_ulong_long:
			pos += (size_t)snprintf(&buf[pos], bufsize - pos, "%llu ", te.arg.val.u_ll);
			break;
		case a_float:
			pos += (size_t)snprintf(&buf[pos], bufsize - pos, "%f ", te.arg.val.f);
			break;
		case a_double:
			pos += (size_t)snprintf(&buf[pos], bufsize - pos, "%f ", te.arg.val.d);
			break;
		case a_void:
			pos += (size_t)snprintf(&buf[pos], bufsize - pos, "%p ", te.arg.val.v);
			break;
		case a_string:
			pos += (size_t)snprintf(&buf[pos], bufsize - pos, "%s ", te.arg.val.s);
			break;
		case a_end:
			xcom_log(LOG_TRACE, buf);
			pos = 0;
			break;
		default:
			pos += (size_t)snprintf(&buf[pos], bufsize - pos, "??? ");
		}
	} else {
		switch (te.arg.type) {
		case a_int:
			pos += (size_t)snprintf(&buf[pos], bufsize - pos, "%d", te.arg.val.i);
			break;
		case a_long:
			pos += (size_t)snprintf(&buf[pos], bufsize - pos, "%ld", te.arg.val.l);
			break;
		case a_uint:
			pos += (size_t)snprintf(&buf[pos], bufsize - pos, "%u", te.arg.val.u_i);
			break;
		case a_ulong:
			pos += (size_t)snprintf(&buf[pos], bufsize - pos, "%lu", te.arg.val.u_l);
			break;
		case a_ulong_long:
			pos += (size_t)snprintf(&buf[pos], bufsize - pos, "%llu", te.arg.val.u_ll);
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
			xcom_log(LOG_TRACE, buf);
			pos = 0;
			break;
		default:
			pos += (size_t)snprintf(&buf[pos], bufsize - pos, "???");
		}
	}
	buf[pos] = 0;
}


void add_event(task_arg te)
{
	task_events[cur_task_event].arg = te;
	task_events[cur_task_event].pad = 1;

	cur_task_event++;
	if (cur_task_event > max_task_event)
		max_task_event = cur_task_event;
	cur_task_event %= MAX_TASK_EVENT;
}


void add_unpad_event(task_arg te)
{
	task_events[cur_task_event].arg = te;
	task_events[cur_task_event].pad = 0;

	cur_task_event++;
	if (cur_task_event > max_task_event)
		max_task_event = cur_task_event;
	cur_task_event %= MAX_TASK_EVENT;
}


void add_base_event(double when, char const *file, int state)
{
	static double	t = 0.0;

	add_event(double_arg(when));
	add_event(double_arg(when - t));
	t = when;
	add_unpad_event(string_arg(file));
	add_unpad_event(string_arg(":"));
	add_event(int_arg(state));
}


void add_task_event(double when, char const *file, int state, char const *what)
{
	add_base_event(when, file, state);
	add_event(string_arg(what));
	add_event(end_arg());
}


void add_wait_event(double when, char *file, int state, char *what, int milli)
{
	add_base_event(when, file, state);
	add_event(string_arg(what));

	add_event(string_arg("milli"));
	add_event(int_arg(milli));
	add_event(end_arg());
}


static void dump_range(double t MY_ATTRIBUTE((unused)
), int start, int end)
{
	int	i;
	for (i = start; i < end; i++) {
		ev_print(task_events[i]);
	}
}


void dump_task_events()
{
	double	t = 0.0;
	G_DEBUG("cur_task_event %d max_task_event %d", cur_task_event, max_task_event);
	add_event(end_arg());
	dump_range(t, cur_task_event, max_task_event);
	dump_range(t, 0, cur_task_event);
}
/* purecov: end */
