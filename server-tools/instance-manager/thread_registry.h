#ifndef INCLUDES_MYSQL_INSTANCE_MANAGER_THREAD_REGISTRY_H
#define INCLUDES_MYSQL_INSTANCE_MANAGER_THREAD_REGISTRY_H
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

/*
  A multi-threaded application shall nicely work with signals.

  This means it shall, first of all, shut down nicely on ``quit'' signals:
  stop all running threads, cleanup and exit.

  Note, that a thread can't be shut down nicely if it doesn't want to be.
  That's why to perform clean shutdown, all threads constituting a process
  must observe certain rules. Here we use the rules, described in Butenhof
  book 'Programming with POSIX threads', namely:
  - all user signals are handled in 'signal thread' in synchronous manner
  (by means of sigwait). To guarantee that the signal thread is the only who
  can receive user signals, all threads block them, and signal thread is
  the only who calls sigwait() with an apporpriate sigmask.
  To propogate a signal to the workers the signal thread sets
  a variable, corresponding to the signal. Additionally the signal thread
  sends each worker an internal signal (by means of pthread_kill) to kick it
  out from possible blocking syscall, and possibly pthread_cond_signal if
  some thread is blocked in pthread_cond_[timed]wait.
  - a worker handles only internal 'kick' signal (the handler does nothing).
  In case when a syscall returns 'EINTR' the worker checks all
  signal-related variables and behaves accordingly.
  Also these variables shall be checked from time to time in long
  CPU-bounded operations, and before/after pthread_cond_wait. (It's supposed
  that a worker thread either waits in a syscall/conditional variable, or
  computes something.)
  - to guarantee signal deliverence, there should be some kind of feedback,
  e. g. all workers shall account in the signal thread Thread Repository and
  unregister from it on exit.

  Configuration reload (on SIGHUP) and thread timeouts/alarms can be handled
  in manner, similar to ``quit'' signals.
*/

#include <my_global.h>
#include <my_pthread.h>

#if defined(__GNUC__) && defined(USE_PRAGMA_INTERFACE)
#pragma interface
#endif

/*
  Thread_info - repository entry for each worker thread
  All entries comprise double-linked list like:
     0 -- entry -- entry -- entry - 0
  Double-linked list is used to unregister threads easy.
*/

class Thread_info
{
public:
  Thread_info();
  Thread_info(pthread_t thread_id_arg);
  friend class Thread_registry;
private:
  pthread_cond_t *current_cond;
  Thread_info *prev, *next;
  pthread_t thread_id;
};


/*
  Thread_registry - contains handles for each worker thread to deliver
  signal information to workers.
*/

class Thread_registry
{
public:
  Thread_registry();
  ~Thread_registry();

  void register_thread(Thread_info *info);
  void unregister_thread(Thread_info *info);
  void deliver_shutdown();
  void request_shutdown();
  inline bool is_shutdown();
  int cond_wait(Thread_info *info, pthread_cond_t *cond,
                 pthread_mutex_t *mutex);
  int cond_timedwait(Thread_info *info, pthread_cond_t *cond,
                     pthread_mutex_t *mutex, struct timespec *wait_time);
private:
  Thread_info head;
  bool shutdown_in_progress;
  pthread_mutex_t LOCK_thread_registry;
  pthread_cond_t COND_thread_registry_is_empty;
  pthread_t sigwait_thread_pid;
};


inline bool Thread_registry::is_shutdown()
{
  pthread_mutex_lock(&LOCK_thread_registry);
  bool res= shutdown_in_progress;
  pthread_mutex_unlock(&LOCK_thread_registry);
  return res;
}


#endif /* INCLUDES_MYSQL_INSTANCE_MANAGER_THREAD_REGISTRY_H */
