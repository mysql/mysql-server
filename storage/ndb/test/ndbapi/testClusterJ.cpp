/*
   Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

#if !(defined(CMAKE_BINARY_DIR) && defined(INSTALL_MYSQLSHAREDIR))
#error CMAKE_BINARY_DIR and INSTALL_MYSQLSHAREDIR must be defined
#endif

#include <string>

#include "BaseString.hpp"
#include "File.hpp"
#include "NdbProcess.hpp"
#include "mgmapi.h"
#include "ndb_global.h"
#include "ndb_version.h"

static constexpr const char *JarSrcPath =
    "storage" DIR_SEPARATOR "ndb" DIR_SEPARATOR "clusterj" DIR_SEPARATOR;

static constexpr const char *JarInstallPath =
    CMAKE_INSTALL_PREFIX DIR_SEPARATOR INSTALL_MYSQLSHAREDIR DIR_SEPARATOR;

static constexpr const char *BinDir = CMAKE_BINARY_DIR DIR_SEPARATOR;

static constexpr const char *verStr = NDB_MAKE_STRING_VERSION(
    NDB_VERSION_MAJOR, NDB_VERSION_MINOR, NDB_VERSION_BUILD);

static constexpr const char *libInstallPath =
    CMAKE_INSTALL_PREFIX DIR_SEPARATOR INSTALL_LIBDIR;

static constexpr const char *compileTimeClassPath = WITH_CLASSPATH;

static constexpr const char *mtrFirstMgmd = "localhost:13000";

#ifdef _WIN32
static constexpr const char *Separator = ";";
#else
static constexpr const char *Separator = ":";
#endif

const char *getMgmStr() {
  const char *env = getenv("NDB_CONNECTSTRING");
  if (env) return env; /* Use NDB_CONNECTSTRING if set */

  NdbMgmHandle handle = ndb_mgm_create_handle();
  ndb_mgm_set_connectstring(handle, mtrFirstMgmd);
  if (ndb_mgm_connect(handle, 0, 0, 0) != -1) {
    ndb_mgm_disconnect(handle);
    return mtrFirstMgmd; /* Use localhost:13000 if available */
  }
  return "localhost:1186";
}

const char *getMySQLStr(bool mtr) {
  const char *env = getenv("CLUSTERJ_MYSQLD");
  if (env) return env;
  if (mtr) return "localhost:13001";
  return "localhost:3306";
}

class Paths {
  std::string ver() { return verStr; }
  std::string sep() { return DIR_SEPARATOR; }
  std::string tmpDir() { return getenv("TMPDIR"); }
  std::string binDir() { return BinDir; }
  std::string jarSrc() { return JarSrcPath; }
  std::string jarInstDir() { return JarInstallPath; }
  std::string jarBuildDir() { return binDir() + jarSrc(); }
  std::string verJar() { return ver() + ".jar"; }

 public:
  // Paths for clusterj.jar
  std::string cjFile() { return "clusterj-" + verJar(); }
  std::string cjBuildJar() { return jarBuildDir() + cjFile(); }
  std::string cjInstJar() { return jarInstDir() + cjFile(); }

  // Paths for clusterj-test.jar
  std::string cjtFile() { return "clusterj-test-" + verJar(); }
  std::string cjtBuildJar() {
    return jarBuildDir() + "clusterj-test" + sep() + cjtFile();
  }
  std::string cjtInstJar() { return jarInstDir() + cjtFile(); }

  // Paths for libndbclient
  std::string libBuildDir() { return binDir() + "library_output_directory"; }
  std::string libInstDir() { return libInstallPath; }

  // Properties file
  std::string propsFile() { return tmpDir() + "clusterj.properties"; }
};

Paths paths;

bool write_properties(const char *connStr, const char *mysqlStr) {
  FILE *fp = fopen(paths.propsFile().c_str(), "w");
  if (!fp) return false;
  fprintf(fp,
          "com.mysql.clusterj.connectstring=%s\n"
          "com.mysql.clusterj.connect.retries=4\n"
          "com.mysql.clusterj.connect.delay=5\n"
          "com.mysql.clusterj.connect.verbose=1\n"
          "com.mysql.clusterj.connect.timeout.before=30\n"
          "com.mysql.clusterj.connect.timeout.after=20\n"
          "com.mysql.clusterj.jdbc.url=jdbc:mysql://%s/test\n"
          "com.mysql.clusterj.jdbc.driver=com.mysql.cj.jdbc.Driver\n"
          "com.mysql.clusterj.jdbc.username=root\n"
          "com.mysql.clusterj.jdbc.password=\n"
          "com.mysql.clusterj.username=\n"
          "com.mysql.clusterj.password=\n"
          "com.mysql.clusterj.database=test\n"
          "com.mysql.clusterj.max.transactions=1024\n",
          connStr, mysqlStr);
  fclose(fp);
  return true;
}

bool write_properties() {
  const char *connStr = getMgmStr();
  const char *mysqlStr = getMySQLStr((connStr == mtrFirstMgmd));
  return write_properties(connStr, mysqlStr);
}

int main(int argc, char **argv) {
  ndb_init();

  std::string clusterjJar, clusterjTestJar, ndbClientDir;

  /* If the Cluster/J jar file exists in the build directory,
     find everything in build. Otherwise, look in install directory. */
  bool isBuild = File_class::exists(paths.cjBuildJar().c_str());
  if (isBuild) {
    clusterjJar = paths.cjBuildJar();
    clusterjTestJar = paths.cjtBuildJar();
    ndbClientDir = paths.libBuildDir();
  } else {
    clusterjJar = paths.cjInstJar();
    clusterjTestJar = paths.cjtInstJar();
    ndbClientDir = paths.libInstDir();
  }

  /* Fail here if no clusterj-test JAR file */
  if (!File_class::exists(clusterjTestJar.c_str())) {
    fprintf(stderr, "Cannot find clusterj-test jar file '%s'\n",
            clusterjTestJar.c_str());
    ndb_end(0);
    return -1;
  }

  /* Create properties file */
  if (!write_properties()) {
    fprintf(stderr, "Cannot open file '%s'\n", paths.propsFile().c_str());
    perror(nullptr);
    ndb_end(0);
    return -1;
  }

  /* Create the CLASSPATH */
  std::string classpath = clusterjJar + Separator + clusterjTestJar;
  const char *mtr_classpath = getenv("MTR_CLASSPATH");
  if (mtr_classpath) {
    classpath += Separator;
    classpath += mtr_classpath;
  }
  if (strlen(compileTimeClassPath) > 0) {
    classpath += Separator;
    classpath += compileTimeClassPath;
  }
  printf("Java Classpath: %s \n", classpath.c_str());

  /* Create the arguments to the Java command line */
  NdbProcess::Args args;
  args.add("-Djava.library.path=", ndbClientDir.c_str());
  args.add("-Dclusterj.properties=", paths.propsFile().c_str());
  args.add2("-cp", classpath.c_str());
  args.add("testsuite.clusterj.AllTests");
  args.add(clusterjTestJar.c_str());

  /* Pass all command-line options verbatim to the Java test-runner */
  for (int i = 1; i < argc; i++) args.add(argv[i]);

  char pwd[PATH_MAX];
  char *wd = getcwd(pwd, sizeof(pwd));

  /* Run the tests */
  auto proc = NdbProcess::create("ClusterJTest", "java", wd, args);
  int ret;
  proc->wait(ret, 500000);

  /* Delete the properties file */
  File_class::remove(paths.propsFile().c_str());

  ndb_end(0);
  return ret;
}
