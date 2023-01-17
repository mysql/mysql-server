/* Copyright (c) 2018, 2023, Oracle and/or its affiliates.

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

#include <mysql/components/services/log_builtins.h>

#include "plugin/group_replication/include/autorejoin.h"
#include "plugin/group_replication/include/plugin.h"
#include "plugin/group_replication/include/plugin_handlers/offline_mode_handler.h"
#include "plugin/group_replication/include/plugin_handlers/read_mode_handler.h"
#include "plugin/group_replication/include/plugin_handlers/stage_monitor_handler.h"

void *Autorejoin_thread::launch_thread(void *arg) {
  Autorejoin_thread *thd = static_cast<Autorejoin_thread *>(arg);
  thd->autorejoin_thread_handle();  // Does not return.
}

Autorejoin_thread::Autorejoin_thread()
    : m_thd(nullptr),
      m_abort(false),
      m_being_terminated(false),
      m_attempts(0UL),
      m_rejoin_timeout(0ULL) {
  mysql_mutex_init(key_GR_LOCK_autorejoin_module, &m_run_lock,
                   MY_MUTEX_INIT_FAST);
  mysql_cond_init(key_GR_COND_autorejoin_module, &m_run_cond);
}

Autorejoin_thread::~Autorejoin_thread() {
  mysql_mutex_destroy(&m_run_lock);
  mysql_cond_destroy(&m_run_cond);
}

void Autorejoin_thread::init() {
  mysql_mutex_lock(&m_run_lock);
  m_being_terminated = false;
  mysql_mutex_unlock(&m_run_lock);
}

bool Autorejoin_thread::abort_rejoin() {
  bool ret = false;
  mysql_mutex_lock(&m_run_lock);

  /*
    We return a flag indicating whether or not the auto-rejoin was ongoing at
    the time of the abort, so that the user can have a thread-safe way of
    knowing if the auto-rejoin was ongoing at this point in time.

    Since the only way to know if the auto-rejoin is ongoing is via
    Autorejoin_thread::is_autorejoin_ongoing(), we needed a critical section
    around a call to that method and abort(). To avoid exposing
    locking/unlocking methods, we simply return the state in which the abort
    was when it was called, since abort itself is already inside a critical
    section.
  */
  ret = m_autorejoin_thd_state.is_running();

  // Update the abort flag.
  m_abort = true;

  // Update the termination flag.
  m_being_terminated = true;

  /*
    Unblock the call to mysql_cond_wait() on the auto-rejoin loop,
    effectively ending the auto-rejoin loop.
  */
  while (m_autorejoin_thd_state.is_thread_alive()) {
    mysql_mutex_lock(&m_thd->LOCK_thd_data);

    mysql_cond_broadcast(&m_run_cond);

    m_thd->awake(THD::NOT_KILLED);
    mysql_mutex_unlock(&m_thd->LOCK_thd_data);

    struct timespec abstime;
    set_timespec(&abstime, 1);
    mysql_cond_timedwait(&m_run_cond, &m_run_lock, &abstime);
  }

  mysql_mutex_unlock(&m_run_lock);
  return ret;
}

int Autorejoin_thread::start_autorejoin(uint attempts, ulonglong timeout) {
  DBUG_TRACE;
  int ret = 0;

  mysql_mutex_lock(&m_run_lock);

  /*
    Do nothing if the thread is already running, i.e. if someone calls init()
    twice or more on the same thread.

    Also skip the scenario where the auto-rejoin module is already being
    terminated.
  */
  if (m_autorejoin_thd_state.is_thread_alive() || m_being_terminated) goto end;

  // Update the number of attempts for this auto-rejoin.
  m_attempts = attempts;

  // Update the rejoin timeout.
  m_rejoin_timeout = timeout;

  // Reset the abort flag.
  m_abort = false;

  /*
    Attempt to create a mysql instrumented thread. Return an error if not
    possible.
  */
  if (mysql_thread_create(
          key_GR_THD_autorejoin, &m_handle, get_connection_attrib(),
          Autorejoin_thread::launch_thread, static_cast<void *>(this))) {
    m_autorejoin_thd_state.set_terminated();
    ret = 1;
    goto end;
  }

  /*
    Wait until the thread actually starts.
  */
  while (m_autorejoin_thd_state.is_alive_not_running()) {
    DBUG_PRINT("sleep", ("Waiting for the auto-rejoin thread to start"));
    mysql_cond_wait(&m_run_cond, &m_run_lock);
  }

end:
  mysql_mutex_unlock(&m_run_lock);
  return ret;
}

bool Autorejoin_thread::is_autorejoin_ongoing() {
  mysql_mutex_lock(&m_run_lock);
  bool ret = m_autorejoin_thd_state.is_running();
  mysql_mutex_unlock(&m_run_lock);
  return ret;
}

void Autorejoin_thread::execute_rejoin_process() {
  int error = 1;
  struct timespec tm;
  Plugin_stage_monitor_handler stage_handler;
  if (stage_handler.initialize_stage_monitor())
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_NO_STAGE_SERVICE);

  stage_handler.set_stage(info_GR_STAGE_autorejoin.m_key, __FILE__, __LINE__, 0,
                          0);
  ulong num_attempts = 0UL;
  DBUG_EXECUTE_IF("group_replication_stop_before_rejoin_loop", {
    const char act[] =
        "now signal signal.autorejoin_entering_loop wait_for "
        "signal.autorejoin_enter_loop";
    assert(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
  });

  while (!m_abort && num_attempts++ < m_attempts) {
    // Update the number of attempts in pfs.
    stage_handler.set_completed_work(num_attempts);

    LogPluginErr(SYSTEM_LEVEL, ER_GRP_RPL_STARTED_AUTO_REJOIN, num_attempts,
                 m_attempts);

    DBUG_EXECUTE_IF("group_replication_stop_before_rejoin", {
      const char act[] =
          "now signal signal.autorejoin_waiting wait_for "
          "signal.autorejoin_continue";
      assert(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
    });

    // Attempt a single rejoin.
    if (!attempt_rejoin()) {
      error = 0;
      break;
    }

    /*
      Wait on m_run_cond up to 5 minutes. This is a simple way to allow the
      thread to be interrupted and/or canceled at will.

      Also, we only wait if we're not already in our last try.
    */
    if (num_attempts < m_attempts) {
      set_timespec(&tm, m_rejoin_timeout);
      mysql_mutex_lock(&m_run_lock);
      mysql_cond_timedwait(&m_run_cond, &m_run_lock, &tm);
      mysql_mutex_unlock(&m_run_lock);
    }
  }
  // Terminate the thread stage events infrastructure.
  stage_handler.end_stage();
  stage_handler.terminate_stage_monitor();

  /*
    If we didn't manage to rejoin, consider
    group_replication_exit_state_action.
  */
  if (error) {
    LogPluginErr(SYSTEM_LEVEL, ER_GRP_RPL_FINISHED_AUTO_REJOIN,
                 num_attempts - 1UL, m_attempts, " not");

    enable_server_read_mode();
    /*
      Only abort() if the auto-rejoin thread wasn't explicitly stopped, i.e.
      if someone called Autorejoin_thread::abort(), because that implies an
      explicit stop and thus we probably don't want to abort right here.
    */
    if (!m_abort) {
      switch (get_exit_state_action_var()) {
        case EXIT_STATE_ACTION_ABORT_SERVER: {
          std::stringstream ss;
          ss << "Could not rejoin the member to the group after " << m_attempts
             << " attempts";
          std::string msg = ss.str();
          abort_plugin_process(msg.c_str());
          break;
        }
        case EXIT_STATE_ACTION_OFFLINE_MODE:
          enable_server_offline_mode();
          break;
      }
    }
  } else {
    LogPluginErr(SYSTEM_LEVEL, ER_GRP_RPL_FINISHED_AUTO_REJOIN, num_attempts,
                 m_attempts, "");
  }
}

[[noreturn]] void Autorejoin_thread::autorejoin_thread_handle() {
  // Initialize the MySQL thread infrastructure.
  THD *thd = new THD;
  my_thread_init();
  thd->set_new_thread_id();
  thd->thread_stack = reinterpret_cast<const char *>(&thd);
  thd->store_globals();
  global_thd_manager_add_thd(thd);
  m_thd = thd;

  // Update the thread state and toggle the auto-rejoin ongoing flag.
  mysql_mutex_lock(&m_run_lock);
  m_autorejoin_thd_state.set_running();
  mysql_cond_broadcast(&m_run_cond);
  mysql_mutex_unlock(&m_run_lock);

  // Go through with the auto-rejoin itself.
  execute_rejoin_process();

  /*
    After an auto-rejoin, whether successful or not, teardown the MySQL
    infrastructure.
  */
  mysql_mutex_lock(&m_run_lock);
  m_thd->release_resources();
  global_thd_manager_remove_thd(m_thd);
  delete m_thd;
  m_thd = nullptr;
  my_thread_end();
  m_autorejoin_thd_state.set_terminated();
  mysql_cond_broadcast(&m_run_cond);
  mysql_mutex_unlock(&m_run_lock);

  my_thread_exit(nullptr);
}
