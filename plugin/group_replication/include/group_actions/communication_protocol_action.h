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

#ifndef COMMUNICATION_PROTOCOL_ACTION_INCLUDED
#define COMMUNICATION_PROTOCOL_ACTION_INCLUDED

#include <future>
#include "plugin/group_replication/include/group_actions/group_action.h"
#include "plugin/group_replication/include/mysql_version_gcs_protocol_map.h"
#include "plugin/group_replication/include/plugin_handlers/stage_monitor_handler.h"
#include "plugin/group_replication/include/plugin_messages/group_action_message.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_communication_interface.h"

/**
  Action that changes the GCS protocol version in use by the group.

  Note that this action's logic is actually done in process_action_message
  instead of execute_action. This is because process_action_message is executed
  in the same logical instant on all group members (in the GCS thread) while
  execute_action is executed nondeterministically on its own thread.
*/
class Communication_protocol_action final : public Group_action {
 public:
  Communication_protocol_action();
  explicit Communication_protocol_action(Gcs_protocol_version gcs_protocol);
  ~Communication_protocol_action() final;

  // Group_action implementation
  void get_action_message(Group_action_message **message) final;
  int process_action_message(Group_action_message &message,
                             const std::string &message_origin) final;
  enum_action_execution_result execute_action(
      bool invoking_member, Plugin_stage_monitor_handler *stage_handler,
      Notification_context *) final;
  bool stop_action_execution(bool killed) final;
  Group_action_diagnostics *get_execution_info() final;

 private:
  int set_consensus_leaders() const;

  /** Stores textual information about the action's execution. */
  Group_action_diagnostics m_diagnostics;

  /** The GCS protocol to change to. */
  Gcs_protocol_version m_gcs_protocol;

  /**
   * Future on which to wait for the protocol change to finish in the local
   * node.
   */
  std::future<void> m_protocol_change_done;
};

#endif /* COMMUNICATION_PROTOCOL_ACTION_INCLUDED */
