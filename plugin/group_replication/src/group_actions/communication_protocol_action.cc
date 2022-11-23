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

#include "plugin/group_replication/include/group_actions/communication_protocol_action.h"
#include <tuple>
#include "plugin/group_replication/include/member_info.h"
#include "plugin/group_replication/include/member_version.h"
#include "plugin/group_replication/include/mysql_version_gcs_protocol_map.h"
#include "plugin/group_replication/include/plugin.h"
#include "plugin/group_replication/include/plugin_handlers/consensus_leaders_handler.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_types.h"

Communication_protocol_action::Communication_protocol_action()
    : m_diagnostics(),
      m_gcs_protocol(Gcs_protocol_version::UNKNOWN),
      m_protocol_change_done() {}

Communication_protocol_action::Communication_protocol_action(
    Gcs_protocol_version gcs_protocol)
    : m_diagnostics(), m_gcs_protocol(gcs_protocol), m_protocol_change_done() {}

Communication_protocol_action::~Communication_protocol_action() = default;

// Group_action implementation
void Communication_protocol_action::get_action_message(
    Group_action_message **message) {
  assert(m_gcs_protocol != Gcs_protocol_version::UNKNOWN);
  *message = new Group_action_message(m_gcs_protocol);
}

int Communication_protocol_action::set_consensus_leaders() const {
  bool is_single_primary_mode = local_member_info->in_primary_mode();
  auto my_role = Group_member_info::MEMBER_ROLE_PRIMARY;
  Gcs_member_identifier const my_gcs_id =
      local_member_info->get_gcs_member_id();
  if (is_single_primary_mode) {
    Group_member_info *primary_info =
        group_member_mgr->get_primary_member_info();
    if (primary_info == nullptr) {
      return 1;
    }

    Gcs_member_identifier const primary_gcs_id =
        primary_info->get_gcs_member_id();
    delete primary_info;
    bool const am_i_the_primary = (my_gcs_id == primary_gcs_id);
    my_role = (am_i_the_primary ? Group_member_info::MEMBER_ROLE_PRIMARY
                                : Group_member_info::MEMBER_ROLE_SECONDARY);
  }

  Member_version const communication_protocol =
      convert_to_mysql_version(m_gcs_protocol);
  consensus_leaders_handler->set_consensus_leaders(
      communication_protocol, is_single_primary_mode, my_role, my_gcs_id,
      []() { return local_member_info->get_allow_single_leader(); });

  return 0;
}

int Communication_protocol_action::process_action_message(
    Group_action_message &message, const std::string &) {
  assert(m_gcs_protocol == Gcs_protocol_version::UNKNOWN ||
         m_gcs_protocol == message.get_gcs_protocol());
  assert(!m_protocol_change_done.valid());

  int result = 1;

  m_gcs_protocol = message.get_gcs_protocol();

  /* Start the protocol change. */
  bool will_change_protocol = false;
  std::tie(will_change_protocol, m_protocol_change_done) =
      gcs_module->set_protocol_version(m_gcs_protocol);

  /* Check if the protocol will be changed. */
  if (will_change_protocol) {
    result = set_consensus_leaders();
  }

  /* Inform action caller of error. */
  if (result == 1) {
    std::string error_message;
    auto const max_supported_protocol =
        gcs_module->get_maximum_protocol_version();
    Member_version const &max_supported_version =
        convert_to_mysql_version(max_supported_protocol);
    error_message =
        "Aborting the communication protocol change because some older "
        "members "
        "of the group only support up to protocol version " +
        max_supported_version.get_version_string() +
        ". To upgrade the protocol first remove the older members from the "
        "group.";
    m_diagnostics.set_execution_message(
        Group_action_diagnostics::GROUP_ACTION_LOG_ERROR,
        error_message.c_str());
  }

  return result;
}

Group_action::enum_action_execution_result
Communication_protocol_action::execute_action(bool,
                                              Plugin_stage_monitor_handler *,
                                              Notification_context *) {
  /* Wait for the protocol change if it is ongoing. */
  m_protocol_change_done.wait();

  return Group_action::GROUP_ACTION_RESULT_TERMINATED;
}

bool Communication_protocol_action::stop_action_execution(bool) {
  bool constexpr SUCCESS = false;
  return SUCCESS;
}

Group_action_diagnostics *Communication_protocol_action::get_execution_info() {
  return &m_diagnostics;
}
