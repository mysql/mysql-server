/* Copyright (c) 2009, 2010, Oracle and/or its affiliates. All rights reserved.

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

// First include (the generated) my_config.h, to get correct platform defines.
#include "my_config.h"
#include <gtest/gtest.h>

#include "thread_utils.h"
#include "mysql/psi/mysql_thread.h"

namespace thread {

/*
  On windows we need an open handle to the thread in order to join it.
  The three instances of Notification are used as follows:
  - The main thread starts sub-thread, waits for m_thread_started.
  - The main thread picks up the thread id, and does OpenThread to get a handle.
  - The main thread tells the sub-thread that it can continue
      (notifies m_thread_continue)
  - The main thread waits until the sub-thread is actually running
    (waits for m_thread_running) before destroying all objects.

  All this is to work around a bug in pthread_create() in mysys/my_winthread,
  which closes the thread handle (it has nowhere to store it).
  When we later call pthread_join() we try to re-open a handle.
  This will fail if the thread has already finished, and then the join will fail.
 */
class Thread_start_arg
{
public:
  Thread_start_arg(Thread *thread) : m_thread(thread) {}

  Notification m_thread_started;
  Notification m_thread_continue;
  Notification m_thread_running;
  Thread *m_thread;
};

namespace {
extern "C"
void *thread_start_routine(void *arg)
{
  Thread_start_arg *start_arg= (Thread_start_arg*) arg;
  Thread::run_wrapper(start_arg);
  return NULL;
}

// We cannot use ASSERT_FALSE in constructors/destructors,
// so we add a local helper routine.
#define LOCAL_ASSERT_FALSE(arg) assert_false(arg, __LINE__)
void assert_false(int arg, int line)
{
  ASSERT_FALSE(arg) << "failed with arg " << arg << " at line " << line;
}

}  // namespace

Thread::~Thread()
{
#ifdef __WIN__
  if (m_thread_handle != NULL)
    CloseHandle(m_thread_handle);
#endif
}


int Thread::start()
{
  Thread_start_arg start_arg(this);
  const int retval=
    pthread_create(&m_thread_id, NULL, thread_start_routine, &start_arg);
  if (retval != 0)
  {
    ADD_FAILURE() << " could not start thread, errno: " << errno;
    return retval;
  }

  start_arg.m_thread_started.wait_for_notification();
#ifdef __WIN__
  m_thread_handle= OpenThread(SYNCHRONIZE, FALSE, m_thread_id);
  if (m_thread_handle == NULL)
  {
    DWORD lasterror= GetLastError();
    ADD_FAILURE()
      << " could not open thread id " << m_thread_id
      << " GetLastError: " << lasterror
      ;
  }
#endif
  start_arg.m_thread_continue.notify();
  start_arg.m_thread_running.wait_for_notification();
  return retval;
}


#ifdef __WIN__
void Thread::join()
{
  DWORD ret= WaitForSingleObject(m_thread_handle, INFINITE);
  if (ret != WAIT_OBJECT_0)
  {
    DWORD lasterror= GetLastError();
    ADD_FAILURE()
      << " could not join thread id " << m_thread_id
      << " handle " << m_thread_handle
      << " GetLastError: " << lasterror
      ;
  }
  CloseHandle(m_thread_handle);
  m_thread_handle= NULL;
}
#else
void Thread::join()
{
  const int failed= pthread_join(m_thread_id, NULL);
  if (failed)
  {
    ADD_FAILURE()
      << " could not join thread id " << m_thread_id
      << " failed: " << failed << " errno: " << errno
      ;
  }
}
#endif


void Thread::run_wrapper(Thread_start_arg *start_arg)
{
  const my_bool error= my_thread_init();
  ASSERT_FALSE(error);
  Thread *thread= start_arg->m_thread;
  start_arg->m_thread_started.notify();
  start_arg->m_thread_continue.wait_for_notification();
  start_arg->m_thread_running.notify();
  thread->run();
  my_thread_end();
}


Mutex_lock::Mutex_lock(mysql_mutex_t *mutex) : m_mutex(mutex)
{
  mysql_mutex_lock(m_mutex);
}


Mutex_lock::~Mutex_lock()
{
  const int failed= mysql_mutex_unlock(m_mutex);
  LOCAL_ASSERT_FALSE(failed);
}


Notification::Notification() : m_notified(FALSE)
{
  const int failed1= mysql_cond_init(0, &m_cond, NULL);
  LOCAL_ASSERT_FALSE(failed1);
  const int failed2= mysql_mutex_init(0, &m_mutex, MY_MUTEX_INIT_FAST);
  LOCAL_ASSERT_FALSE(failed2);
}

Notification::~Notification()
{
  mysql_mutex_destroy(&m_mutex);
  mysql_cond_destroy(&m_cond);
}

bool Notification::has_been_notified()
{
  Mutex_lock lock(&m_mutex);
  return m_notified;
}

void Notification::wait_for_notification()
{
  Mutex_lock lock(&m_mutex);
  while (!m_notified)
  {
    const int failed= mysql_cond_wait(&m_cond, &m_mutex);
    ASSERT_FALSE(failed);
  }
}

void Notification::notify()
{
  Mutex_lock lock(&m_mutex);
  m_notified= TRUE;
  const int failed= mysql_cond_broadcast(&m_cond);
  ASSERT_FALSE(failed);
}


}  // namespace thread
