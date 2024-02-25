/* Copyright (c) 2016, 2023, Oracle and/or its affiliates.

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

#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_group_management.h"

#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_logging_system.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_group_member_information.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_utils.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/site_struct.h"

#include <algorithm>
#include <cstring>
#include <iterator>

Gcs_xcom_group_management::Gcs_xcom_group_management(
    Gcs_xcom_proxy *xcom_proxy, const Gcs_group_identifier &group_identifier,
    Gcs_xcom_view_change_control_interface *view_control)
    : m_xcom_proxy(xcom_proxy),
      m_gid(new Gcs_group_identifier(group_identifier.get_group_id())),
      m_xcom_nodes(),
      m_gid_hash(Gcs_xcom_utils::mhash(
          reinterpret_cast<unsigned char *>(
              const_cast<char *>(m_gid->get_group_id().c_str())),
          m_gid->get_group_id().size())),
      m_nodes_mutex(),
      m_view_control(view_control) {
  m_nodes_mutex.init(key_GCS_MUTEX_Gcs_xcom_group_management_m_nodes_mutex,
                     nullptr);
}

Gcs_xcom_group_management::~Gcs_xcom_group_management() {
  delete m_gid;
  m_nodes_mutex.destroy();
}

void Gcs_xcom_group_management::set_xcom_nodes(
    const Gcs_xcom_nodes &xcom_nodes) {
  m_nodes_mutex.lock();
  m_xcom_nodes.add_nodes(xcom_nodes);
  m_nodes_mutex.unlock();
}

void Gcs_xcom_group_management::get_xcom_nodes(
    Gcs_xcom_nodes &result_xcom_nodes,
    const std::vector<Gcs_member_identifier> &filter) {
  std::vector<std::string> str_filter;
  std::transform(filter.cbegin(), filter.cend(), std::back_inserter(str_filter),
                 [](const Gcs_member_identifier &value) -> std::string {
                   return value.get_member_id();
                 });
  get_xcom_nodes(result_xcom_nodes, str_filter);
}

void Gcs_xcom_group_management::get_xcom_nodes(
    Gcs_xcom_nodes &result_xcom_nodes,
    const std::vector<Gcs_member_identifier *> &filter) {
  std::vector<std::string> str_filter;
  std::transform(filter.cbegin(), filter.cend(), std::back_inserter(str_filter),
                 [](const Gcs_member_identifier *value) -> std::string {
                   return value->get_member_id();
                 });
  get_xcom_nodes(result_xcom_nodes, str_filter);
}

void Gcs_xcom_group_management::get_xcom_nodes(
    Gcs_xcom_nodes &result_xcom_nodes, const std::vector<std::string> &filter) {
  m_nodes_mutex.lock();
  for (const auto &member_id : filter) {
    const Gcs_xcom_node_information *node = m_xcom_nodes.get_node(member_id);
    if (node != nullptr) {
      result_xcom_nodes.add_node(*node);
    }
  }
  m_nodes_mutex.unlock();
}

/*
  Returns the nodes in a string formatted as:

    "host1:port1,host2:port2,...,hostN:portN"
*/
static std::string nodes_to_str(
    std::vector<Gcs_xcom_node_information> const &nodes) {
  std::stringstream ss;
  for (size_t i = 0; i < nodes.size(); i++) {
    ss << nodes.at(i).get_member_id().get_member_id();
    if (i < nodes.size() - 1) {
      ss << ',';
    }
  }
  return ss.str();
}

enum_gcs_error Gcs_xcom_group_management::modify_configuration(
    const Gcs_interface_parameters &reconfigured_group) {
  // Retrieve peers_nodes parameter
  const std::string *peer_nodes_str =
      reconfigured_group.get_parameter("peer_nodes");

  if (peer_nodes_str == nullptr) {
    MYSQL_GCS_LOG_ERROR("No peer list was provided to reconfigure the group.")
    return GCS_NOK;
  }

  std::vector<std::string> processed_peers, invalid_processed_peers;
  Gcs_xcom_utils::process_peer_nodes(peer_nodes_str, processed_peers);
  Gcs_xcom_utils::validate_peer_nodes(processed_peers, invalid_processed_peers);

  if (!invalid_processed_peers.empty()) {
    std::vector<std::string>::iterator invalid_processed_peers_it;
    for (invalid_processed_peers_it = invalid_processed_peers.begin();
         invalid_processed_peers_it != invalid_processed_peers.end();
         ++invalid_processed_peers_it) {
      MYSQL_GCS_LOG_WARN("Peer address \""
                         << (*invalid_processed_peers_it).c_str()
                         << "\" is not valid.");
    }

    MYSQL_GCS_LOG_ERROR(
        "The peers list contains invalid addresses.Please provide a list with "
        << "only valid addresses.")

    return GCS_NOK;
  }

  if (processed_peers.empty() && invalid_processed_peers.empty()) {
    MYSQL_GCS_LOG_ERROR("The peers list to reconfigure the group was empty.")
    return GCS_NOK;
  }

  Gcs_xcom_nodes new_xcom_nodes;
  get_xcom_nodes(new_xcom_nodes, processed_peers);
  if (new_xcom_nodes.get_size() != processed_peers.size()) {
    MYSQL_GCS_LOG_ERROR(
        "The peer is trying to set up a configuration where there are members "
        "that don't belong to the current view.")
    return GCS_NOK;
  }

  if (new_xcom_nodes.get_size() == 0) {
    /* purecov: begin deadcode */
    MYSQL_GCS_LOG_ERROR(
        "Requested peers are not members and cannot be used to start "
        "a reconfiguration.");
    return GCS_NOK;
    /* purecov: end */
  }

  /* Copy from m_xcom_nodes. We can release the lock and use the copy safely
   * afterwards. */
  m_nodes_mutex.lock();
  std::vector<Gcs_xcom_node_information> const current_nodes =
      m_xcom_nodes.get_nodes();
  m_nodes_mutex.unlock();

  if (new_xcom_nodes.get_size() == current_nodes.size()) {
    std::vector<Gcs_xcom_node_information> const &forced_nodes =
        new_xcom_nodes.get_nodes();

    MYSQL_GCS_LOG_ERROR("The requested membership to forcefully set ("
                        << nodes_to_str(forced_nodes)
                        << ") is the same as the "
                           "current membership ("
                        << nodes_to_str(current_nodes) << ").")
    return GCS_NOK;
  }

  bool const sent_to_xcom =
      m_xcom_proxy->xcom_force_nodes(new_xcom_nodes, m_gid_hash);
  if (!sent_to_xcom) {
    /* purecov: begin deadcode */
    MYSQL_GCS_LOG_ERROR("Error reconfiguring group.");
    return GCS_NOK;
    /* purecov: end */
  }

  return GCS_OK;
}

uint32_t Gcs_xcom_group_management::get_minimum_write_concurrency() const {
  return m_xcom_proxy->xcom_get_minimum_event_horizon();
}

uint32_t Gcs_xcom_group_management::get_maximum_write_concurrency() const {
  return m_xcom_proxy->xcom_get_maximum_event_horizon();
}

enum_gcs_error Gcs_xcom_group_management::get_write_concurrency(
    uint32_t &event_horizon) const {
  /*
    We need to check if we are leaving, because xcom_get_event_horizon
    needs to query XCom and wait for a reply, which does not happen
    if we are leaving or if we do not belong to a group.

    For that, we use View Control->is_leaving() and XCom Proxy->xcom_is_exit(),
    which are synchronized inside View Control, although optimistically, because
    they are set in diffenrent moments in time, with small window for races.
    - is_leaving() is set when a client calls leave() on the public
      interface;
    - xcom_is_exit() is set when the XCom thread is stopped;

    We are not using View Control->belongs_to_group(), because that method is
    fully optimistic.
  */
  if (m_view_control->is_leaving() || m_xcom_proxy->xcom_is_exit()) {
    MYSQL_GCS_LOG_DEBUG(
        "Unable to request Write Concurrency. This member is leaving or it is "
        "not on a group.")
    return GCS_NOK;
  }

  MYSQL_GCS_LOG_DEBUG(
      "The member is attempting to retrieve the event horizon.");
  bool const success =
      m_xcom_proxy->xcom_get_event_horizon(m_gid_hash, event_horizon);
  return success ? GCS_OK : GCS_NOK;
}

enum_gcs_error Gcs_xcom_group_management::set_write_concurrency(
    uint32_t event_horizon) {
  MYSQL_GCS_LOG_DEBUG(
      "The member is attempting to reconfigure the event horizon.");
  bool const success =
      m_xcom_proxy->xcom_set_event_horizon(m_gid_hash, event_horizon);
  return success ? GCS_OK : GCS_NOK;
}

enum_gcs_error Gcs_xcom_group_management::set_single_leader(
    Gcs_member_identifier const &leader) {
  u_int constexpr one_preferred_leader = 1;
  char const *preferred_leader[one_preferred_leader] = {
      leader.get_member_id().c_str()};
  node_no constexpr one_active_leader = 1;

  MYSQL_GCS_LOG_DEBUG(
      "The member is attempting to reconfigure XCom to use %s as the single "
      "leader.",
      leader.get_member_id().c_str());

  bool success = m_xcom_proxy->xcom_set_leaders(
      m_gid_hash, one_preferred_leader, preferred_leader, one_active_leader);
  return success ? GCS_OK : GCS_NOK;
}

enum_gcs_error Gcs_xcom_group_management::set_everyone_leader() {
  u_int constexpr zero_preferred_leaders = 0;
  char const **empty_preferred_leaders{nullptr};
  node_no constexpr everyone_active_leader = active_leaders_all;

  MYSQL_GCS_LOG_DEBUG(
      "The member is attempting to reconfigure XCom to use everyone as "
      "leader.");

  bool success = m_xcom_proxy->xcom_set_leaders(
      m_gid_hash, zero_preferred_leaders, empty_preferred_leaders,
      everyone_active_leader);
  return success ? GCS_OK : GCS_NOK;
}

enum_gcs_error Gcs_xcom_group_management::get_leaders(
    std::vector<Gcs_member_identifier> &preferred_leaders,
    std::vector<Gcs_member_identifier> &actual_leaders) {
  MYSQL_GCS_LOG_DEBUG(
      "The member is attempting to retrieve the leader information.");
  leader_info_data leaders;

  bool const success = m_xcom_proxy->xcom_get_leaders(m_gid_hash, leaders);
  if (!success) return GCS_NOK;

  /* Translate the preferred leaders representation. */
  if (leaders.max_nr_leaders == active_leaders_all) {
    // Everyone as leader, so the preferred leaders match the actual leaders.
    for (u_int i = 0; i < leaders.actual_leaders.leader_array_len; i++) {
      preferred_leaders.emplace_back(
          std::string{leaders.actual_leaders.leader_array_val[i].address});
    }
  } else {
    // Translate the reply data into the std::vector<Gcs_member_identifier>.
    for (u_int i = 0; i < leaders.preferred_leaders.leader_array_len; i++) {
      preferred_leaders.emplace_back(
          std::string{leaders.preferred_leaders.leader_array_val[i].address});
    }
  }

  /* Translate the actual leaders representation. */
  for (u_int i = 0; i < leaders.actual_leaders.leader_array_len; i++) {
    actual_leaders.emplace_back(
        std::string{leaders.actual_leaders.leader_array_val[i].address});
  }

  ::xdr_free(reinterpret_cast<xdrproc_t>(xdr_leader_info_data),
             reinterpret_cast<char *>(&leaders));

  return GCS_OK;
}
