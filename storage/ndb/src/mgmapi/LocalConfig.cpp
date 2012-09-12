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

#include "LocalConfig.hpp"
#include <NdbEnv.h>
#include <NdbConfig.h>
#include <NdbAutoPtr.hpp>
#include <util/NdbOut.hpp>

#define _STR_VALUE(x) #x
#define STR_VALUE(x) _STR_VALUE(x)

LocalConfig::LocalConfig(){
  error_line = 0; error_msg[0] = 0;
  _ownNodeId= 0;
  bind_address_port= 0;
}

bool
LocalConfig::init(const char *connectString,
		  const char *fileName)
{
  /** 
   * Escalation:
   *  1. Check connectString
   *  2. Check given filename
   *  3. Check environment variable NDB_CONNECTSTRING
   *  4. Check Ndb.cfg in NDB_HOME
   *  5. Check Ndb.cfg in cwd
   *  6. Check defaultConnectString
   */
  
  _ownNodeId= 0;

  //1. Check connectString
  if(connectString != 0 && connectString[0] != 0){
    if(readConnectString(connectString, "connect string")){
      if (ids.size())
	return true;
      // only nodeid given, continue to find hosts
    } else
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
    char *buf2= NdbConfig_NdbCfgName(1 /*true*/);
    NdbAutoPtr<char> tmp_aptr(buf2);
    if(readFile(buf2, fopenError))
      return true;
    if (!fopenError)
      return false;
  }

  //5. Check Ndb.cfg in cwd
  {
    bool fopenError;
    char *buf2= NdbConfig_NdbCfgName(0 /*false*/);
    NdbAutoPtr<char> tmp_aptr(buf2);
    if(readFile(buf2, fopenError))
      return true;
    if (!fopenError)
      return false;
  }

  //7. Use default connect string
  {
    if(readConnectString("host=localhost:" STR_VALUE(NDB_PORT),
                         "default connect string"))
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

const char *nodeIdTokens[] = {
  "OwnProcessId %i",
  "nodeid=%i",
  0
};

const char *hostNameTokens[] = {
  "host://%[^:]:%i",
  "host=%[^:]:%i",
  "mgmd=%[^:]:%i",
  "%[^:^=^ ]:%i",
  "%s %i",
  0
};

const char *bindAddressTokens[] = {
  "bind-address=%[^:]:%i",
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
        /* assign default bind_address if available */
        if (bind_address.length())
          mgmtSrvrId.bind_address.assign(bind_address);
	mgmtSrvrId.bind_address_port = bind_address_port;
	ids.push_back(mgmtSrvrId);
	return true;
      }
    }
    if (buf == tempString2)
      break;
    // try to add default port to see if it works
    BaseString::snprintf(tempString2, sizeof(tempString2),
                         "%s:%d", buf, NDB_PORT);
    buf= tempString2;
  } while(1);
  return false;
}

bool
LocalConfig::parseBindAddress(const char * buf)
{
  char tempString[1024];
  char tempString2[1024];
  int port;
  do
  {
    for(int i = 0; bindAddressTokens[i] != 0; i++)
    {
      if (sscanf(buf, bindAddressTokens[i], tempString, &port) == 2)
      {
        if (ids.size() == 0)
        {
          /* assign default bind_address */
          bind_address.assign(tempString);
          bind_address_port = port;
          return true;
        }
        /* override bind_address on latest mgmd */
        MgmtSrvrId &mgmtSrvrId= ids[ids.size()-1];
	mgmtSrvrId.bind_address.assign(tempString);
	mgmtSrvrId.bind_address_port = port;
	return true;
      }
    }
    if (buf == tempString2)
      break;
    // try to add port 0 to see if it works
    BaseString::snprintf(tempString2, sizeof(tempString2),"%s:0", buf);
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

  for (char *tok = strtok_r(copy,";,",&for_strtok); tok != 0;
       tok = strtok_r(NULL, ";,", &for_strtok)) {
    if (tok[0] == '#') continue;

    if (!_ownNodeId) // only one nodeid definition allowed
      if (parseNodeId(tok))
	continue;
    if (parseHostName(tok))
      continue;
    if (parseBindAddress(tok))
      continue;
    if (parseFileName(tok))
      continue;
    
    err.assfmt("Unexpected entry: \"%s\"", tok);
    return false;
  }
  bind_address_port= 0;
  bind_address.assign("");
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
    err2.assfmt("Reading %s \"%s\": %s", info, connectString, err.c_str());
    setError(0,err2.c_str());
  }
  return return_value;
}

char *
LocalConfig::makeConnectString(char *buf, int sz)
{
  int p= BaseString::snprintf(buf,sz,"nodeid=%d", _ownNodeId);
  if (p < sz && bind_address.length())
  {
    int new_p= p+BaseString::snprintf(buf+p,sz-p,",bind-address=%s:%d",
                                      bind_address.c_str(), bind_address_port);
    if (new_p < sz)
      p= new_p;
    else 
      buf[p]= 0;
  }
  if (p < sz)
    for (unsigned i = 0; i < ids.size(); i++)
    {
      if (ids[i].type != MgmId_TCP)
	continue;
      int new_p= p+BaseString::snprintf(buf+p,sz-p,",%s:%d",
					ids[i].name.c_str(), ids[i].port);
      if (new_p < sz)
	p= new_p;
      else 
      {
	buf[p]= 0;
	break;
      }
      if (!bind_address.length() && ids[i].bind_address.length())
      {
        new_p= p+BaseString::snprintf(buf+p,sz-p,",bind-address=%s:%d",
                                      ids[i].bind_address.c_str(), ids[i].bind_address_port);
        if (new_p < sz)
          p= new_p;
        else 
        {
            buf[p]= 0;
            break;
        }
      }
    }
  buf[sz-1]=0;
  return buf;
}

template class Vector<MgmtSrvrId>;
