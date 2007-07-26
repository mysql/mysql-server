/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


#ifndef VERSION_H
#define VERSION_H
#include <ndb_version.h>

/* some backwards compatible macros */
#define MAKE_VERSION(A,B,C) NDB_MAKE_VERSION(A,B,C)
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
