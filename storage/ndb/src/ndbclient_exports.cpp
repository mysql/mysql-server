/*
   Copyright (c) 2012 Oracle and/or its affiliates. All rights reserved.

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

/*
  This source file is compiled into the shared ndbclient
  in order to tell the linker which functions to export.

  NOTE! _ndbclient_exports() is not intended to be called
*/

#include "../include/ndbapi/NdbApi.hpp"

extern "C" void* JNI_OnLoad(void*, void*);

void
_ndbclient_exports(void)
{
  Ndb_cluster_connection cluster_connection;
  NdbScanFilter scan_filter((NdbOperation*)0);
#ifdef NDB_WITH_NDBJTIE
  JNI_OnLoad(0,0);
#endif
}
