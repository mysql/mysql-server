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

#include <ndb_global.h>
#include <ndb_version.h>

#include <ConfigRetriever.hpp>
#include <SocketServer.hpp>

#include "LocalConfig.hpp"
#include <NdbSleep.h>
#include <NdbOut.hpp>

#include <NdbTCP.h>
#include <NdbEnv.h>
#include "MgmtErrorReporter.hpp"

#include <uucode.h>
#include <Properties.hpp>

#include <socket_io.h>
#include <NdbConfig.h>

#include <NdbAutoPtr.hpp>
 
#include <mgmapi.h>
#include <mgmapi_config_parameters.h>
#include <mgmapi_configuration.hpp>
#include <ConfigValues.hpp>
#include <NdbHost.h>

//****************************************************************************
//****************************************************************************

ConfigRetriever::ConfigRetriever(LocalConfig &local_config,
				 Uint32 version, Uint32 node_type)
  : _localConfig(local_config)
{
  m_handle= 0;
  m_version = version;
  m_node_type = node_type;
}

ConfigRetriever::~ConfigRetriever(){

  if (m_handle) {
    ndb_mgm_disconnect(m_handle);
    ndb_mgm_destroy_handle(&m_handle);
  }
}


//****************************************************************************
//****************************************************************************
 
int 
ConfigRetriever::init() {
  return _ownNodeId = _localConfig._ownNodeId;
}

int
ConfigRetriever::do_connect(int exit_on_connect_failure){

  if(!m_handle)
    m_handle= ndb_mgm_create_handle();

  if (m_handle == 0) {
    setError(CR_ERROR, "Unable to allocate mgm handle");
    return -1;
  }

  int retry = 1;
  int retry_max = 12;    // Max number of retry attempts
  int retry_interval= 5; // Seconds between each retry
  while(retry < retry_max){
    Uint32 type = CR_ERROR;
    BaseString tmp;
    for (unsigned int i = 0; i<_localConfig.ids.size(); i++){
      MgmtSrvrId * m = &_localConfig.ids[i];
      switch(m->type){
      case MgmId_TCP:
	tmp.assfmt("%s:%d", m->name.c_str(), m->port);
	if (ndb_mgm_connect(m_handle, tmp.c_str()) == 0) {
	  return 0;
	}
	setError(CR_RETRY, ndb_mgm_get_latest_error_desc(m_handle));
      case MgmId_File:
	break;
      }
    }
    if (exit_on_connect_failure)
      return 1;
    if(latestErrorType == CR_RETRY){
      REPORT_WARNING("Failed to retrieve cluster configuration");
      ndbout << "(Cause of failure: " << getErrorString() << ")" << endl;
      ndbout << "Attempt " << retry << " of " << retry_max << ". " 
	     << "Trying again in "<< retry_interval <<" seconds..." 
	     << endl << endl;
      NdbSleep_SecSleep(retry_interval);
    } else {
      break;
    }
    retry++;
  }
  
  ndb_mgm_destroy_handle(&m_handle);
  m_handle= 0;
  return -1;
}

//****************************************************************************
//****************************************************************************
//****************************************************************************
//****************************************************************************
struct ndb_mgm_configuration*
ConfigRetriever::getConfig() {

  struct ndb_mgm_configuration * p = 0;

  if(m_handle != 0){
    p = getConfig(m_handle);
  } else {
    for (unsigned int i = 0; i<_localConfig.ids.size(); i++){
      MgmtSrvrId * m = &_localConfig.ids[i];
      switch(m->type){
      case MgmId_File:
	p = getConfig(m->name.c_str());
	break;
      case MgmId_TCP:
	break;
      }
      if(p)
	break;
    }
  }
  if(p == 0)
    return 0;
  
  if(!verifyConfig(p, _ownNodeId)){
    free(p);
    p= 0;
  }
  
  return p;
}

ndb_mgm_configuration *
ConfigRetriever::getConfig(NdbMgmHandle m_handle){
  
  ndb_mgm_configuration * conf = ndb_mgm_get_configuration(m_handle,m_version);
  if(conf == 0){
    setError(CR_ERROR, ndb_mgm_get_latest_error_desc(m_handle));
    return 0;
  }
  
  return conf;
}
	
ndb_mgm_configuration *
ConfigRetriever::getConfig(const char * filename){

  struct stat sbuf;
  const int res = stat(filename, &sbuf);
  if(res != 0){
    char buf[255];
    snprintf(buf, sizeof(buf), "Could not find file: \"%s\"", filename);
    setError(CR_ERROR, buf);
    return 0;
  }
  const Uint32 bytes = sbuf.st_size;
  
  Uint32 * buf2 = new Uint32[bytes/4+1];
  
  FILE * f = fopen(filename, "rb");
  if(f == 0){
    setError(CR_ERROR, "Failed to open file");
    delete []buf2;
    return 0;
  }
  Uint32 sz = fread(buf2, 1, bytes, f);
  fclose(f);
  if(sz != bytes){
    setError(CR_ERROR, "Failed to read file");
    delete []buf2;
    return 0;
  }
  
  ConfigValuesFactory cvf;
  if(!cvf.unpack(buf2, bytes)){
    char buf[255];
    snprintf(buf, sizeof(buf), "Error while unpacking"); 
    setError(CR_ERROR, buf);
    delete []buf2;
    return 0;
  }
  delete [] buf2;

  return (ndb_mgm_configuration*)cvf.m_cfg;
}			   

void
ConfigRetriever::setError(ErrorType et, const char * s){
  errorString.assign(s ? s : "");
  latestErrorType = et;
}


const char * 
ConfigRetriever::getErrorString(){
  return errorString.c_str();
}

bool
ConfigRetriever::verifyConfig(const struct ndb_mgm_configuration * conf, Uint32 nodeid){

  char buf[255];
  ndb_mgm_configuration_iterator * it;
  it = ndb_mgm_create_configuration_iterator((struct ndb_mgm_configuration *)conf, CFG_SECTION_NODE);

  if(it == 0){
    snprintf(buf, 255, "Unable to create config iterator");
    setError(CR_ERROR, buf);
    return false;
    
  }
  NdbAutoPtr<ndb_mgm_configuration_iterator> ptr(it);
  
  if(ndb_mgm_find(it, CFG_NODE_ID, nodeid) != 0){
    snprintf(buf, 255, "Unable to find node with id: %d", nodeid);
    setError(CR_ERROR, buf);
    return false;
  }
     
  const char * hostname;
  if(ndb_mgm_get_string_parameter(it, CFG_NODE_HOST, &hostname)){
    snprintf(buf, 255, "Unable to get hostname(%d) from config",CFG_NODE_HOST);
    setError(CR_ERROR, buf);
    return false;
  }

  const char * datadir;
  if(!ndb_mgm_get_string_parameter(it, CFG_NODE_DATADIR, &datadir)){
    NdbConfig_SetPath(datadir);
  }

  if (hostname && hostname[0] != 0 &&
      !SocketServer::tryBind(0,hostname)) {
    snprintf(buf, 255, "Config hostname(%s) don't match a local interface,"
	     " tried to bind, error = %d - %s",
	     hostname, errno, strerror(errno));
    setError(CR_ERROR, buf);
    return false;
  }

  unsigned int _type;
  if(ndb_mgm_get_int_parameter(it, CFG_TYPE_OF_SECTION, &_type)){
    snprintf(buf, 255, "Unable to get type of node(%d) from config",
	     CFG_TYPE_OF_SECTION);
    setError(CR_ERROR, buf);
    return false;
  }
  
  if(_type != m_node_type){
    snprintf(buf, 255, "Supplied node type(%d) and config node type(%d) "
	     " don't match", m_node_type, _type);
    setError(CR_ERROR, buf);
    return false;
  }

  /**
   * Check hostnames
   */
  ndb_mgm_configuration_iterator iter(* conf, CFG_SECTION_CONNECTION);
  for(iter.first(); iter.valid(); iter.next()){

    Uint32 type = CONNECTION_TYPE_TCP + 1;
    if(iter.get(CFG_TYPE_OF_SECTION, &type)) continue;
    if(type != CONNECTION_TYPE_TCP) continue;
    
    Uint32 nodeId1, nodeId2, remoteNodeId;
    if(iter.get(CFG_CONNECTION_NODE_1, &nodeId1)) continue;
    if(iter.get(CFG_CONNECTION_NODE_2, &nodeId2)) continue;
    
    if(nodeId1 != nodeid && nodeId2 != nodeid) continue;
    remoteNodeId = (nodeid == nodeId1 ? nodeId2 : nodeId1);

    const char * name;
    struct in_addr addr;
    BaseString tmp;
    if(!iter.get(CFG_CONNECTION_HOSTNAME_1, &name) && strlen(name)){
      if(Ndb_getInAddr(&addr, name) != 0){
	tmp.assfmt("Unable to lookup/illegal hostname %s, "
		   "connection from node %d to node %d",
		   name, nodeid, remoteNodeId);
	setError(CR_ERROR, tmp.c_str());
	return false;
      }
    }

    if(!iter.get(CFG_CONNECTION_HOSTNAME_2, &name) && strlen(name)){
      if(Ndb_getInAddr(&addr, name) != 0){
	tmp.assfmt("Unable to lookup/illegal hostname %s, "
		   "connection from node %d to node %d",
		   name, nodeid, remoteNodeId);
	setError(CR_ERROR, tmp.c_str());
	return false;
      }
    }
  }
  return true;
}

Uint32
ConfigRetriever::allocNodeId(){
  unsigned nodeid= _ownNodeId;
  
  if(m_handle != 0){
    int res= ndb_mgm_alloc_nodeid(m_handle, m_version, &nodeid, m_node_type);
    if(res != 0) {
      setError(CR_ERROR, ndb_mgm_get_latest_error_desc(m_handle));
      return 0;
    }
  }
  
  return _ownNodeId= nodeid;
}
