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
#include <ndb_version.h>
#include <version.h>

Uint32 getMajor(Uint32 version) {
  return (version >> 16) & 0xFF;
}

Uint32 getMinor(Uint32 version) {
  return (version >> 8) & 0xFF;
}

Uint32 getBuild(Uint32 version) {
  return (version >> 0) & 0xFF;
}

Uint32 makeVersion(Uint32 major, Uint32 minor, Uint32 build) {
  return MAKE_VERSION(major, minor, build);
  
}

const char * getVersionString(Uint32 version, const char * status) {
  char buff[100];
  if (status && status[0] != 0)
    snprintf(buff, sizeof(buff),
	     "Version %d.%d.%d (%s)",
	     getMajor(version),
	     getMinor(version),
	     getBuild(version),
	     status);
  else
    snprintf(buff, sizeof(buff),
	     "Version %d.%d.%d",
	     getMajor(version),
	     getMinor(version),
	     getBuild(version));
  return strdup(buff);
}

typedef enum {
  UG_Null,
  UG_Range,
  UG_Exact
} UG_MatchType;

struct NdbUpGradeCompatible {
  Uint32 ownVersion;
  Uint32 otherVersion;
  UG_MatchType matchType;
};

/*#define TEST_VERSION*/

#ifndef TEST_VERSION
struct NdbUpGradeCompatible ndbCompatibleTable_full[] = {
  { MAKE_VERSION(3,5,2), MAKE_VERSION(3,5,1), UG_Exact },
  { 0, 0, UG_Null }
};

struct NdbUpGradeCompatible ndbCompatibleTable_upgrade[] = {
  { 0, 0, UG_Null }
};

void ndbSetOwnVersion() {}

#else /* testing purposes */

struct NdbUpGradeCompatible ndbCompatibleTable_full[] = {
  { MAKE_VERSION(4,1,5), MAKE_VERSION(4,1,0), UG_Range },
  { MAKE_VERSION(3,6,9), MAKE_VERSION(3,6,1), UG_Range },
  { MAKE_VERSION(3,6,2), MAKE_VERSION(3,6,1), UG_Range },
  { MAKE_VERSION(3,5,7), MAKE_VERSION(3,5,0), UG_Range },
  { MAKE_VERSION(3,5,1), MAKE_VERSION(3,5,0), UG_Range },
  { NDB_VERSION_D      , MAKE_VERSION(NDB_VERSION_MAJOR,NDB_VERSION_MINOR,2), UG_Range },
  { 0, 0, UG_Null }
};

struct NdbUpGradeCompatible ndbCompatibleTable_upgrade[] = {
  { MAKE_VERSION(4,1,5), MAKE_VERSION(3,6,9), UG_Exact },
  { MAKE_VERSION(3,6,2), MAKE_VERSION(3,5,7), UG_Exact },
  { MAKE_VERSION(3,5,1), NDB_VERSION_D      , UG_Exact },
  { 0, 0, UG_Null }
};


Uint32 ndbOwnVersionTesting = 0;
void
ndbSetOwnVersion() {
  char buf[256];
  if (NdbEnv_GetEnv("NDB_SETVERSION", buf, sizeof(buf))) {
    Uint32 _v1,_v2,_v3;
    if (sscanf(buf, "%u.%u.%u", &_v1, &_v2, &_v3) == 3) {
      ndbOwnVersionTesting = MAKE_VERSION(_v1,_v2,_v3);
      ndbout_c("Testing: Version set to 0x%x",  ndbOwnVersionTesting);
    }
  }
}

#endif

void ndbPrintVersion()
{
  printf("Version: %u.%u.%u\n",
	 getMajor(ndbGetOwnVersion()),
	 getMinor(ndbGetOwnVersion()),
	 getBuild(ndbGetOwnVersion()));
}

Uint32
ndbGetOwnVersion()
{
#ifndef TEST_VERSION
  return NDB_VERSION_D;
#else /* testing purposes */
  if (ndbOwnVersionTesting == 0)
    return NDB_VERSION_D;
  else
    return ndbOwnVersionTesting;
#endif
}

int
ndbSearchUpgradeCompatibleTable(Uint32 ownVersion, Uint32 otherVersion,
				struct NdbUpGradeCompatible table[])
{
  int i;
  for (i = 0; table[i].ownVersion != 0 && table[i].otherVersion != 0; i++) {
    if (table[i].ownVersion == ownVersion ||
	table[i].ownVersion == (Uint32) ~0) {
      switch (table[i].matchType) {
      case UG_Range:
	if (otherVersion >= table[i].otherVersion){
	  return 1;
	}
	break;
      case UG_Exact:
	if (otherVersion == table[i].otherVersion){
	  return 1;
	}
	break;
      default:
	break;
      }
    }
  }
  return 0;
}

int
ndbCompatible(Uint32 ownVersion, Uint32 otherVersion, struct NdbUpGradeCompatible table[])
{
  if (otherVersion >= ownVersion) {
    return 1;
  }
  return ndbSearchUpgradeCompatibleTable(ownVersion, otherVersion, table);
}

int
ndbCompatible_full(Uint32 ownVersion, Uint32 otherVersion)
{
  return ndbCompatible(ownVersion, otherVersion, ndbCompatibleTable_full);
}

int
ndbCompatible_upgrade(Uint32 ownVersion, Uint32 otherVersion)
{
  if (ndbCompatible_full(ownVersion, otherVersion))
    return 1;
  return ndbCompatible(ownVersion, otherVersion, ndbCompatibleTable_upgrade);
}

int
ndbCompatible_mgmt_ndb(Uint32 ownVersion, Uint32 otherVersion)
{
  return ndbCompatible_upgrade(ownVersion, otherVersion);
}

int
ndbCompatible_mgmt_api(Uint32 ownVersion, Uint32 otherVersion)
{
  return ndbCompatible_upgrade(ownVersion, otherVersion);
}

int
ndbCompatible_ndb_mgmt(Uint32 ownVersion, Uint32 otherVersion)
{
  return ndbCompatible_full(ownVersion, otherVersion);
}

int
ndbCompatible_api_mgmt(Uint32 ownVersion, Uint32 otherVersion)
{
  return ndbCompatible_full(ownVersion, otherVersion);
}

int
ndbCompatible_api_ndb(Uint32 ownVersion, Uint32 otherVersion)
{
  return ndbCompatible_full(ownVersion, otherVersion);
}

int
ndbCompatible_ndb_api(Uint32 ownVersion, Uint32 otherVersion)
{
  return ndbCompatible_upgrade(ownVersion, otherVersion);
}

int
ndbCompatible_ndb_ndb(Uint32 ownVersion, Uint32 otherVersion)
{
  return ndbCompatible_upgrade(ownVersion, otherVersion);
}
