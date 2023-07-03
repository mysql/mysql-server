/* Copyright (c) 2019, 2022, Oracle and/or its affiliates.

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

#include <vector>

#include "plugin/group_replication/include/leave_group_on_failure.h"

#include "mysql/components/services/log_builtins.h"
#include "plugin/group_replication/include/autorejoin.h"
#include "plugin/group_replication/include/gcs_operations.h"
#include "plugin/group_replication/include/gcs_view_modification_notifier.h"
#include "plugin/group_replication/include/plugin.h"
#include "plugin/group_replication/include/plugin_handlers/offline_mode_handler.h"

void leave_group_on_failure::leave(
    const leave_group_on_failure::mask &actions, longlong error_to_log,
    Notification_context *caller_notification_context,
    const char *exit_state_action_abort_log_message) {
  DBUG_TRACE;

  Gcs_operations::enum_leave_state leave_state = Gcs_operations::ALREADY_LEFT;
  Plugin_gcs_view_modification_notifier view_change_notifier;

  Notification_context *notification_context = nullptr;
  bool clean_notification_context = false;
  bool notify_member_update = true;
  if (caller_notification_context != nullptr) {
    notification_context = caller_notification_context;
    notify_member_update = false;
  } else {
    notification_context = new Notification_context();
    clean_notification_context = true;
  }

  const bool start_auto_rejoin =
      actions[leave_group_on_failure::HANDLE_AUTO_REJOIN] &&
      is_autorejoin_enabled();

  if (error_to_log > 0) {
    LogPluginErr(ERROR_LEVEL, error_to_log);
  }

  /*
    Delete all members from group info except the local one.
  */
  if (actions[leave_group_on_failure::CLEAN_GROUP_MEMBERSHIP]) {
    Group_member_info_list to_update(
        (Malloc_allocator<Group_member_info *>(key_group_member_info)));
    group_member_mgr->update(&to_update);
  }

  group_member_mgr->update_member_status(local_member_info->get_uuid(),
                                         Group_member_info::MEMBER_ERROR,
                                         *notification_context);
  group_member_mgr->update_member_role(local_member_info->get_uuid(),
                                       Group_member_info::MEMBER_ROLE_SECONDARY,
                                       *notification_context);

  /*
    unblock threads waiting for the member to become ONLINE
  */
  terminate_wait_on_start_process();

  /*
    Single state update. Notify right away.
  */
  if (notify_member_update) {
    notify_and_reset_ctx(*notification_context);
  }
  if (clean_notification_context) {
    delete notification_context;
    notification_context = nullptr;
  }

  if (!actions[leave_group_on_failure::ALREADY_LEFT_GROUP]) {
    if (!actions[leave_group_on_failure::SKIP_LEAVE_VIEW_WAIT]) {
      view_change_notifier.start_view_modification();
    }

    leave_state = gcs_module->leave(&view_change_notifier);
  }

  Replication_thread_api::rpl_channel_stop_all(
      CHANNEL_APPLIER_THREAD | CHANNEL_RECEIVER_THREAD,
      get_components_stop_timeout_var());
  Replication_thread_api::rpl_binlog_dump_thread_kill();

  if (!actions[leave_group_on_failure::ALREADY_LEFT_GROUP]) {
    longlong errcode = 0;
    enum loglevel log_severity = WARNING_LEVEL;
    switch (leave_state) {
      case Gcs_operations::ERROR_WHEN_LEAVING:
        errcode = ER_GRP_RPL_FAILED_TO_CONFIRM_IF_SERVER_LEFT_GRP;
        log_severity = ERROR_LEVEL;
        break;
      case Gcs_operations::ALREADY_LEAVING:
        errcode = ER_GRP_RPL_SERVER_IS_ALREADY_LEAVING; /* purecov: inspected */
        break;                                          /* purecov: inspected */
      case Gcs_operations::ALREADY_LEFT:
        errcode = ER_GRP_RPL_SERVER_ALREADY_LEFT; /* purecov: inspected */
        break;                                    /* purecov: inspected */
      case Gcs_operations::NOW_LEAVING:
        break;
    }
    if (errcode > 0) {
      LogPluginErr(log_severity, errcode);
    }
  }

  /*
    Suspend the applier for the uncommon case of a network restore happening
    when this termination process is ongoing.
  */
  if (actions[leave_group_on_failure::STOP_APPLIER]) {
    bool aborted = false;
    DBUG_EXECUTE_IF(
        "group_replication_wait_for_current_events_execution_fail", {
          /*
           Inform the test to execute SHUTDOWN and block for shutdown to send
           kill For blocking sleep has been used, hopefully in 10 seconds
           shutdown will send kill to the thread
           */
          const char act[] =
              "now signal "
              "signal.group_replication_wait_for_current_events_execution_fail_"
              "applier_add_suspension_packet";
          assert(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
          my_sleep(10 * 1000 * 1000);
        };);
    applier_module->add_suspension_packet();
    applier_module->wait_for_applier_complete_suspension(&aborted, false);
  }

  /*
    If true it means:
    1) The plugin is stopping and waiting on some transactions to finish.
       No harm in unblocking them first cutting the stop command time
    2) There was an error in the applier and the plugin will leave the group.
       No problem, both processes will try to kill the transactions and set the
       read mode to true.
  */
  bool already_locked = shared_plugin_stop_lock->try_grab_write_lock();

  /*
    Kill pending transactions.
  */
  blocked_transaction_handler->unblock_waiting_transactions();

  DBUG_EXECUTE_IF(
      "group_replication_applier_thread_wait_kill_pending_transaction", {
        const char act[] = "now wait_for signal.gr_applier_early_failure";
        assert(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
      });

  if (!already_locked) shared_plugin_stop_lock->release_write_lock();

  if (!actions[leave_group_on_failure::SKIP_SET_READ_ONLY]) {
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_SERVER_SET_TO_READ_ONLY_DUE_TO_ERRORS);
    enable_server_read_mode();
  }

  /*
    We will only consider group_replication_exit_state_action if the
    auto-rejoin process is not enabled. If it is enabled, GR will first
    attempt to auto-rejoin.
  */
  if (actions[leave_group_on_failure::HANDLE_EXIT_STATE_ACTION] &&
      !start_auto_rejoin) {
    if (get_exit_state_action_var() == EXIT_STATE_ACTION_OFFLINE_MODE) {
      enable_server_offline_mode();
    }
  }

  if (!actions[leave_group_on_failure::ALREADY_LEFT_GROUP]) {
    if (!actions[leave_group_on_failure::SKIP_LEAVE_VIEW_WAIT]) {
      if (Gcs_operations::ERROR_WHEN_LEAVING != leave_state &&
          Gcs_operations::ALREADY_LEFT != leave_state) {
        LogPluginErr(INFORMATION_LEVEL, ER_GRP_RPL_WAITING_FOR_VIEW_UPDATE);
        if (view_change_notifier.wait_for_view_modification()) {
          LogPluginErr(WARNING_LEVEL,
                       ER_GRP_RPL_TIMEOUT_RECEIVING_VIEW_CHANGE_ON_SHUTDOWN);
        }
      }
    }

    gcs_module->remove_view_notifer(&view_change_notifier);
  }

  /*
    We will only consider group_replication_exit_state_action if the
    auto-rejoin process is not enabled. If it is enabled, GR will first
    attempt to auto-rejoin.
  */
  if (actions[leave_group_on_failure::HANDLE_EXIT_STATE_ACTION] &&
      !start_auto_rejoin) {
    if (get_exit_state_action_var() == EXIT_STATE_ACTION_ABORT_SERVER) {
      abort_plugin_process(exit_state_action_abort_log_message);
    }
  }

  /*
    If we have the auto-rejoin process enabled, now is the time to run it!
  */
  if (start_auto_rejoin) {
    autorejoin_module->start_autorejoin(get_number_of_autorejoin_tries(),
                                        get_rejoin_timeout());
  }
}
