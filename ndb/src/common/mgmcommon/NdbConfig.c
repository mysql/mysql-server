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
#include <NdbConfig.h>
#include <NdbEnv.h>

const char* 
NdbConfig_HomePath(char* buf, int buflen){
  const char* p;
  p = NdbEnv_GetEnv("NDB_HOME", buf, buflen);
  if (p == NULL){
    strlcpy(buf, "", buflen);
    p = buf;
  } else {
    const int len = strlen(buf);
    if(len != 0 && buf[len-1] != '/'){
      buf[len]   = '/';
      buf[len+1] = 0;
    }
  }
  return p;
}

const char* 
NdbConfig_NdbCfgName(char* buf, int buflen, int with_ndb_home){
  if (with_ndb_home)
    NdbConfig_HomePath(buf, buflen);
  else
    buf[0] = 0;
  strlcat(buf, "Ndb.cfg", buflen);
  return buf;
}

const char* 
NdbConfig_ErrorFileName(char* buf, int buflen){
  NdbConfig_HomePath(buf, buflen);
  strlcat(buf, "error.log", buflen);
  return buf;
}

const char*
NdbConfig_ClusterLogFileName(char* buf, int buflen){
  NdbConfig_HomePath(buf, buflen);
  strlcat(buf, "cluster.log", buflen);
  return buf;
}
