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

static char* 
NdbConfig_AllocHomePath(int _len)
{
  const char *path= NdbEnv_GetEnv("NDB_HOME", 0, 0);
  int len= _len;
  int path_len= 0;
  char *buf;

  if (path)
    path_len= strlen(path);

  len+= path_len;
  buf= malloc(len);
  if (path_len > 0)
    snprintf(buf, len, "%s%s", path, DIR_SEPARATOR);
  else
    buf[0]= 0;

  return buf;
}

char* 
NdbConfig_NdbCfgName(int with_ndb_home){
  char *buf;
  int len= 0;

  if (with_ndb_home) {
    buf= NdbConfig_AllocHomePath(128);
    len= strlen(buf);
  } else
    buf= malloc(128);
  snprintf(buf+len, 128, "Ndb.cfg");
  return buf;
}

char* 
NdbConfig_ErrorFileName(int node_id){
  char *buf= NdbConfig_AllocHomePath(128);
  int len= strlen(buf);
  snprintf(buf+len, 128, "ndb_%u_error.log", node_id);
  return buf;
}

char*
NdbConfig_ClusterLogFileName(int node_id){
  char *buf= NdbConfig_AllocHomePath(128);
  int len= strlen(buf);
  snprintf(buf+len, 128, "ndb_%u_cluster.log", node_id);
  return buf;
}

char*
NdbConfig_SignalLogFileName(int node_id){
  char *buf= NdbConfig_AllocHomePath(128);
  int len= strlen(buf);
  snprintf(buf+len, 128, "ndb_%u_signal.log", node_id);
  return buf;
}

char*
NdbConfig_TraceFileName(int node_id, int file_no){
  char *buf= NdbConfig_AllocHomePath(128);
  int len= strlen(buf);
  snprintf(buf+len, 128, "ndb_%u_trace.log.%u", node_id, file_no);
  return buf;
}

char*
NdbConfig_NextTraceFileName(int node_id){
  char *buf= NdbConfig_AllocHomePath(128);
  int len= strlen(buf);
  snprintf(buf+len, 128, "ndb_%u_trace.log.next", node_id);
  return buf;
}

char*
NdbConfig_PidFileName(int node_id){
  char *buf= NdbConfig_AllocHomePath(128);
  int len= strlen(buf);
  snprintf(buf+len, 128, "ndb_%u.pid", node_id);
  return buf;
}

char*
NdbConfig_StdoutFileName(int node_id){
  char *buf= NdbConfig_AllocHomePath(128);
  int len= strlen(buf);
  snprintf(buf+len, 128, "ndb_%u_out.log", node_id);
  return buf;
}
