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

static
char *get_prefix_buf(int len, int node_id)
{
  char tmp_buf[sizeof("ndb_pid#########")+1];
  if (node_id > 0)
    snprintf(tmp_buf, sizeof(tmp_buf), "ndb_%u", node_id);
  else
    snprintf(tmp_buf, sizeof(tmp_buf), "ndb_pid%u", getpid());
  tmp_buf[sizeof(tmp_buf)-1]= 0;

  char *buf= NdbConfig_AllocHomePath(len+strlen(tmp_buf));
  strcat(buf, tmp_buf);
  return buf;
}

char* 
NdbConfig_ErrorFileName(int node_id){
  char *buf= get_prefix_buf(128, node_id);
  int len= strlen(buf);
  snprintf(buf+len, 128, "_error.log");
  return buf;
}

char*
NdbConfig_ClusterLogFileName(int node_id){
  char *buf= get_prefix_buf(128, node_id);
  int len= strlen(buf);
  snprintf(buf+len, 128, "_cluster.log");
  return buf;
}

char*
NdbConfig_SignalLogFileName(int node_id){
  char *buf= get_prefix_buf(128, node_id);
  int len= strlen(buf);
  snprintf(buf+len, 128, "_signal.log");
  return buf;
}

char*
NdbConfig_TraceFileName(int node_id, int file_no){
  char *buf= get_prefix_buf(128, node_id);
  int len= strlen(buf);
  snprintf(buf+len, 128, "_trace.log.%u", file_no);
  return buf;
}

char*
NdbConfig_NextTraceFileName(int node_id){
  char *buf= get_prefix_buf(128, node_id);
  int len= strlen(buf);
  snprintf(buf+len, 128, "_trace.log.next");
  return buf;
}

char*
NdbConfig_PidFileName(int node_id){
  char *buf= get_prefix_buf(128, node_id);
  int len= strlen(buf);
  snprintf(buf+len, 128, ".pid");
  return buf;
}

char*
NdbConfig_StdoutFileName(int node_id){
  char *buf= get_prefix_buf(128, node_id);
  int len= strlen(buf);
  snprintf(buf+len, 128, "_out.log");
  return buf;
}
