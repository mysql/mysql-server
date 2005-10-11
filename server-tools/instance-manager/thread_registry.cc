/* cOPYRIght (C) 2003 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

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

#include "log.h"

#include <assert.h>
#include <signal.h>
#include <thr_alarm.h>


#ifndef __WIN__
/* Kick-off signal handler */

enum { THREAD_KICK_OFF_SIGNAL= SIGUSR2 };

static void handle_signal(int __attribute__((unused)) sig_no)
{
}
#endif

/*
  TODO: think about moving signal information (now it's shutdown_in_progress)
  to Thread_info. It will reduce contention and allow signal deliverence to
  a particular thread, not to the whole worker crew
*/

Thread_registry::Thread_registry() :
   shutdown_in_progress(false)
  ,sigwait_thread_pid(pthread_self())
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

void Thread_registry::register_thread(Thread_info *info)
{
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
  pthread_mutex_lock(&LOCK_thread_registry);
  info->prev->next= info->next;
  info->next->prev= info->prev;
  if (head.next == &head)
    pthread_cond_signal(&COND_thread_registry_is_empty);
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
  Thread_info *info;
  struct timespec shutdown_time;
  int error;
  set_timespec(shutdown_time, 1);

  pthread_mutex_lock(&LOCK_thread_registry);
  shutdown_in_progress= true;

#ifndef __WIN__
  /* to stop reading from the network we need to flush alarm queue */
  end_thr_alarm(0);
  /*
    We have to deliver final alarms this way, as the main thread has already
    stopped alarm processing.
  */
  process_alarm(THR_SERVER_ALARM);
#endif

  for (info= head.next; info != &head; info= info->next)
  {
    pthread_kill(info->thread_id, THREAD_KICK_OFF_SIGNAL);
    /*
      sic: race condition here, the thread may not yet fall into
      pthread_cond_wait.
    */
    if (info->current_cond)
      pthread_cond_signal(info->current_cond);
  }
  /*
    The common practice is to test predicate before pthread_cond_wait.
    I don't do that here because the predicate is practically always false
    before wait - is_shutdown's been just set, and the lock's still not
    released - the only case when the predicate is false is when no other
    threads exist.
  */
  while (((error= pthread_cond_timedwait(&COND_thread_registry_is_empty,
                                          &LOCK_thread_registry,
                                          &shutdown_time)) != ETIMEDOUT &&
          error != ETIME) &&
         head.next != &head)
    ;

  /*
    If previous signals did not reach some threads, they must be sleeping
    in pthread_cond_wait or in a blocking syscall. Wake them up:
    every thread shall check signal variables after each syscall/cond_wait,
    so this time everybody should be informed (presumably each worker can
    get CPU during shutdown_time.)
  */
  for (info= head.next; info != &head; info= info->next)
  {
    pthread_kill(info->thread_id, THREAD_KICK_OFF_SIGNAL);
    if (info->current_cond)
      pthread_cond_signal(info->current_cond);
  }

  pthread_mutex_unlock(&LOCK_thread_registry);
}


void Thread_registry::request_shutdown()
{
  pthread_kill(sigwait_thread_pid, SIGTERM);
}
