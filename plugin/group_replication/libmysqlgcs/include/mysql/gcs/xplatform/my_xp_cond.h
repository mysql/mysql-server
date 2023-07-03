/* Copyright (c) 2015, 2022, Oracle and/or its affiliates.

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

#ifndef MY_XP_COND_INCLUDED
#define MY_XP_COND_INCLUDED

#include "mysql/psi/mysql_cond.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/xplatform/my_xp_mutex.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/xplatform/my_xp_util.h"

/**
  @class My_xp_cond

  Abstract class used to wrap condition for various implementations.

  A typical use case of cond is:

  @code{.cpp}

  My_xp_cond *cond= new My_xp_cond_impl();
  cond->init(cond_PSI_key);

  cond->signal();

  @endcode
*/
class My_xp_cond {
 public:
  /**
    Initialize cond.

    @param key cond instrumentation key

    @return success status
  */

  virtual int init(PSI_cond_key key) = 0;

  /**
    Destroy cond.

    @return success status
  */

  virtual int destroy() = 0;

  /**
    Wait for cond to be signaled during some time before unlocking mutex.

    @param mutex mutex to unlock
    @param abstime time to wait
    @return success status
  */

  virtual int timed_wait(mysql_mutex_t *mutex,
                         const struct timespec *abstime) = 0;

  /**
    Wait for cond to be signaled to unlock mutex.

    @param mutex mutex to unlock
    @return success status
  */

  virtual int wait(mysql_mutex_t *mutex) = 0;

  /**
    Signal cond.

    @return success status
  */

  virtual int signal() = 0;

  /**
    Broadcast cond.

    @return success status
  */

  virtual int broadcast() = 0;

  /**
    Get reference to native cond.

    @return native cond
  */

  virtual mysql_cond_t *get_native_cond() = 0;

  virtual ~My_xp_cond() = default;
};

#ifndef XCOM_STANDALONE
class My_xp_cond_server : public My_xp_cond {
 public:
  explicit My_xp_cond_server();
  ~My_xp_cond_server() override;

  int init(PSI_cond_key key) override;
  int destroy() override;
  int timed_wait(mysql_mutex_t *mutex, const struct timespec *abstime) override;
  int wait(mysql_mutex_t *mutex) override;
  int signal() override;
  int broadcast() override;
  mysql_cond_t *get_native_cond() override;

 protected:
  mysql_cond_t *m_cond;
};
#endif

#ifndef XCOM_STANDALONE
class My_xp_cond_impl : public My_xp_cond_server
#endif
{
 public:
  explicit My_xp_cond_impl() = default;
  ~My_xp_cond_impl() override = default;
};

#endif  // MY_XP_COND_INCLUDED
