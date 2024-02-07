/* Copyright (c) 2016, 2024, Oracle and/or its affiliates.

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

#ifndef GCS_XCOM_GROUP_MANAGEMENT_INCLUDED
#define GCS_XCOM_GROUP_MANAGEMENT_INCLUDED

#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_group_management_interface.h"  // Base class: Gcs_group_management_interface
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_member_identifier.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/xplatform/my_xp_mutex.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_group_member_information.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_proxy.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_state_exchange.h"

#include <string>
#include <vector>

class Gcs_xcom_group_management : public Gcs_group_management_interface {
 public:
  explicit Gcs_xcom_group_management(
      Gcs_xcom_proxy *xcom_proxy, const Gcs_group_identifier &group_identifier,
      Gcs_xcom_view_change_control_interface *view_control);
  ~Gcs_xcom_group_management() override;

  enum_gcs_error modify_configuration(
      const Gcs_interface_parameters &reconfigured_group) override;

  enum_gcs_error get_write_concurrency(uint32_t &event_horizon) const override;

  enum_gcs_error set_write_concurrency(uint32_t event_horizon) override;

  enum_gcs_error set_single_leader(
      Gcs_member_identifier const &leader) override;
  enum_gcs_error set_everyone_leader() override;
  enum_gcs_error get_leaders(
      std::vector<Gcs_member_identifier> &preferred_leaders,
      std::vector<Gcs_member_identifier> &actual_leaders) override;

  uint32_t get_minimum_write_concurrency() const override;

  uint32_t get_maximum_write_concurrency() const override;

  /**
    Save information on the latest nodes seen by this node so that it
    can safely reconfigure the group if it loses the majority. This
    information is required to extract the set of possible alive nodes
    and their UUIDs. If the UUIDs in the reconfiguration request do not
    match those stored in XCOM, it will not allow the reconfiguration
    to proceed.

    Note also that the set of nodes is updated by the MySQL GCS thread
    whenever a new configuration is delivered and that a user thread is
    responsible for calling the reconfiguration process. If a user is
    trying to reconfigure the system, this usually and unfortunately
    means that it has lost the majority and nothing will be updated by
    the MySQL GCS thread. However, just for the sake of completeness,
    we will use a mutex here to control access to this data structure.

    Finally, it is worth noting that we cannot use get_site_def() here
    because information on nodes could be concurrently updated by the
    XCOM thread and we cannot add a mutex to it.
  */
  void set_xcom_nodes(const Gcs_xcom_nodes &xcom_nodes);

  /*
   Get a copy of the nodes in the current configuration that are in the
   filter list.

   @param[out] result_xcom_nodes The set of Gcs_xcom_nodes that are in the
              filter list
   @param[in] filter The list of nodes identified as a string that one is
                     interested in retrieving information on
   */
  void get_xcom_nodes(Gcs_xcom_nodes &result_xcom_nodes,
                      const std::vector<std::string> &filter);

  /*
   Get a copy of the nodes in the current configuration that are in the
   filter list.

   @param[out] result_xcom_nodes The set of Gcs_xcom_nodes that are in the
   filter list
   @param[in] filter The list of nodes identified as Gcs_member_identifier(s)
   that one is interested in retrieving information on
   */
  void get_xcom_nodes(Gcs_xcom_nodes &result_xcom_nodes,
                      const std::vector<Gcs_member_identifier> &filter);

  /*
   Get a copy of the nodes in the current configuration that are in the
   filter list.

   @param[out] result_xcom_nodes The set of Gcs_xcom_nodes that are in the
   filter list
   @param[in] filter The list of nodes identified as Gcs_member_identifier(s)
   that one is interested in retrieving information on
   */
  void get_xcom_nodes(Gcs_xcom_nodes &result_xcom_nodes,
                      const std::vector<Gcs_member_identifier *> &filter);

 private:
  Gcs_xcom_proxy *m_xcom_proxy;
  Gcs_group_identifier *m_gid;
  Gcs_xcom_nodes m_xcom_nodes;
  unsigned int m_gid_hash;

  /*
    Mutex used to prevent concurrent access to nodes.
  */
  My_xp_mutex_impl m_nodes_mutex;

  /*
    Regulate the access to certain methods, mainly avoid sending requests
    if this node is stopping.
  */
  Gcs_xcom_view_change_control_interface *m_view_control;

  /*
    Disabling the copy constructor and assignment operator.
  */
  Gcs_xcom_group_management(Gcs_xcom_group_management const &);
  Gcs_xcom_group_management &operator=(Gcs_xcom_group_management const &);
};
#endif  // GCS_XCOM_GROUP_MANAGEMENT_INCLUDED
