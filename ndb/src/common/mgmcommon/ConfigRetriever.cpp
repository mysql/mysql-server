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
#include <ConfigValues.hpp>
#include <NdbHost.h>

//****************************************************************************
//****************************************************************************

ConfigRetriever::ConfigRetriever() {
  
  _localConfigFileName = NULL;
  m_defaultConnectString = NULL;


  errorString = 0;
  _localConfig = new LocalConfig();  
  m_connectString = NULL;
}

ConfigRetriever::~ConfigRetriever(){
  if(_localConfigFileName != 0)
    free(_localConfigFileName);
  
  if(m_defaultConnectString != 0)
    free(m_defaultConnectString);

  if(m_connectString != 0)
    free(m_connectString);

  if(errorString != 0)
    free(errorString);
  
  delete _localConfig;
}


//****************************************************************************
//****************************************************************************
 
int 
ConfigRetriever::init(bool onlyNodeId) {
  if (_localConfig->init(onlyNodeId, m_connectString, _localConfigFileName, m_defaultConnectString)) {
    return _ownNodeId = (*_localConfig)._ownNodeId;
  }

  setError(CR_ERROR, "error in retrieving contact info for mgmtsrvr");
  _localConfig->printError();
  _localConfig->printUsage();

  return -1;
}
  
//****************************************************************************
//****************************************************************************
//****************************************************************************
//****************************************************************************
struct ndb_mgm_configuration*
ConfigRetriever::getConfig(int verId, int nodeType) {

  int res = init();
  if (res == -1) {
    return 0;
  }

  if (_localConfig->items == 0){
    setError(CR_ERROR,"No Management Servers configured in local config file");
    return 0;
  }

  int retry = 1;
  int retry_max = 12;    // Max number of retry attempts
  int retry_interval= 5; // Seconds between each retry
  do {
    Uint32 type = CR_ERROR;
    for (int i = 0; i<_localConfig->items; i++){
      MgmtSrvrId * m = _localConfig->ids[i];
      struct ndb_mgm_configuration * p = 0;
      switch(m->type){
      case MgmId_TCP:
	p = getConfig(m->data.tcp.remoteHost, m->data.tcp.port, verId);
	break;
      case MgmId_File:
	p = getConfig(m->data.file.filename, verId);
	break;
      default:
	setError(CR_ERROR, "Unknown error type");
	break;
      }
      
      if (p != 0) {
	if(!verifyConfig(p, nodeType)){
	  free(p);
	  return 0;
	}
	return p;
      }
      if(latestErrorType == CR_RETRY)
	type = CR_RETRY;
    } // for
    
    if(type == CR_RETRY){
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
    
  } while (retry <= retry_max);
  
  return 0;
}

ndb_mgm_configuration *
ConfigRetriever::getConfig(const char * mgmhost, 
			   short port,
			   int versionId){
  
  NdbMgmHandle h;
  h = ndb_mgm_create_handle();
  if (h == NULL) {
    setError(CR_ERROR, "Unable to allocate mgm handle");
    return 0;
  }

  BaseString tmp;
  tmp.assfmt("%s:%d", mgmhost, port);
  if (ndb_mgm_connect(h, tmp.c_str()) != 0) {
    setError(CR_RETRY, ndb_mgm_get_latest_error_desc(h));
    ndb_mgm_destroy_handle(&h);
    return 0;
  }

  ndb_mgm_configuration * conf = ndb_mgm_get_configuration(h, versionId);
  if(conf == 0){
    setError(CR_ERROR, ndb_mgm_get_latest_error_desc(h));
  }

  ndb_mgm_disconnect(h);
  ndb_mgm_destroy_handle(&h);

  return conf;
#if 0  
  bool compatible;
  if (global_ndb_check)
    compatible = ndbCompatible_ndb_mgmt(versionId, version);
  else
    compatible = ndbCompatible_api_mgmt(versionId, version);

  if(!compatible){ //  if(version != versionId){
    NDB_CLOSE_SOCKET(sockfd);
    snprintf(err_buf, sizeof(err_buf), "Management Server: Invalid version. "
	     "Version from server: %d Own version: %d", version, versionId);
    setError(CR_ERROR, err_buf);
    return 0;
  }
#endif
}
	
ndb_mgm_configuration *
ConfigRetriever::getConfig(const char * filename, int versionId){

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
  if(errorString != 0){
    free(errorString);
  }
  if(s == 0)
    errorString = 0;
  else
    errorString = strdup(s);
  latestErrorType = et;
}


const char * 
ConfigRetriever::getErrorString(){
  return errorString;
}

void 
ConfigRetriever::setLocalConfigFileName(const char * localConfigFileName) {
  if(_localConfigFileName != 0)
    free(_localConfigFileName);
  if(localConfigFileName != 0)
    _localConfigFileName = strdup(localConfigFileName);
  else
    _localConfigFileName = 0;
}

void 
ConfigRetriever::setConnectString(const char * connectString) {
  if(m_connectString != 0)
    free(m_connectString);
  if (connectString != 0) {
    m_connectString = strdup(connectString);
  } else {
    m_connectString = 0;
  }
}

/**
 * @note Do not use!  Use the one above if possible. /elathal
 */
void 
ConfigRetriever::setDefaultConnectString(const char * defaultConnectString) {
  if(m_defaultConnectString != 0)
    free(m_defaultConnectString);
  if (defaultConnectString != 0) {
    m_defaultConnectString = strdup(defaultConnectString);
  } else {
    m_defaultConnectString = 0;
  }
}

bool
ConfigRetriever::verifyConfig(const struct ndb_mgm_configuration * conf, 
			      int type){
  char buf[255];
  ndb_mgm_configuration_iterator * it;
  it = ndb_mgm_create_configuration_iterator((struct ndb_mgm_configuration *)conf, CFG_SECTION_NODE);

  if(it == 0){
    snprintf(buf, 255, "Unable to create config iterator");
    setError(CR_ERROR, buf);
    return false;
    
  }
  NdbAutoPtr<ndb_mgm_configuration_iterator> ptr(it);
  
  if(ndb_mgm_find(it, CFG_NODE_ID, getOwnNodeId()) != 0){
    snprintf(buf, 255, "Unable to find node with id: %d", getOwnNodeId());
    setError(CR_ERROR, buf);
    return false;
  }
     
  const char * hostname;
  if(ndb_mgm_get_string_parameter(it, CFG_NODE_HOST, &hostname)){
    snprintf(buf, 255, "Unable to get hostname(%d) from config",CFG_NODE_HOST);
    setError(CR_ERROR, buf);
    return false;
  }

  char localhost[MAXHOSTNAMELEN];
  if(NdbHost_GetHostName(localhost) != 0){
    snprintf(buf, 255, "Unable to own hostname");
    setError(CR_ERROR, buf);
    return false;
  }

  do {
    if(strcasecmp(hostname, localhost) == 0)
      break;

    if(strcasecmp(hostname, "localhost") == 0)
      break;

    struct in_addr local, config;
    bool b1 = false, b2 = false, b3 = false;
    b1 = Ndb_getInAddr(&local, localhost) == 0;
    b2 = Ndb_getInAddr(&config, hostname) == 0;
    b3 = memcmp(&local, &config, sizeof(local)) == 0;

    if(b1 && b2 && b3)
      break;
    
    b1 = Ndb_getInAddr(&local, "localhost") == 0;
    b3 = memcmp(&local, &config, sizeof(local)) == 0;
    if(b1 && b2 && b3)
      break;
    
    snprintf(buf, 255, "Local hostname(%s) and config hostname(%s) dont match",
	     localhost, hostname);
    setError(CR_ERROR, buf);
    return false;
  } while(false);

  unsigned int _type;
  if(ndb_mgm_get_int_parameter(it, CFG_TYPE_OF_SECTION, &_type)){
    snprintf(buf, 255, "Unable to get type of node(%d) from config",
	     CFG_TYPE_OF_SECTION);
    setError(CR_ERROR, buf);
    return false;
  }
  
  if(_type != type){
    snprintf(buf, 255, "Supplied node type(%d) and config node type(%d) "
	     " don't match", type, _type);
    setError(CR_ERROR, buf);
    return false;
  }
  return true;
}
