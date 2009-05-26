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

#include <ndb_global.h>
#include <ndb_version.h>
#include <version.h>
#include <basestring_vsnprintf.h>
#include <NdbEnv.h>
#include <NdbOut.hpp>

Uint32 ndbGetMajor(Uint32 version) {
  return (version >> 16) & 0xFF;
}

Uint32 ndbGetMinor(Uint32 version) {
  return (version >> 8) & 0xFF;
}

Uint32 ndbGetBuild(Uint32 version) {
  return (version >> 0) & 0xFF;
}

Uint32 ndbMakeVersion(Uint32 major, Uint32 minor, Uint32 build) {
  return NDB_MAKE_VERSION(major, minor, build);
  
}

const char * ndbGetOwnVersionString()
{
  static char ndb_version_string_buf[NDB_VERSION_STRING_BUF_SZ];
  return ndbGetVersionString(NDB_VERSION, NDB_VERSION_STATUS,
                             ndb_version_string_buf,
                             sizeof(ndb_version_string_buf));
}

const char * ndbGetVersionString(Uint32 version, const char * status,
                                 char *buf, unsigned sz)
{
  if (status && status[0] != 0)
	  basestring_snprintf(buf, sz,
	     "Version %d.%d.%d (%s)",
	     getMajor(version),
	     getMinor(version),
	     getBuild(version),
	     status);
  else
    basestring_snprintf(buf, sz,
	     "Version %d.%d.%d",
	     getMajor(version),
	     getMinor(version),
	     getBuild(version));
  return buf;
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

#define HAVE_NDB_SETVERSION
#ifdef HAVE_NDB_SETVERSION
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
#else
void ndbSetOwnVersion() {}
#endif

#ifndef TEST_VERSION
struct NdbUpGradeCompatible ndbCompatibleTable_full[] = {
  { MAKE_VERSION(5,4,NDB_VERSION_BUILD), MAKE_VERSION(5,1,18), UG_Range},
  { MAKE_VERSION(5,1,17), MAKE_VERSION(5,1,0), UG_Range},
  { MAKE_VERSION(5,0,NDB_VERSION_BUILD), MAKE_VERSION(5,0,12), UG_Range},
  { MAKE_VERSION(5,0,11), MAKE_VERSION(5,0,2), UG_Range},
  { MAKE_VERSION(4,1,NDB_VERSION_BUILD), MAKE_VERSION(4,1,15), UG_Range },
  { MAKE_VERSION(4,1,14), MAKE_VERSION(4,1,10), UG_Range },
  { MAKE_VERSION(4,1,10), MAKE_VERSION(4,1,9), UG_Exact },
  { MAKE_VERSION(4,1,9), MAKE_VERSION(4,1,8), UG_Exact },
  { MAKE_VERSION(3,5,2), MAKE_VERSION(3,5,1), UG_Exact },
  { 0, 0, UG_Null }
};

struct NdbUpGradeCompatible ndbCompatibleTable_upgrade[] = {
  { MAKE_VERSION(5,0,12), MAKE_VERSION(5,0,11), UG_Exact },
  { MAKE_VERSION(5,0,2), MAKE_VERSION(4,1,8), UG_Exact },
  { MAKE_VERSION(4,1,15), MAKE_VERSION(4,1,14), UG_Exact },
  { MAKE_VERSION(3,5,4), MAKE_VERSION(3,5,3), UG_Exact },
  { 0, 0, UG_Null }
};

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
#ifdef HAVE_NDB_SETVERSION
  if (ndbOwnVersionTesting == 0)
    return NDB_VERSION_D;
  else
    return ndbOwnVersionTesting;
#else
  return NDB_VERSION_D;
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
