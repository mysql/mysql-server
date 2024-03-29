/* Copyright (c) 2021, 2023, Oracle and/or its affiliates.

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

#include "plugin/group_replication/include/plugin_handlers/consensus_leaders_handler.h"
#include "plugin/group_replication/include/member_info.h"
#include "plugin/group_replication/include/member_version.h"
#include "plugin/group_replication/include/mysql_version_gcs_protocol_map.h"
#include "plugin/group_replication/include/plugin.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_member_identifier.h"

/**
 * @brief First member version where we have XCom's single leader
 */
/// @cond
Member_version const Consensus_leaders_handler::
    s_first_protocol_with_support_for_consensus_leaders{
        FIRST_PROTOCOL_WITH_SUPPORT_FOR_CONSENSUS_LEADERS};
/// @endcond

Consensus_leaders_handler::Consensus_leaders_handler(
    Group_events_observation_manager &group_events_manager)
    : m_group_events_manager(group_events_manager) {
  m_group_events_manager.register_group_event_observer(this);
}

Consensus_leaders_handler::~Consensus_leaders_handler() {
  m_group_events_manager.unregister_group_event_observer(this);
}

int Consensus_leaders_handler::after_view_change(
    const std::vector<Gcs_member_identifier> & /*joining*/,
    const std::vector<Gcs_member_identifier> & /*leaving*/,
    const std::vector<Gcs_member_identifier> & /*group*/, bool /*is_leaving*/,
    bool * /*skip_election*/, enum_primary_election_mode * /*election_mode*/,
    std::string & /*suggested_primary*/) {
  return 0;
}

int Consensus_leaders_handler::after_primary_election(
    std::string primary_uuid,
    enum_primary_election_primary_change_status primary_change_status,
    enum_primary_election_mode /*election_mode*/, int error) {
  if (enum_primary_election_primary_change_status::PRIMARY_DID_CHANGE ==
      primary_change_status) {
    Member_version const communication_protocol =
        convert_to_mysql_version(gcs_module->get_protocol_version());

    Group_member_info *new_primary_info =
        group_member_mgr->get_group_member_info(primary_uuid);
    Gcs_member_identifier const new_primary_gcs_id =
        new_primary_info->get_gcs_member_id();
    Gcs_member_identifier const my_gcs_id =
        local_member_info->get_gcs_member_id();
    bool const i_am_the_new_primary = (new_primary_gcs_id == my_gcs_id);
    auto role = i_am_the_new_primary ? Group_member_info::MEMBER_ROLE_PRIMARY
                                     : Group_member_info::MEMBER_ROLE_SECONDARY;
    set_consensus_leaders(communication_protocol, true, role, my_gcs_id);

    delete new_primary_info;
  }

  return 0;
}

int Consensus_leaders_handler::before_message_handling(
    const Plugin_gcs_message & /*message*/,
    const std::string & /*message_origin*/, bool * /*skip_message*/) {
  return 0;
}

void Consensus_leaders_handler::set_consensus_leaders(
    Member_version const &communication_protocol, bool is_single_primary_mode,
    Group_member_info::Group_member_role role,
    Gcs_member_identifier const &my_gcs_id) {
  return this->set_consensus_leaders(
      communication_protocol, is_single_primary_mode, role, my_gcs_id,
      []() { return get_allow_single_leader(); });
}

void Consensus_leaders_handler::set_consensus_leaders(
    Member_version const &communication_protocol, bool is_single_primary_mode,
    Group_member_info::Group_member_role role,
    Gcs_member_identifier const &my_gcs_id,
    std::function<bool()> allow_single_leader_getter) {
  if (!allow_single_leader_getter()) return;

  bool const support_single_consensus_leader =
      (communication_protocol >=
       s_first_protocol_with_support_for_consensus_leaders);

  if (support_single_consensus_leader && is_single_primary_mode) {
    if (role == Group_member_info::MEMBER_ROLE_PRIMARY) {
      set_as_single_consensus_leader(my_gcs_id);
    }
  } else {
    set_everyone_as_consensus_leader();
  }
}

void Consensus_leaders_handler::set_as_single_consensus_leader(
    Gcs_member_identifier const &leader) const {
  Group_member_info *leader_info =
      group_member_mgr->get_group_member_info_by_member_id(leader);

  enum enum_gcs_error error_code = gcs_module->set_leader(leader);
  if (error_code == GCS_OK) {
    LogPluginErr(SYSTEM_LEVEL, ER_GRP_RPL_SET_SINGLE_CONSENSUS_LEADER,
                 leader_info->get_hostname().c_str(), leader_info->get_port(),
                 leader_info->get_uuid().c_str());
  } else {
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_ERROR_SET_SINGLE_CONSENSUS_LEADER,
                 leader_info->get_hostname().c_str(), leader_info->get_port(),
                 leader_info->get_uuid().c_str());
  }

  if (leader_info) delete leader_info;
}

void Consensus_leaders_handler::set_everyone_as_consensus_leader() const {
  enum enum_gcs_error error_code = gcs_module->set_everyone_leader();
  if (error_code == GCS_OK) {
    LogPluginErr(SYSTEM_LEVEL, ER_GRP_RPL_SET_MULTI_CONSENSUS_LEADER);
  } else {
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_ERROR_SET_MULTI_CONSENSUS_LEADER);
  }
}
