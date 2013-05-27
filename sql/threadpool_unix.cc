/* Copyright (C) 2012 Monty Program Ab

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

#include <my_global.h>
#include <violite.h>
#include <sql_priv.h>
#include <sql_class.h>
#include <my_pthread.h>
#include <scheduler.h>
#include <sql_connect.h>
#include <mysqld.h>
#include <debug_sync.h>
#include <time.h>
#include <sql_plist.h>
#include <threadpool.h>
#include <time.h>
#ifdef __linux__
#include <sys/epoll.h>
typedef struct epoll_event native_event;
#elif defined(HAVE_KQUEUE)
#include <sys/event.h>
typedef struct kevent native_event;
#elif defined (__sun)
#include <port.h>
typedef port_event_t native_event;
#else
#error threadpool is not available on this platform
#endif

/** Maximum number of native events a listener can read in one go */
#define MAX_EVENTS 1024

/** Indicates that threadpool was initialized*/
static bool threadpool_started= false; 

/* 
  Define PSI Keys for performance schema. 
  We have a mutex per group, worker threads, condition per worker thread, 
  and timer thread  with its own mutex and condition.
*/
 
 
static PSI_mutex_key key_group_mutex;
static PSI_mutex_key key_timer_mutex;
static PSI_mutex_info mutex_list[]=
{
  { &key_group_mutex, "group_mutex", 0},
  { &key_timer_mutex, "timer_mutex", PSI_FLAG_GLOBAL}
};

static PSI_cond_key key_worker_cond;
static PSI_cond_key key_timer_cond;
static PSI_cond_info cond_list[]=
{
  { &key_worker_cond, "worker_cond", 0},
  { &key_timer_cond, "timer_cond", PSI_FLAG_GLOBAL}
};

static PSI_thread_key key_worker_thread;
static PSI_thread_key key_timer_thread;
static PSI_thread_info	thread_list[] =
{
 {&key_worker_thread, "worker_thread", 0},
 {&key_timer_thread, "timer_thread", PSI_FLAG_GLOBAL}
};

/* Macro to simplify performance schema registration */ 
#define PSI_register(X) \
 if(PSI_server) PSI_server->register_ ## X("threadpool", X ## _list, array_elements(X ## _list))


struct thread_group_t;

/* Per-thread structure for workers */
struct worker_thread_t
{
  ulonglong  event_count; /* number of request handled by this thread */
  thread_group_t* thread_group;   
  worker_thread_t *next_in_list;
  worker_thread_t **prev_in_list;
  
  mysql_cond_t  cond;
  bool          woken;
};

typedef I_P_List<worker_thread_t, I_P_List_adapter<worker_thread_t,
                 &worker_thread_t::next_in_list,
                 &worker_thread_t::prev_in_list> 
                 >
worker_list_t;

struct connection_t
{

  THD *thd;
  thread_group_t *thread_group;
  connection_t *next_in_queue;
  connection_t **prev_in_queue;
  ulonglong abs_wait_timeout;
  bool logged_in;
  bool bound_to_poll_descriptor;
  bool waiting;
};

typedef I_P_List<connection_t,
                     I_P_List_adapter<connection_t,
                                      &connection_t::next_in_queue,
                                      &connection_t::prev_in_queue>,
                     I_P_List_null_counter,
                     I_P_List_fast_push_back<connection_t> >
connection_queue_t;

struct thread_group_t 
{
  mysql_mutex_t mutex;
  connection_queue_t queue;
  worker_list_t waiting_threads; 
  worker_thread_t *listener;
  pthread_attr_t *pthread_attr;
  int  pollfd;
  int  thread_count;
  int  active_thread_count;
  int  connection_count;
  /* Stats for the deadlock detection timer routine.*/
  int io_event_count;
  int queue_event_count;
  ulonglong last_thread_creation_time;
  int  shutdown_pipe[2];
  bool shutdown;
  bool stalled;
  
} MY_ALIGNED(512);

static thread_group_t all_groups[MAX_THREAD_GROUPS];
static uint group_count;

/**
 Used for printing "pool blocked" message, see
 print_pool_blocked_message();
*/
static ulonglong pool_block_start;

/* Global timer for all groups  */
struct pool_timer_t
{
  mysql_mutex_t mutex;
  mysql_cond_t cond;
  volatile uint64 current_microtime;
  volatile uint64 next_timeout_check;
  int  tick_interval;
  bool shutdown;
};

static pool_timer_t pool_timer;

static void queue_put(thread_group_t *thread_group, connection_t *connection);
static int  wake_thread(thread_group_t *thread_group);
static void handle_event(connection_t *connection);
static int  wake_or_create_thread(thread_group_t *thread_group);
static int  create_worker(thread_group_t *thread_group);
static void *worker_main(void *param);
static void check_stall(thread_group_t *thread_group);
static void connection_abort(connection_t *connection);
static void set_wait_timeout(connection_t *connection);
static void set_next_timeout_check(ulonglong abstime);
static void print_pool_blocked_message(bool);

/**
 Asynchronous network IO.
 
 We use native edge-triggered network IO multiplexing facility. 
 This maps to different APIs on different Unixes.
 
 Supported are currently Linux with epoll, Solaris with event ports,
 OSX and BSD with kevent. All those API's are used with one-shot flags
 (the event is signalled once client has written something into the socket, 
 then socket is removed from the "poll-set" until the  command is finished,
 and we need to re-arm/re-register socket)
 
 No implementation for poll/select/AIO is currently provided.
 
 The API closely resembles all of the above mentioned platform APIs 
 and consists of following functions. 
 
 - io_poll_create()
 Creates an io_poll descriptor 
 On Linux: epoll_create()
 
 - io_poll_associate_fd(int poll_fd, int fd, void *data)
 Associate file descriptor with io poll descriptor 
 On Linux : epoll_ctl(..EPOLL_CTL_ADD))
 
 - io_poll_disassociate_fd(int pollfd, int fd)
  Associate file descriptor with io poll descriptor 
  On Linux: epoll_ctl(..EPOLL_CTL_DEL)
 
 
 - io_poll_start_read(int poll_fd,int fd, void *data)
 The same as io_poll_associate_fd(), but cannot be used before 
 io_poll_associate_fd() was called.
 On Linux : epoll_ctl(..EPOLL_CTL_MOD)
 
 - io_poll_wait (int pollfd, native_event *native_events, int maxevents, 
   int timeout_ms)
 
 wait until one or more descriptors added with io_poll_associate_fd() 
 or io_poll_start_read() becomes readable. Data associated with 
 descriptors can be retrieved from native_events array, using 
 native_event_get_userdata() function.

 
 On Linux: epoll_wait()
*/

#if defined (__linux__)
#ifndef EPOLLRDHUP
/* Early 2.6 kernel did not have EPOLLRDHUP */
#define EPOLLRDHUP 0
#endif
static int io_poll_create()
{
  return epoll_create(1);
}


int io_poll_associate_fd(int pollfd, int fd, void *data)
{
  struct epoll_event ev;
  ev.data.u64= 0; /* Keep valgrind happy */
  ev.data.ptr= data;
  ev.events=  EPOLLIN|EPOLLET|EPOLLERR|EPOLLRDHUP|EPOLLONESHOT;
  return epoll_ctl(pollfd, EPOLL_CTL_ADD,  fd, &ev);
}



int io_poll_start_read(int pollfd, int fd, void *data)
{
  struct epoll_event ev;
  ev.data.u64= 0; /* Keep valgrind happy */
  ev.data.ptr= data;
  ev.events=  EPOLLIN|EPOLLET|EPOLLERR|EPOLLRDHUP|EPOLLONESHOT;
  return epoll_ctl(pollfd, EPOLL_CTL_MOD,  fd, &ev); 
}

int io_poll_disassociate_fd(int pollfd, int fd)
{
  struct epoll_event ev;
  return epoll_ctl(pollfd, EPOLL_CTL_DEL,  fd, &ev);
}


/*
 Wrapper around epoll_wait.
 NOTE - in case of EINTR, it restarts with original timeout. Since we use
 either infinite or 0 timeouts, this is not critical
*/
int io_poll_wait(int pollfd, native_event *native_events, int maxevents, 
              int timeout_ms)
{
  int ret;
  do 
  {
    ret = epoll_wait(pollfd, native_events, maxevents, timeout_ms);
  }
  while(ret == -1 && errno == EINTR);
  return ret;
}


static void *native_event_get_userdata(native_event *event)
{
  return event->data.ptr;
}

#elif defined(HAVE_KQUEUE)

/* 
  NetBSD is incompatible with other BSDs , last parameter in EV_SET macro
  (udata, user data) needs to be intptr_t, whereas it needs to be void* 
  everywhere else.
*/

#ifdef __NetBSD__
#define MY_EV_SET(a, b, c, d, e, f, g) EV_SET(a, b, c, d, e, f, (intptr_t)g)
#else
#define MY_EV_SET(a, b, c, d, e, f, g) EV_SET(a, b, c, d, e, f, g)
#endif


int io_poll_create()
{
  return kqueue();
}

int io_poll_start_read(int pollfd, int fd, void *data)
{
  struct kevent ke;
  MY_EV_SET(&ke, fd, EVFILT_READ, EV_ADD|EV_ONESHOT, 
         0, 0, data);
  return kevent(pollfd, &ke, 1, 0, 0, 0); 
}


int io_poll_associate_fd(int pollfd, int fd, void *data)
{
  struct kevent ke;
  MY_EV_SET(&ke, fd, EVFILT_READ, EV_ADD|EV_ONESHOT, 
         0, 0, data);
  return io_poll_start_read(pollfd,fd, data); 
}


int io_poll_disassociate_fd(int pollfd, int fd)
{
  struct kevent ke;
  MY_EV_SET(&ke,fd, EVFILT_READ, EV_DELETE, 0, 0, 0);
  return kevent(pollfd, &ke, 1, 0, 0, 0);
}


int io_poll_wait(int pollfd, struct kevent *events, int maxevents, int timeout_ms)
{
  struct timespec ts;
  int ret;
  if (timeout_ms >= 0)
  {
    ts.tv_sec= timeout_ms/1000;
    ts.tv_nsec= (timeout_ms%1000)*1000000;
  }
  do
  {
    ret= kevent(pollfd, 0, 0, events, maxevents, 
               (timeout_ms >= 0)?&ts:NULL);
  }
  while (ret == -1 && errno == EINTR);
  return ret;
}

static void* native_event_get_userdata(native_event *event)
{
  return (void *)event->udata;
}

#elif defined (__sun)

static int io_poll_create()
{
  return port_create();
}

int io_poll_start_read(int pollfd, int fd, void *data)
{
  return port_associate(pollfd, PORT_SOURCE_FD, fd, POLLIN, data);
}

static int io_poll_associate_fd(int pollfd, int fd, void *data)
{
  return io_poll_start_read(pollfd, fd, data);
}

int io_poll_disassociate_fd(int pollfd, int fd)
{
  return port_dissociate(pollfd, PORT_SOURCE_FD, fd);
}

int io_poll_wait(int pollfd, native_event *events, int maxevents, int timeout_ms)
{
  struct timespec ts;
  int ret;
  uint_t nget= 1;
  if (timeout_ms >= 0)
  {
    ts.tv_sec= timeout_ms/1000;
    ts.tv_nsec= (timeout_ms%1000)*1000000;
  }
  do
  {
    ret= port_getn(pollfd, events, maxevents, &nget,
            (timeout_ms >= 0)?&ts:NULL);
  }
  while (ret == -1 && errno == EINTR);
  DBUG_ASSERT(nget < INT_MAX);
  return (int)nget;
}

static void* native_event_get_userdata(native_event *event)
{
  return event->portev_user;
}
#endif


/* Dequeue element from a workqueue */

static connection_t *queue_get(thread_group_t *thread_group)
{
  DBUG_ENTER("queue_get");
  thread_group->queue_event_count++;
  connection_t *c= thread_group->queue.front();
  if (c)
  {
    thread_group->queue.remove(c);
  }
  DBUG_RETURN(c);  
}


/* 
  Handle wait timeout : 
  Find connections that have been idle for too long and kill them.
  Also, recalculate time when next timeout check should run.
*/

static void timeout_check(pool_timer_t *timer)
{
  DBUG_ENTER("timeout_check");
  
  mysql_mutex_lock(&LOCK_thread_count);
  I_List_iterator<THD> it(threads);

  /* Reset next timeout check, it will be recalculated in the loop below */
  my_atomic_fas64((volatile int64*)&timer->next_timeout_check, ULONGLONG_MAX);

  THD *thd;
  while ((thd=it++))
  {
    if (thd->net.reading_or_writing != 1)
      continue;
 
    connection_t *connection= (connection_t *)thd->event_scheduler.data;
    if (!connection)
    {
      /* 
        Connection does not have scheduler data. This happens for example
        if THD belongs to a different scheduler, that is listening to extra_port.
      */
      continue;
    }

    if(connection->abs_wait_timeout < timer->current_microtime)
    {
      /* Wait timeout exceeded, kill connection. */
      mysql_mutex_lock(&thd->LOCK_thd_data);
      thd->killed = KILL_CONNECTION;
      post_kill_notification(thd);
      mysql_mutex_unlock(&thd->LOCK_thd_data);
    }
    else 
    {
      set_next_timeout_check(connection->abs_wait_timeout);
    }
  }
  mysql_mutex_unlock(&LOCK_thread_count);
  DBUG_VOID_RETURN;
}


/* 
 Timer thread. 
 
  Periodically, check if one of the thread groups is stalled. Stalls happen if
  events are not being dequeued from the queue, or from the network, Primary  
  reason for stall can be a lengthy executing non-blocking request. It could 
  also happen that thread is waiting but wait_begin/wait_end is forgotten by 
  storage engine. Timer thread will create a new thread in group in case of 
  a stall.
 
  Besides checking for stalls, timer thread is also responsible for terminating
  clients that have been idle for longer than wait_timeout seconds.

  TODO: Let the timer sleep for long time if there is no work to be done.
  Currently it wakes up rather often on and idle server.
*/

static void* timer_thread(void *param)
{
  uint i;
  pool_timer_t* timer=(pool_timer_t *)param;

  my_thread_init();
  DBUG_ENTER("timer_thread");
  timer->next_timeout_check= ULONGLONG_MAX;
  timer->current_microtime= microsecond_interval_timer();

  for(;;)
  {
    struct timespec ts;
    int err;

    set_timespec_nsec(ts,timer->tick_interval*1000000);
    mysql_mutex_lock(&timer->mutex);
    err= mysql_cond_timedwait(&timer->cond, &timer->mutex, &ts);
    if (timer->shutdown)
    {
      mysql_mutex_unlock(&timer->mutex);
      break;
    }
    if (err == ETIMEDOUT)
    {
      timer->current_microtime= microsecond_interval_timer();
      
      /* Check stalls in thread groups */
      for(i=0; i< array_elements(all_groups);i++)
      {
        if(all_groups[i].connection_count)
           check_stall(&all_groups[i]);
      }
      
      /* Check if any client exceeded wait_timeout */
      if (timer->next_timeout_check <= timer->current_microtime)
        timeout_check(timer);
    }
    mysql_mutex_unlock(&timer->mutex);
  }

  mysql_mutex_destroy(&timer->mutex);
  my_thread_end();
  return NULL;
}



void check_stall(thread_group_t *thread_group)
{
  if (mysql_mutex_trylock(&thread_group->mutex) != 0)
  {
    /* Something happens. Don't disturb */
    return;
  }

  /*
    Check if listener is present. If not,  check whether any IO 
    events were dequeued since last time. If not, this means 
    listener is either in tight loop or thd_wait_begin() 
    was forgotten. Create a new worker(it will make itself listener).
  */
  if (!thread_group->listener && !thread_group->io_event_count)
  {
    wake_or_create_thread(thread_group);
    mysql_mutex_unlock(&thread_group->mutex);
    return;
  }
  
  /*  Reset io event count */
  thread_group->io_event_count= 0;

  /* 
    Check whether requests from the workqueue are being dequeued.

    The stall detection and resolution works as follows:

    1. There is a counter thread_group->queue_event_count for the number of 
       events removed from the queue. Timer resets the counter to 0 on each run.
    2. Timer determines stall if this counter remains 0 since last check
       and the queue is not empty.
    3. Once timer determined a stall it sets thread_group->stalled flag and
       wakes and idle worker (or creates a new one, subject to throttling).
    4. The stalled flag is reset, when an event is dequeued.

    Q : Will this handling lead to an unbound growth of threads, if queue
    stalls permanently?
    A : No. If queue stalls permanently, it is an indication for many very long
    simultaneous queries. The maximum number of simultanoues queries is 
    max_connections, further we have threadpool_max_threads limit, upon which no
    worker threads are created. So in case there is a flood of very long 
    queries, threadpool would slowly approach thread-per-connection behavior.
    NOTE:
    If long queries never wait, creation of the new threads is done by timer,
    so it is slower than in real thread-per-connection. However if long queries 
    do wait and indicate that via thd_wait_begin/end callbacks, thread creation
    will be faster.
  */
  if (!thread_group->queue.is_empty() && !thread_group->queue_event_count)
  {
    thread_group->stalled= true;
    wake_or_create_thread(thread_group);
  }
  
  /* Reset queue event count */
  thread_group->queue_event_count= 0;
  
  mysql_mutex_unlock(&thread_group->mutex);
}


static void start_timer(pool_timer_t* timer)
{
  pthread_t thread_id;
  DBUG_ENTER("start_timer");
  mysql_mutex_init(key_timer_mutex,&timer->mutex, NULL);
  mysql_cond_init(key_timer_cond, &timer->cond, NULL);
  timer->shutdown = false;
  mysql_thread_create(key_timer_thread,&thread_id, NULL, timer_thread, timer);
  DBUG_VOID_RETURN;
}


static void stop_timer(pool_timer_t *timer)
{
  DBUG_ENTER("stop_timer");
  mysql_mutex_lock(&timer->mutex);
  timer->shutdown = true;
  mysql_cond_signal(&timer->cond);
  mysql_mutex_unlock(&timer->mutex);
  DBUG_VOID_RETURN;
}


/**
  Poll for socket events and distribute them to worker threads
  In many case current thread will handle single event itself.
  
  @return a ready connection, or NULL on shutdown
*/
static connection_t * listener(worker_thread_t *current_thread, 
                               thread_group_t *thread_group)
{
  DBUG_ENTER("listener");
  connection_t *retval= NULL;

  for(;;)
  {
    native_event ev[MAX_EVENTS];
    int cnt;
    
    if (thread_group->shutdown)
      break;
  
    cnt = io_poll_wait(thread_group->pollfd, ev, MAX_EVENTS, -1);
    
    if (cnt <=0)
    {
      DBUG_ASSERT(thread_group->shutdown);
      break;
    }

    mysql_mutex_lock(&thread_group->mutex);

    if (thread_group->shutdown)
    {
      mysql_mutex_unlock(&thread_group->mutex);
      break;
    }
    
    thread_group->io_event_count += cnt;  
    
    /* 
     We got some network events and need to make decisions : whether
     listener  hould handle events and whether or not any wake worker
     threads so they can handle events.
     
     Q1 : Should listener handle an event itself, or put all events into 
     queue  and let workers handle the events?
     
     Solution :
     Generally, listener that handles events itself is preferable. We do not 
     want listener thread to change its state from waiting  to running too 
     often, Since listener has just woken from poll, it better uses its time
     slice and does some work. Besides, not handling events means they go to
     the  queue, and often to wake another worker must wake up to handle the
     event. This is not good, as we want to avoid wakeups.
     
     The downside of listener that also handles queries is that we can
     potentially leave thread group  for long time not picking the new 
     network events. It is not  a major problem, because this stall will be
     detected  sooner or later by  the timer thread. Still, relying on timer
     is not always good, because it may "tick" too slow (large timer_interval)
     
     We use following strategy to solve this problem - if queue was not empty
     we suspect flood of network events and listener stays, Otherwise, it 
     handles a query.
     
     
     Q2: If queue is not empty, how many workers to wake?
     
     Solution:
     We generally try to keep one thread per group active (threads handling 
     queries   are considered active, unless they stuck in inside some "wait")
     Thus, we will wake only one worker, and only if there is not active 
     threads currently,and listener is not going to handle a query. When we 
     don't wake, we hope that  currently active  threads will finish fast and 
     handle the queue. If this does  not happen, timer thread will detect stall
     and wake a worker.
     
     NOTE: Currently nothing is done to detect or prevent long queuing times. 
     A solutionc for the future would be to give up "one active thread per 
     group" principle, if events stay  in the queue for too long, and just wake 
     more workers.
    */
    
    bool listener_picks_event= thread_group->queue.is_empty();
    
    /* 
      If listener_picks_event is set, listener thread will handle first event, 
      and put the rest into the queue. If listener_pick_event is not set, all 
      events go to the queue.
    */
    for(int i=(listener_picks_event)?1:0; i < cnt ; i++)
    {
      connection_t *c= (connection_t *)native_event_get_userdata(&ev[i]);
      thread_group->queue.push_back(c);
    }
    
    if (listener_picks_event)
    {
      /* Handle the first event. */
      retval= (connection_t *)native_event_get_userdata(&ev[0]);
      mysql_mutex_unlock(&thread_group->mutex);
      break;
    }

    if(thread_group->active_thread_count==0)
    {
      /* We added some work items to queue, now wake a worker. */
      if(wake_thread(thread_group))
      {
        /* 
          Wake failed, hence groups has no idle threads. Now check if there are
          any threads in the group except listener.
        */ 
        if(thread_group->thread_count == 1)
        {
           /*
             Currently there is no worker thread in the group, as indicated by
             thread_count == 1 (this means listener is the only one thread in 
             the group).
             The queue is not empty, and listener is not going to handle
             events. In order to drain the queue,  we create a worker here.
             Alternatively, we could just rely on timer to detect stall, and 
             create thread, but waiting for timer would be an inefficient and
             pointless delay.
           */
           create_worker(thread_group);
        }
      }
    }
    mysql_mutex_unlock(&thread_group->mutex);
  }

  DBUG_RETURN(retval);
}

/**
  Adjust thread counters in group or global 
  whenever thread is created or is about to exit

  @param thread_group
  @param count -  1, when new thread is created
                 -1, when thread is about to exit
*/

static void add_thread_count(thread_group_t *thread_group, int32 count)
{
  thread_group->thread_count += count;
  /* worker starts out and end in "active" state */
  thread_group->active_thread_count += count;
  my_atomic_add32(&tp_stats.num_worker_threads, count);
}


/**
  Creates a new worker thread. 
  thread_mutex must be held when calling this function 

  NOTE: in rare cases, the number of threads can exceed
  threadpool_max_threads, because we need at least 2 threads
  per group to prevent deadlocks (one listener + one worker)
*/

static int create_worker(thread_group_t *thread_group)
{
  pthread_t thread_id;
  bool max_threads_reached= false;
  int err;
  
  DBUG_ENTER("create_worker");
  if (tp_stats.num_worker_threads >= (int)threadpool_max_threads
     && thread_group->thread_count >= 2)
  {
    err= 1;
    max_threads_reached= true;
    goto end;
  }

  
  err= mysql_thread_create(key_worker_thread, &thread_id, 
         thread_group->pthread_attr, worker_main, thread_group);
  if (!err)
  {
    thread_group->last_thread_creation_time=microsecond_interval_timer();
    thread_created++;
    add_thread_count(thread_group, 1);
  }
  else
  {
    my_errno= errno;
  }

end:
  if (err)
    print_pool_blocked_message(max_threads_reached);
  else
    pool_block_start= 0; /* Reset pool blocked timer, if it was set */
    
  DBUG_RETURN(err);
}


/**
 Calculate microseconds throttling delay for thread creation.
 
 The value depends on how many threads are already in the group:
 small number of threads means no delay, the more threads the larger
 the delay.
 
 The actual values were not calculated using any scientific methods.
 They just look right, and behave well in practice.
 
 TODO: Should throttling depend on thread_pool_stall_limit?
*/
static ulonglong microsecond_throttling_interval(thread_group_t *thread_group)
{
  int count= thread_group->thread_count;
  
  if (count < 4)
    return 0;
  
  if (count < 8)
    return 50*1000; 
  
  if(count < 16)
    return 100*1000;
  
  return 200*1000;
}


/**
  Wakes a worker thread, or creates a new one. 
  
  Worker creation is throttled, so we avoid too many threads
  to be created during the short time.
*/
static int wake_or_create_thread(thread_group_t *thread_group)
{
  DBUG_ENTER("wake_or_create_thread");
  
  if (thread_group->shutdown)
   DBUG_RETURN(0);

  if (wake_thread(thread_group) == 0)
    DBUG_RETURN(0);

  if (thread_group->thread_count > thread_group->connection_count)
    DBUG_RETURN(-1);

 
  if (thread_group->active_thread_count == 0)
  {
    /*
     We're better off creating a new thread here  with no delay, either there 
     are no workers at all, or they all are all blocking and there was no 
     idle  thread to wakeup. Smells like a potential deadlock or very slowly 
     executing requests, e.g sleeps or user locks.
    */
    DBUG_RETURN(create_worker(thread_group));
  }

  ulonglong now = microsecond_interval_timer();
  ulonglong time_since_last_thread_created =
    (now - thread_group->last_thread_creation_time);
  
  /* Throttle thread creation. */  
  if (time_since_last_thread_created >
       microsecond_throttling_interval(thread_group))
  {
    DBUG_RETURN(create_worker(thread_group));
  }
  
  DBUG_RETURN(-1);
}



int thread_group_init(thread_group_t *thread_group, pthread_attr_t* thread_attr)
{
  DBUG_ENTER("thread_group_init");
  thread_group->pthread_attr = thread_attr;
  mysql_mutex_init(key_group_mutex, &thread_group->mutex, NULL);
  thread_group->pollfd= -1;
  thread_group->shutdown_pipe[0]= -1;
  thread_group->shutdown_pipe[1]= -1;
  DBUG_RETURN(0);
}


void thread_group_destroy(thread_group_t *thread_group)
{
  mysql_mutex_destroy(&thread_group->mutex);
  if (thread_group->pollfd != -1)
  {
    close(thread_group->pollfd);
    thread_group->pollfd= -1;
  }
  for(int i=0; i < 2; i++)
  {
    if(thread_group->shutdown_pipe[i] != -1)
    {
      close(thread_group->shutdown_pipe[i]);
      thread_group->shutdown_pipe[i]= -1;
    }
  }
}

/**
  Wake sleeping thread from waiting list
*/

static int wake_thread(thread_group_t *thread_group)
{
  DBUG_ENTER("wake_thread");
  worker_thread_t *thread = thread_group->waiting_threads.front();
  if(thread)
  {
    thread->woken= true;
    thread_group->waiting_threads.remove(thread);
    mysql_cond_signal(&thread->cond);
    DBUG_RETURN(0);
  }
  DBUG_RETURN(1); /* no thread in waiter list => missed wakeup */
}


/**
  Initiate shutdown for thread group.

  The shutdown is asynchronous, we only care to  wake all threads in here, so 
  they can finish. We do not wait here until threads terminate. Final cleanup 
  of the group (thread_group_destroy) will be done by the last exiting threads.
*/

static void thread_group_close(thread_group_t *thread_group)
{
  DBUG_ENTER("thread_group_close");

  mysql_mutex_lock(&thread_group->mutex);
  if (thread_group->thread_count == 0) 
  {
    mysql_mutex_unlock(&thread_group->mutex);
    thread_group_destroy(thread_group);
    DBUG_VOID_RETURN;
  }

  thread_group->shutdown= true; 
  thread_group->listener= NULL;

  if (pipe(thread_group->shutdown_pipe))
  {
    DBUG_VOID_RETURN;
  }
  
  /* Wake listener */
  if (io_poll_associate_fd(thread_group->pollfd, 
      thread_group->shutdown_pipe[0], NULL))
  {
    DBUG_VOID_RETURN;
  }
  char c= 0;
  if (write(thread_group->shutdown_pipe[1], &c, 1) < 0)
    DBUG_VOID_RETURN;

  /* Wake all workers. */
  while(wake_thread(thread_group) == 0) 
  { 
  }
  
  mysql_mutex_unlock(&thread_group->mutex);

  DBUG_VOID_RETURN;
}


/* 
  Add work to the queue. Maybe wake a worker if they all sleep.
  
  Currently, this function is only used when new connections need to
  perform login (this is done in worker threads).

*/

static void queue_put(thread_group_t *thread_group, connection_t *connection)
{
  DBUG_ENTER("queue_put");

  mysql_mutex_lock(&thread_group->mutex);
  thread_group->queue.push_back(connection);

  if (thread_group->active_thread_count == 0)
    wake_or_create_thread(thread_group);

  mysql_mutex_unlock(&thread_group->mutex);

  DBUG_VOID_RETURN;
}


/* 
  Prevent too many threads executing at the same time,if the workload is 
  not CPU bound.
*/

static bool too_many_threads(thread_group_t *thread_group)
{
  return (thread_group->active_thread_count >= 1+(int)threadpool_oversubscribe 
   && !thread_group->stalled);
}


/**
  Retrieve a connection with pending event.
  
  Pending event in our case means that there is either a pending login request 
  (if connection is not yet logged in), or there are unread bytes on the socket.

  If there are no pending events currently, thread will wait. 
  If timeout specified in abstime parameter passes, the function returns NULL.
 
  @param current_thread - current worker thread
  @param thread_group - current thread group
  @param abstime - absolute wait timeout
  
  @return
  connection with pending event. 
  NULL is returned if timeout has expired,or on shutdown.
*/

connection_t *get_event(worker_thread_t *current_thread, 
  thread_group_t *thread_group,  struct timespec *abstime)
{ 
  DBUG_ENTER("get_event");
  connection_t *connection = NULL;
  int err=0;

  mysql_mutex_lock(&thread_group->mutex);
  DBUG_ASSERT(thread_group->active_thread_count >= 0);

  for(;;) 
  {
    bool oversubscribed = too_many_threads(thread_group); 
    if (thread_group->shutdown)
     break;

    /* Check if queue is not empty */
    if (!oversubscribed)
    {
      connection = queue_get(thread_group);
      if(connection)
        break;
    }

    /* If there is  currently no listener in the group, become one. */
    if(!thread_group->listener)
    {
      thread_group->listener= current_thread;
      thread_group->active_thread_count--;
      mysql_mutex_unlock(&thread_group->mutex);

      connection = listener(current_thread, thread_group);

      mysql_mutex_lock(&thread_group->mutex);
      thread_group->active_thread_count++;
      /* There is no listener anymore, it just returned. */
      thread_group->listener= NULL;
      break;
    }
    
    /* 
      Last thing we try before going to sleep is to 
      pick a single event via epoll, without waiting (timeout 0)
    */
    if (!oversubscribed)
    {
      native_event nev;
      if (io_poll_wait(thread_group->pollfd,&nev,1, 0) == 1)
      {
        thread_group->io_event_count++;
        connection = (connection_t *)native_event_get_userdata(&nev);
        break;
      }
    }

    /* And now, finally sleep */ 
    current_thread->woken = false; /* wake() sets this to true */

    /* 
      Add current thread to the head of the waiting list  and wait.
      It is important to add thread to the head rather than tail
      as it ensures LIFO wakeup order (hot caches, working inactivity timeout)
    */
    thread_group->waiting_threads.push_front(current_thread);
    
    thread_group->active_thread_count--;
    if (abstime)
    {
      err = mysql_cond_timedwait(&current_thread->cond, &thread_group->mutex, 
                                 abstime);
    }
    else
    {
      err = mysql_cond_wait(&current_thread->cond, &thread_group->mutex);
    }
    thread_group->active_thread_count++;
    
    if (!current_thread->woken)
    {
      /*
        Thread was not signalled by wake(), it might be a spurious wakeup or
        a timeout. Anyhow, we need to remove ourselves from the list now.
        If thread was explicitly woken, than caller removed us from the list.
      */
      thread_group->waiting_threads.remove(current_thread);
    }

    if (err)
      break;
  }

  thread_group->stalled= false;
  mysql_mutex_unlock(&thread_group->mutex);
 
  DBUG_RETURN(connection);
}



/**
  Tells the pool that worker starts waiting  on IO, lock, condition, 
  sleep() or similar.
*/

void wait_begin(thread_group_t *thread_group)
{
  DBUG_ENTER("wait_begin");
  mysql_mutex_lock(&thread_group->mutex);
  thread_group->active_thread_count--;
  
  DBUG_ASSERT(thread_group->active_thread_count >=0);
  DBUG_ASSERT(thread_group->connection_count > 0);
 
  if ((thread_group->active_thread_count == 0) && 
     (thread_group->queue.is_empty() || !thread_group->listener))
  {
    /* 
      Group might stall while this thread waits, thus wake 
      or create a worker to prevent stall.
    */
    wake_or_create_thread(thread_group);
  }
  
  mysql_mutex_unlock(&thread_group->mutex);
  DBUG_VOID_RETURN;
}

/**
  Tells the pool has finished waiting.
*/

void wait_end(thread_group_t *thread_group)
{
  DBUG_ENTER("wait_end");
  mysql_mutex_lock(&thread_group->mutex);
  thread_group->active_thread_count++;
  mysql_mutex_unlock(&thread_group->mutex);
  DBUG_VOID_RETURN;
}


/**
  Allocate/initialize a new connection structure.
*/

connection_t *alloc_connection(THD *thd)
{
  DBUG_ENTER("alloc_connection");
  
  connection_t* connection = (connection_t *)my_malloc(sizeof(connection_t),0);
  if (connection)
  {
    connection->thd = thd;
    connection->waiting= false;
    connection->logged_in= false;
    connection->bound_to_poll_descriptor= false;
    connection->abs_wait_timeout= ULONGLONG_MAX;
  }
  DBUG_RETURN(connection);
}



/**
  Add a new connection to thread pool..
*/

void tp_add_connection(THD *thd)
{
  DBUG_ENTER("tp_add_connection");
  
  threads.append(thd);
  mysql_mutex_unlock(&LOCK_thread_count);
  connection_t *connection= alloc_connection(thd);
  if (connection)
  {
    thd->event_scheduler.data= connection;
      
    /* Assign connection to a group. */
    thread_group_t *group= 
      &all_groups[thd->thread_id%group_count];
    
    connection->thread_group=group;
      
    mysql_mutex_lock(&group->mutex);
    group->connection_count++;
    mysql_mutex_unlock(&group->mutex);
    
    /*
       Add connection to the work queue.Actual logon 
       will be done by a worker thread.
    */
    queue_put(group, connection);
  }
  else
  {
    /* Allocation failed */
    threadpool_remove_connection(thd);
  } 
  DBUG_VOID_RETURN;
}


/**
  Terminate connection.
*/

static void connection_abort(connection_t *connection)
{
  DBUG_ENTER("connection_abort");
  thread_group_t *group= connection->thread_group;
  
  threadpool_remove_connection(connection->thd); 
  
  mysql_mutex_lock(&group->mutex);
  group->connection_count--;
  mysql_mutex_unlock(&group->mutex);
  
  my_free(connection);
  DBUG_VOID_RETURN;
}


/**
  MySQL scheduler callback: wait begin
*/

void tp_wait_begin(THD *thd, int type)
{
  DBUG_ENTER("tp_wait_begin");
  DBUG_ASSERT(thd);
  connection_t *connection = (connection_t *)thd->event_scheduler.data;
  if (connection)
  {
    DBUG_ASSERT(!connection->waiting);
    connection->waiting= true;
    wait_begin(connection->thread_group);
  }
  DBUG_VOID_RETURN;
}


/**
  MySQL scheduler callback: wait end
*/

void tp_wait_end(THD *thd) 
{ 
  DBUG_ENTER("tp_wait_end");
  DBUG_ASSERT(thd);

  connection_t *connection = (connection_t *)thd->event_scheduler.data;
  if (connection)
  {
    DBUG_ASSERT(connection->waiting);
    connection->waiting = false;
    wait_end(connection->thread_group);
  }
  DBUG_VOID_RETURN;
}


static void set_next_timeout_check(ulonglong abstime)
{
  DBUG_ENTER("set_next_timeout_check");
  while(abstime < pool_timer.next_timeout_check)
  {
    longlong old= (longlong)pool_timer.next_timeout_check;
    my_atomic_cas64((volatile int64*)&pool_timer.next_timeout_check,
          &old, abstime);
  }
  DBUG_VOID_RETURN;
}


/**
  Set wait timeout for connection. 
*/

static void set_wait_timeout(connection_t *c)
{
  DBUG_ENTER("set_wait_timeout");
  /* 
    Calculate wait deadline for this connection.
    Instead of using microsecond_interval_timer() which has a syscall 
    overhead, use pool_timer.current_microtime and take 
    into account that its value could be off by at most 
    one tick interval.
  */

  c->abs_wait_timeout= pool_timer.current_microtime +
    1000LL*pool_timer.tick_interval +
    1000000LL*c->thd->variables.net_wait_timeout;

  set_next_timeout_check(c->abs_wait_timeout);
  DBUG_VOID_RETURN;
}



/**
  Handle a (rare) special case,where connection needs to 
  migrate to a different group because group_count has changed
  after thread_pool_size setting. 
*/

static int change_group(connection_t *c, 
 thread_group_t *old_group,
 thread_group_t *new_group)
{ 
  int ret= 0;
  int fd = c->thd->net.vio->sd;

  DBUG_ASSERT(c->thread_group == old_group);

  /* Remove connection from the old group. */
  mysql_mutex_lock(&old_group->mutex);
  if (c->bound_to_poll_descriptor)
  {
    io_poll_disassociate_fd(old_group->pollfd,fd);
    c->bound_to_poll_descriptor= false;
  }
  c->thread_group->connection_count--;
  mysql_mutex_unlock(&old_group->mutex);
  
  /* Add connection to the new group. */
  mysql_mutex_lock(&new_group->mutex);
  c->thread_group= new_group;
  new_group->connection_count++;
  /* Ensure that there is a listener in the new group. */
  if (!new_group->thread_count)
    ret= create_worker(new_group);
  mysql_mutex_unlock(&new_group->mutex);
  return ret;
}


static int start_io(connection_t *connection)
{ 
  int fd = connection->thd->net.vio->sd;

  /*
    Usually, connection will stay in the same group for the entire
    connection's life. However, we do allow group_count to
    change at runtime, which means in rare cases when it changes is 
    connection should need to migrate  to another group, this ensures
    to ensure equal load between groups.

    So we recalculate in which group the connection should be, based
    on thread_id and current group count, and migrate if necessary.
  */ 
  thread_group_t *group = 
    &all_groups[connection->thd->thread_id%group_count];

  if (group != connection->thread_group)
  {
    if (change_group(connection, connection->thread_group, group))
      return -1;
  }
    
  /* 
    Bind to poll descriptor if not yet done. 
  */ 
  if (!connection->bound_to_poll_descriptor)
  {
    connection->bound_to_poll_descriptor= true;
    return io_poll_associate_fd(group->pollfd, fd, connection);
  }
  
  return io_poll_start_read(group->pollfd, fd, connection);
}



static void handle_event(connection_t *connection)
{

  DBUG_ENTER("handle_event");
  int err;

  if (!connection->logged_in)
  {
    err= threadpool_add_connection(connection->thd);
    connection->logged_in= true;
  }
  else 
  {
    err= threadpool_process_request(connection->thd);
  }

  if(err)
    goto end;

  set_wait_timeout(connection);
  err= start_io(connection);

end:
  if (err)
    connection_abort(connection);

  DBUG_VOID_RETURN;
}



/**
  Worker thread's main
*/

static void *worker_main(void *param)
{
  
  worker_thread_t this_thread;
  pthread_detach_this_thread();
  my_thread_init();
  
  DBUG_ENTER("worker_main");
  
  thread_group_t *thread_group = (thread_group_t *)param;

  /* Init per-thread structure */
  mysql_cond_init(key_worker_cond, &this_thread.cond, NULL);
  this_thread.thread_group= thread_group;
  this_thread.event_count=0;

  /* Run event loop */
  for(;;)
  {
    connection_t *connection;
    struct timespec ts;
    set_timespec(ts,threadpool_idle_timeout);
    connection = get_event(&this_thread, thread_group, &ts);
    if (!connection)
      break;
    this_thread.event_count++;
    handle_event(connection);
  }

  /* Thread shutdown: cleanup per-worker-thread structure. */
  mysql_cond_destroy(&this_thread.cond);

  bool last_thread;                    /* last thread in group exits */
  mysql_mutex_lock(&thread_group->mutex);
  add_thread_count(thread_group, -1);
  last_thread= ((thread_group->thread_count == 0) && thread_group->shutdown);
  mysql_mutex_unlock(&thread_group->mutex);

  /* Last thread in group exits and pool is terminating, destroy group.*/
  if (last_thread)
    thread_group_destroy(thread_group);

  my_thread_end();
  return NULL;
}


bool tp_init()
{
  DBUG_ENTER("tp_init");
  threadpool_started= true;
  scheduler_init();

  for(uint i=0; i < array_elements(all_groups); i++)
  {
    thread_group_init(&all_groups[i], get_connection_attrib());  
  }
  tp_set_threadpool_size(threadpool_size);
  if(group_count == 0)
  {
    /* Something went wrong */
    sql_print_error("Can't set threadpool size to %d",threadpool_size);
    DBUG_RETURN(1);
  }
  PSI_register(mutex);
  PSI_register(cond);
  PSI_register(thread);
  
  pool_timer.tick_interval= threadpool_stall_limit;
  start_timer(&pool_timer);
  DBUG_RETURN(0);
}


void tp_end()
{
  DBUG_ENTER("tp_end");
  
  if (!threadpool_started)
    DBUG_VOID_RETURN;

  stop_timer(&pool_timer);
  for(uint i=0; i< array_elements(all_groups); i++)
  {
    thread_group_close(&all_groups[i]);
  }
  threadpool_started= false;
  DBUG_VOID_RETURN;
}


/** Ensure that poll descriptors are created when threadpool_size changes */

void tp_set_threadpool_size(uint size)
{
  bool success= true;
  if (!threadpool_started)
    return;

  for(uint i=0; i< size; i++)
  {
    thread_group_t *group= &all_groups[i];
    mysql_mutex_lock(&group->mutex);
    if (group->pollfd == -1)
    {
      group->pollfd= io_poll_create();
      success= (group->pollfd >= 0);
      if(!success)
      {
        sql_print_error("io_poll_create() failed, errno=%d\n", errno);
        break;
      }
    }  
    mysql_mutex_unlock(&all_groups[i].mutex);
    if (!success)
    {
      group_count= i;
      return;
    }
  }
  group_count= size;
}

void tp_set_threadpool_stall_limit(uint limit)
{
  if (!threadpool_started)
    return;
  mysql_mutex_lock(&(pool_timer.mutex));
  pool_timer.tick_interval= limit;
  mysql_mutex_unlock(&(pool_timer.mutex));
  mysql_cond_signal(&(pool_timer.cond));
}


/**
 Calculate number of idle/waiting threads in the pool.
 
 Sum idle threads over all groups. 
 Don't do any locking, it is not required for stats.
*/

int tp_get_idle_thread_count()
{
  int sum=0;
  for(uint i= 0; 
      i< array_elements(all_groups) && (all_groups[i].pollfd >= 0); 
      i++)
  {
    sum+= (all_groups[i].thread_count - all_groups[i].active_thread_count);
  }
  return sum;
}


/* Report threadpool problems */

/** 
   Delay in microseconds, after which "pool blocked" message is printed.
   (30 sec == 30 Mio usec)
*/
#define BLOCK_MSG_DELAY 30*1000000

#define MAX_THREADS_REACHED_MSG \
"Threadpool could not create additional thread to handle queries, because the \
number of allowed threads was reached. Increasing 'thread_pool_max_threads' \
parameter can help in this situation.\n \
If 'extra_port' parameter is set, you can still connect to the database with \
superuser account (it must be TCP connection using extra_port as TCP port) \
and troubleshoot the situation. \
A likely cause of pool blocks are clients that lock resources for long time. \
'show processlist' or 'show engine innodb status' can give additional hints."

#define CREATE_THREAD_ERROR_MSG "Can't create threads in threadpool (errno=%d)."

/**
 Write a message when blocking situation in threadpool occurs.
 The message is written only when pool blocks for BLOCK_MSG_DELAY (30) seconds.
 It will be just a single message for each blocking situation (to prevent
 log flood).
*/

static void print_pool_blocked_message(bool max_threads_reached)
{
  ulonglong now;
  static bool msg_written;
  
  now= microsecond_interval_timer();
  if (pool_block_start == 0)
  {
    pool_block_start= now;
    msg_written = false;
    return;
  }
  
  if (now > pool_block_start + BLOCK_MSG_DELAY && !msg_written)
  {
    if (max_threads_reached)
      sql_print_error(MAX_THREADS_REACHED_MSG);
    else
      sql_print_error(CREATE_THREAD_ERROR_MSG, my_errno);
    
    sql_print_information("Threadpool has been blocked for %u seconds\n",
      (uint)((now- pool_block_start)/1000000));
    /* avoid reperated messages for the same blocking situation */
    msg_written= true;
  }
}
