/*
 * Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *  
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#include "plugin/x/ngs/include/ngs/thread.h"

#include <time.h>

#include "my_sys.h"                             // my_thread_stack_size
#include "my_systime.h"
#include "my_thread.h"
#include "plugin/x/ngs/include/ngs/memory.h"


void ngs::thread_create(PSI_thread_key key MY_ATTRIBUTE((unused)),
                        Thread_t *thread,
                        Start_routine_t func, void *arg)
{
  my_thread_attr_t connection_attrib;

  (void)my_thread_attr_init(&connection_attrib);
  /*
   check_stack_overrun() assumes that stack size is (at least)
   my_thread_stack_size. If it is smaller, we may segfault.
  */
  my_thread_attr_setstacksize(&connection_attrib, my_thread_stack_size);

  if (mysql_thread_create(key, thread, &connection_attrib, func, arg))
    throw std::runtime_error("Could not create a thread");
}


int ngs::thread_join(Thread_t *thread, void **ret)
{
  return my_thread_join(thread, ret);
}


ngs::Mutex::Mutex(PSI_mutex_key key MY_ATTRIBUTE((unused)))
{
  mysql_mutex_init(key, &m_mutex, NULL);
}


ngs::Mutex::~Mutex()
{
  mysql_mutex_destroy(&m_mutex);
}


ngs::Mutex::operator mysql_mutex_t*()
{
  return &m_mutex;
}



ngs::RWLock::RWLock(PSI_rwlock_key key MY_ATTRIBUTE((unused)))
{
  mysql_rwlock_init(key, &m_rwlock);
}


ngs::RWLock::~RWLock()
{
  mysql_rwlock_destroy(&m_rwlock);
}


ngs::Cond::Cond(PSI_cond_key key MY_ATTRIBUTE((unused)))
{
  mysql_cond_init(key, &m_cond);
}


ngs::Cond::~Cond()
{
  mysql_cond_destroy(&m_cond);
}


void ngs::Cond::wait(Mutex& mutex)
{
  mysql_cond_wait(&m_cond, &mutex.m_mutex);
}


int ngs::Cond::timed_wait(Mutex& mutex, unsigned long long nanoseconds)
{
  timespec ts;

  set_timespec_nsec(&ts, nanoseconds);

  return mysql_cond_timedwait(&m_cond, &mutex.m_mutex, &ts);
}


void ngs::Cond::signal()
{
  mysql_cond_signal(&m_cond);
}


void ngs::Cond::signal(Mutex& mutex)
{
  MUTEX_LOCK(lock, mutex);

  signal();
}


void ngs::Cond::broadcast()
{
  mysql_cond_broadcast(&m_cond);
}


void ngs::Cond::broadcast(Mutex& mutex)
{
  MUTEX_LOCK(lock, mutex);

  broadcast();
}

unsigned int ngs::x_psf_objects_key = 0;

