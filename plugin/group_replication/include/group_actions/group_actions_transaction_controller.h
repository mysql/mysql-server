/* Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#ifndef GR_GROUP_ACTIONS_TRANSACTION_CONTROLLER_INCLUDED
#define GR_GROUP_ACTIONS_TRANSACTION_CONTROLLER_INCLUDED

#include <mysql/components/my_service.h>
#include <mysql/components/services/mysql_transaction_delegate_control.h>  // Service mysql_transaction_delegate_control
#include "plugin/group_replication/include/plugin_utils.h"  // thread_state

/**
  @class Transaction_monitor_thread
  @brief Class for creating a new thread that allows to stop the new
  transactions allowing some management queries to run. This thread also
  gracefully disconnects the client which are running the binloggable
  transaction after specified time.
*/
class Transaction_monitor_thread {
 public:
  /**
    Deleted copy constructor.
  */
  Transaction_monitor_thread(const Transaction_monitor_thread &) = delete;

  /**
    Deleted move constructor.
  */
  Transaction_monitor_thread(const Transaction_monitor_thread &&) = delete;

  /**
    Deleted assignment operator.
  */
  Transaction_monitor_thread &operator=(const Transaction_monitor_thread &) =
      delete;

  /**
    Deleted move operator.
  */
  Transaction_monitor_thread &operator=(const Transaction_monitor_thread &&) =
      delete;

  /**
    Initializes the synchronization primitives of the thread.
  */
  Transaction_monitor_thread(uint32 timeout_arg);

  /**
    The destructor for the thread will destroy the mutex and cond_var.
  */
  ~Transaction_monitor_thread();

  /**
    Terminates the thread. Thread sets the abort flag and affectively waits for
    the operations to terminate.

    @return success
    @retval true failed to terminate
    @retval false thread terminated successfully.
  */
  bool terminate();

  /**
    Starts the process of monitoring transactions.

    @return whether or not we managed to launch the transaction_monitor_thread
    thread.
      @retval 0 the thread launched successfully
      @retval != 0 the thread couldn't be launched
    @sa mysql_thread_create
  */
  int start();

 private:
  /**
    The thread callback passed onto mysql_thread_create.

    @param[in] arg a pointer to an Transaction_monitor_thread instance.

    @return nullptr, since the return value is not used.
  */
  static void *launch_thread(void *arg);

  /**
    The thread handle, i.e. setups and tearsdown the infrastructure for this
    mysql_thread.
  */
  [[noreturn]] void transaction_thread_handle();

 private:
  /**
    This function acquires the below services:
    mysql_new_transaction_control
    mysql_before_commit_transaction_control
    mysql_close_connection_of_binloggable_transaction_not_reached_commit

    @return false success
            true fail
  */
  bool acquire_services();
  /**
    This function releases the services.

    @return false success
            true fail
  */
  bool release_services();

 private:
  /** the state of the thread. */
  thread_state m_transaction_monitor_thd_state;
  /** the thread handle. */
  my_thread_handle m_handle;
  /** the mutex for controlling access to the thread itself. */
  mysql_mutex_t m_run_lock;
  /** the cond_var used to signal the thread. */
  mysql_cond_t m_run_cond;
  /** flag to indicate whether or not the thread is to be aborted. */
  bool m_abort;
  /**
    The number of seconds to wait before setting the THD::KILL_CONNECTION flag
    for the transactions that did not reach commit stage.
  */
  int32 m_transaction_timeout{-1};
  /**
    Stores operation start time.
  */
  std::chrono::time_point<std::chrono::steady_clock> m_time_start_of_operation;

  /**
    Pointer to the `mysql_new_transaction_control_imp` service.
  */
  SERVICE_TYPE_NO_CONST(mysql_new_transaction_control) *
      m_mysql_new_transaction_control{nullptr};

  /**
    Pointer to the `mysql_before_commit_transaction_control` service.
  */
  SERVICE_TYPE_NO_CONST(mysql_before_commit_transaction_control) *
      m_mysql_before_commit_transaction_control{nullptr};

  /**
    Pointer to the
    `mysql_close_connection_of_binloggable_transaction_not_reached_commit`
    service.
  */
  SERVICE_TYPE_NO_CONST(
      mysql_close_connection_of_binloggable_transaction_not_reached_commit) *
      m_mysql_close_connection_of_binloggable_transaction_not_reached_commit{
          nullptr};
};

#endif /* GR_GROUP_ACTIONS_TRANSACTION_CONTROLLER_INCLUDED */
