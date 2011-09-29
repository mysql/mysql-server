/*
   Copyright (c) 2005, 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef MGMAPI_INTERNAL_H
#define MGMAPI_INTERNAL_H

#include <portlib/NdbTCP.h>

#ifdef __cplusplus
extern "C" {
#endif

  /**
   * Set an integer parameter for a connection
   *
   * @param handle the NDB management handle.
   * @param node1 the node1 id
   * @param node2 the node2 id
   * @param param the parameter (e.g. CFG_CONNECTION_SERVER_PORT)
   * @param value what to set it to
   * @param reply from ndb_mgmd
   */
  int ndb_mgm_set_connection_int_parameter(NdbMgmHandle handle,
					   int node1,
					   int node2,
					   int param,
					   int value,
					   struct ndb_mgm_reply* reply);

  /**
   * Get an integer parameter for a connection
   *
   * @param handle the NDB management handle.
   * @param node1 the node1 id
   * @param node2 the node2 id
   * @param param the parameter (e.g. CFG_CONNECTION_SERVER_PORT)
   * @param value where to store the retreived value. In the case of 
   * error, value is not changed.
   * @param reply from ndb_mgmd
   * @return 0 on success. < 0 on error.
   */
  int ndb_mgm_get_connection_int_parameter(NdbMgmHandle handle,
					   int node1,
					   int node2,
					   int param,
					   int *value,
					   struct ndb_mgm_reply* reply);

  /**
   * Convert connection to transporter
   * @param   handle    NDB management handle.
   *
   * @return socket
   *
   * @note the socket is now able to be used as a transporter connection
   */
  NDB_SOCKET_TYPE ndb_mgm_convert_to_transporter(NdbMgmHandle *handle);

  int ndb_mgm_disconnect_quiet(NdbMgmHandle handle);

  /**
   * Set configuration
   *
   * @param   handle    NDB management handle.
   * @param   config    The new configuration to set
   */
  int ndb_mgm_set_configuration(NdbMgmHandle handle,
                                struct ndb_mgm_configuration* config);


  NDB_SOCKET_TYPE _ndb_mgm_get_socket(NdbMgmHandle handle);

  /**
   * Get configuration
   *
   * @param   handle    NDB management handle.
   * @param   version   version of this node
   * @param   nodetype   type of this node
   */
  struct ndb_mgm_configuration *
  ndb_mgm_get_configuration2(NdbMgmHandle handle,
                             unsigned version,
                             enum ndb_mgm_node_type nodetype,
                             int from_node = 0);


#ifdef __cplusplus
}
#endif


#endif
