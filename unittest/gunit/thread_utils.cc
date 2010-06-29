/* Copyright (C) 2009 Sun Microsystems, Inc.

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

#include <gtest/gtest.h>
#include "thread_utils.h"

namespace thread {

namespace {
void *thread_start_routine(void *arg)
{
  Thread *thread= (Thread*) arg;
  Thread::run_wrapper(thread);
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
}


int Thread::start()
{
  return pthread_create(&m_thread_id, NULL, thread_start_routine, this);
}


void Thread::join()
{
  int failed= pthread_join(m_thread_id, NULL);
  ASSERT_FALSE(failed);
}


void Thread::run_wrapper(Thread *thread)
{
  const my_bool error= my_thread_init();
  ASSERT_FALSE(error);
  thread->run();
  my_thread_end();
}


Mutex_lock::Mutex_lock(pthread_mutex_t *mutex) : m_mutex(mutex)
{
  pthread_mutex_lock(m_mutex);
}


Mutex_lock::~Mutex_lock()
{
  const int failed= pthread_mutex_unlock(m_mutex);
  LOCAL_ASSERT_FALSE(failed);
}


Notification::Notification() : m_notified(FALSE)
{
  const int failed1= pthread_cond_init(&m_cond, NULL);
  LOCAL_ASSERT_FALSE(failed1);
  const int failed2= pthread_mutex_init(&m_mutex, MY_MUTEX_INIT_FAST);
  LOCAL_ASSERT_FALSE(failed2);
}

Notification::~Notification()
{
  pthread_mutex_destroy(&m_mutex);
  pthread_cond_destroy(&m_cond);
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
    const int failed= pthread_cond_wait(&m_cond, &m_mutex);
    ASSERT_FALSE(failed);
  }
}

void Notification::notify()
{
  Mutex_lock lock(&m_mutex);
  m_notified= TRUE;
  const int failed= pthread_cond_broadcast(&m_cond);
  ASSERT_FALSE(failed);
}


}  // namespace thread
