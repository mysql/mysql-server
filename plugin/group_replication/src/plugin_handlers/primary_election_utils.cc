/* Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "plugin/group_replication/include/plugin_handlers/primary_election_utils.h"

Election_member_info::Election_member_info(const std::string uuid,
                                           const Member_version &version,
                                           bool is_primary)
    : member_uuid(uuid),
      member_version(version),
      is_member_primary(is_primary),
      has_running_channels(false),
      has_member_left(false),
      info_is_set(false) {}

std::string &Election_member_info::get_uuid() { return member_uuid; }

Member_version &Election_member_info::get_member_version() {
  return member_version;
}

bool Election_member_info::is_primary() { return is_member_primary; }

bool Election_member_info::member_left() { return has_member_left; }

bool Election_member_info::has_channels() { return has_running_channels; }

bool Election_member_info::is_information_set() { return info_is_set; }

void Election_member_info::set_has_running_channels(bool running_channels) {
  has_running_channels = running_channels;
}

void Election_member_info::set_information_set(bool set) { info_is_set = set; }

void Election_member_info::set_member_left(bool left) {
  has_member_left = left;
}

bool send_message(Plugin_gcs_message *message) {
  enum_gcs_error msg_error = gcs_module->send_message(*message);
  if (msg_error != GCS_OK) {
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_ERROR_ON_MESSAGE_SENDING,
                 "primary election process."); /* purecov: inspected */
    return true;                               /* purecov: inspected */
  }
  return false;
}

void kill_transactions_and_leave_on_election_error(std::string &err_msg) {
  DBUG_ENTER("kill_transactions_and_leave_on_election_error");

  // Action errors might have expelled the member already
  if (Group_member_info::MEMBER_ERROR ==
      local_member_info->get_recovery_status()) {
    return;
  }

  Notification_context ctx;

  LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_PRIMARY_ELECTION_PROCESS_ERROR,
               err_msg.c_str());

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
  if (view_change_notifier != NULL &&
      !view_change_notifier->is_view_modification_ongoing()) {
    view_change_notifier->start_view_modification();
  }
  Gcs_operations::enum_leave_state state = gcs_module->leave();

  longlong errcode = 0;
  longlong log_severity = WARNING_LEVEL;
  switch (state) {
    case Gcs_operations::ERROR_WHEN_LEAVING:
      errcode = ER_GRP_RPL_FAILED_TO_CONFIRM_IF_SERVER_LEFT_GRP; /* purecov:
                                                                    inspected */
      log_severity = ERROR_LEVEL; /* purecov: inspected */
      set_read_mode = true;       /* purecov: inspected */
      break;                      /* purecov: inspected */
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
  bool already_locked = shared_plugin_stop_lock->try_grab_write_lock();

  // kill pending transactions
  blocked_transaction_handler->unblock_waiting_transactions();

  if (!already_locked) shared_plugin_stop_lock->release_write_lock();

  if (set_read_mode) enable_server_read_mode(PSESSION_INIT_THREAD);

  if (view_change_notifier != NULL) {
    LogPluginErr(INFORMATION_LEVEL, ER_GRP_RPL_WAITING_FOR_VIEW_UPDATE);
    if (view_change_notifier->wait_for_view_modification()) {
      LogPluginErr(
          WARNING_LEVEL,
          ER_GRP_RPL_TIMEOUT_RECEIVING_VIEW_CHANGE_ON_SHUTDOWN); /* purecov:
                                                                    inspected */
    }
  }

  if (exit_state_action_var == EXIT_STATE_ACTION_ABORT_SERVER) {
    std::string error_message(
        "Fatal error during the primary election process: ");
    error_message.append(err_msg);
    abort_plugin_process(error_message.c_str());
  }

  DBUG_VOID_RETURN;
}