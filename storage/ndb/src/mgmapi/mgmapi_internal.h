/* Copyright (C) 2005 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef MGMAPI_INTERNAL_H
#define MGMAPI_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <NdbTCP.h>

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

#ifdef __cplusplus
}
#endif


#endif
