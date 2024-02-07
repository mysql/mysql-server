/* Copyright (c) 2019, 2024, Oracle and/or its affiliates.

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

#ifndef LEAVE_GROUP_ON_FAILURE_INCLUDED
#define LEAVE_GROUP_ON_FAILURE_INCLUDED

#include <bitset>

#include "my_inttypes.h"
#include "plugin/group_replication/include/services/notification/notification.h"

/**
  Structure that holds the actions taken by the plugin when the
  member leaves the group after a failure.
*/
struct leave_group_on_failure {
  /**
    @enum enum_actions
    @brief Actions taken by the plugin when the member leaves the
    group after a failure.
  */
  enum enum_actions {
    ALREADY_LEFT_GROUP = 0,
    SKIP_SET_READ_ONLY,
    SKIP_LEAVE_VIEW_WAIT,
    CLEAN_GROUP_MEMBERSHIP,
    STOP_APPLIER,
    HANDLE_EXIT_STATE_ACTION,
    HANDLE_AUTO_REJOIN,
    ACTION_MAX
  };
  using mask = std::bitset<ACTION_MAX>;

  /**
    Do the instructed actions after a failure.

    @param[in]  actions
                  Actions performed.
    @param[in]  error_to_log
                  Error logged into error log.
    @param[in]  caller_notification_context
                  If defined the member state change notification
                  will update this notification context and the
                  notification signal responsibility belongs to the
                  caller.
                  If not defined (nullptr) a notification will be
                  sent by this function.
    @param[in]  exit_state_action_abort_log_message
                  The log message used on abort_plugin_process() if
                  that function is called.
  */
  static void leave(const mask &actions, longlong error_to_log,
                    Notification_context *caller_notification_context,
                    const char *exit_state_action_abort_log_message);
};

#endif /* LEAVE_GROUP_ON_FAILURE_INCLUDED */
