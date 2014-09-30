/*
   Copyright (c) 2011, 2014 Oracle and/or its affiliates. All rights reserved.

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

#ifndef NDB_ANYVALUE_H
#define NDB_ANYVALUE_H

#include <ndb_types.h>

bool ndbcluster_anyvalue_is_reserved(Uint32 anyValue);

bool ndbcluster_anyvalue_is_nologging(Uint32 anyValue);
void ndbcluster_anyvalue_set_nologging(Uint32& anyValue);

bool ndbcluster_anyvalue_is_refresh_op(Uint32 anyValue);
void ndbcluster_anyvalue_set_refresh_op(Uint32& anyValue);

bool ndbcluster_anyvalue_is_reflect_op(Uint32 anyValue);
void ndbcluster_anyvalue_set_reflect_op(Uint32& anyValue);

bool ndbcluster_anyvalue_is_read_op(Uint32 anyValue);
void ndbcluster_anyvalue_set_read_op(Uint32& anyValue);

bool ndbcluster_anyvalue_is_serverid_in_range(Uint32 serverId);
void ndbcluster_anyvalue_set_normal(Uint32& anyValue);
Uint32 ndbcluster_anyvalue_get_serverid(Uint32 anyValue);
void ndbcluster_anyvalue_set_serverid(Uint32& anyValue, Uint32 serverId);

#ifndef DBUG_OFF
void dbug_ndbcluster_anyvalue_set_userbits(Uint32& anyValue);
#endif

#endif
