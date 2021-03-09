/* Copyright (c) 2021, Oracle and/or its affiliates.

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

#ifndef MYSQL_THREAD_INCLUDE
#define MYSQL_THREAD_INCLUDE

#include "plugin/group_replication/include/plugin_server_include.h"
#include "plugin/group_replication/include/plugin_utils.h"

class THD;

/**
  @class Mysql_thread_body_parameters

  Interface for Mysql_thread_body parameters.
*/
class Mysql_thread_body_parameters {
 public:
  Mysql_thread_body_parameters() {}
  virtual ~Mysql_thread_body_parameters() {}
};

/**
  @class Mysql_thread_body

  Interface for Mysql_thread_body, the task of a Mysql_thread.
*/
class Mysql_thread_body {
 public:
  virtual ~Mysql_thread_body() {}
  virtual void run(Mysql_thread_body_parameters *parameters) = 0;
};

/**
  @class Mysql_thread

  A generic single thread executor.
*/
class Mysql_thread {
 public:
  /**
    Mysql_thread constructor
  */
  Mysql_thread(Mysql_thread_body *body);
  virtual ~Mysql_thread();

  /**
    Initialize the thread.

    @return the operation status
      @retval false  Successful
      @retval true   Error
  */
  bool initialize();

  /**
    Terminate the thread.

    @return the operation status
      @retval false  Successful
      @retval true   Error
  */
  bool terminate();

  /**
    Thread worker method.
  */
  void dispatcher();

  /**
    Trigger a task to run synchronously.

    @param[in] parameters  parameters of the task to run

    @return the operation status
      @retval false  Successful
      @retval true   Error
  */
  bool trigger(Mysql_thread_body_parameters *parameters);

 private:
  THD *m_thd{nullptr};
  my_thread_handle m_pthd;
  mysql_mutex_t m_run_lock;
  mysql_cond_t m_run_cond;
  thread_state m_state;
  bool m_aborted{false};

  mysql_mutex_t m_dispatcher_lock;
  mysql_cond_t m_dispatcher_cond;
  bool m_trigger_run_complete{false};

  Mysql_thread_body *m_body{nullptr};
  Abortable_synchronized_queue<Mysql_thread_body_parameters *> *m_trigger_queue{
      nullptr};
};

#endif /* MYSQL_THREAD_INCLUDE */
