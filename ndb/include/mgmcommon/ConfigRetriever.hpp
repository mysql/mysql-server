/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef ConfigRetriever_H
#define ConfigRetriever_H

#include <ndb_types.h>
#include <mgmapi.h>
#include <BaseString.hpp>
#include <LocalConfig.hpp>

/**
 * @class ConfigRetriever
 * @brief Used by nodes (DB, MGM, API) to get their config from MGM server. 
 */
class ConfigRetriever {
public:
  ConfigRetriever(Uint32 version, Uint32 nodeType);
  ~ConfigRetriever();

  /**
   * Read local config 
   * @return Own node id, -1 means fail
   */
  int init();

  int do_connect(int exit_on_connect_failure= false);
  
  /**
   * Get configuration for current (nodeId given in local config file) node.
   * 
   * Configuration is fetched from one MGM server configured in local config 
   * file.  The method loops over all the configured MGM servers and tries
   * to establish a connection.  This is repeated until a connection is 
   * established, so the function hangs until a connection is established.
   * 
   * @return ndb_mgm_configuration object if succeeded, 
   *         NULL if erroneous local config file or configuration error.
   */
  struct ndb_mgm_configuration * getConfig();
  
  const char * getErrorString();

  /**
   * Sets connectstring which can be used instead of local config file
   */
  void setConnectString(const char * connectString);

  /**
   * Sets name of local config file (usually not needed)
   */
  void setLocalConfigFileName(const char * connectString);

  /**
   * @return Node id of this node (as stated in local config or connectString)
   */
  Uint32 allocNodeId();

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
private:
  BaseString errorString;
  enum ErrorType {
    CR_ERROR = 0,
    CR_RETRY = 1
  };
  ErrorType latestErrorType;
  
  void setError(ErrorType, const char * errorMsg);
  
  BaseString            _localConfigFileName;
  struct LocalConfig    _localConfig;
  Uint32                _ownNodeId;
  
  BaseString            m_connectString;
  
  Uint32 m_version;
  Uint32 m_node_type;
  NdbMgmHandle m_handle;
};

#endif


