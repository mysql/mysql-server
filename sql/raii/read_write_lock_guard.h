/* Copyright (c) 2020, 2022, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef READ_WRITE_LOCK_GUARD_INCLUDED
#define READ_WRITE_LOCK_GUARD_INCLUDED

#include "sentry.h"  // raii::Sentry

/**
  Generic sentry class for read locking.
  For the given callable type the class assumes a read lock can be acquired
  with rdlock() in the constructor.
  On deletion the class will unlock with an unlock() invocation
 */
template <typename Rd_lockable>
class Rdlock_guard : public raii::Sentry<> {
 public:
  /**
   Constructor for the class that creates a sentry that will unlock
   the callable object on destruction and then read locks the object.

   @param lock The callable object to be locked/unlocked
 */
  Rdlock_guard(Rd_lockable &lock)
      : Sentry{[&lock]() -> void { lock.unlock(); }} {
    lock.rdlock();
  }
  virtual ~Rdlock_guard() override = default;
};

/**
  Generic sentry class for write locking.
  For the given callable type the class assumes a write lock can be acquired
  with wrlock() in the constructor.
  On deletion the class will unlock with an unlock() invocation
 */
template <typename Wr_lockable>
class Wrlock_guard : public raii::Sentry<> {
 public:
  /**
    Constructor for the class that creates a sentry that will unlock
    the callable object on destruction and then write locks the object.

    @param lock The callable object to be locked/unlocked
  */
  Wrlock_guard(Wr_lockable &lock)
      : Sentry{[&lock]() -> void { lock.unlock(); }} {
    lock.wrlock();
  }
  virtual ~Wrlock_guard() override = default;
};

#endif  // READ_WRITE_LOCK_GUARD_INCLUDED
