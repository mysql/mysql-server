/*
   Copyright (c) 2003, 2024, Oracle and/or its affiliates.

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

#ifndef VERSION_H
#define VERSION_H
#include <ndb_version.h>

/* some backwards compatible macros */
#define MAKE_VERSION(A, B, C) NDB_MAKE_VERSION(A, B, C)
#define getMajor(a) ndbGetMajor(a)
#define getMinor(a) ndbGetMinor(a)
#define getBuild(a) ndbGetBuild(a)

#ifdef __cplusplus
extern "C" {
#endif

int ndbCompatible_mgmt_ndb(Uint32 ownVersion, Uint32 otherVersion);
int ndbCompatible_ndb_mgmt(Uint32 ownVersion, Uint32 otherVersion);
int ndbCompatible_mgmt_api(Uint32 ownVersion, Uint32 otherVersion);
int ndbCompatible_api_mgmt(Uint32 ownVersion, Uint32 otherVersion);
int ndbCompatible_api_ndb(Uint32 ownVersion, Uint32 otherVersion);
int ndbCompatible_ndb_api(Uint32 ownVersion, Uint32 otherVersion);
int ndbCompatible_ndb_ndb(Uint32 ownVersion, Uint32 otherVersion);

#ifdef __cplusplus
}
#endif

#endif
