/* Copyright (C) 2004-2006 MySQL AB

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

#if defined(__GNUC__) && defined(USE_PRAGMA_IMPLEMENTATION)
#pragma implementation
#endif

#include "thread_registry.h"
#include <thr_alarm.h>
#include <signal.h>
#include "log.h"

#ifndef __WIN__
/* Kick-off signal handler */

enum { THREAD_KICK_OFF_SIGNAL= SIGUSR2 };

extern "C" void handle_signal(int);

void handle_signal(int __attribute__((unused)) sig_no)
{
}
#endif

/* Thread_info initializer methods */

void Thread_info::init(bool send_signal_on_shutdown_arg)
{
  thread_id= pthread_self();
  send_signal_on_shutdown= send_signal_on_shutdown_arg;
}

/*
  TODO: think about moving signal information (now it's shutdown_in_progress)
  to Thread_info. It will reduce contention and allow signal deliverence to
  a particular thread, not to the whole worker crew
*/

Thread_registry::Thread_registry() :
   shutdown_in_progress(FALSE)
  ,sigwait_thread_pid(pthread_self())
  ,error_status(FALSE)
{
  pthread_mutex_init(&LOCK_thread_registry, 0);
  pthread_cond_init(&COND_thread_registry_is_empty, 0);

  /* head is used by-value to simplify nodes inserting */
  head.next= head.prev= &head;
}


Thread_registry::~Thread_registry()
{
  /* Check that no one uses the repository. */
  pthread_mutex_lock(&LOCK_thread_registry);

  for (Thread_info *ti= head.next; ti != &head; ti= ti->next)
  {
    log_error("Thread_registry: unregistered thread: %lu.",
              (unsigned long) ti->thread_id);
  }

  /* All threads must unregister */
  DBUG_ASSERT(head.next == &head);

  pthread_mutex_unlock(&LOCK_thread_registry);
  pthread_cond_destroy(&COND_thread_registry_is_empty);
  pthread_mutex_destroy(&LOCK_thread_registry);
}


/*
  Set signal handler for kick-off thread, and insert a thread info to the
  repository. New node is appended to the end of the list; head.prev always
  points to the last node.
*/

void Thread_registry::register_thread(Thread_info *info,
                                      bool send_signal_on_shutdown)
{
  info->init(send_signal_on_shutdown);

  DBUG_PRINT("info", ("Thread_registry: registering thread %lu...",
                      (unsigned long) info->thread_id));

#ifndef __WIN__
  struct sigaction sa;
  sa.sa_handler= handle_signal;
  sa.sa_flags= 0;
  sigemptyset(&sa.sa_mask);
  sigaction(THREAD_KICK_OFF_SIGNAL, &sa, 0);
#endif
  info->current_cond= 0;

  pthread_mutex_lock(&LOCK_thread_registry);
  info->next= &head;
  info->prev= head.prev;
  head.prev->next= info;
  head.prev= info;
  pthread_mutex_unlock(&LOCK_thread_registry);
}


/*
  Unregister a thread from the repository and free Thread_info structure.
  Every registered thread must unregister. Unregistering should be the last
  thing a thread is doing, otherwise it could have no time to finalize.
*/

void Thread_registry::unregister_thread(Thread_info *info)
{
  DBUG_PRINT("info", ("Thread_registry: unregistering thread %lu...",
                      (unsigned long) info->thread_id));

  pthread_mutex_lock(&LOCK_thread_registry);
  info->prev->next= info->next;
  info->next->prev= info->prev;

  if (head.next == &head)
  {
    DBUG_PRINT("info", ("Thread_registry: thread registry is empty!"));
    pthread_cond_signal(&COND_thread_registry_is_empty);
  }

  pthread_mutex_unlock(&LOCK_thread_registry);
}


/*
  Check whether shutdown is in progress, and if yes, return immediately.
  Else set info->current_cond and call pthread_cond_wait. When
  pthread_cond_wait returns, unregister current cond and check the shutdown
  status again.
  RETURN VALUE
    return value from pthread_cond_wait
*/

int Thread_registry::cond_wait(Thread_info *info, pthread_cond_t *cond,
                                  pthread_mutex_t *mutex)
{
  pthread_mutex_lock(&LOCK_thread_registry);
  if (shutdown_in_progress)
  {
    pthread_mutex_unlock(&LOCK_thread_registry);
    return 0;
  }
  info->current_cond= cond;
  pthread_mutex_unlock(&LOCK_thread_registry);
  /* sic: race condition here, cond can be signaled in deliver_shutdown */
  int rc= pthread_cond_wait(cond, mutex);
  pthread_mutex_lock(&LOCK_thread_registry);
  info->current_cond= 0;
  pthread_mutex_unlock(&LOCK_thread_registry);
  return rc;
}


int Thread_registry::cond_timedwait(Thread_info *info, pthread_cond_t *cond,
                                    pthread_mutex_t *mutex,
                                    struct timespec *wait_time)
{
  int rc;
  pthread_mutex_lock(&LOCK_thread_registry);
  if (shutdown_in_progress)
  {
    pthread_mutex_unlock(&LOCK_thread_registry);
    return 0;
  }
  info->current_cond= cond;
  pthread_mutex_unlock(&LOCK_thread_registry);
  /* sic: race condition here, cond can be signaled in deliver_shutdown */
  if ((rc= pthread_cond_timedwait(cond, mutex, wait_time)) == ETIME)
    rc= ETIMEDOUT;                             // For easier usage
  pthread_mutex_lock(&LOCK_thread_registry);
  info->current_cond= 0;
  pthread_mutex_unlock(&LOCK_thread_registry);
  return rc;
}


/*
  Deliver shutdown message to the workers crew.
  As it's impossible to avoid all race conditions, signal latecomers
  again.
*/

void Thread_registry::deliver_shutdown()
{
  pthread_mutex_lock(&LOCK_thread_registry);
  shutdown_in_progress= TRUE;

#ifndef __WIN__
  /* to stop reading from the network we need to flush alarm queue */
  end_thr_alarm(0);
  /*
    We have to deliver final alarms this way, as the main thread has already
    stopped alarm processing.
  */
  process_alarm(THR_SERVER_ALARM);
#endif

  /*
    sic: race condition here, the thread may not yet fall into
    pthread_cond_wait.
  */

  interrupt_threads();

  wait_for_threads_to_unregister();

  /*
    If previous signals did not reach some threads, they must be sleeping
    in pthread_cond_wait or in a blocking syscall. Wake them up:
    every thread shall check signal variables after each syscall/cond_wait,
    so this time everybody should be informed (presumably each worker can
    get CPU during shutdown_time.)
  */

  interrupt_threads();

  /* Get the last chance to threads to stop. */

  wait_for_threads_to_unregister();

#ifndef DBUG_OFF
  /*
    Print out threads, that didn't stopped. Thread_registry destructor will
    probably abort the program if there is still any alive thread.
  */

  if (head.next != &head)
  {
    DBUG_PRINT("info", ("Thread_registry: non-stopped threads:"));

    for (Thread_info *info= head.next; info != &head; info= info->next)
      DBUG_PRINT("info", ("  - %lu", (unsigned long) info->thread_id));
  }
  else
  {
    DBUG_PRINT("info", ("Thread_registry: all threads stopped."));
  }
#endif // DBUG_OFF

  pthread_mutex_unlock(&LOCK_thread_registry);
}


void Thread_registry::request_shutdown()
{
  pthread_kill(sigwait_thread_pid, SIGTERM);
}


void Thread_registry::interrupt_threads()
{
  for (Thread_info *info= head.next; info != &head; info= info->next)
  {
    if (!info->send_signal_on_shutdown)
      continue;

    pthread_kill(info->thread_id, THREAD_KICK_OFF_SIGNAL);
    if (info->current_cond)
      pthread_cond_signal(info->current_cond);
  }
}


void Thread_registry::wait_for_threads_to_unregister()
{
  struct timespec shutdown_time;

  set_timespec(shutdown_time, 1);

  DBUG_PRINT("info", ("Thread_registry: joining threads..."));

  while (true)
  {
    if (head.next == &head)
    {
      DBUG_PRINT("info", ("Thread_registry: emptied."));
      return;
    }

    int error= pthread_cond_timedwait(&COND_thread_registry_is_empty,
                                      &LOCK_thread_registry,
                                      &shutdown_time);

    if (error == ETIMEDOUT || error == ETIME)
    {
      DBUG_PRINT("info", ("Thread_registry: threads shutdown timed out."));
      return;
    }
  }
}


/*********************************************************************
  class Thread
*********************************************************************/

#if defined(__ia64__) || defined(__ia64)
/*
  We can live with 32K, but reserve 64K. Just to be safe.
  On ia64 we need to reserve double of the size.
*/
#define IM_THREAD_STACK_SIZE    (128*1024L)
#else
#define IM_THREAD_STACK_SIZE    (64*1024)
#endif

/*
  Change the stack size and start a thread. Return an error if either
  pthread_attr_setstacksize or pthread_create fails.
  Arguments are the same as for pthread_create().
*/

static
int set_stacksize_and_create_thread(pthread_t  *thread, pthread_attr_t *attr,
                                    void *(*start_routine)(void *), void *arg)
{
  int rc= 0;

#ifndef __WIN__
#ifndef PTHREAD_STACK_MIN
#define PTHREAD_STACK_MIN      32768
#endif
  /*
    Set stack size to be safe on the platforms with too small
    default thread stack.
  */
  rc= pthread_attr_setstacksize(attr,
                                (size_t) (PTHREAD_STACK_MIN +
                                          IM_THREAD_STACK_SIZE));
#endif
  if (!rc)
    rc= pthread_create(thread, attr, start_routine, arg);
  return rc;
}


Thread::~Thread()
{
}


void *Thread::thread_func(void *arg)
{
  Thread *thread= (Thread *) arg;
  my_thread_init();

  thread->run();

  my_thread_end();
  return NULL;
}


bool Thread::start(enum_thread_type thread_type)
{
  pthread_attr_t attr;
  int rc;

  pthread_attr_init(&attr);

  if (thread_type == DETACHED)
  {
    detached = TRUE;
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
  }
  else
  {
    detached = FALSE;
  }

  rc= set_stacksize_and_create_thread(&id, &attr, Thread::thread_func, this);
  pthread_attr_destroy(&attr);

  return rc != 0;
}


bool Thread::join()
{
  DBUG_ASSERT(!detached);

  return pthread_join(id, NULL) != 0;
}


int Thread_registry::get_error_status()
{
  int ret_error_status;

  pthread_mutex_lock(&LOCK_thread_registry);
  ret_error_status= error_status;
  pthread_mutex_unlock(&LOCK_thread_registry);

  return ret_error_status;
}


void Thread_registry::set_error_status()
{
  pthread_mutex_lock(&LOCK_thread_registry);
  error_status= TRUE;
  pthread_mutex_unlock(&LOCK_thread_registry);
}
