/* Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "plugin/group_replication/include/group_partition_handling.h"

#include <mysql/components/services/log_builtins.h>
#include <mysql/group_replication_priv.h>

#include "plugin/group_replication/include/plugin.h"
#include "plugin/group_replication/include/plugin_psi.h"

using std::string;

static void *launch_handler_thread(void *arg) {
  Group_partition_handling *handler = (Group_partition_handling *)arg;
  handler->partition_thread_handler();
  return 0;
}

Group_partition_handling::Group_partition_handling(
    Shared_writelock *shared_stop_lock, ulong unreachable_timeout)
    : member_in_partition(false),
      group_partition_thd_state(),
      partition_handling_aborted(false),
      partition_handling_terminated(false),
      timeout_on_unreachable(unreachable_timeout),
      shared_stop_write_lock(shared_stop_lock) {
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

void Group_partition_handling::kill_transactions_and_leave() {
  DBUG_ENTER("Group_partition_handling::kill_transactions_and_leave");

  Notification_context ctx;

  LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_UNREACHABLE_MAJORITY_TIMEOUT_FOR_MEMBER,
               timeout_on_unreachable);

  /*
    Suspend the applier for the uncommon case of a network restore happening
    when this termination process is ongoing.
    Don't care if an error is returned because the applier failed.
  */
  applier_module->add_suspension_packet();

  /* Notify member status update. */
  group_member_mgr->update_member_status(local_member_info->get_uuid(),
                                         Group_member_info::MEMBER_ERROR, ctx);

  /*
    unblock threads waiting for the member to become ONLINE
  */
  terminate_wait_on_start_process();

  /* Single state update. Notify right away. */
  notify_and_reset_ctx(ctx);

  bool set_read_mode = false;
  Gcs_operations::enum_leave_state state = gcs_module->leave();

  longlong errcode = 0;
  longlong log_severity = WARNING_LEVEL;
  switch (state) {
    case Gcs_operations::ERROR_WHEN_LEAVING:
      errcode = ER_GRP_RPL_FAILED_TO_CONFIRM_IF_SERVER_LEFT_GRP;
      log_severity = ERROR_LEVEL;
      set_read_mode = true;
      break;
    case Gcs_operations::ALREADY_LEAVING:
      errcode = ER_GRP_RPL_SERVER_IS_ALREADY_LEAVING; /* purecov: inspected */
      break;                                          /* purecov: inspected */
    case Gcs_operations::ALREADY_LEFT:
      errcode = ER_GRP_RPL_SERVER_ALREADY_LEFT; /* purecov: inspected */
      break;                                    /* purecov: inspected */
    case Gcs_operations::NOW_LEAVING:
      set_read_mode = true;
      errcode = ER_GRP_RPL_SERVER_SET_TO_READ_ONLY_DUE_TO_ERRORS;
      log_severity = ERROR_LEVEL;
      break;
  }
  LogPluginErr(log_severity, errcode);

  /*
    If true it means:
    1) The plugin is stopping and waiting on some transactions to finish.
       No harm in unblocking them first cutting the stop command time
    2) There was an error in the applier and the plugin will leave the group.
       No problem, both processes will try to kill the transactions and set the
       read mode to true.
  */
  bool already_locked = shared_stop_write_lock->try_grab_write_lock();

  // kill pending transactions
  blocked_transaction_handler->unblock_waiting_transactions();

  if (!already_locked) shared_stop_write_lock->release_write_lock();

  if (set_read_mode) enable_server_read_mode(PSESSION_INIT_THREAD);

  if (exit_state_action_var == EXIT_STATE_ACTION_ABORT_SERVER) {
    abort_plugin_process("Fatal error during execution of Group Replication");
  }

  DBUG_VOID_RETURN;
}

bool Group_partition_handling::abort_partition_handler_if_running() {
  DBUG_ENTER("Group_partition_handling::abort_partition_handler_if_running");

  // if someone tried to cancel it, we are no longer in a partition.
  member_in_partition = false;

  /*
    This check is safe to invoke as the start method and abort method are only
    invoked in GCS serialized operations.
  */
  if (group_partition_thd_state.is_thread_alive())
    terminate_partition_handler_thread();

  DBUG_RETURN(partition_handling_terminated);
}

int Group_partition_handling::launch_partition_handler_thread() {
  DBUG_ENTER("Group_partition_handling::launch_partition_handler_thread");

  member_in_partition = true;

  // If the timeout is set to 0 do nothing
  if (!timeout_on_unreachable) return 0;

  mysql_mutex_lock(&run_lock);

  partition_handling_aborted = false;

  if (group_partition_thd_state.is_thread_alive()) {
    mysql_mutex_unlock(&run_lock); /* purecov: inspected */
    DBUG_RETURN(0);                /* purecov: inspected */
  }

  if (mysql_thread_create(key_GR_THD_group_partition_handler,
                          &partition_trx_handler_pthd, get_connection_attrib(),
                          launch_handler_thread, (void *)this)) {
    DBUG_RETURN(1); /* purecov: inspected */
  }
  group_partition_thd_state.set_created();

  while (group_partition_thd_state.is_alive_not_running()) {
    DBUG_PRINT("sleep", ("Waiting for the partition handler thread to start"));
    mysql_cond_wait(&run_cond, &run_lock);
  }
  mysql_mutex_unlock(&run_lock);

  DBUG_RETURN(0);
}

int Group_partition_handling::terminate_partition_handler_thread() {
  DBUG_ENTER("Group_partition_handling::terminate_partition_handler_thread");

  mysql_mutex_lock(&run_lock);

  if (group_partition_thd_state.is_thread_dead()) {
    mysql_mutex_unlock(&run_lock);
    DBUG_RETURN(0);
  }

  mysql_mutex_lock(&trx_termination_aborted_lock);
  partition_handling_aborted = true;
  mysql_cond_broadcast(&trx_termination_aborted_cond);
  mysql_mutex_unlock(&trx_termination_aborted_lock);

  ulong stop_wait_timeout = TRANSACTION_KILL_TIMEOUT;

  while (group_partition_thd_state.is_thread_alive()) {
    DBUG_PRINT("loop", ("killing group replication partition handler thread"));

    struct timespec abstime;
    set_timespec(&abstime, 2);
#ifndef DBUG_OFF
    int error =
#endif
        mysql_cond_timedwait(&run_cond, &run_lock, &abstime);
    if (stop_wait_timeout >= 2) {
      stop_wait_timeout = stop_wait_timeout - 2;
    }
    /* purecov: begin inspected */
    else if (group_partition_thd_state.is_thread_alive())  // quit waiting
    {
      mysql_mutex_unlock(&run_lock);
      DBUG_RETURN(1);
    }
    /* purecov: inspected */
    DBUG_ASSERT(error == ETIMEDOUT || error == 0);
  }

  DBUG_ASSERT(!group_partition_thd_state.is_running());

  mysql_mutex_unlock(&run_lock);

  DBUG_RETURN(0);
}

int Group_partition_handling::partition_thread_handler() {
  DBUG_ENTER("Group_partition_handling::partition_thread_handler");

  mysql_mutex_lock(&run_lock);
  group_partition_thd_state.set_running();
  mysql_cond_broadcast(&run_cond);
  mysql_mutex_unlock(&run_lock);

  struct timespec abstime;
  bool timeout = false;

  longlong timeout_remaining_time = timeout_on_unreachable;

  mysql_mutex_lock(&trx_termination_aborted_lock);
  while (!timeout && !partition_handling_aborted) {
    set_timespec(&abstime, 2);
    mysql_cond_timedwait(&trx_termination_aborted_cond,
                         &trx_termination_aborted_lock, &abstime);

    timeout_remaining_time -= 2;
    timeout = (timeout_remaining_time <= 0);
  }

  mysql_mutex_unlock(&trx_termination_aborted_lock);

  if (!partition_handling_aborted) {
    partition_handling_terminated = true;
    kill_transactions_and_leave();
  }

  mysql_mutex_lock(&run_lock);
  group_partition_thd_state.set_terminated();
  mysql_cond_broadcast(&run_cond);
  mysql_mutex_unlock(&run_lock);

  DBUG_RETURN(0);
}
