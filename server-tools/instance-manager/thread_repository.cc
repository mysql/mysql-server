/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

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

#ifdef __GNUC__
#pragma implementation 
#endif

#include "thread_repository.h"
#include <assert.h>
#include <signal.h>
#include "log.h"


/* Kick-off signal handler */
  
enum { THREAD_KICK_OFF_SIGNAL= SIGUSR2 };

static void handle_signal(int __attribute__((unused)) sig_no)
{
}


/*
  TODO: think about moving signal information (now it's shutdown_in_progress)
  to Thread_info. It will reduce contention and allow signal deliverence to
  a particular thread, not to the whole worker crew 
*/

Thread_repository::Thread_repository() :
  shutdown_in_progress(false)
{
  pthread_mutex_init(&LOCK_thread_repository, 0);
  pthread_cond_init(&COND_thread_repository_is_empty, 0);

  /* head is used by-value to simplify nodes inserting */
  head.next= head.prev= &head;
}


Thread_repository::~Thread_repository()
{
  /* Check that no one uses the repository. */
  pthread_mutex_lock(&LOCK_thread_repository);

  /* All threads must unregister */
  DBUG_ASSERT(head.next == &head);

  pthread_mutex_unlock(&LOCK_thread_repository);
  pthread_cond_destroy(&COND_thread_repository_is_empty);
  pthread_mutex_destroy(&LOCK_thread_repository);
}


/*
  
  Set signal handler for kick-off thread, and insert a thread info to the
  repository. New node is appended to the end of the list; head.prev always
  points to the last node.
*/

void Thread_repository::register_thread(Thread_info *info)
{
  struct sigaction sa;
  sa.sa_handler= handle_signal;
  sa.sa_flags= 0;
  sigemptyset(&sa.sa_mask);
  sigaction(THREAD_KICK_OFF_SIGNAL, &sa, 0);

  info->current_cond= 0;

  pthread_mutex_lock(&LOCK_thread_repository);
  info->next= &head;
  info->prev= head.prev;
  head.prev->next= info;
  head.prev= info;
  pthread_mutex_unlock(&LOCK_thread_repository);
}


/*
  Unregister a thread from the repository and free Thread_info structure.
  Every registered thread must unregister. Unregistering should be the last 
  thing a thread is doing, otherwise it could have no time to finalize.
*/

void Thread_repository::unregister_thread(Thread_info *info)
{
  pthread_mutex_lock(&LOCK_thread_repository);
  info->prev->next= info->next;
  info->next->prev= info->prev;
  if (head.next == &head)
    pthread_cond_signal(&COND_thread_repository_is_empty);
  pthread_mutex_unlock(&LOCK_thread_repository);
}


/*
  Check whether shutdown is in progress, and if yes, return immidiately.
  Else set info->current_cond and call pthread_cond_wait. When
  pthread_cond_wait returns, unregister current cond and check the shutdown
  status again.
  RETURN VALUE
    return value from pthread_cond_wait
*/

int Thread_repository::cond_wait(Thread_info *info, pthread_cond_t *cond,
                                  pthread_mutex_t *mutex, bool *is_shutdown)
{
  pthread_mutex_lock(&LOCK_thread_repository);
  *is_shutdown= shutdown_in_progress;
  if (*is_shutdown)
  {
    pthread_mutex_unlock(&LOCK_thread_repository);
    return 0;
  }
  info->current_cond= cond;
  pthread_mutex_unlock(&LOCK_thread_repository);
  /* sic: race condition here, cond can be signaled in deliver_shutdown */
  int rc= pthread_cond_wait(cond, mutex);
  pthread_mutex_lock(&LOCK_thread_repository);
  info->current_cond= 0;
  *is_shutdown= shutdown_in_progress;
  pthread_mutex_unlock(&LOCK_thread_repository);
  return rc;
}


/*
  Deliver shutdown message to the workers crew.
  As it's impossible to avoid all race conditions, we signal latecomers
  again.
*/

void Thread_repository::deliver_shutdown()
{
  struct timespec shutdown_time;
  set_timespec(shutdown_time, 1);
  Thread_info *info;

  pthread_mutex_lock(&LOCK_thread_repository);
  shutdown_in_progress= true;
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
  while (pthread_cond_timedwait(&COND_thread_repository_is_empty,
                                &LOCK_thread_repository,
                                &shutdown_time) != ETIMEDOUT &&
         head.next != &head)
    ;
  /*
    If previous signals did not reach some threads, they must be sleeping 
    in pthread_cond_wait or a blocking syscall. Wake them up: 
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
  pthread_mutex_unlock(&LOCK_thread_repository);
}

