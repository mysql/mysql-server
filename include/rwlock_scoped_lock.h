/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#ifndef MYSQL_RWLOCK_SCOPED_LOCK_H
#define MYSQL_RWLOCK_SCOPED_LOCK_H

#include "mysql/psi/mysql_rwlock.h"

/**
  Locks RW-lock and releases lock on scope exit.
*/
class rwlock_scoped_lock
{
public:
  rwlock_scoped_lock(mysql_rwlock_t* lock, bool lock_for_write,
    const char* file, int line);
  rwlock_scoped_lock(rwlock_scoped_lock&& lock);
  rwlock_scoped_lock(const rwlock_scoped_lock&) = delete;
  ~rwlock_scoped_lock();

  rwlock_scoped_lock& operator=(const rwlock_scoped_lock&) = delete;

private:
  mysql_rwlock_t* m_lock;
};


#endif /* MYSQL_RWLOCK_SCOPE_LOCK_H */
