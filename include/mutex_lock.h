/* Copyright (c) 2014, 2015, Oracle and/or its affiliates. All rights reserved.

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

#ifndef MUTEX_LOCK_INCLUDED
#define MUTEX_LOCK_INCLUDED

#include <my_global.h>
#include <mysql/psi/mysql_thread.h>

/**
  A simple wrapper around a mutex:
  Grabs the mutex in the CTOR, releases it in the DTOR.
  The mutex may be NULL, in which case this is a no-op.
*/
class Mutex_lock
{
public:
  explicit Mutex_lock(mysql_mutex_t *mutex) : m_mutex(mutex)
  {
    if (m_mutex)
      mysql_mutex_lock(m_mutex);
  }
  ~Mutex_lock()
  {
    if (m_mutex)
      mysql_mutex_unlock(m_mutex);
  }
private:
  mysql_mutex_t *m_mutex;

  Mutex_lock(const Mutex_lock&);                /* Not copyable. */
  void operator=(const Mutex_lock&);            /* Not assignable. */
};

#endif  // MUTEX_LOCK_INCLUDED
