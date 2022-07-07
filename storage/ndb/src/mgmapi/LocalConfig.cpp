/*
   Copyright (c) 2003, 2022, Oracle and/or its affiliates.

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

#include "LocalConfig.hpp"
#include <NdbEnv.h>
#include <NdbConfig.h>
#include <NdbAutoPtr.hpp>
#include <util/NdbOut.hpp>
#include <NdbTCP.h>
#include "util/cstrbuf.h"

#include <stdlib.h>
#include <string.h>

#define MAX_PORT_NO 65535

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
  if(connectString != nullptr && connectString[0] != 0){
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
  
void LocalConfig::setError(int lineNumber, const char * _msg)
{
  error_line = lineNumber;
  if (cstrbuf_copy(error_msg, _msg) == 1)
  {
    // ignore truncated error message
  }
}

bool LocalConfig::parseNodeId(const char *value)
{
  if (_ownNodeId != 0)
    return false; // already set

  char* endp = nullptr;
  long v = strtol(value, &endp, 0);
  if (endp == nullptr || *endp != 0 || v < 0 || MAX_PORT_NO < v)
    return false; // bad value

  _ownNodeId = v;
  return true;
}

bool LocalConfig::parseHostName(const char *value)
{
  char host[NDB_DNS_HOST_NAME_LENGTH + 1];
  char serv[NDB_IANA_SERVICE_NAME_LENGTH + 1];

  int r = Ndb_split_string_address_port(value, host, sizeof(host), serv,
                                        sizeof(serv));
  if (r != 0)
  {
    return false;
  }

  int port = 1186;
  if (serv[0] != 0)
  {
    char * endp = nullptr;
    long v = strtol(serv, &endp, 0);
    if (endp == nullptr || *endp != 0 || v < 0 || v > MAX_PORT_NO)
    {
      return false; // bad port
    }
    port = v;
  }

  MgmtSrvrId mgmtSrvrId;
  mgmtSrvrId.type = MgmId_TCP;
  mgmtSrvrId.name.assign(host);
  mgmtSrvrId.port = port;

  /* assign default bind_address if available */
  if (bind_address.length())
    mgmtSrvrId.bind_address.assign(bind_address);
  mgmtSrvrId.bind_address_port = bind_address_port;
  ids.push_back(mgmtSrvrId);

  return true;
}

bool LocalConfig::parseBindAddress(const char *value)
{
  char host[NDB_DNS_HOST_NAME_LENGTH + 1];
  char serv[NDB_IANA_SERVICE_NAME_LENGTH + 1];

  int r = Ndb_split_string_address_port(value, host, sizeof(host), serv,
                                        sizeof(serv));
  if (r != 0)
    return false;

  int port = 0;
  if (serv[0] != 0)
  {
    char * endp = nullptr;
    long v = strtol(serv, &endp, 0);
    if (endp == nullptr || *endp != 0 || v < 0 || v > MAX_PORT_NO)
      return false; // bad port
    port = v;
  }

  if (ids.size() == 0)
  {
    /* assign default bind_address */
    bind_address.assign(host);
    bind_address_port = port;
  }
  else
  {
    /* override bind_address on latest mgmd */
    MgmtSrvrId &mgmtSrvrId= ids[ids.size()-1];
    mgmtSrvrId.bind_address.assign(host);
    mgmtSrvrId.bind_address_port = port;
  }
  return true;
}

bool LocalConfig::parseFileName(const char *value)
{
  MgmtSrvrId mgmtSrvrId;
  mgmtSrvrId.type = MgmId_File;
  mgmtSrvrId.name.assign(value);
  ids.push_back(mgmtSrvrId);
  return true;
}

bool LocalConfig::parseComment(const char * /*value*/)
{
  /* ignore */
  return true;
}

const LocalConfig::param_prefix LocalConfig::param_prefixes[] =
{
  // Documented prefix
  {"nodeid=", &LocalConfig::parseNodeId},
  {"bind-address=", &LocalConfig::parseBindAddress},
  // Prefix generated and occurring in some public examples
  {"host=", &LocalConfig::parseHostName},
  // Undocumented prefix
  {"OwnProcessId ", &LocalConfig::parseNodeId},
  {"file://", &LocalConfig::parseFileName},
  {"file=", &LocalConfig::parseFileName},
  {"host://", &LocalConfig::parseHostName},
  {"mgmd=", &LocalConfig::parseHostName},
  {"#", &LocalConfig::parseComment},
  // Must be last since it will always match
  {"", &LocalConfig::parseHostName}
};

bool
LocalConfig::parseString(const char * connectString, BaseString &err)
{
  char * for_strtok;
  char * copy = strdup(connectString);
  NdbAutoPtr<char> tmp_aptr(copy);

  _ownNodeId = 0;
  bind_address_port = 0;
  bind_address.assign("");

  for (char *tok = my_strtok_r(copy,";,",&for_strtok); tok != nullptr;
       tok = my_strtok_r(nullptr, ";,", &for_strtok))
  {
    bool ok = false;
    for (size_t i = 0; i < std::size(param_prefixes); i++)
    {
      ok = false;
      if (strncmp(tok,
                  param_prefixes[i].prefix,
                  param_prefixes[i].prefix_len) == 0)
      {
        const char * value = tok + param_prefixes[i].prefix_len;
        ok = (this->*param_prefixes[i].param_func)(value);
        break;
      }
    }
    if (ok) continue;

    err.assfmt("Unexpected entry: \"%s\"", tok);
    return false;
  }
  return true;
}

bool LocalConfig::readFile(const char * filename, bool &fopenError)
{
  char line[1024];
  
  fopenError = false;
  
  FILE * file = fopen(filename, "r");
  if(file == nullptr){
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
  char addrbuf[512];

  if (p < sz && bind_address.length())
  {
    int new_p = 0;
    char *sockaddr_string = Ndb_combine_address_port(addrbuf, sizeof(addrbuf),
                                                     bind_address.c_str(),
                                                     bind_address_port);
    new_p = p + BaseString::snprintf(buf + p, sz - p, ",bind-address=%s",
                                     sockaddr_string);

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
      int new_p = 0;
      char *sockaddr_string = Ndb_combine_address_port(addrbuf,
                                                       sizeof(addrbuf),
                                                       ids[i].name.c_str(),
                                                       ids[i].port);

      new_p = p + BaseString::snprintf(buf + p, sz - p, ",%s",
                                       sockaddr_string);
      if (new_p < sz)
	p= new_p;
      else 
      {
	buf[p]= 0;
	break;
      }
      if (!bind_address.length() && ids[i].bind_address.length())
      {
        char *sockaddr_string =
            Ndb_combine_address_port(addrbuf, sizeof(addrbuf),
                                     ids[i].bind_address.c_str(),
                                     ids[i].bind_address_port);

        new_p = p + BaseString::snprintf(buf + p, sz - p, ";bind-address=%s",
                                         sockaddr_string);

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
