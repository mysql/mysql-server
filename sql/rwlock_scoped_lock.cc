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
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02111-1307  USA */

#include "rwlock_scoped_lock.h"

#include <stddef.h>

/**
  Acquires lock on specified lock object.
  The lock may be NULL, in which case this is a no-op.

  @param lock Lock object to lock.
  @param lock_for_write Specifies if to lock for write or read.
  @param file File in which lock acquisition is to be presented.
  @param line Line of file in which lock acquisition is to be presented.
*/
rwlock_scoped_lock::rwlock_scoped_lock(
  mysql_rwlock_t* lock, bool lock_for_write, const char* file, int line)
  : m_lock(lock)
{
  if (lock_for_write)
  {
    if (!mysql_rwlock_wrlock_indirect(lock, file, line))
    {
      m_lock= lock;
    }
  }
  else
  {
    if (!mysql_rwlock_rdlock_indirect(lock, file, line))
    {
      m_lock= lock;
    }
  }
}

/**
  Moves lock from another object.

  @param lock Scoped lock object to move from.
*/
rwlock_scoped_lock::rwlock_scoped_lock(
  rwlock_scoped_lock&& lock)
  : m_lock(lock.m_lock)
{
  lock.m_lock= NULL;
}

rwlock_scoped_lock::~rwlock_scoped_lock()
{
  /* If lock is NULL, then lock was set to remain locked when going out of
    scope or was moved to other object. */
  if (m_lock != NULL)
  {
    mysql_rwlock_unlock(m_lock);
  }
}
