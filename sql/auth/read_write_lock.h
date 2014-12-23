#ifndef READ_WRITE_LOCK_INCLUDED
#define READ_WRITE_LOCK_INCLUDED

/* Copyright (c) 2008, 2014, Oracle and/or its affiliates. All rights reserved.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software Foundation,
51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#include <boost/noncopyable.hpp>
#include <mysql/psi/mysql_thread.h>


enum enum_lock_at
{
  CREATION,
  DEFER
};


class Read_lock : boost::noncopyable
{
public:
  explicit Read_lock(mysql_rwlock_t* rw_lock, enum_lock_at lock_at= CREATION)
    : rw_lock(rw_lock),
    is_locked(false)
  {
    if (lock_at == CREATION)
    {
      this->lock();
    }
  }


  ~Read_lock()
  {
    this->unlock();
  }


  void inline lock()
  {
    if (!is_locked)
    {
      is_locked= true;
      mysql_rwlock_rdlock(rw_lock);
    }
  }


  void inline unlock()
  {
    if (is_locked)
    {
      mysql_rwlock_unlock(rw_lock);
      is_locked= false;
    }
  }
private:
  mysql_rwlock_t* rw_lock;
  volatile bool is_locked;
};


class Write_lock : boost::noncopyable
{
public:
  explicit Write_lock(mysql_rwlock_t* rw_lock, enum_lock_at lock_at= CREATION)
    : rw_lock(rw_lock),
    is_locked(false)
  {
    if (lock_at == CREATION)
    {
      this->lock();
    }
  }


  ~Write_lock()
  {
    this->unlock();
  }


  void inline lock()
  {
    if (!is_locked)
    {
      is_locked= true;
      mysql_rwlock_wrlock(rw_lock);
    }
  }


  void inline unlock()
  {
    if (is_locked)
    {
      mysql_rwlock_unlock(rw_lock);
      is_locked= false;
    }
  }
private:
  mysql_rwlock_t* rw_lock;
  volatile bool is_locked;
};


#endif /* READ_WRITE_LOCK_INCLUDED */
