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

#include <ndb_global.h>

#include <ConfigRetriever.hpp>
#include <NdbOut.hpp>

#include <SocketServer.hpp>
#include <NdbSleep.h>

#include <mgmapi.h>
#include <mgmapi_config_parameters.h>
#include <mgmapi_configuration.hpp>
#include <mgmapi_internal.h>
#include <ConfigValues.hpp>
#include <DnsCache.hpp>
#include <EventLogger.hpp>
#include "portlib/NdbTCP.h"

//****************************************************************************
//****************************************************************************

ConfigRetriever::ConfigRetriever(const char * _connect_string,
                                 int force_nodeid,
				 Uint32 version,
                                 ndb_mgm_node_type node_type,
				 const char * _bindaddress,
                                 int timeout_ms) :
  m_end_session(true),
  m_version(version),
  m_node_type(node_type)
{
  DBUG_ENTER("ConfigRetriever::ConfigRetriever");
  DBUG_PRINT("enter", ("connect_string: '%s', force_nodeid: %d",
                       _connect_string, force_nodeid));
  DBUG_PRINT("enter", ("version: %d, node_type: %d, bind: %s, timeout: %d",
                       version, node_type,_bindaddress, timeout_ms));

  m_handle= ndb_mgm_create_handle();

  if (m_handle == nullptr) {
    setError(CR_ERROR, "Unable to allocate mgm handle");
    DBUG_VOID_RETURN;
  }

  ndb_mgm_set_timeout(m_handle, timeout_ms);

  if (ndb_mgm_set_connectstring(m_handle, _connect_string))
  {
    BaseString tmp(ndb_mgm_get_latest_error_msg(m_handle));
    tmp.append(" : ");
    tmp.append(ndb_mgm_get_latest_error_desc(m_handle));
    setError(CR_ERROR, tmp.c_str());
    DBUG_VOID_RETURN;
  }

  if (force_nodeid &&
      ndb_mgm_set_configuration_nodeid(m_handle, force_nodeid))
  {
    setError(CR_ERROR, "Failed to set forced nodeid");
    DBUG_VOID_RETURN;
  }

  if (_bindaddress)
  {
    if (ndb_mgm_set_bindaddress(m_handle, _bindaddress))
    {
      setError(CR_ERROR, ndb_mgm_get_latest_error_desc(m_handle));
      DBUG_VOID_RETURN;
    }
  }
  resetError();
  DBUG_VOID_RETURN;
}

ConfigRetriever::~ConfigRetriever()
{
  DBUG_ENTER("ConfigRetriever::~ConfigRetriever");
  if (m_handle) {
    if (ndb_mgm_is_connected(m_handle)) {
      if(m_end_session)
        ndb_mgm_end_session(m_handle);
      ndb_mgm_disconnect(m_handle);
    }
    ndb_mgm_destroy_handle(&m_handle);
  }
  DBUG_VOID_RETURN;
}

Uint32 
ConfigRetriever::get_configuration_nodeid() const
{
  return ndb_mgm_get_configuration_nodeid(m_handle);
}

Uint32 ConfigRetriever::get_mgmd_port() const
{
  return ndb_mgm_get_connected_port(m_handle);
}

const char *ConfigRetriever::get_mgmd_host() const
{
  return ndb_mgm_get_connected_host(m_handle);
}

const char *ConfigRetriever::get_connectstring(char *buf, int buf_sz) const
{
  return ndb_mgm_get_connectstring(m_handle, buf, buf_sz);
}

//****************************************************************************
//****************************************************************************
 
int
ConfigRetriever::do_connect(int no_retries,
			    int retry_delay_in_seconds, int verbose)
{
  if (ndb_mgm_connect(m_handle, no_retries, retry_delay_in_seconds, verbose) == 0)
  {
    return 0;
  }
  else
  {
    const int err = ndb_mgm_get_latest_error(m_handle);
    if (err == NDB_MGM_ILLEGAL_CONNECT_STRING)
    {
      BaseString tmp(ndb_mgm_get_latest_error_msg(m_handle));
      tmp.append(" : ");
      tmp.append(ndb_mgm_get_latest_error_desc(m_handle));
      setError(CR_ERROR, tmp.c_str());
      return -2;
    }
    return -1;
  }

}

int
ConfigRetriever::disconnect()
{
  return ndb_mgm_disconnect(m_handle);
}

bool
ConfigRetriever::is_connected(void)
{
  return (ndb_mgm_is_connected(m_handle) != 0);
}

ndb_mgm::config_ptr
ConfigRetriever::getConfig(Uint32 nodeid)
{
  if (!m_handle)
    return {};

  /**
   * Communicate node id through ConfigRetriever, but restore it to old value
   * before returning.
   */  
  const Uint32 save_nodeid = get_configuration_nodeid();
  setNodeId(nodeid);

  ndb_mgm::config_ptr conf = getConfig(m_handle);

  setNodeId(save_nodeid);

  if (!conf)
    return {};
  
  if(!verifyConfig(conf.get(), nodeid))
    return {};
  
  return conf;
}

ndb_mgm::config_ptr
ConfigRetriever::getConfig(NdbMgmHandle mgm_handle)
{
  const int from_node = 0;
  ndb_mgm::config_ptr conf(
    ndb_mgm_get_configuration2(mgm_handle,
                               m_version,
                               m_node_type,
                               from_node));
  if (!conf)
  {
    BaseString tmp(ndb_mgm_get_latest_error_msg(mgm_handle));
    tmp.append(" : ");
    tmp.append(ndb_mgm_get_latest_error_desc(mgm_handle));
    setError(CR_ERROR, tmp.c_str());
  }
  return conf;
}

ndb_mgm::config_ptr
ConfigRetriever::getConfig(const char * filename)
{
  BaseString err;
  ndb_mgm::config_ptr conf = getConfig(filename, err);
  if (!conf)
  {
    setError(CR_ERROR, err);
  }
  return conf;
}

ndb_mgm::config_ptr
ConfigRetriever::getConfig(const char * filename, BaseString& err)
{
  if (access(filename, F_OK))
  {
    err.assfmt("Could not find file '%s'", filename);
    return {};
  }

  FILE * f = fopen(filename, "rb");
  if(f == nullptr)
  {
    err.assfmt("Failed to open file '%s'", filename);
    return {};
  }

  size_t read_sz;
  char read_buf[512];
  UtilBuffer config_buf;
  while ((read_sz = fread(read_buf, 1, sizeof(read_buf), f)) != 0)
  {
    if (config_buf.append(read_buf, read_sz) != 0)
    {
      err.assfmt("Out of memory when appending read data from file '%s'",
                 filename);
      fclose(f);
      return {};
    }
  }
  fclose(f);

  ConfigValuesFactory cvf;
  if(!cvf.unpack_buf(config_buf))
  {
    err.assfmt("Error while unpacking file '%s'", filename);
    return {};
  }
  return ndb_mgm::config_ptr(
      reinterpret_cast<ndb_mgm_configuration *>(cvf.getConfigValues()));
}

void
ConfigRetriever::setError(ErrorType et, const char * s){
  errorString.assign(s ? s : "");
  latestErrorType = et;
  DBUG_PRINT("info", ("latestErrorType: %u, '%s'",
                      latestErrorType, errorString.c_str()));
}

void
ConfigRetriever::setError(ErrorType et, BaseString err){
  setError(et, err.c_str());
}

void
ConfigRetriever::resetError(){
  setError(CR_NO_ERROR,nullptr);
}

int
ConfigRetriever::hasError()
{
  return latestErrorType != CR_NO_ERROR;
}

const char * 
ConfigRetriever::getErrorString(){
  return errorString.c_str();
}


bool
ConfigRetriever::verifyConfig(const ndb_mgm_configuration *conf,
                              Uint32 nodeid, bool validate_port)
{
  char buf[255];
  ndb_mgm_configuration_iterator it(conf, CFG_SECTION_NODE);

  if(it.find(CFG_NODE_ID, nodeid)){
    BaseString::snprintf(buf, 255, "Unable to find node with id: %d", nodeid);
    setError(CR_ERROR, buf);
    return false;
  }

  unsigned int _type;
  if(it.get(CFG_TYPE_OF_SECTION, &_type)){
    BaseString::snprintf(buf, 255, "Unable to get type of node(%d) from config",
                         CFG_TYPE_OF_SECTION);
    setError(CR_ERROR, buf);
    return false;
  }

  if(_type != (unsigned int)m_node_type){
    const char *alias_s, *alias_s2;
    const char *type_s = nullptr;
    const char *type_s2 = nullptr;
    alias_s=
      ndb_mgm_get_node_type_alias_string((enum ndb_mgm_node_type)m_node_type,
                                         &type_s);
    alias_s2=
      ndb_mgm_get_node_type_alias_string((enum ndb_mgm_node_type)_type,
                                         &type_s2);
    BaseString::snprintf(buf, 255,
                         "This node type %s(%s) and config "
			 "node type %s(%s) don't match for nodeid %d",
			 alias_s, type_s, alias_s2, type_s2, nodeid);
    setError(CR_ERROR, buf);
    return false;
  }

  const char *hostname;
  if (it.get(CFG_NODE_HOST, &hostname)) {
    BaseString::snprintf(buf, 255, "Unable to get hostname(%d) from config",
                         CFG_NODE_HOST);
    setError(CR_ERROR, buf);
    return false;
  }

  if (hostname && hostname[0] != 0 && !SocketServer::tryBind(0, hostname)) {
    BaseString::snprintf(buf, 255,
                         "The hostname this node should have according "
                         "to the configuration does not match a local "
                         "interface. Attempt to bind '%s' "
                         "failed with error: %d '%s'",
                         hostname, errno, strerror(errno));
    setError(CR_ERROR, buf);
    return false;
  }

  /**
   * Get Portnumber if Node type is management node and bind to
   * address "*:port" to check if "port" is free on all local
   * interfaces.
   *
   * Note: Default behaviour of management node is to listen on all
   * local interfaces.
   */
  if (_type == NODE_TYPE_MGM && validate_port) {
    Uint32 port = 0;
    if (it.get(CFG_MGM_PORT, &port)) {
      BaseString::snprintf(buf, 255,
                           "Unable to get Port of node(%d) from config",
                           CFG_TYPE_OF_SECTION);
      setError(CR_ERROR, buf);
      return false;
    }

    char msg[150];
    if (!SocketServer::tryBind(port, nullptr, msg, sizeof(msg))) {
      BaseString::snprintf(buf, 255,
                           "Mgmd node is started on port that is "
                           "already in use. Attempt to bind '*:%d' "
                           "failed with error: %s",
                           port, msg);
      setError(CR_ERROR, buf);
      return false;
    }
  }

  /**
   * Check hostnames
   */
  LocalDnsCache dnsCache;
  int ip_ver_preference = -1;
  ndb_mgm_configuration_iterator iter(conf, CFG_SECTION_CONNECTION);
  for(iter.first(); iter.valid(); iter.next()){

    Uint32 type = CONNECTION_TYPE_TCP + 1;
    if(iter.get(CFG_TYPE_OF_SECTION, &type)) continue;
    if(type != CONNECTION_TYPE_TCP) continue;

    Uint32 nodeId1, nodeId2;
    if(iter.get(CFG_CONNECTION_NODE_1, &nodeId1)) continue;
    if(iter.get(CFG_CONNECTION_NODE_2, &nodeId2)) continue;
    
    if(nodeId1 != nodeid && nodeId2 != nodeid) continue;

    Uint32 allow_unresolved = false;
    iter.get(CFG_CONNECTION_UNRES_HOSTS, & allow_unresolved);

    BaseString tmp;
    Uint32 conn_preferred_ip_version = 4;

    iter.get(CFG_CONNECTION_PREFER_IP_VER, &conn_preferred_ip_version);
    if(! (conn_preferred_ip_version == 6 || conn_preferred_ip_version == 4)) {
      tmp.assfmt("Invalid IP version: %d", conn_preferred_ip_version);
      setError(CR_ERROR, tmp);
      return false;
    }
    if(ip_ver_preference == -1) {              // Set the preference globally
      ip_ver_preference = conn_preferred_ip_version;
      NdbTCP_set_preferred_IP_version(ip_ver_preference);
    } else if(ip_ver_preference != (int) conn_preferred_ip_version) {
      setError(CR_ERROR, "All connections must prefer the same IP version");
      return false;
    }

    const char * name;
    struct in6_addr addr;
    if(!iter.get(CFG_CONNECTION_HOSTNAME_1, &name) && strlen(name)){
      if(dnsCache.getAddress(&addr, name) != 0){
	tmp.assfmt("Could not resolve hostname [node %d]: %s", nodeId1, name);
        if(! allow_unresolved) {
          setError(CR_ERROR, tmp.c_str());
          return false;
        }
        g_eventLogger->info("Warning: %s", tmp.c_str());
      }
    }

    if(!iter.get(CFG_CONNECTION_HOSTNAME_2, &name) && strlen(name)){
      if(dnsCache.getAddress(&addr, name) != 0){
        tmp.assfmt("Could not resolve hostname [node %d]: %s", nodeId2, name);
        if(! allow_unresolved) {
          setError(CR_ERROR, tmp.c_str());
          return false;
        }
        g_eventLogger->info("Warning: %s", tmp.c_str());
      }
    }
  }

  return true;
}

int
ConfigRetriever::setNodeId(Uint32 nodeid)
{
  return ndb_mgm_set_configuration_nodeid(m_handle, nodeid);
}

Uint32
ConfigRetriever::allocNodeId(int no_retries, int retry_delay_in_seconds,
                             int verbose, int& error)
{
  if (!m_handle)
  {
    setError(CR_ERROR, "management server handle not initialized");
    return 0;  // Error
  }

  while (1)
  {
    if (ndb_mgm_is_connected(m_handle) == 1 ||
        ndb_mgm_connect(m_handle, 0, 0, verbose) == 0)
    {
      int res =
        ndb_mgm_alloc_nodeid(m_handle, m_version, m_node_type,
                             no_retries == 0 /* only log last retry */);
      if (res >= 0)
        return (Uint32)res; // Success!!
    }

    error = ndb_mgm_get_latest_error(m_handle);
    if (no_retries == 0 ||                        /* No more retries */
        error == NDB_MGM_ALLOCID_CONFIG_MISMATCH) /* Fatal error */
    {
      break;
    }
    no_retries--;
    NdbSleep_SecSleep(retry_delay_in_seconds);
  }
  BaseString tmp;
  tmp.assfmt("%s: %s",
             ndb_mgm_get_latest_error_msg(m_handle),
             ndb_mgm_get_latest_error_desc(m_handle));
  setError(CR_ERROR, tmp.c_str());
  return 0; // Error
}


Uint32
ConfigRetriever::allocNodeId(int no_retries, int retry_delay_in_seconds)
{
  int error;
  return allocNodeId(no_retries, retry_delay_in_seconds, 0, error);
}
