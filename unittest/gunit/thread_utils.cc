/* Copyright (c) 2009, 2015, Oracle and/or its affiliates. All rights reserved.

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
#include "mutex_lock.h"

namespace thread {

namespace {
extern "C"
void *thread_start_routine(void *arg)
{
  Thread *start_arg= (Thread*) arg;
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


int Thread::start()
{
  const int retval=
    my_thread_create(&m_thread_handle, NULL, thread_start_routine, this);
  if (retval != 0)
  {
    ADD_FAILURE() << " could not start thread, errno: " << errno;
    return retval;
  }

  return retval;
}


void Thread::join()
{
  const int failed= my_thread_join(&m_thread_handle, NULL);
  if (failed)
  {
    ADD_FAILURE()
      << " could not join thread id " << m_thread_handle.thread
      << " failed: " << failed << " errno: " << errno
      ;
  }
}


void Thread::run_wrapper(Thread *start_arg)
{
  const my_bool error= my_thread_init();
  ASSERT_FALSE(error);
  start_arg->run();
  my_thread_end();
}


Notification::Notification() : m_notified(FALSE)
{
  const int failed1= mysql_cond_init(0, &m_cond);
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
