/*
   Copyright (C) 2003-2006 MySQL AB
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

#ifndef NDB_CONFIG_H
#define NDB_CONFIG_H

#ifdef	__cplusplus
extern "C" {
#endif

const char* NdbConfig_get_path(int *len);
void NdbConfig_SetPath(const char *path);
char* NdbConfig_NdbCfgName(int with_ndb_home);
char* NdbConfig_ErrorFileName(int node_id);
char* NdbConfig_ClusterLogFileName(int node_id);
char* NdbConfig_SignalLogFileName(int node_id);
char* NdbConfig_TraceFileName(int node_id, int file_no);
char* NdbConfig_NextTraceFileName(int node_id);
char* NdbConfig_PidFileName(int node_id);
char* NdbConfig_StdoutFileName(int node_id);

#ifdef	__cplusplus
}
#endif

#endif
