/*
   Copyright (c) 2003, 2023, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef ConfigRetriever_H
#define ConfigRetriever_H

#include <ndb_types.h>
#include <mgmapi.h>
#include "mgmcommon/NdbMgm.hpp"
#include <BaseString.hpp>

/**
 * @class ConfigRetriever
 * @brief Used by nodes (DB, MGM, API) to get their config from MGM server. 
 */
class ConfigRetriever {
public:
  ConfigRetriever(const char * _connect_string, int force_nodeid,
                  Uint32 version, ndb_mgm_node_type nodeType,
		  const char * _bind_address = nullptr,
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
   * @param nodeid   The nodeid of the node to fetch config for
   * 
   * @return ndb_mgm_config_unique_ptr which may be empty on failure
   */
  ndb_mgm::config_ptr getConfig(Uint32 nodeid);
  
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
  ndb_mgm::config_ptr getConfig(NdbMgmHandle handle);
  
  /**
   * Get config from file
   */
  static ndb_mgm::config_ptr getConfig(const char * file, BaseString& err);
  ndb_mgm::config_ptr getConfig(const char * file);

  /**
   * Verify config
   */
  bool verifyConfig(const ndb_mgm_configuration *, Uint32 nodeid,
                    bool validate_port = false);

  Uint32 get_mgmd_port() const;
  const char *get_mgmd_host() const;
  const char *get_connectstring(char *buf, int buf_sz) const;
  NdbMgmHandle get_mgmHandle() const { return m_handle; }
  NdbMgmHandle* get_mgmHandlePtr() { return &m_handle; }
  void end_session(bool end) { m_end_session= end; }

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


