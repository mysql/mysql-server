/*
   Copyright (C) 2003-2006, 2008 MySQL AB
    All rights reserved. Use is subject to license terms.

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

#include <ndb_global.h>
#include <NdbConfig.h>
#include <NdbEnv.h>
#include <NdbMem.h>
#include <NdbHost.h>
#include <basestring_vsnprintf.h>

static const char *datadir_path= 0;

const char *
NdbConfig_get_path(int *_len)
{
  const char *path= NdbEnv_GetEnv("NDB_HOME", 0, 0);
  int path_len= 0;
  if (path)
    path_len= strlen(path);
  if (path_len == 0 && datadir_path) {
    path= datadir_path;
    path_len= strlen(path);
  }
  if (path_len == 0) {
    path= ".";
    path_len= strlen(path);
  }
  if (_len)
    *_len= path_len;
  return path;
}

static char* 
NdbConfig_AllocHomePath(int _len)
{
  int path_len;
  const char *path= NdbConfig_get_path(&path_len);
  int len= _len+path_len;
  char *buf= NdbMem_Allocate(len);
  basestring_snprintf(buf, len, "%s%s", path, DIR_SEPARATOR);
  return buf;
}

void
NdbConfig_SetPath(const char* path){
  datadir_path= path;
}

char* 
NdbConfig_NdbCfgName(int with_ndb_home){
  char *buf;
  int len= 0;

  if (with_ndb_home) {
    buf= NdbConfig_AllocHomePath(PATH_MAX);
    len= strlen(buf);
  } else
    buf= NdbMem_Allocate(PATH_MAX);
  basestring_snprintf(buf+len, PATH_MAX, "Ndb.cfg");
  return buf;
}

static
char *get_prefix_buf(int len, int node_id)
{
  char tmp_buf[sizeof("ndb_pid#############")+1];
  char *buf;
  if (node_id > 0)
    basestring_snprintf(tmp_buf, sizeof(tmp_buf), "ndb_%u", node_id);
  else
    basestring_snprintf(tmp_buf, sizeof(tmp_buf), "ndb_pid%u",
                        NdbHost_GetProcessId());
  tmp_buf[sizeof(tmp_buf)-1]= 0;

  buf= NdbConfig_AllocHomePath(len+strlen(tmp_buf));
  strcat(buf, tmp_buf);
  return buf;
}

char* 
NdbConfig_ErrorFileName(int node_id){
  char *buf= get_prefix_buf(PATH_MAX, node_id);
  int len= strlen(buf);
  basestring_snprintf(buf+len, PATH_MAX, "_error.log");
  return buf;
}

char*
NdbConfig_ClusterLogFileName(int node_id){
  char *buf= get_prefix_buf(PATH_MAX, node_id);
  int len= strlen(buf);
  basestring_snprintf(buf+len, PATH_MAX, "_cluster.log");
  return buf;
}

char*
NdbConfig_SignalLogFileName(int node_id){
  char *buf= get_prefix_buf(PATH_MAX, node_id);
  int len= strlen(buf);
  basestring_snprintf(buf+len, PATH_MAX, "_signal.log");
  return buf;
}

char*
NdbConfig_TraceFileName(int node_id, int file_no){
  char *buf= get_prefix_buf(PATH_MAX, node_id);
  int len= strlen(buf);
  basestring_snprintf(buf+len, PATH_MAX, "_trace.log.%u", file_no);
  return buf;
}

char*
NdbConfig_NextTraceFileName(int node_id){
  char *buf= get_prefix_buf(PATH_MAX, node_id);
  int len= strlen(buf);
  basestring_snprintf(buf+len, PATH_MAX, "_trace.log.next");
  return buf;
}

char*
NdbConfig_PidFileName(int node_id){
  char *buf= get_prefix_buf(PATH_MAX, node_id);
  int len= strlen(buf);
  basestring_snprintf(buf+len, PATH_MAX, ".pid");
  return buf;
}

char*
NdbConfig_StdoutFileName(int node_id){
  char *buf= get_prefix_buf(PATH_MAX, node_id);
  int len= strlen(buf);
  basestring_snprintf(buf+len, PATH_MAX, "_out.log");
  return buf;
}
