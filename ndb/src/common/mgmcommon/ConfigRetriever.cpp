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

Properties * 
ConfigRetriever::getConfig(const char * nodeType, int versionId) {
  Properties * p = getConfig(versionId);

  if (p == 0) {
    char err_buf[255];
    snprintf(err_buf, sizeof(err_buf),
	     "No configuration retrieved for this %s node ", nodeType);
    setError(CR_ERROR, err_buf);
    return 0;
  }

  const Uint32 nodeId = _ownNodeId;

  if (strcmp(nodeType, "DB") == 0) {
    if (!verifyProperties("DB", p, nodeId, versionId)) return 0;
  } else if (strcmp(nodeType, "API") == 0) {
    if (!verifyProperties("API", p, nodeId, versionId)) return 0;
  } else if (strcmp(nodeType, "REP") == 0) {
    if (!verifyProperties("REP", p, nodeId, versionId)) return 0;
  } else if (strcmp(nodeType, "MGM") == 0) {
    if (!verifyProperties("MGM", p, nodeId, versionId)) return 0;    
  } else {
    return 0;
  }
   
  return p;
}


//****************************************************************************
//****************************************************************************
Properties *
ConfigRetriever::getConfig(int verId) {

  int res = init();
  if (res == -1) {
    return 0;
  }

  if (_localConfig->items == 0){
    setError(CR_ERROR, "No Management Servers configured in local config file");
    return 0;
  }

  int retry = 1;
  int retry_max = 12;    // Max number of retry attempts
  int retry_interval= 5; // Seconds between each retry
  do {
    Uint32 type = CR_ERROR;
    for (int i = 0; i<_localConfig->items; i++){
      MgmtSrvrId * m = _localConfig->ids[i];
      Properties * p = 0;
      const Uint32 nodeId = _ownNodeId;
      switch(m->type){
      case MgmId_TCP:
	p = getConfig(m->data.tcp.remoteHost, m->data.tcp.port, nodeId, verId);
	break;
      case MgmId_File:
	p = getConfig(m->data.file.filename, nodeId, verId);
	break;
      default:
	setError(CR_ERROR, "Unknown error type");
	break;
      }
      
      if (p != 0) {
	return p;
      }
      if(latestErrorType == CR_RETRY)
	type = CR_RETRY;
    } // for
    
    if(type == CR_RETRY){
      REPORT_WARNING("Failed to retrieve cluster configuration");
      ndbout << "(Cause of failure: " << getErrorString() << ")" << endl;
      ndbout << "Attempt " << retry << " of " << retry_max << ". " 
	     << "Trying again in "<<retry_interval<<" seconds..." << endl << endl;
      NdbSleep_SecSleep(retry_interval);
    } else {
      break;
    }
    retry++;

  } while (retry <= retry_max);
  
  return 0;
}

int global_ndb_check = 0; // set to one in ndb main;
Properties *
ConfigRetriever::getConfig(const char * mgmhost, 
			   unsigned int port,
			   Uint32 nodeId,
			   int versionId){
  const int socketTimeout = 10000;
  int result;
  const NDB_SOCKET_TYPE sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd == NDB_INVALID_SOCKET) {
    setError(CR_RETRY, "Could not create socket to Management Server");
    return 0;
  }

  char err_buf[255];    
  struct sockaddr_in servaddr;
  memset(&servaddr, 0, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_port = htons(port);
  // Convert ip address presentation format to numeric format
  result = Ndb_getInAddr(&servaddr.sin_addr, mgmhost);
  if (result != 0) {
    snprintf(err_buf, sizeof(err_buf),
	     "Name lookup failed: host \"%s\"", mgmhost);
    setError(CR_ERROR, err_buf);
    return 0;
  }

  result = connect(sockfd, (struct sockaddr*) &servaddr, sizeof(servaddr));
  if (result == -1) {
    snprintf(err_buf, sizeof(err_buf),
	     "Failed to connect to \"%s:%d\"", mgmhost, port);
    setError(CR_RETRY, err_buf);
    NDB_CLOSE_SOCKET(sockfd);
    return 0;
  }
  
  if(println_socket(sockfd, 1000, "GET CONFIG %d %d" ,
		    versionId, nodeId) != 0){
    NDB_CLOSE_SOCKET(sockfd);
    setError(CR_ERROR, "IO error, write");
    return 0;
  }
    
  char buf[255];
  {
    const int tmp = readln_socket(sockfd, socketTimeout, buf, 255);
    if(tmp == -1){
      NDB_CLOSE_SOCKET(sockfd);
      setError(CR_ERROR, "IO error, read");
      return 0;
    }

    if(tmp == 0){
      snprintf(err_buf, 256, 
	       "IO error, failed request: " 
	       "GET CONFIG %d %d", versionId, nodeId);
      NDB_CLOSE_SOCKET(sockfd);
      setError(CR_ERROR, err_buf);
      return 0;
    }
  }

  int status, version, node, bytes, bytesUU;
  if(sscanf(buf, "GET CONFIG %d %d %d %d %d", 
	    &status, &version, &node, &bytes, &bytesUU) != 5){
    NDB_CLOSE_SOCKET(sockfd);
    snprintf(err_buf, sizeof(err_buf),
	     "Invalid response: %s", buf);
    setError(CR_ERROR, err_buf);
    return 0;
  }
  
  if(status != 0){
    NDB_CLOSE_SOCKET(sockfd);
    if (status == 1){
      snprintf(err_buf, sizeof(err_buf),
	       "Management Server: Requested version id is invalid");
    } else if (status == 2){
      snprintf(err_buf, sizeof(err_buf),
	       "Management Server: Node with id %d has not been specified",
	       nodeId);
    } else if (status == 3){
      snprintf(err_buf, sizeof(err_buf), "Management Server: Internal error");
    } else {
      snprintf(err_buf, sizeof(err_buf),
	       "Management Server returned unknown error: %d", status);
    }
    setError(CR_ERROR, err_buf);
    return 0;
  }
  
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
  
  if(node != (int)nodeId){
    NDB_CLOSE_SOCKET(sockfd);
    snprintf(err_buf, sizeof(err_buf), "Management Server: Invalid node id. "
	     "Node id from server: %d Own node id: %d", node, nodeId);
    setError(CR_ERROR, err_buf);
    return 0;
  }

  if(readln_socket(sockfd, socketTimeout, buf, 255) == -1){
    NDB_CLOSE_SOCKET(sockfd);
    setError(CR_ERROR, "IO error, read");
    return 0;
  }

  if(strncmp("begin", buf, strlen("begin")) != 0){
    NDB_CLOSE_SOCKET(sockfd);
    snprintf(err_buf, sizeof(err_buf),
	     "Invalid response: %s", buf);
    setError(CR_ERROR, err_buf);
    return 0;
  }

  char* bufUU = new char[bytesUU];
  int read = 0;
  int start = 0;
  do {
    if((read = read_socket(sockfd, socketTimeout, &bufUU[start], bytesUU-start)) == -1){
      delete[] bufUU;
      NDB_CLOSE_SOCKET(sockfd);
      setError(CR_ERROR, "IO error, read(bufUU)");
      return 0;
    }
    start += read;
  } while(start < bytesUU);

  Uint32 * buf2 = new Uint32[bytes/4+1]; // Properties byte size
  char * dst = (char *)buf2;
  int sz = 0;
  start = 0;

  for (int i = 0; i < bytesUU; i++) {
    if (bufUU[i] == '\n') {
      bufUU[i] = 0;
      if (bufUU[i-1] == '\r') {
	bufUU[i-1] = 0;
      }
      sz = uudecode_mem(dst, bytes, &bufUU[start]);
      dst += sz;
      start = i + 1; // Next row
    }
  }

  delete[] bufUU;

  if(sz < 0){
    delete []buf2;
    NDB_CLOSE_SOCKET(sockfd);
    setError(CR_ERROR, "IO error, sz < 0");
    return 0;
  }
  
  Properties * p = new Properties();
  if(!p->unpack(buf2, bytes+4)){
    snprintf(buf, sizeof(buf), "Error while unpacking %d,%d", 
	     p->getPropertiesErrno(),
	     p->getOSErrno());
    setError(CR_ERROR, buf);
    delete []buf2;
    delete p;
    return 0;
  }
  delete []buf2;
  
  NDB_CLOSE_SOCKET(sockfd);

  return p;
  
}
	
Properties *
ConfigRetriever::getConfig(const char * filename,
			   Uint32 nodeId,
			   int versionId){

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
  
  Properties * p = new Properties();
  if(!p->unpack(buf2, bytes+4)){
    char buf[255];
    snprintf(buf, sizeof(buf), "Error while unpacking %d,%d", 
	    p->getPropertiesErrno(),
	    p->getOSErrno());
    setError(CR_ERROR, buf);
    delete []buf2;
    delete p;
    return 0;
  }
  delete [] buf2;

  return p;
}			   

bool
ConfigRetriever::verifyProperties(const char* nodeType, Properties * p, 
				  Uint32 nodeId, int versionId){

  Uint32             t = 0;
  const Properties   *tmp;
  const char         *type;

  if (p == 0) return false;

  bool compatible = false;
  if (p->get("Version", &t))
    if (global_ndb_check)
      compatible = ndbCompatible_ndb_mgmt(versionId, t);
    else
      compatible = ndbCompatible_api_mgmt(versionId, t);

  if(!compatible){ // if(!p->get("Version", &t) || versionId != (int)t){
    setError(CR_ERROR, "Invalid configuration version");
    delete p;
    return false;
  }

  if(!p->get("LocalNodeId", &t) || nodeId != t){
    setError(CR_ERROR, "Invalid node identity in configuration");
    delete p;
    return false;
  }

  if(!p->get("Node", nodeId, &tmp)){
    setError(CR_ERROR, "Internal error while processing configuration");
    ndbout_c("nodeId = %d", nodeId);
    p->print();
    delete p;
    return false;
  }

  if(!tmp->get("Type", &type) || strcmp(type, nodeType)) {
    if (!(!strcmp(type, "REP") && !strcmp(nodeType, "API"))) {
      char buf[1024];
      snprintf(buf, sizeof(buf),
	       "Configuration error: Node with id %d is not of type %s.\n"
	       "Check local config file: %s", nodeId, nodeType, 
	       _localConfigFileName);
      setError(CR_ERROR, buf);
      return false;
    }
  }
  
  return true;
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
