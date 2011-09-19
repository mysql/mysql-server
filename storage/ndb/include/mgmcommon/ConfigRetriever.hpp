/*
   Copyright (c) 2003, 2010, Oracle and/or its affiliates. All rights reserved.

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

#ifndef ConfigRetriever_H
#define ConfigRetriever_H

#include <ndb_types.h>
#include <mgmapi.h>
#include <BaseString.hpp>

/**
 * @class ConfigRetriever
 * @brief Used by nodes (DB, MGM, API) to get their config from MGM server. 
 */
class ConfigRetriever {
public:
  ConfigRetriever(const char * _connect_string, int force_nodeid,
                  Uint32 version, ndb_mgm_node_type nodeType,
		  const char * _bind_address = 0,
                  int timeout_ms = 30000);
  ~ConfigRetriever();

  int do_connect(int no_retries, int retry_delay_in_seconds, int verbose);
  int disconnect();
  bool is_connected();

  /**
   * Get configuration for current node.
   * 
   * Configuration is fetched from one MGM server configured in local config 
   * file.  The method loops over all the configured MGM servers and tries
   * to establish a connection.  This is repeated until a connection is 
   * established, so the function hangs until a connection is established.
   * 
   * @return ndb_mgm_configuration object if succeeded, 
   *         NULL if erroneous local config file or configuration error.
   */
  struct ndb_mgm_configuration * getConfig(Uint32 nodeid);
  
  void resetError();
  int hasError();
  const char * getErrorString();

  /**
   * @return Node id of this node (as stated in local config or connectString)
   */
  Uint32 allocNodeId(int no_retries, int retry_delay_in_seconds);
  Uint32 allocNodeId(int no_retries, int retry_delay_in_seconds,
                     int verbose, int& error);

  int setNodeId(Uint32 nodeid);

  /**
   * Get config using socket
   */
  struct ndb_mgm_configuration * getConfig(NdbMgmHandle handle);
  
  /**
   * Get config from file
   */
  struct ndb_mgm_configuration * getConfig(const char * file);

  /**
   * Verify config
   */
  bool verifyConfig(const struct ndb_mgm_configuration *, Uint32 nodeid);

  Uint32 get_mgmd_port() const;
  const char *get_mgmd_host() const;
  const char *get_connectstring(char *buf, int buf_sz) const;
  NdbMgmHandle get_mgmHandle() { return m_handle; };
  NdbMgmHandle* get_mgmHandlePtr() { return &m_handle; };
  void end_session(bool end) { m_end_session= end; };

  Uint32 get_configuration_nodeid() const;
private:
  BaseString errorString;
  enum ErrorType {
    CR_NO_ERROR = 0,
    CR_ERROR = 1
  };
  ErrorType latestErrorType;

  void setError(ErrorType, const char * errorMsg);
  void setError(ErrorType, BaseString err);

  bool m_end_session;
  Uint32 m_version;
  ndb_mgm_node_type m_node_type;
  NdbMgmHandle m_handle;
};

#endif


