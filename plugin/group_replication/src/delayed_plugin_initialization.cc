/* Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "plugin/group_replication/include/delayed_plugin_initialization.h"

#include <mysql/components/services/log_builtins.h>
#include <mysql/group_replication_priv.h>
#include <stddef.h>

#include "mutex_lock.h"
#include "my_dbug.h"
#include "plugin/group_replication/include/plugin.h"
#include "plugin/group_replication/include/plugin_psi.h"

using std::string;

static void *launch_handler_thread(void *arg) {
  Delayed_initialization_thread *handler = (Delayed_initialization_thread *)arg;
  handler->initialization_thread_handler();
  return 0;
}

Delayed_initialization_thread::Delayed_initialization_thread()
    : delayed_thd_state(),
      is_server_ready(false),
      is_super_read_only_set(false) {
  mysql_mutex_init(key_GR_LOCK_delayed_init_run, &run_lock, MY_MUTEX_INIT_FAST);
  mysql_mutex_init(key_GR_LOCK_delayed_init_server_ready, &server_ready_lock,
                   MY_MUTEX_INIT_FAST);
  mysql_cond_init(key_GR_COND_delayed_init_run, &run_cond);
  mysql_cond_init(key_GR_COND_delayed_init_server_ready, &server_ready_cond);
}

Delayed_initialization_thread::~Delayed_initialization_thread() {
  mysql_mutex_destroy(&run_lock);
  mysql_cond_destroy(&run_cond);
  mysql_mutex_destroy(&server_ready_lock);
  mysql_cond_destroy(&server_ready_cond);
}

void Delayed_initialization_thread::signal_thread_ready() {
  mysql_mutex_lock(&server_ready_lock);
  is_server_ready = true;
  mysql_cond_broadcast(&server_ready_cond);
  mysql_mutex_unlock(&server_ready_lock);
}

void Delayed_initialization_thread::wait_for_thread_end() {
  mysql_mutex_lock(&run_lock);
  while (delayed_thd_state.is_thread_alive()) {
    DBUG_PRINT("sleep",
               ("Waiting for the Delayed initialization thread to finish"));
    mysql_cond_wait(&run_cond, &run_lock);
  }
  mysql_mutex_unlock(&run_lock);

  // give extra time for the thread to terminate
  my_sleep(1);
}

void Delayed_initialization_thread::signal_read_mode_ready() {
  mysql_mutex_lock(&run_lock);
  is_super_read_only_set = true;
  mysql_cond_broadcast(&run_cond);
  mysql_mutex_unlock(&run_lock);
}

void Delayed_initialization_thread::wait_for_read_mode() {
  mysql_mutex_lock(&run_lock);
  while (!is_super_read_only_set) {
    DBUG_PRINT("sleep", ("Waiting for the Delayed initialization thread to set "
                         "super_read_only"));
    mysql_cond_wait(&run_cond, &run_lock);
  }
  mysql_mutex_unlock(&run_lock);
}

int Delayed_initialization_thread::launch_initialization_thread() {
  mysql_mutex_lock(&run_lock);

  if (delayed_thd_state.is_thread_alive()) {
    mysql_mutex_unlock(&run_lock); /* purecov: inspected */
    return 0;                      /* purecov: inspected */
  }

  if (mysql_thread_create(key_GR_THD_delayed_init, &delayed_init_pthd,
                          get_connection_attrib(), launch_handler_thread,
                          (void *)this)) {
    mysql_mutex_unlock(&run_lock); /* purecov: inspected */
    return 1;                      /* purecov: inspected */
  }

  while (delayed_thd_state.is_alive_not_running()) {
    DBUG_PRINT("sleep",
               ("Waiting for the Delayed initialization thread to start"));
    mysql_cond_wait(&run_cond, &run_lock);
  }
  mysql_mutex_unlock(&run_lock);

  return 0;
}

int Delayed_initialization_thread::initialization_thread_handler() {
  int error = 0;

  THD *thd = NULL;
  thd = new THD;
  my_thread_init();
  thd->set_new_thread_id();
  thd->thread_stack = (char *)&thd;
  thd->store_globals();

  mysql_mutex_lock(&run_lock);
  delayed_thd_state.set_running();
  mysql_cond_broadcast(&run_cond);
  mysql_mutex_unlock(&run_lock);

  mysql_mutex_lock(&server_ready_lock);
  while (!is_server_ready) {
    DBUG_PRINT("sleep", ("Waiting for server start signal"));
    mysql_cond_wait(&server_ready_cond, &server_ready_lock);
  }
  mysql_mutex_unlock(&server_ready_lock);

  if (server_engine_initialized()) {
    // Protect this delayed start against other start/stop requests
    MUTEX_LOCK(lock, get_plugin_running_lock());

    plugin_is_setting_read_mode = true;

    error = initialize_plugin_and_join(PSESSION_INIT_THREAD, this);
  } else {
    error = 1;
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_START_GRP_RPL_FAILED);
  }

  mysql_mutex_lock(&run_lock);
  delayed_thd_state.set_terminated();
  mysql_cond_broadcast(&run_cond);
  mysql_mutex_unlock(&run_lock);

  delete thd;

  return error;
}
