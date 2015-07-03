#ifndef PARTITIONED_LOCK_INCLUDED
#define PARTITIONED_LOCK_INCLUDED

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

/**
  Interface to a partitioned lock.

  This lock provides better scalability in read-heavy environments by
  employing the following simple trick:
  *) Read lock is acquired only on one of its partitions. The specific
     partition is chosen according to thread id.
  *) Write lock is acquired on all partitions.

  This way concurrent request for read lock made by different threads
  have a good chance not to disturb each other by doing cache invalidation
  and atomic operations. As result scalability in this scenario improves.
  OTOH acquisition of write lock becomes more expensive. So this lock
  is not supposed to be used in cases when number of write requests is
  significant.
*/

class Partitioned_lock
{
public:
  Partitioned_lock() {};
  virtual ~Partitioned_lock() {};

  virtual void wrlock()= 0;
  virtual void wrunlock()= 0;
  virtual int rdlock(uint part_id)= 0;
  /*
    One should use the same thread number for releasing read lock
    as was used for acquiring it,
  */
  virtual int rdunlock(uint part_id)= 0;

  virtual void assert_not_owner()= 0;
  virtual void assert_rdlock_owner(uint part_id)= 0;
  virtual void assert_wrlock_owner()= 0;


private:
  Partitioned_lock(const Partitioned_lock&);            // Non-copyable
  Partitioned_lock& operator=(const Partitioned_lock&); // Non-copyable
};


/**
  Read lock guard class for Partitioned_lock. Supports early unlocking.
*/

class Partitioned_lock_read_guard
{
public:
  /**
    Acquires read lock on partitioned lock on behalf of thread.
    Automatically release lock in destructor.
  */
  Partitioned_lock_read_guard(Partitioned_lock *lock, uint part_id)
    : m_lock(lock), m_part_id(part_id)
  {
    m_lock->rdlock(m_part_id);
  }

  ~Partitioned_lock_read_guard()
  {
    if (m_lock)
      m_lock->rdunlock(m_part_id);
  }

  /** Release read lock. Optional method for early unlocking. */
  void unlock()
  {
    m_lock->rdunlock(m_part_id);
    m_lock= NULL;
  }

private:
  /**
    Pointer to partitioned lock which was acquired. NULL if lock was
    released early so destructor should not do anything.
  */
  Partitioned_lock *m_lock;
  /**
    Id of partition on which behalf lock was acquired and which is to be used for
    unlocking.
  */
  uint m_part_id;

  // Non-copyable
  Partitioned_lock_read_guard(const Partitioned_lock_read_guard&);
  Partitioned_lock_read_guard& operator=(const Partitioned_lock_read_guard&);
};


/**
  Write lock guard class for Partitioned_lock. Supports early unlocking.
*/

class Partitioned_lock_write_guard
{
public:
  /**
    Acquires write lock on partitioned lock.
    Automatically release it in destructor.
  */
  explicit Partitioned_lock_write_guard(Partitioned_lock *lock)
    : m_lock(lock)
  {
    m_lock->wrlock();
  }

  ~Partitioned_lock_write_guard()
  {
    if (m_lock)
      m_lock->wrunlock();
  }

  /** Release write lock. Optional method for early unlocking. */
  void unlock()
  {
    m_lock->wrunlock();
    m_lock= NULL;
  }

private:
  /**
    Pointer to partitioned lock which was acquired. NULL if lock was
    released early so destructor should not do anything.
  */
  Partitioned_lock *m_lock;

  // Non-copyable
  Partitioned_lock_write_guard(const Partitioned_lock_write_guard&);
  Partitioned_lock_write_guard& operator=(const Partitioned_lock_write_guard&);
};

#endif /* PARTITIONED_LOCK_INCLUDED */
