/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef GCS_XCOM_GROUP_MANAGEMENT_INCLUDED
#define GCS_XCOM_GROUP_MANAGEMENT_INCLUDED

#include "mysql/gcs/gcs_group_management_interface.h" // Base class: Gcs_group_management_interface
#include "mysql/gcs/xplatform/my_xp_mutex.h"
#include "gcs_xcom_utils.h"
#include "gcs_xcom_state_exchange.h"
#include "gcs_xcom_group_member_information.h"

class Gcs_xcom_group_management : public Gcs_group_management_interface
{
public:
  explicit Gcs_xcom_group_management(
    Gcs_xcom_proxy *xcom_proxy,
    const Gcs_group_identifier& group_identifier);
  virtual ~Gcs_xcom_group_management();

  enum_gcs_error
       modify_configuration(const Gcs_interface_parameters& reconfigured_group);

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

private:
  Gcs_xcom_proxy *m_xcom_proxy;
  Gcs_group_identifier* m_gid;
  Gcs_xcom_nodes m_xcom_nodes;
  unsigned int m_gid_hash;

  /*
    Mutex used to prevent concurrent access to nodes.
  */
  My_xp_mutex_impl m_nodes_mutex;

  /*
    Disabling the copy constructor and assignment operator.
  */
  Gcs_xcom_group_management(Gcs_xcom_group_management const&);
  Gcs_xcom_group_management& operator=(Gcs_xcom_group_management const&);
};
#endif // GCS_XCOM_GROUP_MANAGEMENT_INCLUDED
