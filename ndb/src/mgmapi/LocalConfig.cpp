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

#include "LocalConfig.hpp"
#include <NdbEnv.h>
#include <NdbConfig.h>
#include <NdbAutoPtr.hpp>
#include <NdbMem.h>

LocalConfig::LocalConfig(){
  error_line = 0; error_msg[0] = 0;
  _ownNodeId= 0;
}

bool
LocalConfig::init(const char *connectString,
		  const char *fileName) {
  /** 
   * Escalation:
   *  1. Check connectString
   *  2. Check given filename
   *  3. Check environment variable NDB_CONNECTSTRING
   *  4. Check Ndb.cfg in NDB_HOME
   *  5. Check Ndb.cfg in cwd
   *  6. Check defaultConnectString
   */
  
  //1. Check connectString
  if(connectString != 0 && connectString[0] != 0){
    if(readConnectString(connectString, "connect string")){
      return true;
    }
    return false;
  }

  //2. Check given filename
  if (fileName && strlen(fileName) > 0) {
    bool fopenError;
    if(readFile(fileName, fopenError)){
      return true;
    }
    return false;
  }

  //3. Check environment variable
  char buf[255];  
  if(NdbEnv_GetEnv("NDB_CONNECTSTRING", buf, sizeof(buf)) &&
     strlen(buf) != 0){
    if(readConnectString(buf, "NDB_CONNECTSTRING")){
      return true;
    }
    return false;
  }
  
  //4. Check Ndb.cfg in NDB_HOME
  {
    bool fopenError;
    char *buf= NdbConfig_NdbCfgName(1 /*true*/);
    NdbAutoPtr<char> tmp_aptr(buf);
    if(readFile(buf, fopenError))
      return true;
    if (!fopenError)
      return false;
  }

  //5. Check Ndb.cfg in cwd
  {
    bool fopenError;
    char *buf= NdbConfig_NdbCfgName(0 /*false*/);
    NdbAutoPtr<char> tmp_aptr(buf);
    if(readFile(buf, fopenError))
      return true;
    if (!fopenError)
      return false;
  }

  //7. Check
  {
    char buf[256];
    BaseString::snprintf(buf, sizeof(buf), "host=localhost:%s", NDB_PORT);
    if(readConnectString(buf, "default connect string"))
      return true;
  }

  setError(0, "");

  return false;
}

LocalConfig::~LocalConfig(){
}
  
void LocalConfig::setError(int lineNumber, const char * _msg) {
  error_line = lineNumber;
  strncpy(error_msg, _msg, sizeof(error_msg));
}

void LocalConfig::printError() const {
  ndbout << "Configuration error" << endl;
  if (error_line)
    ndbout << "Line: "<< error_line << ", ";
  ndbout << error_msg << endl << endl;
}

void LocalConfig::printUsage() const {
  ndbout << "This node needs information on how to connect"<<endl
	 << "to the NDB Management Server."<<endl
	 << "The information can be supplied in one of the following ways:"
	 << endl;
    
  ndbout << "1. Put a Ndb.cfg file in the directory where you start"<<endl 
	 << "   the node. "<< endl
	 << "   Ex: Ndb.cfg" << endl
	 << "   | host=localhost:"<<NDB_PORT<<endl;
    
  ndbout << "2. Use the environment variable NDB_CONNECTSTRING to "<<endl
	 << "   provide this information." <<endl
	 << "   Ex: " << endl
	 << "   >export NDB_CONNECTSTRING=\"host=localhost:"<<NDB_PORT<<"\""
	 <<endl<<endl;
}
  
const char *nodeIdTokens[] = {
  "OwnProcessId %i",
  "nodeid=%i",
  0
};

const char *hostNameTokens[] = {
  "host://%[^:]:%i",
  "host=%[^:]:%i",
  "%[^:^=^ ]:%i",
  "%s %i",
  0
};

const char *fileNameTokens[] = {
  "file://%s",
  "file=%s",
  0
};

bool
LocalConfig::parseNodeId(const char * buf){
  for(int i = 0; nodeIdTokens[i] != 0; i++)
    if (sscanf(buf, nodeIdTokens[i], &_ownNodeId) == 1)
      return true;
  return false;
}

bool
LocalConfig::parseHostName(const char * buf){
  char tempString[1024];
  char tempString2[1024];
  int port;
  do {
    for(int i = 0; hostNameTokens[i] != 0; i++) {
      if (sscanf(buf, hostNameTokens[i], tempString, &port) == 2) {
	MgmtSrvrId mgmtSrvrId;
	mgmtSrvrId.type = MgmId_TCP;
	mgmtSrvrId.name.assign(tempString);
	mgmtSrvrId.port = port;
	ids.push_back(mgmtSrvrId);
	return true;
      }
    }
    if (buf == tempString2)
      break;
    // try to add default port to see if it works
    snprintf(tempString2, sizeof(tempString2),"%s:%s", buf, NDB_PORT);
    buf= tempString2;
  } while(1);
  return false;
}

bool
LocalConfig::parseFileName(const char * buf){
  char tempString[1024];
  for(int i = 0; fileNameTokens[i] != 0; i++) {
    if (sscanf(buf, fileNameTokens[i], tempString) == 1) {
      MgmtSrvrId mgmtSrvrId;
      mgmtSrvrId.type = MgmId_File;
      mgmtSrvrId.name.assign(tempString);
      ids.push_back(mgmtSrvrId);
      return true;
    }
  }
  return false;
}

bool
LocalConfig::parseString(const char * connectString, BaseString &err){
  char * for_strtok;
  char * copy = strdup(connectString);
  NdbAutoPtr<char> tmp_aptr(copy);

  bool b_nodeId = false;
  bool found_other = false;

  for (char *tok = strtok_r(copy,";,",&for_strtok); tok != 0;
       tok = strtok_r(NULL, ";,", &for_strtok)) {
    if (tok[0] == '#') continue;

    if (!b_nodeId) // only one nodeid definition allowed
      if (b_nodeId = parseNodeId(tok))
	continue;
    if (found_other = parseHostName(tok))
      continue;
    if (found_other = parseFileName(tok))
      continue;
    
    err.assfmt("Unexpected entry: \"%s\"", tok);
    return false;
  }

  if (!found_other) {
    err.appfmt("Missing host/file name extry in \"%s\"", connectString);
    return false;
  }

  return true;
}

bool LocalConfig::readFile(const char * filename, bool &fopenError)
{
  char line[1024];
  
  fopenError = false;
  
  FILE * file = fopen(filename, "r");
  if(file == 0){
    BaseString::snprintf(line, sizeof(line),
	     "Unable to open local config file: %s", filename);
    setError(0, line);
    fopenError = true;
    return false;
  }

  BaseString theString;

  while(fgets(line, sizeof(line), file)){
    BaseString tmp(line);
    tmp.trim(" \t\n\r");
    if(tmp.length() > 0 && tmp.c_str()[0] != '#'){
      theString.append(tmp);
      break;
    }
  }
  while (fgets(line, sizeof(line), file)) {
    BaseString tmp(line);
    tmp.trim(" \t\n\r");
    if(tmp.length() > 0 && tmp.c_str()[0] != '#'){
      theString.append(";");
      theString.append(tmp);
    }
  }
  
  BaseString err;
  bool return_value = parseString(theString.c_str(), err);

  if (!return_value) {
    BaseString tmp;
    tmp.assfmt("Reading %s: %s", filename, err.c_str());
    setError(0, tmp.c_str());
  }

  fclose(file);
  return return_value;
}

bool
LocalConfig::readConnectString(const char * connectString,
			       const char * info){
  BaseString err;
  bool return_value = parseString(connectString, err);
  if (!return_value) {
    BaseString err2;
    err2.assfmt("Reading %d \"%s\": %s", info, connectString, err.c_str());
    setError(0,err2.c_str());
  }
  return return_value;
}

char *
LocalConfig::makeConnectString(char *buf, int sz)
{
  int p= BaseString::snprintf(buf,sz,"nodeid=%d", _ownNodeId);
  for (int i = 0; (i < ids.size()) && (sz-p > 0); i++)
  {
    if (ids[i].type != MgmId_TCP)
      continue;
    p+=BaseString::snprintf(buf+p,sz-p,",%s:%d",
			    ids[i].name.c_str(), ids[i].port);
  }
  buf[sz-1]=0;
  return buf;
}

template class Vector<MgmtSrvrId>;
