#ifndef PARTITIONED_RWLOCK_INCLUDED
#define PARTITIONED_RWLOCK_INCLUDED

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
  Implementation of read-write lock partitioned by thread id.

  This rwlock provides better scalability in read-heavy environments by
  employing the following simple trick:
  *) Read lock is acquired only on one of its partitions. The specific
     partition is chosen according to thread id.
  *) Write lock is acquired on all partitions.

  This way concurrent request for read lock made by different threads
  have a good chance not to disturb each other by doing cache invalidation
  and atomic operations. As result scalability in this scenario improves.
  OTOH acquisition of write lock becomes more expensive. So this rwlock
  is not supposed to be used in cases when number of write requests is
  significant.
*/

class Partitioned_rwlock : public Partitioned_lock
{
public:
  Partitioned_rwlock() {}

  /**
    @param parts    Number of partitions.
    @param psi_key  P_S instrumentation key to use for rwlock instances
                    for partitions.
  */
  bool init(uint parts
#ifdef HAVE_PSI_INTERFACE
            , PSI_rwlock_key psi_key
#endif
            )
  {
    m_parts= parts;
    if (!(m_locks_array= new (std::nothrow) mysql_rwlock_t[m_parts]))
      return true;
    for (uint i= 0 ; i < m_parts; ++i)
      mysql_rwlock_init(psi_key, &m_locks_array[i]);
    return false;
  }
  void destroy()
  {
    for (uint i= 0 ; i < m_parts; ++i)
      mysql_rwlock_destroy(&m_locks_array[i]);
    delete [] m_locks_array;
  }
  void wrlock()
  {
    for (uint i= 0 ; i < m_parts; ++i)
      mysql_rwlock_wrlock(&m_locks_array[i]);
  }
  void wrunlock()
  {
    for (uint i= 0 ; i < m_parts; ++i)
      mysql_rwlock_unlock(&m_locks_array[i]);
  }
  int rdlock(uint part_id)
  {
    return mysql_rwlock_rdlock(&m_locks_array[part_id%m_parts]);
  }
  /*
    One should use the same partition id for releasing read lock
    as was used for acquiring it.
  */
  int rdunlock(uint part_id)
  {
    return mysql_rwlock_unlock(&m_locks_array[part_id%m_parts]);
  }

  /** not applicable to this type of locks */
  virtual void assert_not_owner() { };
  /** not applicable to this type of locks */
  virtual void assert_rdlock_owner(uint part_id) {};
  /** not applicable to this type of locks */
  virtual void assert_wrlock_owner() {};


private:
  mysql_rwlock_t* m_locks_array;
  uint m_parts;

  Partitioned_rwlock(const Partitioned_rwlock&);            // Non-copyable
  Partitioned_rwlock& operator=(const Partitioned_rwlock&); // Non-copyable
};

#endif /* PARTITIONED_RWLOCK_INCLUDED */
