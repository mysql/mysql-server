/*
   Copyright (c) 2003, 2026, Oracle and/or its affiliates.

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

#include <assert.h>
#include <ndb_global.h>
#include <ndb_version.h>
#include <version.h>
#include <NdbOut.hpp>

extern "C" Uint32 ndbGetMajor(Uint32 version) { return (version >> 16) & 0xFF; }

extern "C" Uint32 ndbGetMinor(Uint32 version) { return (version >> 8) & 0xFF; }

extern "C" Uint32 ndbGetBuild(Uint32 version) { return (version >> 0) & 0xFF; }

extern "C" Uint32 ndbMakeVersion(Uint32 major, Uint32 minor, Uint32 build) {
  return NDB_MAKE_VERSION(major, minor, build);
}

extern "C" const char *ndbGetOwnVersionString() {
  static char ndb_version_string_buf[NDB_VERSION_STRING_BUF_SZ];
  return ndbGetVersionString(NDB_VERSION, NDB_MYSQL_VERSION_D,
                             NDB_VERSION_STATUS, ndb_version_string_buf,
                             sizeof(ndb_version_string_buf));
}

extern "C" const char *ndbGetVersionString(Uint32 version, Uint32 mysql_version,
                                           const char *status, char *buf,
                                           unsigned sz) {
  const char *tmp = (status == nullptr) ? "" : status;
  if (mysql_version) {
    snprintf(buf, sz, "mysql-%d.%d.%d ndb-%d.%d.%d%s", getMajor(mysql_version),
             getMinor(mysql_version), getBuild(mysql_version),
             getMajor(version), getMinor(version), getBuild(version), tmp);
  } else {
    snprintf(buf, sz, "ndb-%d.%d.%d%s", getMajor(version), getMinor(version),
             getBuild(version), tmp);
  }
  return buf;
}

extern "C" void ndbPrintVersion() {
  printf("Version: %u.%u.%u\n", getMajor(ndbGetOwnVersion()),
         getMinor(ndbGetOwnVersion()), getBuild(ndbGetOwnVersion()));
}

extern "C" Uint32 ndbGetOwnVersion() { return NDB_VERSION_D; }

static int ndbCompatible_full(Uint32 ownVersion [[maybe_unused]],
                              Uint32 otherVersion) {
  assert(ownVersion == NDB_VERSION);
  return (otherVersion >= MAKE_VERSION(7, 0, 0));
}

static int ndbCompatible_upgrade(Uint32 ownVersion, Uint32 otherVersion) {
  return ndbCompatible_full(ownVersion, otherVersion);
}

extern "C" int ndbCompatible_mgmt_ndb(Uint32 ownVersion, Uint32 otherVersion) {
  return ndbCompatible_upgrade(ownVersion, otherVersion);
}

extern "C" int ndbCompatible_mgmt_api(Uint32 ownVersion, Uint32 otherVersion) {
  return ndbCompatible_upgrade(ownVersion, otherVersion);
}

extern "C" int ndbCompatible_ndb_mgmt(Uint32 ownVersion, Uint32 otherVersion) {
  return ndbCompatible_full(ownVersion, otherVersion);
}

extern "C" int ndbCompatible_api_mgmt(Uint32 ownVersion, Uint32 otherVersion) {
  return ndbCompatible_full(ownVersion, otherVersion);
}

extern "C" int ndbCompatible_api_ndb(Uint32 ownVersion, Uint32 otherVersion) {
  return ndbCompatible_full(ownVersion, otherVersion);
}

extern "C" int ndbCompatible_ndb_api(Uint32 ownVersion, Uint32 otherVersion) {
  return ndbCompatible_upgrade(ownVersion, otherVersion);
}

extern "C" int ndbCompatible_ndb_ndb(Uint32 ownVersion, Uint32 otherVersion) {
  return ndbCompatible_upgrade(ownVersion, otherVersion);
}

#ifdef TEST_NDB_VERSION

#include <NdbTap.hpp>

TAPTEST(ndb_version) {
  printf("Checking NDB version defines and functions...\n\n");

  printf(" version string: '%s'\n", MYSQL_SERVER_VERSION);

  printf(" NDB_MYSQL_VERSION_MAJOR: %d\n", NDB_MYSQL_VERSION_MAJOR);
  printf(" NDB_MYSQL_VERSION_MINOR: %d\n", NDB_MYSQL_VERSION_MINOR);
  printf(" NDB_MYSQL_VERSION_BUILD: %d\n\n", NDB_MYSQL_VERSION_BUILD);
  printf(" NDB_VERSION_MAJOR: %d\n", NDB_VERSION_MAJOR);
  printf(" NDB_VERSION_MINOR: %d\n", NDB_VERSION_MINOR);
  printf(" NDB_VERSION_BUILD: %d\n", NDB_VERSION_BUILD);
  printf(" NDB_VERSION_STATUS: '%s'\n\n", NDB_VERSION_STATUS);
  printf(" NDB_VERSION_STRING: '%s'\n", NDB_VERSION_STRING);
  printf(" NDB_NDB_VERSION_STRING: '%s'\n\n", NDB_NDB_VERSION_STRING);

  /*
    Parse the VERSION string as X.X.X-status */
  unsigned mysql_major, mysql_minor, mysql_build;
  char mysql_status[100];
  const int matches_version =
      sscanf(MYSQL_SERVER_VERSION, "%u.%u.%u-%s", &mysql_major, &mysql_minor,
             &mysql_build, mysql_status);
  OK(matches_version == 3 || matches_version == 4);

  /*
    Check that defined MySQL version numbers X.X.X match those parsed
    from the version string
  */
  OK(NDB_MYSQL_VERSION_MAJOR == mysql_major ||
     NDB_MYSQL_VERSION_MINOR == mysql_minor ||
     NDB_MYSQL_VERSION_BUILD == mysql_build);

  if (matches_version == 4 && strncmp(mysql_status, "ndb", 3) == 0) {
    /* This is a MySQL Cluster build */
    unsigned ndb_major, ndb_minor, ndb_build;
    char ndb_status[100];
    int matches_ndb = sscanf(mysql_status, "ndb-%u.%u.%u%s", &ndb_major,
                             &ndb_minor, &ndb_build, ndb_status);

    printf("This is a MySQL Cluster build!\n");
    printf(" MySQL Server version(X.X.X): %u.%u.%u\n", mysql_major, mysql_minor,
           mysql_build);
    printf(" NDB version(Y.Y.Y): %u.%u.%u\n", ndb_major, ndb_minor, ndb_build);

    OK(matches_ndb == 3 || matches_ndb == 4);

    /*
      Check that defined NDB version numbers Y.Y.Y match
      those parsed from the version string
    */
    OK(NDB_VERSION_MAJOR == ndb_major || NDB_VERSION_MINOR == ndb_minor ||
       NDB_VERSION_BUILD == ndb_build);

  } else {
    /* This is a MySQL Server with NDB build */
    printf("This is a MySQL Server with NDB build!\n");
    printf(" MySQL Server version(X.X.X): %u.%u.%u\n", mysql_major, mysql_minor,
           mysql_build);
    printf(" NDB version(Y.Y.Y): %u.%u.%u\n", NDB_VERSION_MAJOR,
           NDB_VERSION_MINOR, NDB_VERSION_BUILD);
  }

  /* ndbPrintVersion */
  printf("ndbPrintVersion() => ");
  ndbPrintVersion();

  /* ndbMakeVersion */
  Uint32 major = 1;
  Uint32 minor = 2;
  Uint32 build = 3;
  Uint32 version = ndbMakeVersion(major, minor, build);
  OK(version == 0x00010203);
  OK(ndbGetMajor(version) == major);
  OK(ndbGetMinor(version) == minor);
  OK(ndbGetBuild(version) == build);

  /* ndbGetVersionString */
  char buf[64];
  printf("ndbGetVersionString(0x00010203, 0x00030201): '%s'\n",
         ndbGetVersionString(version, 0x00030201, "-status", buf, sizeof(buf)));

  /* ndbGetOwnVersionString */
  printf("ndbGetOwnVersionString: '%s'\n", ndbGetOwnVersionString());
  OK(strcmp(NDB_VERSION_STRING, ndbGetOwnVersionString()) ==
     0);  // should match

  /* ndbGetOwnVersion */
  OK(ndbGetOwnVersion() ==
     ndbMakeVersion(NDB_VERSION_MAJOR, NDB_VERSION_MINOR, NDB_VERSION_BUILD));
  OK(ndbGetOwnVersion() == NDB_VERSION_D);
  OK(ndbGetOwnVersion() == NDB_VERSION);

  /* NDB_MYSQL_VERSION_D */
  OK(NDB_MYSQL_VERSION_D == ndbMakeVersion(NDB_MYSQL_VERSION_MAJOR,
                                           NDB_MYSQL_VERSION_MINOR,
                                           NDB_MYSQL_VERSION_BUILD));

  /* Check sanity of version defines(we don't own a time machine yet...) */
  OK(ndbMakeVersion(NDB_MYSQL_VERSION_MAJOR, NDB_MYSQL_VERSION_MINOR,
                    NDB_MYSQL_VERSION_BUILD) >= 0x0005012F);  // 5.1.47
  OK(ndbMakeVersion(NDB_VERSION_MAJOR, NDB_VERSION_MINOR,
                    NDB_VERSION_BUILD) >= 0x00070011);  // 7.0.17

  /* Check MYSQL_VERSION_ID matches NDB_MYSQL_VERSION_XX variables */
  OK(MYSQL_VERSION_ID ==
     (NDB_MYSQL_VERSION_MAJOR * 10000 + NDB_MYSQL_VERSION_MINOR * 100 +
      NDB_MYSQL_VERSION_BUILD));

  /* Check that this node is compatible with 8.0.13 API and data nodes */
  printf("Testing compatibility\n");
  Uint32 ver8013 = ndbMakeVersion(8, 0, 13);
  int c1 = ndbCompatible_ndb_api(NDB_VERSION, ver8013);
  int c2 = ndbCompatible_api_ndb(NDB_VERSION, ver8013);
  OK(c1 == 1);
  OK(c2 == 1);

  return 1;  // OK
}

#endif
