/*
   Copyright (c) 2024, 2026, Oracle and/or its affiliates.

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

#if !(defined(CMAKE_BINARY_DIR))
#error CMAKE_BINARY_DIR must be defined
#endif

#include <stdlib.h>
#include <string>
#include <vector>

#include "File.hpp"
#include "NdbProcess.hpp"
#include "mgmapi.h"
#include "ndb_version.h"

#include "helpers/Properties.hpp"
#include "helpers/string_helpers.hpp"

#ifdef _WIN32
#include <direct.h>
#define CLASS_PATH_SEP ";"
#define setenv(a, b, c) _putenv_s(a, b)
#else
#include <unistd.h>
#define CLASS_PATH_SEP ":"
#endif

static constexpr const char *helpText = R"(
  Usage:
    runCrund [-p properties-file]... [property=value]... [--show-properties]

  Any number of properties files and literal properties can be supplied
  on the command line. All remaining arguments, beginning with the first
  option that is not "-p", will be passed verbatim to the crund programs.
  Use --show-properties to display the properties without running any tests.

  CRUND is a benchmarking tool designed to measure latency across two
  workloads called AB and S. The AB load measures basic create, update,
  navigate, and delete operations using two tables "a" and "b" with a foreign
  key relation; the S load uses a single predominantly string/varchar-based
  table "s". AB is implemented in C++ for NDBAPI, and both AB and S have three
  implementations in Java using NDBJtie, Cluster/J, and JDBC.

  Crund can be run in a build environment. Properties files and
  result files will be saved in the directory:
     ${CMAKE_BINARY_DIR}/crund-results

  Properties vary depending on load. The supported loads are:
    NdbapiAB, NdbjtieAB, NdbjtieS, ClusterjAB, ClusterjS, JdbcAB, JdbcS

  To start a crund environment, begin with MTR:
     cd mysql-test
     ./mtr --test-and-start crund

  Then use runCrund.
)";
// Load "S" evolved from a separate project called TWS ("Table with strings").
// When Martin Zaun, who originally designed crund, left the NDB team in 2014,
// he had done some work on "NdbapiS" (the missing C++ workload), which can
// still be found in the cpp source directory. The C++ design was always
// intended to track the Java design very closely, so the best path to
// completing this work seems to be making the C++ design match the Java
// design whereever the two differ.

static constexpr const char *JarSrcPath =
    "storage" DIR_SEPARATOR "ndb" DIR_SEPARATOR "clusterj" DIR_SEPARATOR;

static constexpr const char *BinDir = CMAKE_BINARY_DIR DIR_SEPARATOR;

static constexpr const char *verStr = NDB_MAKE_STRING_VERSION(
    NDB_VERSION_MAJOR, NDB_VERSION_MINOR, NDB_VERSION_BUILD);

static constexpr const char *compileTimeClassPath = WITH_CLASSPATH;

static constexpr const char *mtrFirstMgmd = "localhost:13000";

static constexpr const char *crundPath =
    CMAKE_BINARY_DIR DIR_SEPARATOR "storage" DIR_SEPARATOR "ndb" DIR_SEPARATOR
                                   "test" DIR_SEPARATOR "crund" DIR_SEPARATOR;

static constexpr const char *javaRunClassPrefix = "com.mysql.cluster.crund.";

static constexpr const char *runtimeDir = "runtime_output_directory";
static constexpr const char *libraryDir = "library_output_directory";

static bool envIsMtr{false};

const char *getMgmStr() {
  const char *env = getenv("NDB_CONNECTSTRING");
  if (env) return env; /* Use NDB_CONNECTSTRING if set */

  NdbMgmHandle handle = ndb_mgm_create_handle();
  ndb_mgm_set_connectstring(handle, mtrFirstMgmd);
  if (ndb_mgm_connect(handle, 0, 0, 0) != -1) {
    ndb_mgm_disconnect(handle);
    envIsMtr = true;
    return mtrFirstMgmd; /* Use localhost:13000 if available */
  }
  return "localhost:1186";
}

static constexpr std::array<const char *, 6> javaLoads = {
    "NdbjtieAB", "NdbjtieS", "JdbcAB", "JdbcS", "ClusterjAB", "ClusterjS"};

const char *getMySQLStr() {
  const char *env = getenv("CLUSTERJ_MYSQLD");
  if (env) return env;
  if (envIsMtr) return "localhost:13001";
  return "localhost:3306";
}

class Paths {
  static const std::string ver() { return verStr; }
  static const std::string sep() { return DIR_SEPARATOR; }
  static const std::string binDir() { return BinDir; }
  static const std::string jarSrc() { return JarSrcPath; }
  static const std::string crundDir() { return crundPath; }
  static const std::string jarBuildDir() { return binDir() + jarSrc(); }
  static const std::string verJar() { return ver() + ".jar"; }
  static const std::string logPrefix() { return "run-"; }
  static std::string cppCrundDir() { return crundDir() + "cpp" + sep(); }
  static std::string javaCrundDir() { return crundDir() + "java" + sep(); }
  static constexpr const char *dateFormat = "%Y%m%d_%H%M%S";
  static constexpr size_t dateFormatSize = sizeof("yyyymmdd_HHMMSS");

 public:
  // Paths for clusterj.jar
  static const std::string cjFile() { return "clusterj-" + verJar(); }
  static const std::string cjBuildJar() { return jarBuildDir() + cjFile(); }

  // Paths for crund
  static std::string crundJar() { return javaCrundDir() + "crund-" + verJar(); }
  static const std::string runClass() { return javaRunClassPrefix; }
  static const std::string crundCpp() { return cppCrundDir() + "crundAB"; }

  // Paths for libndbclient
  static const std::string libBuildDir() { return binDir() + libraryDir; }

  // mysql client
  static std::string mysql() { return binDir() + runtimeDir + sep() + "mysql"; }

  // Test results and properties file
  static std::string resultsDir() { return binDir() + "crund-results" + sep(); }
  static std::string propsFile() { return resultsDir() + "test.properties"; }

  static const std::string generateLogFileName() {
    char timestamp[dateFormatSize];
    const time_t now = time(nullptr);
    (void)strftime(timestamp, dateFormatSize, dateFormat, localtime(&now));
    return logPrefix() + timestamp + "_";
  }
};

static std::stringstream initialPropertyList(
    "com.mysql.clusterj.jdbc.username=root\n"
    "com.mysql.clusterj.jdbc.password=\n"
    "com.mysql.clusterj.max.transactions=1024\n"
    "com.mysql.clusterj.database=crunddb\n"
    "jdbc.driver=com.mysql.cj.jdbc.Driver\n"
    "jdbc.user=root\n"
    "loads=NdbjtieAB,ClusterjAB\n"
    "xMode=indy,each,bulk\n");

static utils::Properties properties;
static NdbProcess::Args extraArgs;

void init_properties() {
  const char *connStr = getMgmStr();
  const char *mysqlStr = getMySQLStr();

  std::stringstream p1, p2, p3, p4;
  p1 << "com.mysql.clusterj.connectstring=" << connStr << std::endl;
  properties.load(p1);

  p2 << "ndb.mgmdConnect=" << connStr << std::endl;
  properties.load(p2);

  p3 << "com.mysql.clusterj.jdbc.url=jdbc:mysql://" << mysqlStr << "/crunddb\n";
  properties.load(p3);

  p4 << "jdbc.url=jdbc:mysql://" << mysqlStr
     << "/crunddb?allowMultiQueries=true\n";
  properties.load(p4);

  properties.load(initialPropertyList);
}

void process_args(int argc, char **argv) {
  init_properties();
  int passArg = argc;

  for (int i = 1; i < argc; i++) {
    const std::string arg = argv[i];
    if (arg.compare("-h") == 0 || arg.compare("--help") == 0) {
      puts(helpText);
      exit(1);
    }

    if (i < argc && arg.compare("-p") == 0)
      properties.load(argv[i + 1]);  // read file
    else if (arg[0] == '-') {
      passArg = i;
      break;
    } else {
      std::stringstream p(arg);
      properties.load(p);
    }
  }

  for (int i = passArg; i < argc; i++) extraArgs.add(argv[i]);
}

int run_crund_cpp(const std::string &logFile) {
  NdbProcess::Args args;
  args.add2("-p", Paths::propsFile().c_str());
  args.add2("-l", logFile.c_str());
  args.add(extraArgs);
  setenv("LD_LIBRARY_PATH", Paths::libBuildDir().c_str(), 1);
  setenv("DYLD_LIBRARY_PATH", Paths::libBuildDir().c_str(), 1);
  auto proc = NdbProcess::create("crund-cpp", Paths::crundCpp().c_str(),
                                 Paths::resultsDir().c_str(), args);
  int ret;
  proc->wait(ret, 10000000);
  return ret;
}

int run_crund_java(const std::string &classPath, const std::string &logFile,
                   const char *crundLoad) {
  /* Create the arguments to the Java command line */
  NdbProcess::Args args;
  args.add("-Djava.library.path=", Paths::libBuildDir().c_str());
  args.add2("-cp", classPath.c_str());
  args.add((Paths::runClass() + crundLoad).c_str());
  args.add2("-p", Paths::propsFile().c_str());
  args.add2("-l", logFile.c_str());
  args.add(extraArgs);

  /* Run Crund */
  auto proc = NdbProcess::create("crund-java", "java",
                                 Paths::resultsDir().c_str(), args);
  int ret;
  proc->wait(ret, 10000000);
  return ret;
}

int analyze_logs(const std::string &classPath,
                 const std::vector<std::string> &logs) {
  NdbProcess::Args args;
  args.add2("-cp", classPath.c_str());
  args.add("com.mysql.cluster.crund.ResultProcessor");
  args.add2("-w", 2);                  // check whether iterations < 3 ?
  for (const std::string &log : logs)  // input files
    args.add((Paths::resultsDir() + log).c_str());

  /* Run ResultProcessor */
  int ret;
  auto proc = NdbProcess::create("ResultProcessor", "java",
                                 Paths::resultsDir().c_str(), args);
  proc->wait(ret, 10000);

  return ret;
}

void import_results() {
  NdbProcess::Args args;
  if (envIsMtr) {  // otherwise maybe a client my.cnf will get us there...
    args.add("--port=13001");
    args.add2("-u", "root");
  }
  args.add("--local-infile=1");
  args.add2("-e",
            "LOAD DATA LOCAL INFILE 'log_results.csv' INTO TABLE results"
            " fields terminated by ',' ignore 1 lines");
  args.add("crunddb");

  int ret;
  auto proc = NdbProcess::create("Store Results", Paths::mysql().c_str(),
                                 Paths::resultsDir().c_str(), args);
  proc->wait(ret, 10000);
}

std::string get_classpath() {
  static constexpr const char *Separator = CLASS_PATH_SEP;

  /* Create the CLASSPATH */
  std::string classpath = Paths::cjBuildJar() + Separator + Paths::crundJar();
  const char *mtr_classpath = getenv("MTR_CLASSPATH");
  if (mtr_classpath) {
    classpath += Separator;
    classpath += mtr_classpath;
  } else if (strlen(compileTimeClassPath) > 0) {
    classpath += Separator;
    classpath += compileTimeClassPath;
  }
  printf("Java Classpath: %s \n", classpath.c_str());
  return classpath;
}

int run_tests() {
  std::string ndbClientDir = Paths::libBuildDir();
  std::string outFileBase = Paths::generateLogFileName();
  std::vector<std::string> resultLogs;

  /* Fail here if no clusterj-test JAR file */
  if (!File_class::exists(Paths::crundJar().c_str())) {
    fprintf(stderr, "Cannot find jar file '%s'\n", Paths::crundJar().c_str());
    return -1;
  }

  /* Fail here if no results directory */
  if (!File_class::exists(Paths::resultsDir().c_str())) {
    fprintf(stderr, "You must first create the results directory:\n   %s\n",
            Paths::resultsDir().c_str());
    return -1;
  }

  std::string classpath = get_classpath();

  /* Create properties file */
  properties.store(Paths::propsFile().c_str());

  int ret = 0;
  int nRun = 0;
  std::string loads(toS(properties[L"loads"]));

  /* Run C++ Crund */
  if (loads.find("NdbapiAB") != loads.npos) {
    resultLogs.push_back(outFileBase + "ndbapi_log.txt");
    ret = run_crund_cpp(resultLogs.back());
    nRun += ret ? 0 : 1;
  }

  /* Run Java Crund */
  for (const char *load : javaLoads) {
    if (ret == 0 && loads.find(load) != loads.npos) {
      resultLogs.push_back(outFileBase + load + "_log.txt");
      ret = run_crund_java(classpath, resultLogs.back(), load);
      nRun += ret ? 0 : 1;
    }
  }

  printf("Ran %d load%s.\n", nRun, nRun == 1 ? "" : "s");
  if (nRun == 0) {
    printf(
        "The supported loads are: \n"
        "   NdbapiAB, NdbjtieAB, NdbjtieS, "
        "ClusterjAB, ClusterjS, JdbcAB, JdbcS"
        "\n");
    return -1;
  }

  if (ret != 0) return ret;

  /* Analyze log files if 3 or more runs */
  if (toI(properties[L"nRuns"], 0, -1) > 2) {
    printf("\n Running ResultProcessor:\n");
    ret = analyze_logs(classpath, resultLogs);

    if (ret != 0) return ret;

    /* Save the results in MySQL; don't mind if this fails. */
    printf("\n Load results back into database:\n");
    import_results();

    /* Rename the results file */
    printf("\n Renaming:  log_results.csv -> %sresults.csv\n",
           outFileBase.c_str());
    File_class::rename(
        (Paths::resultsDir() + "log_results.csv").c_str(),
        (Paths::resultsDir() + outFileBase + "results.csv").c_str());
  }

  return ret;
}

int main(int argc, char **argv) {
  ndb_init();
  process_args(argc, argv);
  int status = run_tests();
  ndb_end(0);
  return status;
}
