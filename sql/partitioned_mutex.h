#ifndef PARTITIONED_MUTEX_INCLUDED
#define PARTITIONED_MUTEX_INCLUDED

/* Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

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

#include "mysql/psi/mysql_thread.h"
#include <partitioned_lock.h>

/**
  Implementation of a partitioned read-write lock using mutexes.

  This rwlock provides better scalability in read-heavy environments by
  employing the following simple trick:
  *) Read lock is acquired by locking only on one of its partition mutexes.
     The specific partition is chosen according to thread id.
  *) Write lock is acquired by locking all partition mutexes.

  This way concurrent request for read lock made by different threads
  have a good chance not to disturb each other by doing cache invalidation
  and atomic operations. As result scalability in this scenario improves.
  OTOH acquisition of write lock becomes more expensive. So this rwlock
  is not supposed to be used in cases when number of write requests is
  significant.
*/

class Partitioned_mutex : public Partitioned_lock
{
public:
  Partitioned_mutex()
  {}

  /**
    @param parts    Number of partitions.
    @param psi_key  P_S instrumentation key to use for rwlock instances
    for partitions.
    */
  bool init(uint parts,
#ifdef HAVE_PSI_INTERFACE
            PSI_mutex_key psi_key,
#endif
            const native_mutexattr_t *attr
            )
  {
    m_parts= parts;
    if (!(m_locks_array= new (std::nothrow) mysql_mutex_t[m_parts]))
      return true;
    for (uint i= 0; i < m_parts; ++i)
      mysql_mutex_init(psi_key, &m_locks_array[i], attr);
    return false;
  }
  void destroy()
  {
    for (uint i= 0; i < m_parts; ++i)
      mysql_mutex_destroy(&m_locks_array[i]);
    delete[] m_locks_array;
  }
  void wrlock()
  {
    for (uint i= 0; i < m_parts; ++i)
      mysql_mutex_lock(&m_locks_array[i]);
  }
  void wrunlock()
  {
    for (uint i= 0; i < m_parts; ++i)
      mysql_mutex_unlock(&m_locks_array[i]);
  }
  int rdlock(uint part_id)
  {
    return mysql_mutex_lock(&m_locks_array[part_id%m_parts]);
  }
  /*
    One should use the same partition id for releasing read lock
    as was used for acquiring it.
  */
  int rdunlock(uint part_id)
  {
    return mysql_mutex_unlock(&m_locks_array[part_id%m_parts]);
  }

  void assert_not_owner()
  {
    for (uint i= 0; i < m_parts; ++i)
      mysql_mutex_assert_not_owner(&m_locks_array[i]);
  }

  /**
    Check the relevant mutex

    Note that we don't check the rest since wrlock is also a rdlock

    @param part_id   the partitioning thread id
  */
  void assert_rdlock_owner(uint part_id)
  {
    mysql_mutex_assert_owner(&m_locks_array[part_id%m_parts]);
  }

  void assert_wrlock_owner()
  {
    for (uint i= 0; i < m_parts; ++i)
      mysql_mutex_assert_owner(&m_locks_array[i]);
  }

private:
  mysql_mutex_t* m_locks_array;
  uint m_parts;

  Partitioned_mutex(const Partitioned_mutex&);            // Non-copyable
  Partitioned_mutex& operator=(const Partitioned_mutex&); // Non-copyable
};

#endif /* PARTITIONED_MUTEX_INCLUDED */
