/* Copyright (c) 2018, 2022, Oracle and/or its affiliates.

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

#include "plugin/group_replication/include/leave_group_on_failure.h"
#include "plugin/group_replication/include/plugin.h"
#include "plugin/group_replication/include/replication_threads_api.h"

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
  DBUG_TRACE;

  // Action errors might have expelled the member already
  if (Group_member_info::MEMBER_ERROR ==
      local_member_info->get_recovery_status()) {
    return;
  }

  LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_PRIMARY_ELECTION_PROCESS_ERROR,
               err_msg.c_str());

  std::string exit_state_action_abort_log_message(
      "Fatal error during the primary election process: ");
  exit_state_action_abort_log_message.append(err_msg);
  leave_group_on_failure::mask leave_actions;
  leave_actions.set(leave_group_on_failure::STOP_APPLIER, true);
  leave_actions.set(leave_group_on_failure::HANDLE_EXIT_STATE_ACTION, true);
  leave_group_on_failure::leave(leave_actions, 0, nullptr,
                                exit_state_action_abort_log_message.c_str());
}
