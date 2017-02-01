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

#include "gcs_xcom_group_management.h"

#include "gcs_xcom_utils.h"
#include "gcs_logging.h"

Gcs_xcom_group_management::
Gcs_xcom_group_management(Gcs_xcom_proxy *xcom_proxy,
                          Gcs_xcom_view_change_control_interface *view_control,
                          const Gcs_group_identifier& group_identifier)
  : m_xcom_proxy(xcom_proxy),
    m_view_control(view_control),
    m_gid(new Gcs_group_identifier(group_identifier.get_group_id())),
    m_gid_hash(Gcs_xcom_utils::mhash(
     reinterpret_cast<unsigned char*>(const_cast<char *>(m_gid->get_group_id().c_str())),
     m_gid->get_group_id().size()))
{}


Gcs_xcom_group_management::~Gcs_xcom_group_management()
{
  delete m_gid;
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

  Gcs_view *view= m_view_control->get_current_view();
  if (view == NULL)
  {
    MYSQL_GCS_LOG_ERROR(
      "The peer is not part of any group and cannot be used to start "
      "a reconfiguration."
    );
    return GCS_NOK;
  }

  unsigned int len= static_cast<unsigned int>(processed_peers.size());
  char **addrs= static_cast<char **>(malloc(len * sizeof(char *)));
  blob *uuids= static_cast<blob *>(malloc(len * sizeof(blob)));

  const Gcs_member_identifier *member= NULL;
  std::vector<std::string>::const_iterator nodes_it= processed_peers.begin();
  std::vector<std::string>::const_iterator nodes_end= processed_peers.end();
  for (int i= 0; nodes_it != nodes_end; i++, ++nodes_it)
  {
    member= view->get_member(*nodes_it);
    if (member == NULL)
    {
      MYSQL_GCS_LOG_ERROR(
        "The peer is trying to set up a configuration where there is a "
        "member '" << addrs[i] << "' that doesn't belong to its current "
        "view."
      );

      /*
        We have to free the memory allocated to convey the uuid in blob
        format because it will not be handed over to XCOM and noboby
        will free it otherwise.
      */
      for (int j=i; j > 0; j--)
      {
        free(uuids[j - 1].data.data_val);
      } 
      
      free(addrs);
      free(uuids);
      delete view;
      return GCS_NOK;
    }

    addrs[i]= const_cast<char *>((*nodes_it).c_str());
    uuids[i].data.data_len= Gcs_uuid::size;
    uuids[i].data.data_val=
      static_cast<char *>(malloc(uuids[i].data.data_len * sizeof(char)));
    member->get_member_uuid().encode(
      reinterpret_cast<uchar **>(&uuids[i].data.data_val)
    );
 
    MYSQL_GCS_LOG_TRACE(
      "::modify_configuration():: Node[" << i << "]=" << addrs[i]  << " "
      << member->get_member_uuid().value
    );
  }

  node_list nl;
  nl.node_list_len= len;
  nl.node_list_val= m_xcom_proxy->new_node_address_uuid(len, addrs, uuids);

  int result= m_xcom_proxy->xcom_client_force_config(&nl, m_gid_hash);
  m_xcom_proxy->delete_node_address(len, nl.node_list_val);

  if (result != 1)
    MYSQL_GCS_LOG_ERROR("Error reconfiguring group.");

  free(addrs);
  free(uuids);
  delete view;

  return (result == 1)? GCS_OK: GCS_NOK;
}
