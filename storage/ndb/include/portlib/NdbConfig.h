/*
   Copyright (c) 2003, 2024, Oracle and/or its affiliates.
    Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NDB_CONFIG_H
#define NDB_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

const char *NdbConfig_get_path(int *len);
void NdbConfig_SetPath(const char *path);
char *NdbConfig_NdbCfgName(int with_ndb_home);
char *NdbConfig_ErrorFileName(int node_id);
char *NdbConfig_ClusterLogFileName(int node_id);
char *NdbConfig_SignalLogFileName(int node_id);
char *NdbConfig_TraceFileName(int node_id, int file_no);
char *NdbConfig_NextTraceFileName(int node_id);
char *NdbConfig_PidFileName(int node_id);
char *NdbConfig_StdoutFileName(int node_id);

#ifdef __cplusplus
}
#endif

#endif
