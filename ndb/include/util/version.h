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


#ifndef VERSION_H
#define VERSION_H
#include <ndb_types.h>
#ifdef __cplusplus
extern "C" {
#endif

  Uint32 getMajor(Uint32 version);
  
  Uint32 getMinor(Uint32 version);
  
  Uint32 getBuild(Uint32 version);

  Uint32 makeVersion(Uint32 major, Uint32 minor, Uint32 build);

  const char* getVersionString(Uint32 version, const char * status);
  
  void ndbPrintVersion();
  Uint32 ndbGetOwnVersion();

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
