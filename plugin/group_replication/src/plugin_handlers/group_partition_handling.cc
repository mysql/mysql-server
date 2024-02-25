/* Copyright (c) 2017, 2023, Oracle and/or its affiliates.

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

#include "plugin/group_replication/include/plugin_handlers/group_partition_handling.h"

#include <mysql/components/services/log_builtins.h>
#include <mysql/group_replication_priv.h>

#include "plugin/group_replication/include/autorejoin.h"
#include "plugin/group_replication/include/leave_group_on_failure.h"
#include "plugin/group_replication/include/plugin.h"
#include "plugin/group_replication/include/plugin_psi.h"
#include "plugin/group_replication/include/replication_threads_api.h"

using std::string;

static void *launch_handler_thread(void *arg) {
  Group_partition_handling *handler = (Group_partition_handling *)arg;
  handler->partition_thread_handler();
  return nullptr;
}

Group_partition_handling::Group_partition_handling(ulong unreachable_timeout)
    : member_in_partition(false),
      group_partition_thd_state(),
      partition_handling_aborted(false),
      partition_handling_terminated(false),
      timeout_on_unreachable(unreachable_timeout) {
  mysql_mutex_init(key_GR_LOCK_group_part_handler_run, &run_lock,
                   MY_MUTEX_INIT_FAST);
  mysql_mutex_init(key_GR_LOCK_group_part_handler_abort,
                   &trx_termination_aborted_lock, MY_MUTEX_INIT_FAST);

  mysql_cond_init(key_GR_COND_group_part_handler_run, &run_cond);
  mysql_cond_init(key_GR_COND_group_part_handler_abort,
                  &trx_termination_aborted_cond);
}

Group_partition_handling::~Group_partition_handling() {
  mysql_mutex_destroy(&run_lock);
  mysql_cond_destroy(&run_cond);
  mysql_mutex_destroy(&trx_termination_aborted_lock);
  mysql_cond_destroy(&trx_termination_aborted_cond);
}

void Group_partition_handling::update_timeout_on_unreachable(
    ulong unreachable_timeout) {
  timeout_on_unreachable = unreachable_timeout;
}

ulong Group_partition_handling::get_timeout_on_unreachable() {
  return timeout_on_unreachable;
}

bool Group_partition_handling::is_member_on_partition() {
  return member_in_partition;
}

bool Group_partition_handling::is_partition_handler_running() {
  return group_partition_thd_state.is_running();
}

bool Group_partition_handling::is_partition_handling_terminated() {
  return partition_handling_terminated;
}

bool Group_partition_handling::abort_partition_handler_if_running() {
  DBUG_TRACE;

  // if someone tried to cancel it, we are no longer in a partition.
  member_in_partition = false;

  /*
    This check is safe to invoke as the start method and abort method are only
    invoked in GCS serialized operations.
  */
  if (group_partition_thd_state.is_thread_alive())
    terminate_partition_handler_thread();

  return partition_handling_terminated;
}

int Group_partition_handling::launch_partition_handler_thread() {
  DBUG_TRACE;

  member_in_partition = true;

  // If the timeout is set to 0 do nothing
  if (!timeout_on_unreachable) return 0;

  mysql_mutex_lock(&run_lock);

  partition_handling_aborted = false;

  if (group_partition_thd_state.is_thread_alive()) {
    mysql_mutex_unlock(&run_lock); /* purecov: inspected */
    return 0;                      /* purecov: inspected */
  }

  if (mysql_thread_create(key_GR_THD_group_partition_handler,
                          &partition_trx_handler_pthd, get_connection_attrib(),
                          launch_handler_thread, (void *)this)) {
    return 1; /* purecov: inspected */
  }
  group_partition_thd_state.set_created();

  while (group_partition_thd_state.is_alive_not_running()) {
    DBUG_PRINT("sleep", ("Waiting for the partition handler thread to start"));
    mysql_cond_wait(&run_cond, &run_lock);
  }
  mysql_mutex_unlock(&run_lock);

  return 0;
}

int Group_partition_handling::terminate_partition_handler_thread() {
  DBUG_TRACE;

  mysql_mutex_lock(&run_lock);

  if (group_partition_thd_state.is_thread_dead()) {
    mysql_mutex_unlock(&run_lock);
    return 0;
  }

  mysql_mutex_lock(&trx_termination_aborted_lock);
  partition_handling_aborted = true;
  mysql_cond_broadcast(&trx_termination_aborted_cond);
  mysql_mutex_unlock(&trx_termination_aborted_lock);

  ulong stop_wait_timeout = TRANSACTION_KILL_TIMEOUT;

  while (group_partition_thd_state.is_thread_alive()) {
    DBUG_PRINT("loop", ("killing group replication partition handler thread"));

    struct timespec abstime;
    set_timespec(&abstime, (stop_wait_timeout == 1 ? 1 : 2));
#ifndef NDEBUG
    int error =
#endif
        mysql_cond_timedwait(&run_cond, &run_lock, &abstime);
    if (stop_wait_timeout >= 1) {
      stop_wait_timeout = stop_wait_timeout - (stop_wait_timeout == 1 ? 1 : 2);
    }
    /* purecov: begin inspected */
    if (group_partition_thd_state.is_thread_alive() &&
        stop_wait_timeout <= 0)  // quit waiting
    {
      mysql_mutex_unlock(&run_lock);
      return 1;
    }
    /* purecov: inspected */
    assert(error == ETIMEDOUT || error == 0);
  }

  assert(!group_partition_thd_state.is_running());

  mysql_mutex_unlock(&run_lock);

  return 0;
}

int Group_partition_handling::partition_thread_handler() {
  DBUG_TRACE;

  THD *ph_thd = new THD;
  my_thread_init();
  ph_thd->set_new_thread_id();
  ph_thd->thread_stack = reinterpret_cast<const char *>(&ph_thd);
  ph_thd->store_globals();
  global_thd_manager_add_thd(ph_thd);

  mysql_mutex_lock(&run_lock);
  group_partition_thd_state.set_running();
  mysql_cond_broadcast(&run_cond);
  mysql_mutex_unlock(&run_lock);

  bool timeout = false;

  longlong timeout_remaining_time = timeout_on_unreachable;

  mysql_mutex_lock(&trx_termination_aborted_lock);
  while (!timeout && !partition_handling_aborted) {
    struct timespec abstime;
    set_timespec(&abstime, (timeout_remaining_time == 1 ? 1 : 2));
    mysql_cond_timedwait(&trx_termination_aborted_cond,
                         &trx_termination_aborted_lock, &abstime);

    timeout_remaining_time -= (timeout_remaining_time == 1 ? 1 : 2);
    timeout = (timeout_remaining_time <= 0);
  }

  mysql_mutex_unlock(&trx_termination_aborted_lock);

  if (!partition_handling_aborted) {
    partition_handling_terminated = true;

    LogPluginErr(ERROR_LEVEL,
                 ER_GRP_RPL_UNREACHABLE_MAJORITY_TIMEOUT_FOR_MEMBER,
                 timeout_on_unreachable);

    const char *exit_state_action_abort_log_message =
        "This member could not reach a majority of the members.";
    leave_group_on_failure::mask leave_actions;
    leave_actions.set(leave_group_on_failure::STOP_APPLIER, true);
    leave_actions.set(leave_group_on_failure::HANDLE_EXIT_STATE_ACTION, true);
    leave_actions.set(leave_group_on_failure::HANDLE_AUTO_REJOIN, true);
    leave_group_on_failure::leave(leave_actions, 0, nullptr,
                                  exit_state_action_abort_log_message);
  }

  mysql_mutex_lock(&run_lock);
  ph_thd->release_resources();
  global_thd_manager_remove_thd(ph_thd);
  delete ph_thd;
  my_thread_end();
  group_partition_thd_state.set_terminated();
  mysql_cond_broadcast(&run_cond);
  mysql_mutex_unlock(&run_lock);

  my_thread_exit(nullptr);

  return 0;
}
