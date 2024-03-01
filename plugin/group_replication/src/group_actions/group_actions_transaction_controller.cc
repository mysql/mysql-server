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

#include "plugin/group_replication/include/group_actions/group_actions_transaction_controller.h"
#include "plugin/group_replication/include/plugin.h"      // THDs
#include "plugin/group_replication/include/plugin_psi.h"  // For key_GR_*

void *Transaction_monitor_thread::launch_thread(void *arg) {
  Transaction_monitor_thread *thd =
      static_cast<Transaction_monitor_thread *>(arg);
  thd->transaction_thread_handle();
}

Transaction_monitor_thread::Transaction_monitor_thread(uint32 timeout_arg)
    : m_abort(false), m_transaction_timeout(timeout_arg) {
  m_time_start_of_operation = std::chrono::steady_clock::now();
  mysql_mutex_init(key_GR_LOCK_transaction_monitor_module, &m_run_lock,
                   MY_MUTEX_INIT_FAST);
  mysql_cond_init(key_GR_COND_transaction_monitor_module, &m_run_cond);
}

Transaction_monitor_thread::~Transaction_monitor_thread() {
  mysql_mutex_destroy(&m_run_lock);
  mysql_cond_destroy(&m_run_cond);
}

bool Transaction_monitor_thread::acquire_services() {
  bool ret = false;
  // Acquire the 'mysql_new_transaction_control' service.
  if (nullptr == m_mysql_new_transaction_control) {
    my_h_service h_mysql_new_transaction_control = nullptr;
    if (get_plugin_registry()->acquire("mysql_new_transaction_control",
                                       &h_mysql_new_transaction_control) ||
        nullptr == h_mysql_new_transaction_control) {
      /* purecov: begin inspected */
      m_mysql_new_transaction_control = nullptr;
      ret = true;
      goto end;
      /* purecov: end */
    }
    m_mysql_new_transaction_control = reinterpret_cast<SERVICE_TYPE_NO_CONST(
        mysql_new_transaction_control) *>(h_mysql_new_transaction_control);
  }
  // Acquire the 'mysql_before_commit_transaction_control' service.
  if (nullptr == m_mysql_before_commit_transaction_control) {
    my_h_service h_mysql_before_commit_transaction_control = nullptr;
    if (get_plugin_registry()->acquire(
            "mysql_before_commit_transaction_control",
            &h_mysql_before_commit_transaction_control) ||
        nullptr == h_mysql_before_commit_transaction_control) {
      /* purecov: begin inspected */
      m_mysql_before_commit_transaction_control = nullptr;
      ret = true;
      goto end;
      /* purecov: end */
    }
    m_mysql_before_commit_transaction_control =
        reinterpret_cast<SERVICE_TYPE_NO_CONST(
            mysql_before_commit_transaction_control) *>(
            h_mysql_before_commit_transaction_control);
  }
  // Acquire the
  // 'mysql_close_connection_of_binloggable_transaction_not_reached_commit'
  // service.
  if (nullptr ==
      m_mysql_close_connection_of_binloggable_transaction_not_reached_commit) {
    my_h_service
        h_mysql_close_connection_of_binloggable_transaction_not_reached_commit =
            nullptr;
    if (get_plugin_registry()->acquire(
            "mysql_close_connection_of_binloggable_transaction_not_reached_"
            "commit",
            &h_mysql_close_connection_of_binloggable_transaction_not_reached_commit) ||
        nullptr ==
            h_mysql_close_connection_of_binloggable_transaction_not_reached_commit) {
      /* purecov: begin inspected */
      m_mysql_close_connection_of_binloggable_transaction_not_reached_commit =
          nullptr;
      ret = true;
      goto end;
      /* purecov: end */
    }
    m_mysql_close_connection_of_binloggable_transaction_not_reached_commit =
        reinterpret_cast<SERVICE_TYPE_NO_CONST(
            mysql_close_connection_of_binloggable_transaction_not_reached_commit)
                             *>(
            h_mysql_close_connection_of_binloggable_transaction_not_reached_commit);
  }
end:
  return ret;
}

bool Transaction_monitor_thread::release_services() {
  bool ret = false;
  // Release the 'mysql_new_transaction_control' service.
  if (nullptr != m_mysql_new_transaction_control) {
    my_h_service h_mysql_new_transaction_control =
        reinterpret_cast<my_h_service>(m_mysql_new_transaction_control);
    ret |= get_plugin_registry()->release(h_mysql_new_transaction_control) != 0;
    m_mysql_new_transaction_control = nullptr;
  }
  // Release the 'mysql_before_commit_transaction_control' service.
  if (nullptr != m_mysql_before_commit_transaction_control) {
    my_h_service h_mysql_before_commit_transaction_control =
        reinterpret_cast<my_h_service>(
            m_mysql_before_commit_transaction_control);
    ret |= get_plugin_registry()->release(
               h_mysql_before_commit_transaction_control) != 0;
    m_mysql_before_commit_transaction_control = nullptr;
  }
  // Release the
  // 'mysql_close_connection_of_binloggable_transaction_not_reached_commit'
  // service.
  if (nullptr !=
      m_mysql_close_connection_of_binloggable_transaction_not_reached_commit) {
    my_h_service
        h_mysql_close_connection_of_binloggable_transaction_not_reached_commit =
            reinterpret_cast<my_h_service>(
                m_mysql_close_connection_of_binloggable_transaction_not_reached_commit);
    ret |=
        get_plugin_registry()->release(
            h_mysql_close_connection_of_binloggable_transaction_not_reached_commit) !=
        0;
    m_mysql_close_connection_of_binloggable_transaction_not_reached_commit =
        nullptr;
  }
  return ret;
}

bool Transaction_monitor_thread::terminate() {
  DBUG_TRACE;
  bool ret = false;
  mysql_mutex_lock(&m_run_lock);

  m_abort = true;

  while (m_transaction_monitor_thd_state.is_thread_alive()) {
    // Release all waiting threads so they die
    mysql_cond_broadcast(&m_run_cond);

    struct timespec abstime;
    set_timespec(&abstime, 1);
    mysql_cond_timedwait(&m_run_cond, &m_run_lock, &abstime);
  }
  ret |= m_transaction_monitor_thd_state.is_running();
  ret |= release_services();

  mysql_mutex_unlock(&m_run_lock);
  return ret;
}

int Transaction_monitor_thread::start() {
  DBUG_TRACE;
  int ret = 0;

  mysql_mutex_lock(&m_run_lock);

  if (m_transaction_monitor_thd_state.is_thread_alive()) goto end;
  DBUG_EXECUTE_IF(
      "group_replication_transaction_monitor_thread_creation_failed", {
        ret = 1;
        goto end;
      });

  if (acquire_services()) {
    /* purecov: begin inspected */
    ret = 1;
    goto end;
    /* purecov: end */
  }
  m_abort = false;

  if (mysql_thread_create(key_GR_THD_transaction_monitor, &m_handle,
                          get_connection_attrib(),
                          Transaction_monitor_thread::launch_thread,
                          static_cast<void *>(this))) {
    /* purecov: begin inspected */
    m_transaction_monitor_thd_state.set_terminated();
    ret = 1;
    goto end;
    /* purecov: end */
  }

  while (m_transaction_monitor_thd_state.is_alive_not_running()) {
    DBUG_PRINT("sleep",
               ("Waiting for the transaction monitor thread to start"));
    struct timespec abstime;
    set_timespec(&abstime, 1);
    mysql_cond_timedwait(&m_run_cond, &m_run_lock, &abstime);
  }

end:
  mysql_mutex_unlock(&m_run_lock);
  if (ret) {
    release_services();
  }
  return ret;
}

[[noreturn]] void Transaction_monitor_thread::transaction_thread_handle() {
  DBUG_TRACE;

  // Thread context operations
  THD *thd = new THD;
  my_thread_init();
  thd->set_new_thread_id();
  thd->thread_stack = reinterpret_cast<const char *>(&thd);
  thd->store_globals();
  global_thd_manager_add_thd(thd);

  // Timers and operation controller
  using SteadyClock = std::chrono::steady_clock;
  bool clients_disconnected = false;
  std::chrono::time_point<SteadyClock> transaction_timeout_time =
      m_time_start_of_operation + std::chrono::seconds(m_transaction_timeout);
  // time_now is kept to have consistent time at different places during run of
  // code, and cleaner look of code
  std::chrono::time_point<SteadyClock> time_now = SteadyClock::now();

#ifdef HAVE_PSI_THREAD_INTERFACE
  std::string status_info = "Group replication transaction monitor";

  PSI_THREAD_CALL(set_thread_info)(status_info.c_str(), status_info.length());
#endif /* HAVE_PSI_THREAD_INTERFACE */

  mysql_mutex_lock(&m_run_lock);
  m_transaction_monitor_thd_state.set_running();
  mysql_cond_broadcast(&m_run_cond);
  mysql_mutex_unlock(&m_run_lock);

  m_mysql_new_transaction_control->stop();

#ifdef HAVE_PSI_THREAD_INTERFACE
  status_info =
      "Group replication transaction monitor: Stopped new transactions";

  PSI_THREAD_CALL(set_thread_info)(status_info.c_str(), status_info.length());
#endif /* HAVE_PSI_THREAD_INTERFACE */

  // main_loop
  while (!thd->is_killed()) {
    mysql_mutex_lock(&m_run_lock);
    if (m_abort) {
      mysql_mutex_unlock(&m_run_lock);
      break;
    }
    time_now = SteadyClock::now();
    /**
      If time has elapsed disconnect the client connections running the
      transaction which have yet not reached commit.
      Else wait for 1 second periods for the primary change to happen until the
      specified timeout elapses.
      @note if UDF finishes it will call terminate to terminate this thread.
            terminate will awake this thread and thread will exit due to
            m_abort flag.
      @note ongoing transactions does not impact this thread.
            If ongoing transactions finishes UDF will finish execution at
            Primary_election_action::execute_action later calling the terminate
            function of this thread. Terminate will simply unblock the wait,
            allow the transactions and end the execution of this thread.
      @note before closing the client connection, time elapse is checked, so
            it safe to come out of sleep early.
      @note clients_disconnected makes sure client disconnection only happens
            once.
      @note post client disconnection this thread waits for UDF to finish so
            that new queries are still blocked. Once UDF finishes at time of
            after_primary_election terminate will be called. Terminate will
            simply unblock the wait, allow the transactions and end the
            execution of this thread.
    */
    if (clients_disconnected) {
      struct timespec abstime;
      set_timespec(&abstime, 1);
      mysql_cond_timedwait(&m_run_cond, &m_run_lock, &abstime);
    } else {
      long int time_pending = std::chrono::duration_cast<std::chrono::seconds>(
                                  transaction_timeout_time - time_now)
                                  .count();
      if (time_pending > 0) {
        struct timespec abstime;
        set_timespec(&abstime, 1);
        mysql_cond_timedwait(&m_run_cond, &m_run_lock, &abstime);
      }
    }
    mysql_mutex_unlock(&m_run_lock);
    /**
      1. Refresh time_now.
      2. Disconnect the clients only once.
      3. Check time has elapsed before disconnecting the clients
    */
    time_now = SteadyClock::now();
    if (!clients_disconnected && (time_now > transaction_timeout_time) &&
        !thd->is_killed()) {
      m_mysql_before_commit_transaction_control->stop();
      m_mysql_close_connection_of_binloggable_transaction_not_reached_commit
          ->close();
      clients_disconnected = true;
#ifdef HAVE_PSI_THREAD_INTERFACE
      status_info =
          "Group replication transaction monitor: Stopped client connections";
      PSI_THREAD_CALL(set_thread_info)
      (status_info.c_str(), status_info.length());
#endif /* HAVE_PSI_THREAD_INTERFACE */
    }
  }

  m_mysql_before_commit_transaction_control->allow();
  m_mysql_new_transaction_control->allow();

#ifdef HAVE_PSI_THREAD_INTERFACE
  status_info =
      "Group replication transaction monitor: Allowing new transactions";
  PSI_THREAD_CALL(set_thread_info)(status_info.c_str(), status_info.length());
#endif /* HAVE_PSI_THREAD_INTERFACE */

  DBUG_EXECUTE_IF("group_replication_transaction_monitor_end", {
    const char act[] =
        "now wait_for signal.group_replication_wait_on_transaction_monitor_end";
    assert(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
  });

  thd->release_resources();
  global_thd_manager_remove_thd(thd);
  delete thd;
  my_thread_end();
  mysql_mutex_lock(&m_run_lock);
  m_transaction_monitor_thd_state.set_terminated();
  mysql_cond_broadcast(&m_run_cond);  // Unblock terminate
  mysql_mutex_unlock(&m_run_lock);

  my_thread_exit(nullptr);
}
