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

#include "thread.h"

namespace thread {

namespace {
void *thread_start_routine(void *arg)
{
  Thread *thread= (Thread*) arg;
  Thread::run_wrapper(thread);
  return NULL;
}
}  // namespace

Thread::~Thread()
{
}


int Thread::start(const Options &options)
{
  m_options = options;
  DBUG_ASSERT(m_options.stack_size() >= PTHREAD_STACK_MIN);
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  if (options.detached())
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
  pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
  pthread_attr_setstacksize(&attr, m_options.stack_size());
  const int error=
    pthread_create(&m_thread_id, &attr, thread_start_routine, this);
  pthread_attr_destroy(&attr);
  return error;                            
}


void Thread::join()
{
  DBUG_ASSERT(!m_options.detached());
  int failed= pthread_join(m_thread_id, NULL);
  DBUG_ASSERT(!failed);
}


void Thread::run_wrapper(Thread *thread)
{
  const my_bool error= my_thread_init();
  DBUG_ASSERT(!error);
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
  DBUG_ASSERT(!failed);
}


Notification::Notification() : m_notified(false)
{
  const int failed1= pthread_cond_init(&m_cond, NULL);
  DBUG_ASSERT(!failed1);
  const int failed2= pthread_mutex_init(&m_mutex, MY_MUTEX_INIT_FAST);
  DBUG_ASSERT(!failed2);
}

Notification::~Notification()
{
  const int failed1= pthread_mutex_destroy(&m_mutex);
  DBUG_ASSERT(!failed1);
  const int failed2= pthread_cond_destroy(&m_cond);
  DBUG_ASSERT(!failed2);
}

bool Notification::has_been_notified()
{
  Mutex_lock lock(&m_mutex);
  return m_notified;
}

void Notification::wait_for_notification()
{
  Mutex_lock lock(&m_mutex);
  while (!m_notified) {
    const int failed= pthread_cond_wait(&m_cond, &m_mutex);
    DBUG_ASSERT(!failed);
  }
}

void Notification::notify()
{
  Mutex_lock lock(&m_mutex);
  m_notified= true;
  const int failed= pthread_cond_broadcast(&m_cond);
  DBUG_ASSERT(!failed);
}


}  // namespace thread
