/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

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

Gcs_xcom_group_management::
Gcs_xcom_group_management(Gcs_xcom_proxy *xcom_proxy,
                          const Gcs_group_identifier& group_identifier)
  : m_xcom_proxy(xcom_proxy),
    m_gid(new Gcs_group_identifier(group_identifier.get_group_id())),
    m_xcom_nodes(),
    m_gid_hash(Gcs_xcom_utils::mhash(
      reinterpret_cast<unsigned char*>(const_cast<char *>(m_gid->get_group_id().c_str())),
      m_gid->get_group_id().size())
    ),
    m_nodes_mutex()
{
  m_nodes_mutex.init(
    key_GCS_MUTEX_Gcs_xcom_group_management_m_nodes_mutex, NULL);
}


Gcs_xcom_group_management::~Gcs_xcom_group_management()
{
  delete m_gid;
  m_nodes_mutex.destroy();
}


void Gcs_xcom_group_management::set_xcom_nodes(const Gcs_xcom_nodes &xcom_nodes)
{
  m_nodes_mutex.lock();
  m_xcom_nodes.add_nodes(xcom_nodes);
  m_nodes_mutex.unlock();
}

enum_gcs_error
Gcs_xcom_group_management::
modify_configuration(const Gcs_interface_parameters& reconfigured_group)
{
  // Retrieve peers_nodes parameter
  const std::string *peer_nodes_str=
                                reconfigured_group.get_parameter("peer_nodes");

  if (peer_nodes_str == NULL)
  {
    MYSQL_GCS_LOG_ERROR(
      "No peer list was provided to reconfigure the group."
    )
    return GCS_NOK;
  }

  std::vector<std::string> processed_peers, invalid_processed_peers;
  Gcs_xcom_utils::process_peer_nodes(peer_nodes_str,
                                     processed_peers);
  Gcs_xcom_utils::validate_peer_nodes(processed_peers,
                                      invalid_processed_peers);

  if(!invalid_processed_peers.empty())
  {
    std::vector<std::string>::iterator invalid_processed_peers_it;
    for(invalid_processed_peers_it= invalid_processed_peers.begin();
        invalid_processed_peers_it != invalid_processed_peers.end();
        ++invalid_processed_peers_it)
    {
      MYSQL_GCS_LOG_WARN("Peer address \"" <<
                         (*invalid_processed_peers_it).c_str()
                         << "\" is not valid.");
    }

    MYSQL_GCS_LOG_ERROR(
      "The peers list contains invalid addresses.Please provide a list with " <<
      "only valid addresses."
    )

    return GCS_NOK;
  }

  if(processed_peers.empty() && invalid_processed_peers.empty())
  {
    MYSQL_GCS_LOG_ERROR(
      "The peers list to reconfigure the group was empty."
    )
    return GCS_NOK;
  }

  m_nodes_mutex.lock();
  Gcs_xcom_nodes new_xcom_nodes;
  std::vector<std::string>::const_iterator nodes_it= processed_peers.begin();
  std::vector<std::string>::const_iterator nodes_end= processed_peers.end();
  for (int i= 0; nodes_it != nodes_end; i++, ++nodes_it)
  {
    const Gcs_xcom_node_information *node= m_xcom_nodes.get_node((*nodes_it));
    if (node == NULL)
    {
/* purecov: begin deadcode */
      MYSQL_GCS_LOG_ERROR(
        "The peer is trying to set up a configuration where there is a "
        "member '" << (*nodes_it) << "' that doesn't belong to its current "
        "view."
      );
      m_nodes_mutex.unlock();
      return GCS_NOK;
/* purecov: end */
    }
    new_xcom_nodes.add_node(*node);
  }
  m_nodes_mutex.unlock();

  if (new_xcom_nodes.get_size() == 0)
  {
/* purecov: begin deadcode */
    MYSQL_GCS_LOG_ERROR(
      "Requested peers are not members and cannot be used to start "
      "a reconfiguration."
    );
    return GCS_NOK;
/* purecov: end */
  }

  if (m_xcom_proxy->xcom_force_nodes(new_xcom_nodes, m_gid_hash) != 1)
  {
/* purecov: begin deadcode */
    MYSQL_GCS_LOG_ERROR("Error reconfiguring group.");
    return GCS_NOK;
/* purecov: end */
  }

  return GCS_OK;
}
