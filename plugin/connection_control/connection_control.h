/* Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

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

#ifndef CONNECTION_CONTROL_H
#define CONNECTION_CONTROL_H

#include <mysql/psi/mysql_thread.h>     /* mysql_rwlock_t */

#include "connection_control_data.h"

namespace connection_control
{

  /** Helper class : Wrapper on READ lock */

  class RD_lock
  {
  public:
    explicit RD_lock(mysql_rwlock_t *lock) : m_lock(lock)
    {
      if (m_lock)
        mysql_rwlock_rdlock(m_lock);
    }
    ~RD_lock()
    {
      if (m_lock)
        mysql_rwlock_unlock(m_lock);
    }
    void lock()
    {
      mysql_rwlock_rdlock(m_lock);
    }
    void unlock()
    {
      mysql_rwlock_unlock(m_lock);
    }
  private:
    mysql_rwlock_t *m_lock;

    RD_lock(const RD_lock&);             /* Not copyable. */
    void operator=(const RD_lock&);      /* Not assignable. */
  };


  /** Helper class : Wrapper on write lock */

  class WR_lock
  {
  public:
    explicit WR_lock(mysql_rwlock_t *lock) : m_lock(lock)
    {
      if (m_lock)
        mysql_rwlock_wrlock(m_lock);
    }
    ~WR_lock()
    {
      if (m_lock)
        mysql_rwlock_unlock(m_lock);
    }
    void lock()
    {
      mysql_rwlock_wrlock(m_lock);
    }
    void unlock()
    {
      mysql_rwlock_unlock(m_lock);
    }
  private:
    mysql_rwlock_t *m_lock;

    WR_lock(const WR_lock&);             /* Not copyable. */
    void operator=(const WR_lock&);      /* Not assignable. */
  };
}
#endif /* !CONNECTION_CONTROL_H */
