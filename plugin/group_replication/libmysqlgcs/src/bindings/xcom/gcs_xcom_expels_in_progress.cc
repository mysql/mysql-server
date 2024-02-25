/* Copyright (c) 2020, 2023, Oracle and/or its affiliates.

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

#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_expels_in_progress.h"
#include <algorithm>
#include <sstream>
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_logging_system.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/synode_no.h"

void Gcs_xcom_expels_in_progress::remember_expels_issued(
    synode_no const config_id_where_expels_were_issued,
    Gcs_xcom_nodes const &expels_issued) {
  for (auto const &expelled_node : expels_issued.get_nodes()) {
    m_expels_in_progress.emplace_back(expelled_node.get_member_id(),
                                      config_id_where_expels_were_issued);
  }
}

void Gcs_xcom_expels_in_progress::forget_expels_that_have_taken_effect(
    synode_no const config_id_where_members_left,
    std::vector<Gcs_member_identifier *> const &members_that_left) {
  MYSQL_GCS_TRACE_EXECUTE({
    std::stringstream ss;
    ss << "(";
    for (auto const *const member_that_left : members_that_left) {
      ss << " " << member_that_left->get_member_id();
    }
    ss << " )";
    MYSQL_GCS_LOG_TRACE("%s: config_id_where_members_left={%" PRIu64 " %" PRIu32
                        "} members_that_left=%s",
                        __func__, config_id_where_members_left.msgno,
                        config_id_where_members_left.node, ss.str().c_str());
  });

  for (auto const *const member_that_left : members_that_left) {
    /* Lambda that tells us whether some `expelled_member_info` should be
       removed from `expels_in_progress` because it matches `member_that_left`.
     */
    auto const func = __func__;
    auto const expel_has_taken_effect =
        [&func, config_id_where_members_left,
         member_that_left](std::pair<Gcs_member_identifier, synode_no> const
                               &expelled_member_info) {
          auto const &expelled_member = expelled_member_info.first;
          auto const &expel_config_id = expelled_member_info.second;
          bool const expelled_member_left =
              (expelled_member == *member_that_left &&
               synode_lt(expel_config_id, config_id_where_members_left) != 0);

          MYSQL_GCS_LOG_TRACE(
              "%s: expelled_member_info=(%s {%" PRIu64 " %" PRIu32
              "}) member_that_left=%s config_id_where_members_left=%" PRIu64
              " %" PRIu32 " expelled_member_left=%d",
              func, expelled_member.get_member_id().c_str(),
              expel_config_id.msgno, expel_config_id.node,
              member_that_left->get_member_id().c_str(),
              config_id_where_members_left.msgno,
              config_id_where_members_left.node, expelled_member_left);

          return expelled_member_left;
        };

    /* Remove the information about expels of `member_that_left` because they
       have already taken effect. */
    m_expels_in_progress.erase(
        std::remove_if(m_expels_in_progress.begin(), m_expels_in_progress.end(),
                       expel_has_taken_effect),
        m_expels_in_progress.end());
  }

  MYSQL_GCS_DEBUG_EXECUTE({
    std::stringstream ss;
    ss << "[";
    for (auto const &expelled_member : m_expels_in_progress) {
      ss << " (" << expelled_member.first.get_member_id() << " {"
         << expelled_member.second.msgno << " " << expelled_member.second.node
         << "})";
    }
    ss << " ]";
    MYSQL_GCS_LOG_DEBUG("%s: expels_in_progress=%s", __func__,
                        ss.str().c_str());
  });
}

std::size_t Gcs_xcom_expels_in_progress::number_of_expels_not_about_suspects(
    std::vector<Gcs_member_identifier *> const &suspected_members,
    std::vector<Gcs_member_identifier *> const &suspected_nonmembers) const {
  std::size_t number_of_expelled_but_not_suspected_nodes = 0;

  for (auto const &expelled_node_info : m_expels_in_progress) {
    auto const &expelled_node = expelled_node_info.first;

    /* Count `expelled_node` as suspected if it is not already suspected. */
    auto const suspect_is_expelled_node =
        [&expelled_node](Gcs_member_identifier const *const suspect_node) {
          return expelled_node == *suspect_node;
        };
    if (std::none_of(suspected_members.cbegin(), suspected_members.cend(),
                     suspect_is_expelled_node) &&
        std::none_of(suspected_nonmembers.cbegin(), suspected_nonmembers.cend(),
                     suspect_is_expelled_node)) {
      number_of_expelled_but_not_suspected_nodes++;
    }
  }

  return number_of_expelled_but_not_suspected_nodes;
}

bool Gcs_xcom_expels_in_progress::all_still_in_view(
    Gcs_xcom_nodes const &xcom_nodes) const {
  for (auto const &expelled_member_info : m_expels_in_progress) {
    auto const &expelled_member = expelled_member_info.first;
    auto const is_expelled_member =
        [&expelled_member](Gcs_xcom_node_information const &xcom_node) {
          return expelled_member == xcom_node.get_member_id();
        };
    bool const still_in_view =
        std::any_of(xcom_nodes.get_nodes().cbegin(),
                    xcom_nodes.get_nodes().cend(), is_expelled_member);
    if (!still_in_view) {
      return false;
    }
  }

  return true;
}

std::size_t Gcs_xcom_expels_in_progress::size() const {
  return m_expels_in_progress.size();
}

bool Gcs_xcom_expels_in_progress::contains(Gcs_member_identifier const &member,
                                           synode_no const synode) const {
  auto const is_expel_for_member_on_synode =
      [&member, &synode](std::pair<Gcs_member_identifier, synode_no> const
                             &expelled_member_info) {
        return expelled_member_info.first == member &&
               synode_eq(expelled_member_info.second, synode) != 0;
      };
  return std::any_of(m_expels_in_progress.cbegin(), m_expels_in_progress.cend(),
                     is_expel_for_member_on_synode);
}