/* Copyright (c) 2014, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef MUTEX_LOCK_INCLUDED
#define MUTEX_LOCK_INCLUDED

/**
  @file include/mutex_lock.h
*/
#include <mysql/psi/mysql_mutex.h>
#include <utility>

/**
  A simple wrapper around a mutex:
  Grabs the mutex in the CTOR, releases it in the DTOR.
  The mutex may be NULL, in which case this is a no-op.
  Templated to allow unit testing with mocked mutex. Not copyable since
  ownership of a mutex cannot be shared, but movable so that ownership can be
  transferred to a different Mutex_lock.
*/
template <class MUTEX>
class Generic_mutex_lock {
 public:
  Generic_mutex_lock() noexcept = default;
  Generic_mutex_lock(MUTEX *mutex, const char *src_file, int src_line) noexcept
      : m_mutex(mutex), m_src_file(src_file), m_src_line(src_line) {
    if (m_mutex) {
      mysql_mutex_lock_with_src(m_mutex, m_src_file, m_src_line);
    }
  }
  ~Generic_mutex_lock() noexcept {
    if (m_mutex) {
      mysql_mutex_unlock_with_src(m_mutex, m_src_file, m_src_line);
    }
  }
  Generic_mutex_lock(const Generic_mutex_lock &) = delete;
  Generic_mutex_lock(Generic_mutex_lock &&src) noexcept
      : m_mutex{src.m_mutex},
        m_src_file{src.m_src_file},
        m_src_line{src.m_src_line} {
    src.m_mutex = nullptr;
    src.m_src_file = nullptr;
    src.m_src_line = 0;
  }

  Generic_mutex_lock &operator=(const Generic_mutex_lock &) = delete;
  Generic_mutex_lock &operator=(Generic_mutex_lock &&src) noexcept {
    Generic_mutex_lock tmp{std::move(src)};
    std::swap(m_mutex, tmp.m_mutex);
    m_src_file = tmp.m_src_file;
    m_src_line = tmp.m_src_line;
    return *this;
  }

 private:
  MUTEX *m_mutex = nullptr;
  const char *m_src_file = nullptr;
  int m_src_line = 0;
};

using Mutex_lock = Generic_mutex_lock<mysql_mutex_t>;

#define MUTEX_LOCK(NAME, X) const Mutex_lock NAME(X, __FILE__, __LINE__)

#endif  // MUTEX_LOCK_INCLUDED
