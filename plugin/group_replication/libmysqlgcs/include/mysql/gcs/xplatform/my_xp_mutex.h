/* Copyright (c) 2015, 2024, Oracle and/or its affiliates.

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

#ifndef MY_XP_MUTEX_INCLUDED
#define MY_XP_MUTEX_INCLUDED

#include "mysql/psi/mysql_mutex.h"

/**
  @class My_xp_mutex

  Abstract class used to wrap mutex for various implementations.

  A typical use case is:

  @code{.cpp}

  My_xp_mutex *mutex= new My_xp_mutex_impl();
  mutex->init(mutex_PSI_key, NULL);

  mutex->lock();
  ...
  mutex->unlock();

  @endcode
*/
class My_xp_mutex {
 public:
  /**
    Initialize mutex.

    @param key mutex instrumentation key
    @param attr mutex attributes reference
    @return success status
  */

  virtual int init(PSI_mutex_key key, const native_mutexattr_t *attr) = 0;

  /**
    Destroy mutex.

    @return success status
  */

  virtual int destroy() = 0;

  /**
    Lock mutex.

    @return success status
  */

  virtual int lock() = 0;

  /**
    Trylock mutex.

    @return success status
  */

  virtual int trylock() = 0;

  /**
    Unlock mutex.

    @return success status
  */

  virtual int unlock() = 0;

  /**
    To get native mutex reference.

    @return native mutex pointer
  */

  virtual mysql_mutex_t *get_native_mutex() = 0;

  virtual ~My_xp_mutex() = default;
};

#ifndef XCOM_STANDALONE
class My_xp_mutex_server : public My_xp_mutex {
 public:
  explicit My_xp_mutex_server();
  ~My_xp_mutex_server() override;

  int init(PSI_mutex_key key, const native_mutexattr_t *attr) override;
  int destroy() override;
  int lock() override;
  int trylock() override;
  int unlock() override;
  mysql_mutex_t *get_native_mutex() override;

 protected:
  mysql_mutex_t *m_mutex;
};
#endif

#ifndef XCOM_STANDALONE
class My_xp_mutex_impl : public My_xp_mutex_server
#endif
{
 public:
  explicit My_xp_mutex_impl() = default;
  ~My_xp_mutex_impl() override = default;
};

class My_xp_mutex_util {
 public:
  /**
    Initialize mutex attributes object

    @param attr mutex attributes reference
    @return success status
  */

  static int attr_init(native_mutexattr_t *attr);

  /**
    Destroy mutex attributes object

    @param attr mutex attributes reference
    @return success status
  */

  static int attr_destroy(native_mutexattr_t *attr);
};

#endif  // MY_XP_MUTEX_INCLUDED
