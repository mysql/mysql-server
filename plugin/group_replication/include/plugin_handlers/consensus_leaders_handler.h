/* Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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

#ifndef CONSENSUS_LEADERS_HANDLER_INCLUDED
#define CONSENSUS_LEADERS_HANDLER_INCLUDED

#include "plugin/group_replication/include/member_info.h"
#include "plugin/group_replication/include/member_version.h"
#include "plugin/group_replication/include/plugin_observers/group_event_observer.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_member_identifier.h"

/**
 * @brief Deals with the orchestration necessary to set the appropriate
 * "consensus leaders" on GCS.
 *
 * Reacts to successful primary elections by setting the newly elected primary
 * as the single preferred "consensus leader" in GCS.
 */
class Consensus_leaders_handler : public Group_event_observer {
 public:
  /**
   * Register as a group event observer in @c group_events_manager.
   */
  Consensus_leaders_handler(
      Group_events_observation_manager &group_events_manager);

  /**
   * Unregister as a group event observer from @c m_group_events_manager.
   */
  ~Consensus_leaders_handler() override;

  int after_view_change(const std::vector<Gcs_member_identifier> &joining,
                        const std::vector<Gcs_member_identifier> &leaving,
                        const std::vector<Gcs_member_identifier> &group,
                        bool is_leaving, bool *skip_election,
                        enum_primary_election_mode *election_mode,
                        std::string &suggested_primary) override;

  /**
   * @brief Sets the newly elected primary as the single preferred "consensus
   * leader" in GCS.
   *
   * @param primary_uuid the elected primary
   * @param primary_change_status if the primary changed after the election
   * @param election_mode what was the election mode
   * @param error if there was and error on the process
   * @return int 0 on success
   */
  int after_primary_election(
      std::string primary_uuid,
      enum_primary_election_primary_change_status primary_change_status,
      enum_primary_election_mode election_mode, int error) override;

  int before_message_handling(const Plugin_gcs_message &message,
                              const std::string &message_origin,
                              bool *skip_message) override;

  /**
   * @brief Set the appropriate "consensus leaders" in GCS.
   *
   * The "consensus leaders" are set according to the following rules:
   * 0) If the the return result  of allow_single_leader_getter lambda is false
   *    :do nothing
   * a) @c communication_protocol is < 8.0.27: do nothing
   * b) @c communication_protocol is >= 8.0.27:
   *    b1) @c primary_mode is SINGLE and @c my_role is PRIMARY: set myself,
   *        @c my_gcs_id, as the single preferred "consensus leader"
   *    b2) @c primary_mode is SINGLE and @c my_role is SECONDARY: do nothing
   *    b3) @c primary_mode is MULTI: set everyone as "consensus leader"
   */
  void set_consensus_leaders(Member_version const &communication_protocol,
                             bool is_single_primary_mode,
                             Group_member_info::Group_member_role role,
                             Gcs_member_identifier const &my_gcs_id,
                             std::function<bool()> allow_single_leader_getter);

  /**
   * @brief Set the appropriate "consensus leaders" in GCS.
   *
   * The "consensus leaders" are set according to the following rules:
   * a) @c communication_protocol is < 8.0.27: do nothing
   * b) @c communication_protocol is >= 8.0.27:
   *    b1) @c primary_mode is SINGLE and @c my_role is PRIMARY: set myself,
   *        @c my_gcs_id, as the single preferred "consensus leader"
   *    b2) @c primary_mode is SINGLE and @c my_role is SECONDARY: do nothing
   *    b3) @c primary_mode is MULTI: set everyone as "consensus leader"
   *
   * It inserts a default implementation of allow_single_leader_getter that
   * verifies the current value of group_replication_paxos_single_leader var
   */
  void set_consensus_leaders(Member_version const &communication_protocol,
                             bool is_single_primary_mode,
                             Group_member_info::Group_member_role role,
                             Gcs_member_identifier const &my_gcs_id);

 private:
  void set_as_single_consensus_leader(
      Gcs_member_identifier const &leader) const;
  void set_everyone_as_consensus_leader() const;

  static Member_version const
      s_first_protocol_with_support_for_consensus_leaders;
  Group_events_observation_manager &m_group_events_manager;
};

#endif /* CONSENSUS_LEADERS_HANDLER_INCLUDED */
