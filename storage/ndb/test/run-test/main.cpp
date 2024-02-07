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

#include <time.h>

#ifdef _WIN32
#define DEFAULT_PREFIX "c:/atrt"
#endif

#include <NdbAutoPtr.hpp>
#include <NdbOut.hpp>
#include "atrt.hpp"
#include "process_management.hpp"
#include "test_execution_resources.hpp"

#include <FileLogHandler.hpp>
#include <SysLogHandler.hpp>
#include <util/File.hpp>

#include <NdbSleep.h>
#include <ndb_opts.h>
#include <ndb_version.h>
#include <vector>
#include "my_alloc.h"  // MEM_ROOT
#include "my_io.h"     // FN_REFLEN
#include "my_sys.h"    // my_realpath()
#include "template_utils.h"
#include "typelib.h"

#define PATH_SEPARATOR DIR_SEPARATOR
#define TESTCASE_RETRIES_THRESHOLD_WARNING 5
#define ATRT_VERSION_NUMBER 11

/** Global variables */
static const char progname[] = "ndb_atrt";
static const char *g_gather_progname = 0;
static const char *g_analyze_progname = 0;
static const char *g_setup_progname = 0;
static const char *g_analyze_coverage_progname = 0;
static const char *g_compute_coverage_progname = 0;

static const char *g_log_filename = 0;
static const char *g_test_case_filename = 0;
static const char *g_report_filename = 0;

static int g_do_setup = 0;
static int g_do_deploy = 0;
static int g_do_sshx = 0;
static int g_do_start = 0;
static int g_do_quit = 0;

static int g_help = 0;
static int g_verbosity = 1;
static FILE *g_report_file = 0;
static FILE *g_test_case_file = stdin;
static int g_mode = 0;

Logger g_logger;
atrt_config g_config;
const char *g_user = 0;
int g_baseport = 10000;
int g_fqpn = 0;
int g_fix_nodeid = 0;
int g_default_ports = 0;
int g_mt = 0;
int g_mt_rr = 0;
int g_restart = 0;
int g_default_max_retries = 0;
bool g_clean_shutdown = false;

FailureMode g_default_behaviour_on_failure = Restart;
const char *default_behaviour_on_failure[] = {"Restart", "Abort", "Skip",
                                              "Continue", nullptr};
TYPELIB behaviour_typelib = {array_elements(default_behaviour_on_failure) - 1,
                             "default_behaviour_on_failure",
                             default_behaviour_on_failure, nullptr};

RestartMode g_default_force_cluster_restart = None;
const char *force_cluster_restart_mode[] = {"none", "before", "after", "both",
                                            nullptr};
TYPELIB restart_typelib = {array_elements(force_cluster_restart_mode) - 1,
                           "force_cluster_restart_mode",
                           force_cluster_restart_mode, nullptr};

coverage::Coverage g_coverage = coverage::Coverage::None;
const char *coverage_mode[] = {"none", "testcase", "testsuite", nullptr};
TYPELIB coverage_typelib = {array_elements(coverage_mode) - 1, "coverage_mode",
                            coverage_mode, nullptr};

CoverageTools g_coverage_tool = Lcov;
const char *coverage_tools[] = {"lcov", "fastcov", nullptr};
TYPELIB coverage_tools_typelib = {array_elements(coverage_tools) - 1,
                                  "coverage_tools", coverage_tools, nullptr};

const char *g_cwd = 0;
const char *g_basedir = 0;
const char *g_my_cnf = 0;
const char *g_prefix = NULL;
const char *g_prefix0 = NULL;
const char *g_prefix1 = NULL;
const char *g_build_dir = NULL;
const char *g_clusters = 0;
const char *g_config_type = NULL;  // "cnf" or "ini"
const char *g_site = NULL;
BaseString g_replicate;
const char *save_file = 0;
const char *save_group_suffix = 0;
const char *g_dummy;
char *g_env_path = 0;
const char *g_mysqld_host = 0;

TestExecutionResources g_resources;

static BaseString get_atrt_path(const char *arg);

const char *g_search_path[] = {"bin", "libexec",   "sbin", "scripts",
                               "lib", "lib/mysql", 0};
static bool find_scripts(const char *path);
static bool find_config_ini_files();

TestResult run_test_case(ProcessManagement &processManagement,
                         const atrt_testcase &testcase, bool is_last_testcase,
                         RestartMode next_testcase_forces_restart,
                         atrt_coverage_config &coverage_config);
int test_case_init(ProcessManagement &, const atrt_testcase &);
int test_case_execution_loop(ProcessManagement &, const time_t, const time_t);
void test_case_results(TestResult *, const atrt_testcase &);

void test_case_coverage_results(TestResult *, atrt_config &,
                                atrt_coverage_config &, int);
bool gather_coverage_results(atrt_config &, atrt_coverage_config &,
                             int test_case = 0);
int compute_path_level(const char *);
int compute_test_coverage(atrt_coverage_config &, const char *);

bool do_command(ProcessManagement &processManagement, atrt_config &config);
bool setup_test_case(ProcessManagement &processManagement, atrt_config &,
                     const atrt_testcase &);
/**
 * check configuration if any changes has been
 *   done for the duration of the latest running test
 *   if so, return true, and reset those changes
 *   (true, indicates that a restart is needed to actually
 *    reset the running processes)
 */
bool reset_config(ProcessManagement &processManagement, atrt_config &);

static struct my_option g_options[] = {
    {"help", '?', "Display this help and exit.", &g_help, &g_help, 0, GET_BOOL,
     NO_ARG, 0, 0, 0, 0, 0, 0},
    {"version", 'V', "Output version information and exit.", 0, 0, 0, GET_BOOL,
     NO_ARG, 0, 0, 0, 0, 0, 0},
    NdbStdOpt::tls_search_path,
    NdbStdOpt::mgm_tls,
    {"site", 256, "Site", &g_site, &g_site, 0, GET_STR, REQUIRED_ARG, 0, 0, 0,
     0, 0, 0},
    {"clusters", 256, "Cluster", &g_clusters, &g_clusters, 0, GET_STR,
     REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
    {"config-type", 256, "cnf (default) or ini", &g_config_type, &g_config_type,
     0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
    {"mysqld", 256, "atrt mysqld", &g_mysqld_host, &g_mysqld_host, 0, GET_STR,
     REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
    {"replicate", 1024, "replicate", &g_dummy, &g_dummy, 0, GET_STR,
     REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
    {"log-file", 256, "log-file", &g_log_filename, &g_log_filename, 0, GET_STR,
     REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
    {"testcase-file", 'f', "testcase-file", &g_test_case_filename,
     &g_test_case_filename, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
    {"report-file", 'r', "report-file", &g_report_filename, &g_report_filename,
     0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
    {"basedir", 256, "Base path", &g_basedir, &g_basedir, 0, GET_STR,
     REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
    {"baseport", 256, "Base port", &g_baseport, &g_baseport, 0, GET_INT,
     REQUIRED_ARG, g_baseport, 0, 0, 0, 0, 0},
    {"prefix", 256, "atrt install dir", &g_prefix, &g_prefix, 0, GET_STR,
     REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
    {"prefix0", 256, "mysql install dir", &g_prefix0, &g_prefix0, 0, GET_STR,
     REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
    {"prefix1", 256, "mysql install dir 1", &g_prefix1, &g_prefix1, 0, GET_STR,
     REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
    {"verbose", 'v', "Verbosity", &g_verbosity, &g_verbosity, 0, GET_INT,
     REQUIRED_ARG, g_verbosity, 0, 0, 0, 0, 0},
    {"configure", 256, "configure", &g_do_setup, &g_do_setup, 0, GET_INT,
     REQUIRED_ARG, g_do_setup, 0, 0, 0, 0, 0},
    {"deploy", 256, "deploy", &g_do_deploy, &g_do_deploy, 0, GET_INT,
     REQUIRED_ARG, g_do_deploy, 0, 0, 0, 0, 0},
    {"sshx", 256, "sshx", &g_do_sshx, &g_do_sshx, 0, GET_INT, REQUIRED_ARG,
     g_do_sshx, 0, 0, 0, 0, 0},
    {"start", 256, "start", &g_do_start, &g_do_start, 0, GET_INT, REQUIRED_ARG,
     g_do_start, 0, 0, 0, 0, 0},
    {"fqpn", 256, "Fully qualified path-names ", &g_fqpn, &g_fqpn, 0, GET_INT,
     REQUIRED_ARG, g_fqpn, 0, 0, 0, 0, 0},
    {"fix-nodeid", 256, "Fix nodeid for each started process ", &g_fix_nodeid,
     &g_fix_nodeid, 0, GET_INT, REQUIRED_ARG, g_fqpn, 0, 0, 0, 0, 0},
    {"default-ports", 256, "Use default ports when possible", &g_default_ports,
     &g_default_ports, 0, GET_INT, REQUIRED_ARG, g_default_ports, 0, 0, 0, 0,
     0},
    {"mode", 256, "Mode 0=interactive 1=regression 2=bench", &g_mode, &g_mode,
     0, GET_INT, REQUIRED_ARG, g_mode, 0, 0, 0, 0, 0},
    {"quit", 256, "Quit before starting tests", &g_do_quit, &g_do_quit, 0,
     GET_BOOL, NO_ARG, g_do_quit, 0, 0, 0, 0, 0},
    {"mt", 256, "Use ndbmtd (0 = never, 1 = round-robin, 2 = only)", &g_mt,
     &g_mt, 0, GET_INT, REQUIRED_ARG, g_mt, 0, 0, 0, 0, 0},
    {"default-max-retries", 256,
     "default number of retries after a test case fails (can be overwritten in "
     "the test suite file)",
     &g_default_max_retries, &g_default_max_retries, 0, GET_INT, REQUIRED_ARG,
     g_default_max_retries, 0, 0, 0, 0, 0},
    {"default-force-cluster-restart", 256,
     "Force cluster to restart for each testrun (can be overwritten in test "
     "suite file)",
     &g_default_force_cluster_restart, &g_default_force_cluster_restart,
     &restart_typelib, GET_ENUM, REQUIRED_ARG, g_default_force_cluster_restart,
     0, 0, 0, 0, 0},
    {"default-behaviour-on-failure", 256, "default to do when a test fails",
     &g_default_behaviour_on_failure, &g_default_behaviour_on_failure,
     &behaviour_typelib, GET_ENUM, REQUIRED_ARG, g_default_behaviour_on_failure,
     0, 0, 0, 0, 0},
    {"clean-shutdown", 0,
     "Enables clean cluster shutdown when passed as a command line argument",
     &g_clean_shutdown, &g_clean_shutdown, 0, GET_BOOL, NO_ARG,
     g_clean_shutdown, 0, 0, 0, 0, 0},
    {"coverage", 256,
     "Enables coverage and specifies if coverage is computed, "
     "per 'testcase' (default) or  per 'testsuite'.",
     &g_coverage, &g_coverage, &coverage_typelib, GET_ENUM, OPT_ARG, g_coverage,
     0, 0, 0, 0, 0},
    {"build-dir", 256, "Full path to build directory which contains gcno files",
     &g_build_dir, &g_build_dir, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
    {"coverage-tool", 256,
     "Specifies if coverage is computed using 'lcov'(default) or 'fastcov'",
     &g_coverage_tool, &g_coverage_tool, &coverage_tools_typelib, GET_ENUM,
     REQUIRED_ARG, g_coverage_tool, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}};

static int check_testcase_file_main(int argc, char **argv);
static void print_testcase_file_syntax();

int main(int argc, char **argv) {
  ndb_init();

  AtrtExitCodes return_code = AtrtExitCodes::TESTSUITE_SUCCESS;

  g_logger.setCategory(progname);
  g_logger.enable(Logger::LL_ALL);
  g_logger.createConsoleHandler();

  // If program is called with --check-testcase-files as first option
  // it is assumed that the rest of command line arguments are
  // testcase-filenames and those files will be syntax checked.
  if (argc >= 2 && strcmp(argv[1], "--check-testcase-files") == 0) {
    exit(check_testcase_file_main(argc, argv));
  }

  MEM_ROOT alloc = MEM_ROOT{PSI_NOT_INSTRUMENTED, 512};
  if (!parse_args(argc, argv, &alloc)) {
    g_logger.critical("Failed to parse arguments");
    return atrt_exit(ATRT_FAILURE);
  }

  g_logger.info("Starting ATRT version : %s", getAtrtVersion().c_str());

  atrt_coverage_config coverage_config = {0, g_coverage, g_coverage_tool};

  if (coverage_config.m_analysis != coverage::Coverage::None) {
    if (g_default_force_cluster_restart == Before ||
        g_default_force_cluster_restart == Both) {
      g_logger.critical(
          "Conflicting cluster restart parameter used with coverage parameter");
      return atrt_exit(ATRT_FAILURE);
    }
    g_default_force_cluster_restart = After;
    g_clean_shutdown = true;

    if (g_build_dir == nullptr) {
      g_logger.critical(
          "--build-dir parameter is required for coverage builds");
      return atrt_exit(ATRT_FAILURE);
    }
    struct stat buf;
    if (lstat(g_build_dir, &buf) != 0) {
      g_logger.critical(
          "Build directory does not exist at location specified "
          "in --build-dir parameter");
      return atrt_exit(ATRT_FAILURE);
    }
    coverage_config.m_prefix_strip = compute_path_level(g_build_dir);
  }

  if (g_mt != 0) {
    g_resources.setRequired(g_resources.NDBMTD);
  }

  {
    std::vector<std::string> error;
    std::vector<std::string> info;
    if (!g_resources.loadPaths(g_prefix0, g_prefix1, &error, &info)) {
      g_logger.critical("Failed to find required binaries for execution");

      for (auto msg : error) {
        g_logger.critical("%s", msg.c_str());
      }
      return atrt_exit(ATRT_FAILURE);
    }

    for (auto msg : info) {
      g_logger.info("%s", msg.c_str());
    }
  }

  {
    BaseString atrt_path = get_atrt_path(argv[0]);
    assert(atrt_path != "");

    if (!find_scripts(atrt_path.c_str())) {
      g_logger.critical("Failed to find required atrt scripts for execution");
      return atrt_exit(ATRT_FAILURE);
    }
  }

  g_config.m_config_type = atrt_config::CNF;
  if (g_config_type != NULL && strcmp(g_config_type, "ini") == 0) {
    g_logger.info("Using config.ini for cluster configuration");
    g_config.m_config_type = atrt_config::INI;

    if (!find_config_ini_files()) {
      g_logger.critical("Failed to find required config.ini files");
      return atrt_exit(ATRT_FAILURE);
    }
  }

  g_config.m_generated = false;
  g_config.m_replication = g_replicate;
  if (!setup_config(g_config, coverage_config, g_mysqld_host,
                    g_clean_shutdown)) {
    g_logger.critical("Failed to setup configuration");
    return atrt_exit(ATRT_FAILURE);
  }

  if (!g_config.m_processes.size()) {
    g_logger.critical("Error: No processes defined in cluster configuration");
    return atrt_exit(ATRT_FAILURE);
  }

  if (!configure(g_config, g_do_setup)) {
    g_logger.critical("Failed to configure");
    return atrt_exit(ATRT_FAILURE);
  }

  g_logger.info("Setting up directories...");
  if (!setup_directories(g_config, g_do_setup)) {
    g_logger.critical("Failed to set up directories");
    return atrt_exit(ATRT_FAILURE);
  }

  if (g_do_setup) {
    g_logger.info("Setting up files...");
    if (!setup_files(g_config, g_do_setup, g_do_sshx)) {
      g_logger.critical("Failed to set up files");
      return atrt_exit(ATRT_FAILURE);
    }
  }

  if (g_do_deploy) {
    g_logger.info("Deploying files...");
    if (!deploy(g_do_deploy, g_config)) {
      g_logger.critical("Failed to deploy");
      return atrt_exit(ATRT_FAILURE);
    }
  }

  if (g_do_quit) {
    return atrt_exit(TESTSUITE_SUCCESS);
  }

  if (!setup_hosts(g_config)) {
    g_logger.critical("Failed to setup hosts");
    return atrt_exit(ATRT_FAILURE);
  }

  ProcessManagement processManagement(g_config, g_setup_progname);

  if (g_do_sshx) {
    g_logger.info("Starting xterm-ssh");
    if (!sshx(g_config, g_do_sshx)) {
      g_logger.critical("Failed to start xterm-ssh");
      return atrt_exit(ATRT_FAILURE);
    }

    g_logger.info("Done...sleeping");
    while (true) {
      if (!do_command(processManagement, g_config)) {
        g_logger.critical("Failed to do ssh command");
        return atrt_exit(ATRT_FAILURE);
      }

      NdbSleep_SecSleep(1);
    }
    return atrt_exit(TESTSUITE_SUCCESS);
  }

  /**
   * contact each ndb_cpcd
   */
  g_logger.info("Connecting to hosts...");
  if (!connect_hosts(g_config)) {
    g_logger.critical("Failed to connect to CPCD on hosts");
    return atrt_exit(ATRT_FAILURE);
  }

  /**
   * Collect all the testcases
   */
  std::vector<atrt_testcase> testcases;
  if (!read_test_cases(g_test_case_file, &testcases)) {
    g_logger.critical("Failed to read all the testcases");
    return atrt_exit(ATRT_FAILURE);
  }

  switch (coverage_config.m_analysis) {
    case coverage::Coverage::Testcase:
      g_logger.info("Running coverage analysis per test case");
      break;
    case coverage::Coverage::Testsuite:
      g_logger.info("Running coverage analysis per test suite");
      break;
    case coverage::Coverage::None:
      break;
  }

  if (coverage_config.m_analysis != coverage::Coverage::None) {
    const char *coverage_tool =
        (g_coverage_tool == CoverageTools::Lcov) ? "lcov" : "fastcov";
    g_logger.info("Using %s for coverage analysis", coverage_tool);
  }

  /**
   * Run all tests
   */

  g_logger.debug("Entering main loop");
  FailureMode current_failure_mode = FailureMode::Continue;
  unsigned int last_testcase_idx = testcases.size() - 1;
  for (unsigned int i = 0; i <= last_testcase_idx; i++) {
    atrt_testcase testcase = testcases[i];
    g_logger.info("#%d - %s", testcase.test_no, testcase.m_name.c_str());

    TestResult test_result;
    bool is_last_testcase = (last_testcase_idx == i);
    if (current_failure_mode == FailureMode::Skip) {
      test_result = {0, 0, ERR_TEST_SKIPPED};
    } else {
      RestartMode next_testcase_forces_restart = None;
      if (!is_last_testcase) {
        next_testcase_forces_restart = testcases[i + 1].m_force_cluster_restart;
      }
      test_result =
          run_test_case(processManagement, testcase, is_last_testcase,
                        next_testcase_forces_restart, coverage_config);
      if (test_result.result != ErrorCodes::ERR_OK) {
        current_failure_mode = testcase.m_behaviour_on_failure;
      }
    }
    update_atrt_result_code(test_result, &return_code);

    if (g_report_file != 0) {
      fprintf(g_report_file, "%s ; %d ; %d ; %ld ; %d\n",
              testcase.m_name.c_str(), testcase.test_no, test_result.result,
              test_result.elapsed, test_result.testruns);
      fflush(g_report_file);
    }

    if (g_mode == 0 && test_result.result != ERR_OK) {
      g_logger.info("Encountered failed test in interactive mode");
    }

    const char *test_status = get_test_status(test_result.result);
    g_logger.info("#%d %s(%d)", testcase.test_no, test_status,
                  test_result.result);

    if (current_failure_mode == FailureMode::Abort) {
      g_logger.info("Aborting the test suite execution!");
      break;
    }
  }

  if (coverage_config.m_analysis != coverage::Coverage::None) {
    if (testcases.empty()) {
      g_logger.debug("No testcases were run to compute coverage report");
    } else {
      if (g_coverage == coverage::Coverage::Testsuite) {
        gather_coverage_results(g_config, coverage_config);
      }
      g_logger.debug("Computing coverage report..");
      if (compute_test_coverage(coverage_config, g_build_dir) == 0) {
        g_logger.debug("Coverage report generated for the run!!");
      }
    }
  }

  if (g_report_file != 0) {
    fclose(g_report_file);
    g_report_file = 0;
  }

  g_logger.info("Finishing, result: %d", return_code);
  return return_code;
}

extern "C" bool get_one_option(int arg, const struct my_option *opt,
                               char *value) {
  if (arg == 1024) {
    if (g_replicate.length()) g_replicate.append(";");
    g_replicate.append(value);
    return 0;
  }
  return 0;
}

bool parse_args(int argc, char **argv, MEM_ROOT *alloc) {
  bool fail_after_help = false;
  char buf[2048];

  if (argc >= 2 &&
      (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-V") == 0)) {
    ndbout << getAtrtVersion().c_str() << endl;
    exit(0);
  }

  if (getcwd(buf, sizeof(buf)) == 0) {
    g_logger.error("Unable to get current working directory");
    return false;
  }
  g_cwd = strdup(buf);

  struct stat sbuf;
  BaseString mycnf;
  mycnf.append(g_cwd);
  mycnf.append(DIR_SEPARATOR);

  if (argc > 1 && lstat(argv[argc - 1], &sbuf) == 0) {
    mycnf.append(argv[argc - 1]);
  } else {
    mycnf.append("my.cnf");
    if (lstat(mycnf.c_str(), &sbuf) != 0) {
      g_logger.error(
          "Could not find out which config file to use! "
          "Pass it as last argument to atrt: 'atrt <config file>' "
          "(default: '%s')",
          mycnf.c_str());
      fail_after_help = true;
    }
  }

  to_fwd_slashes((char *)g_cwd);

  g_logger.info("Bootstrapping using %s", mycnf.c_str());

  const char *groups[] = {"atrt", 0};
  int ret = load_defaults(mycnf.c_str(), groups, &argc, &argv, alloc);

  if (ret) {
    g_logger.error("Failed to load defaults, returned (%d)", ret);
    return false;
  }

  save_file = my_defaults_file;
  save_group_suffix = my_defaults_group_suffix;

  if (my_defaults_extra_file) {
    g_logger.error("--defaults-extra-file(%s) is not supported...",
                   my_defaults_extra_file);
    return false;
  }

  ret = handle_options(&argc, &argv, g_options, get_one_option);
  if (ret) {
    g_logger.error("handle_options failed, ret: %d, argc: %d, *argv: '%s'", ret,
                   argc, *argv);
    return false;
  }

  if (argc >= 2) {
    const char *arg = argv[argc - 2];
    while (*arg) {
      switch (*arg) {
        case 'c':
          g_do_setup = (g_do_setup == 0) ? 1 : g_do_setup;
          break;
        case 'C':
          g_do_setup = 2;
          break;
        case 'd':
          g_do_deploy = 3;
          break;
        case 'D':
          g_do_deploy = 2;  // only binaries
          break;
        case 'x':
          g_do_sshx = atrt_process::AP_CLIENT | atrt_process::AP_NDB_API;
          break;
        case 'X':
          g_do_sshx = atrt_process::AP_ALL;
          break;
        case 's':
          g_do_start = ProcessManagement::P_NDB;
          break;
        case 'S':
          g_do_start = ProcessManagement::P_NDB | ProcessManagement::P_SERVERS;
          break;
        case 'f':
          g_fqpn = 1;
          break;
        case 'z':
          g_fix_nodeid = 1;
          break;
        case 'q':
          g_do_quit = 1;
          break;
        case 'r':
          g_restart = 1;
          break;
        default:
          g_logger.error("Unknown switch '%c'", *arg);
          return false;
      }
      arg++;
    }
  }

  if (g_log_filename != 0) {
    g_logger.removeConsoleHandler();
    g_logger.addHandler(new FileLogHandler(g_log_filename));
  }

  {
    int tmp = Logger::LL_WARNING - g_verbosity;
    tmp = (tmp < Logger::LL_DEBUG ? Logger::LL_DEBUG : tmp);
    g_logger.disable(Logger::LL_ALL);
    g_logger.enable(Logger::LL_ON);
    g_logger.enable((Logger::LoggerLevel)tmp, Logger::LL_ALERT);
  }

  if (!g_basedir) {
    g_basedir = g_cwd;
    g_logger.info("basedir not specified, using %s", g_basedir);
  } else {
    g_logger.info("basedir, %s", g_basedir);
  }

  const char *default_prefix;
  if (g_prefix != NULL) {
    default_prefix = g_prefix;
  } else if (g_prefix0 != NULL) {
    default_prefix = g_prefix0;
  } else {
    default_prefix = DEFAULT_PREFIX;
  }

  if (g_prefix == NULL) {
    g_prefix = DEFAULT_PREFIX;
  }

  if (g_prefix0 == NULL) {
    g_prefix0 = DEFAULT_PREFIX;
  }

  /**
   * Add path to atrt-*.sh
   */
  {
    BaseString tmp;
    const char *env = getenv("PATH");
    if (env && strlen(env)) {
      tmp.assfmt("PATH=%s:%s/mysql-test/ndb", env, g_prefix);
    } else {
      tmp.assfmt("PATH=%s/mysql-test/ndb", g_prefix);
    }
    to_native(tmp);
    g_env_path = strdup(tmp.c_str());
    putenv(g_env_path);
  }

  if (g_help) {
    my_print_help(g_options);
    my_print_variables(g_options);
    print_testcase_file_syntax();
    return 0;
  }
  if (fail_after_help) {
    return false;
  }

  if (g_test_case_filename) {
    g_test_case_file = fopen(g_test_case_filename, "r");
    if (g_test_case_file == 0) {
      g_logger.critical("Unable to open file: %s", g_test_case_filename);
      return false;
    }
    if (g_do_setup == 0) g_do_setup = 2;

    if (g_do_start == 0)
      g_do_start = ProcessManagement::P_NDB | ProcessManagement::P_SERVERS;

    if (g_mode == 0) g_mode = 1;

    if (g_do_sshx) {
      g_logger.critical("ssx specified...not possible with testfile");
      return false;
    }
  } else {
    g_logger.info(
        "No test case file given with -f <test file>, "
        "running in interactive mode from stdin");
  }

  if (g_do_setup == 0) {
    BaseString tmp;
    tmp.append(g_basedir);
    tmp.append(PATH_SEPARATOR);
    tmp.append("my.cnf");
    if (lstat(tmp.c_str(), &sbuf) != 0) {
      g_logger.error(
          "Could not find a my.cnf file in the basedir '%s', "
          "you probably need to configure it with "
          "'atrt --configure=1 <config_file>'",
          g_basedir);
      return false;
    }

    if (!S_ISREG(sbuf.st_mode)) {
      g_logger.error("%s is not a regular file", tmp.c_str());
      return false;
    }

    g_my_cnf = strdup(tmp.c_str());
    g_logger.info("Using %s", tmp.c_str());
  } else {
    g_my_cnf = strdup(mycnf.c_str());
  }

  if (g_prefix1) {
    g_logger.info("Using --prefix1=\"%s\"", g_prefix1);
  }

  if (g_report_filename) {
    g_report_file = fopen(g_report_filename, "w");
    if (g_report_file == 0) {
      g_logger.critical("Unable to create report file: %s", g_report_filename);
      return false;
    }
  }

  if (g_clusters == 0) {
    g_logger.critical("No clusters specified");
    return false;
  }

  /* Read username from environment, default to sakila */
  const char *logname = getenv("LOGNAME");
  if ((logname != nullptr) && (strlen(logname) > 0)) {
    g_user = strdup(logname);
  } else {
    g_user = "sakila";
    g_logger.info(
        "No default user specified, will use 'sakila'. "
        "Please set LOGNAME environment variable for other username");
  }

  return true;
}

std::string getAtrtVersion() {
  int mysql_version = ndbGetOwnVersion();
  std::string version = std::to_string(ndbGetMajor(mysql_version)) + "." +
                        std::to_string(ndbGetMinor(mysql_version)) + "." +
                        std::to_string(ndbGetBuild(mysql_version)) + "." +
                        std::to_string(ATRT_VERSION_NUMBER);
  return version;
}

bool connect_hosts(atrt_config &config) {
  for (unsigned i = 0; i < config.m_hosts.size(); i++) {
    if (config.m_hosts[i]->m_hostname.length() == 0) continue;

    if (config.m_hosts[i]->m_cpcd->connect() != 0) {
      g_logger.error("Unable to connect to cpc %s:%d",
                     config.m_hosts[i]->m_cpcd->getHost(),
                     config.m_hosts[i]->m_cpcd->getPort());
      return false;
    }
    g_logger.debug("Connected to %s:%d", config.m_hosts[i]->m_cpcd->getHost(),
                   config.m_hosts[i]->m_cpcd->getPort());
  }

  return true;
}

bool is_client_running(atrt_config &config) {
  for (unsigned i = 0; i < config.m_processes.size(); i++) {
    atrt_process &proc = *config.m_processes[i];
    if ((ProcessManagement::P_CLIENTS & proc.m_type) != 0 &&
        proc.m_proc.m_status == "running") {
      return true;
    }
  }
  return false;
}

const char *get_test_status(int result) {
  switch (result) {
    case ErrorCodes::ERR_OK:
      return "OK";
    case ErrorCodes::ERR_TEST_SKIPPED:
      return "SKIPPED";
    case ErrorCodes::ERR_CRITICAL:
      return "CRITICAL";
  }
  return "FAILED";
}

int atrt_exit(int return_code) {
  g_logger.info("Finishing, result: %d", return_code);
  return return_code;
}

bool read_test_cases(FILE *file, std::vector<atrt_testcase> *testcases) {
  int lineno = 1;
  int test_no = 1;
  while (!feof(file)) {
    atrt_testcase testcase;
    const int num_element_lines = read_test_case(file, lineno, testcase);
    if (num_element_lines == 0) {
      continue;
    }
    if (num_element_lines == ERR_CORRUPT_TESTCASE) {
      g_logger.critical("Corrupted testcase at line %d (error %d)", lineno,
                        num_element_lines);
      return false;
    }
    testcase.test_no = test_no++;
    testcases->push_back(std::move(testcase));
  }

  if (file != stdin) {
    fclose(file);
  }

  return true;
}

TestResult run_test_case(ProcessManagement &processManagement,
                         const atrt_testcase &testcase, bool is_last_testcase,
                         RestartMode next_testcase_forces_restart,
                         atrt_coverage_config &coverage_config) {
  TestResult test_result = {0, 0, 0};
  for (; test_result.testruns <= testcase.m_max_retries;
       test_result.testruns++) {
    if (test_result.testruns > 0) {
      if (test_result.result == ERR_OK ||
          test_result.result == ERR_TEST_SKIPPED) {
        break;
      }
      g_logger.info("Retrying #%d - %s (%d/%d)...", testcase.test_no,
                    testcase.m_name.c_str(), test_result.testruns,
                    testcase.m_max_retries);
    }

    test_result.result = test_case_init(processManagement, testcase);

    if (test_result.result == ERR_OK) {
      const time_t start = time(0);
      test_result.result = test_case_execution_loop(processManagement, start,
                                                    testcase.m_max_time);
      test_result.elapsed = time(0) - start;
    }

    if (!processManagement.stopClientProcesses()) {
      g_logger.critical("Failed to stop client processes");
      test_result.result = ERR_CRITICAL;
    }

    test_case_results(&test_result, testcase);

    bool configuration_reset = reset_config(processManagement, g_config);
    bool restart_on_error =
        test_result.result != ERR_TEST_SKIPPED &&
        test_result.result != ERR_OK &&
        testcase.m_behaviour_on_failure == FailureMode::Restart;

    bool current_testcase_requires_restart =
        testcase.m_force_cluster_restart == RestartMode::After ||
        testcase.m_force_cluster_restart == RestartMode::Both;
    bool next_testcase_requires_restart =
        next_testcase_forces_restart == RestartMode::Before ||
        next_testcase_forces_restart == RestartMode::Both;

    bool stop_cluster = is_last_testcase || current_testcase_requires_restart ||
                        next_testcase_requires_restart || configuration_reset ||
                        restart_on_error;
    if (stop_cluster) {
      g_logger.debug("Stopping all cluster processes on condition(s):");
      if (is_last_testcase) g_logger.debug("- Last test case");
      if (current_testcase_requires_restart)
        g_logger.debug("- Current test case forces restart");
      if (next_testcase_requires_restart)
        g_logger.debug("- Next test case forces restart");
      if (configuration_reset) g_logger.debug("- Configuration forces reset");
      if (restart_on_error) g_logger.debug("- Restart on test error");

      if (!processManagement.stopAllProcesses()) {
        g_logger.critical("Failed to stop all processes");
        test_result.result = ERR_CRITICAL;
      }
    }

    if (coverage_config.m_analysis == coverage::Coverage::Testcase) {
      test_case_coverage_results(&test_result, g_config, coverage_config,
                                 testcase.test_no);
    }
  }

  return test_result;
}

int test_case_init(ProcessManagement &processManagement,
                   const atrt_testcase &testcase) {
  g_logger.debug("Starting test case initialization");

  if (!processManagement.startAllProcesses()) {
    g_logger.critical("Cluster could not be started");
    return ERR_CRITICAL;
  }

  g_logger.info("All servers are running and ready");

  // Assign processes to programs
  if (!setup_test_case(processManagement, g_config, testcase)) {
    g_logger.critical("Failed to setup test case");
    return ERR_CRITICAL;
  }

  if (!processManagement.startClientProcesses()) {
    g_logger.critical("Failed to start client processes");
    return ERR_CRITICAL;
  }

  g_logger.debug("Successful test case initialization");

  return ERR_OK;
}

int test_case_execution_loop(ProcessManagement &processManagement,
                             const time_t start_time,
                             const time_t max_execution_time) {
  g_logger.debug("Starting test case execution loop");

  const time_t stop_time = start_time + max_execution_time;
  int result = ERR_OK;

  do {
    result = processManagement.updateProcessesStatus();
    if (result != ERR_OK) {
      g_logger.critical("Failed to get updated status for all processes");
      return result;
    }

    if (!is_client_running(g_config)) {
      g_logger.debug("Finished test case execution loop");
      return result;
    }

    if (!do_command(processManagement, g_config)) {
      g_logger.critical("Failure on client command execution");
      return ERR_COMMAND_FAILED;
    }

    time_t now = time(0);
    if (now > stop_time) {
      g_logger.info("Timeout after %ld seconds", max_execution_time);
      return ERR_MAX_TIME_ELAPSED;
    }
    NdbSleep_SecSleep(1);
  } while (true);
}

void test_case_results(TestResult *test_result, const atrt_testcase &testcase) {
  int tmp, *rp = test_result->result != ERR_OK ? &tmp : &(test_result->result);
  g_logger.debug("Starting result gathering");

  if (!gather_result(g_config, rp)) {
    g_logger.critical("Failed to gather result after test run");
    test_result->result = ERR_CRITICAL;
  }

  BaseString res_dir;
  res_dir.assfmt("result.%d", testcase.test_no);
  remove_dir(res_dir.c_str(), true);

  if (testcase.m_report || test_result->result != ERR_OK) {
    if (rename("result", res_dir.c_str()) != 0) {
      g_logger.critical("Failed to rename %s as %s", "result", res_dir.c_str());
      remove_dir("result", true);
      test_result->result = ERR_CRITICAL;
    }
  } else {
    remove_dir("result", true);
  }

  g_logger.debug("Finished result gathering");
}

void test_case_coverage_results(TestResult *test_result, atrt_config &config,
                                atrt_coverage_config &coverage_config,
                                int test_number) {
  g_logger.debug("Gathering coverage files");

  if (!gather_coverage_results(config, coverage_config, test_number)) {
    g_logger.critical("Failed to gather coverage result after test run");
    test_result->result = ERR_CRITICAL;
  }
  remove_dir("coverage_result", true);

  g_logger.debug("Finished coverage files gathering");
}

int compute_path_level(const char *g_build_dir) {
  int path_level = 0;
  for (unsigned i = 0; g_build_dir[i] != '\0'; i++) {
    if (g_build_dir[i] == '/' && g_build_dir[i + 1] != '/' &&
        g_build_dir[i + 1] != '\0') {
      path_level++;
    }
  }
  return path_level;
}

int compute_test_coverage(atrt_coverage_config &coverage_config,
                          const char *build_dir) {
  BaseString compute_coverage_cmd = g_compute_coverage_progname;
  compute_coverage_cmd.appfmt(" --results-dir=%s", g_cwd);
  compute_coverage_cmd.appfmt(" --build-dir=%s", build_dir);

  switch (coverage_config.m_tool) {
    case CoverageTools::Lcov:
      compute_coverage_cmd.appfmt(" --coverage-tool=lcov");
      break;
    case CoverageTools::Fastcov:
      compute_coverage_cmd.appfmt(" --coverage-tool=fastcov");
      break;
  }
  const int result = sh(compute_coverage_cmd.c_str());
  if (result != 0) {
    g_logger.critical("Failed to compute coverage report");
    return -1;
  }
  return 0;
}

void update_atrt_result_code(const TestResult &test_result,
                             AtrtExitCodes *return_code) {
  if (*return_code == ATRT_FAILURE) return;

  switch (test_result.result) {
    case ErrorCodes::ERR_OK:
      break;
    case ErrorCodes::ERR_CRITICAL:
      *return_code = ATRT_FAILURE;
      break;
    default:
      *return_code = TESTSUITE_FAILURES;
      break;
  }
}

int insert(const char *pair, Properties &p) {
  BaseString tmp(pair);

  Vector<BaseString> split;
  tmp.split(split, ":=", 2);

  if (split.size() != 2) return -1;

  p.put(split[0].trim().c_str(), split[1].trim().c_str());

  return 0;
}

/*
 * read_test_case - extract one testcase from file
 *
 * On success return a positive number with actual lines describing
 * the test case not counting blank lines and comments.
 * On end of file, it returns 0.
 * On failure, ERR_CORRUPT_TESTCASE is returned.
 */
int read_test_case(FILE *file, int &line, atrt_testcase &tc) {
  Properties p;
  int elements = 0;
  char buf[1024];

  while (!feof(file)) {
    if (file == stdin) printf("atrt> ");
    if (!fgets(buf, 1024, file)) break;

    line++;
    BaseString tmp = buf;

    if (tmp.length() > 0 && tmp.c_str()[0] == '#') continue;

    tmp.trim(" \t\n\r");

    if (tmp.length() == 0) {
      if (elements == 0) {
        continue;  // Blank line before test case definition
      }
      break;  // End of test case definition
    }

    if (insert(tmp.c_str(), p) != 0) {
      // Element line had no : or =
      if (elements == 0 && file == stdin) {
        // Assume a single line command with command and arguments
        // separated with a space
        Vector<BaseString> split;
        tmp.split(split, " ", 2);
        tc.m_cmd.m_exe = split[0];
        if (split.size() == 2)
          tc.m_cmd.m_args = split[1];
        else
          tc.m_cmd.m_args = "";
        tc.m_max_time = 60000;
        return 1;
      }
      g_logger.critical("Invalid test file: Corrupt line: %d: %s", line, buf);
      return ERR_CORRUPT_TESTCASE;
    }

    elements++;
  }

  if (elements == 0) {
    // End of file
    return 0;
  }

  int used_elements = 0;

  if (!p.get("cmd", tc.m_cmd.m_exe)) {
    g_logger.critical(
        "Invalid test file: cmd is missing in test case above line: %d", line);
    return ERR_CORRUPT_TESTCASE;
  }
  used_elements++;

  if (!p.get("args", tc.m_cmd.m_args))
    tc.m_cmd.m_args = "";
  else
    used_elements++;

  const char *mt = 0;
  if (!p.get("max-time", &mt))
    tc.m_max_time = 60000;
  else {
    tc.m_max_time = atoi(mt);
    used_elements++;
  }

  if (p.get("type", &mt)) {
    tc.m_report = (strcmp(mt, "bench") == 0);
    used_elements++;
  } else
    tc.m_report = false;

  if (p.get("run-all", &mt)) {
    tc.m_run_all = (strcmp(mt, "yes") == 0);
    used_elements++;
  } else
    tc.m_run_all = false;

  const char *str;
  if (p.get("mysqld", &str)) {
    tc.m_mysqld_options.assign(str);
    used_elements++;
  } else {
    tc.m_mysqld_options.assign("");
  }

  tc.m_cmd.m_cmd_type = atrt_process::AP_NDB_API;
  if (p.get("cmd-type", &str)) {
    if (strcmp(str, "mysql") == 0)
      tc.m_cmd.m_cmd_type = atrt_process::AP_CLIENT;
    used_elements++;
  }

  if (!p.get("name", &mt)) {
    tc.m_name.assfmt("%s %s", tc.m_cmd.m_exe.c_str(), tc.m_cmd.m_args.c_str());
  } else {
    tc.m_name.assign(mt);
    used_elements++;
  }

  tc.m_force_cluster_restart = g_default_force_cluster_restart;
  if (p.get("force-cluster-restart", &str)) {
    std::map<std::string, RestartMode> restart_mode_values = {
        {"after", RestartMode::After},
        {"before", RestartMode::Before},
        {"both", RestartMode::Both}};
    if (restart_mode_values.find(str) == restart_mode_values.end()) {
      g_logger.critical("Invalid Restart Type!!");
      return ERR_CORRUPT_TESTCASE;
    }
    tc.m_force_cluster_restart = restart_mode_values[str];
    used_elements++;
  }

  tc.m_max_retries = g_default_max_retries;
  if (p.get("max-retries", &mt)) {
    tc.m_max_retries = atoi(mt);
    used_elements++;
  }

  if (tc.m_max_retries < 0) {
    g_logger.error("No of retries must not be less than zero for test '%s'",
                   tc.m_name.c_str());
    return ERR_CORRUPT_TESTCASE;
  }

  if (tc.m_max_retries > TESTCASE_RETRIES_THRESHOLD_WARNING)
    g_logger.warning(
        "No of retries should be less than or equal to %d for test '%s'",
        TESTCASE_RETRIES_THRESHOLD_WARNING, tc.m_name.c_str());

  tc.m_behaviour_on_failure = (FailureMode)g_default_behaviour_on_failure;
  if (p.get("on-failure", &str)) {
    std::map<std::string, FailureMode> failure_mode_values = {
        {"Restart", FailureMode::Restart},
        {"Abort", FailureMode::Abort},
        {"Skip", FailureMode::Skip},
        {"Continue", FailureMode::Continue}};
    if (failure_mode_values.find(str) == failure_mode_values.end()) {
      g_logger.critical("Invalid Failure mode!!");
      return ERR_CORRUPT_TESTCASE;
    }
    tc.m_behaviour_on_failure = failure_mode_values[str];
    used_elements++;
  }

  if (used_elements != elements) {
    g_logger.critical(
        "Invalid test file: unknown properties in test case above line: %d",
        line);
    return ERR_CORRUPT_TESTCASE;
  }

  return elements;
}

bool setup_test_case(ProcessManagement &processManagement, atrt_config &config,
                     const atrt_testcase &tc) {
  if (!remove_dir("result", true)) {
    g_logger.critical("setup_test_case: Failed to clear result");
    return false;
  }

  for (unsigned i = 0; i < config.m_processes.size(); i++) {
    atrt_process &proc = *config.m_processes[i];
    if (proc.m_type == atrt_process::AP_NDB_API ||
        proc.m_type == atrt_process::AP_CLIENT) {
      proc.m_proc.m_path.assign("");
      proc.m_proc.m_args.assign("");
    }
  }

  BaseString cmd;
  char *p = find_bin_path(tc.m_cmd.m_exe.c_str());
  if (p == 0) {
    g_logger.critical("Failed to locate '%s'", tc.m_cmd.m_exe.c_str());
    return false;
  }
  cmd.assign(p);
  free(p);

  for (unsigned i = 0; i < config.m_processes.size(); i++) {
    atrt_process &proc = *config.m_processes[i];
    if (proc.m_type == tc.m_cmd.m_cmd_type && proc.m_proc.m_path == "") {
      proc.m_save.m_proc = proc.m_proc;
      proc.m_save.m_saved = true;

      proc.m_proc.m_env.appfmt(" ATRT_TIMEOUT=%ld", tc.m_max_time);
      if (0)  // valgrind
      {
        proc.m_proc.m_path = "/usr/bin/valgrind";
        proc.m_proc.m_args.appfmt("%s %s", cmd.c_str(),
                                  tc.m_cmd.m_args.c_str());
      } else {
        proc.m_proc.m_path = cmd;
        proc.m_proc.m_args.assign(tc.m_cmd.m_args.c_str());
      }
      if (!tc.m_run_all) break;
    }
  }

  if (tc.m_mysqld_options != "") {
    g_logger.info("restarting mysqld with extra options: %s",
                  tc.m_mysqld_options.c_str());

    /**
     * Apply testcase specific mysqld options
     */
    for (unsigned i = 0; i < config.m_processes.size(); i++) {
      atrt_process &proc = *config.m_processes[i];
      if (proc.m_type == atrt_process::AP_MYSQLD) {
        if (!processManagement.stopProcess(proc)) {
          return false;
        }

        if (!processManagement.waitForProcessToStop(proc)) {
          return false;
        }

        proc.m_save.m_proc = proc.m_proc;
        proc.m_save.m_saved = true;
        proc.m_proc.m_args.appfmt(" %s", tc.m_mysqld_options.c_str());

        if (!processManagement.startProcess(proc)) {
          return false;
        }

        if (!connect_mysqld(proc)) {
          return false;
        }
      }
    }
  }

  return true;
}

bool gather_result(atrt_config &config, int *result) {
  BaseString tmp = g_gather_progname;

  tmp.appfmt(" --result");
  for (unsigned i = 0; i < config.m_hosts.size(); i++) {
    if (config.m_hosts[i]->m_hostname.length() == 0) continue;

    tmp.appfmt(" %s:%s", config.m_hosts[i]->m_hostname.c_str(),
               config.m_hosts[i]->m_basedir.c_str());
  }

  g_logger.debug("system(%s)", tmp.c_str());
  const int r1 = sh(tmp.c_str());
  if (r1 != 0) {
    g_logger.critical("Failed to gather result!");
    return false;
  }

  g_logger.debug("system(%s)", g_analyze_progname);
  const int r2 = sh(g_analyze_progname);

  if (r2 == -1 || r2 == (127 << 8)) {
    g_logger.critical("Failed to analyze results");
    return false;
  }

  *result = r2;
  return true;
}

bool setup_hosts(atrt_config &config) {
  if (!remove_dir("result", true)) {
    g_logger.critical("setup_hosts: Failed to clear result");
    return false;
  }

  for (unsigned i = 0; i < config.m_hosts.size(); i++) {
    if (config.m_hosts[i]->m_hostname.length() == 0) continue;
    BaseString tmp = g_setup_progname;
    tmp.appfmt(" %s %s/ %s/", config.m_hosts[i]->m_hostname.c_str(), g_basedir,
               config.m_hosts[i]->m_basedir.c_str());

    g_logger.debug("system(%s)", tmp.c_str());
    const int r1 = sh(tmp.c_str());
    if (r1 != 0) {
      g_logger.critical("Failed to setup %s",
                        config.m_hosts[i]->m_hostname.c_str());
      return false;
    }
  }
  return true;
}

bool gather_coverage_results(atrt_config &config,
                             atrt_coverage_config &coverage_config,
                             int test_number) {
  BaseString gather_cmd = g_gather_progname;
  gather_cmd.appfmt(" --coverage");

  BaseString coverage_gather_dir;
  if (coverage_config.m_analysis == coverage::Coverage::Testsuite) {
    coverage_gather_dir = g_cwd;
  }

  for (unsigned i = 0; i < config.m_hosts.size(); i++) {
    if (config.m_hosts[i]->m_hostname.length() == 0) continue;
    const char *hostname = config.m_hosts[i]->m_hostname.c_str();

    if (coverage_config.m_analysis == coverage::Coverage::Testcase) {
      coverage_gather_dir = config.m_hosts[i]->m_basedir.c_str();
    }
    gather_cmd.appfmt(" %s:%s/%s/%s", hostname, coverage_gather_dir.c_str(),
                      "gcov", hostname);
  }

  g_logger.debug("system(%s)", gather_cmd.c_str());
  const int r1 = sh(gather_cmd.c_str());
  if (r1 != 0) {
    g_logger.critical("Failed to gather coverage files!");
    return false;
  }

  BaseString analyze_coverage_cmd = g_analyze_coverage_progname;
  analyze_coverage_cmd.appfmt(" --results-dir=%s", g_cwd);
  analyze_coverage_cmd.appfmt(" --build-dir=%s", g_build_dir);

  switch (coverage_config.m_analysis) {
    case coverage::Coverage::Testcase:
      analyze_coverage_cmd.appfmt(" --test-case-no=%d", test_number);
      break;
    case coverage::Coverage::Testsuite:
      [[fallthrough]];
    case coverage::Coverage::None:
      break;
  }

  switch (coverage_config.m_tool) {
    case CoverageTools::Lcov:
      analyze_coverage_cmd.appfmt(" --coverage-tool=lcov");
      break;
    case CoverageTools::Fastcov:
      analyze_coverage_cmd.appfmt(" --coverage-tool=fastcov");
      break;
  }
  g_logger.debug("system(%s)", analyze_coverage_cmd.c_str());
  const int r2 = sh(analyze_coverage_cmd.c_str());

  if (r2 != 0) {
    g_logger.critical("Failed to analyse coverage files!");
    return false;
  }
  return true;
}

static bool do_rsync(const char *dir, const char *dst) {
  BaseString tmp = g_setup_progname;
  tmp.appfmt(" %s %s/ %s", dst, dir, dir);

  g_logger.info("rsyncing %s to %s", dir, dst);
  g_logger.debug("system(%s)", tmp.c_str());
  const int r1 = sh(tmp.c_str());
  if (r1 != 0) {
    g_logger.critical("Failed to rsync %s to %s", dir, dst);
    return false;
  }

  return true;
}

bool deploy(int d, atrt_config &config) {
  for (unsigned i = 0; i < config.m_hosts.size(); i++) {
    if (config.m_hosts[i]->m_hostname.length() == 0) continue;

    if (d & 1) {
      if (!do_rsync(g_basedir, config.m_hosts[i]->m_hostname.c_str()))
        return false;
    }

    if (d & 2) {
      if (!do_rsync(g_prefix0, config.m_hosts[i]->m_hostname.c_str()))
        return false;

      if (g_prefix1 &&
          !do_rsync(g_prefix1, config.m_hosts[i]->m_hostname.c_str()))
        return false;
    }
  }

  return true;
}

bool sshx(atrt_config &config, unsigned mask) {
  for (unsigned i = 0; i < config.m_processes.size(); i++) {
    atrt_process &proc = *config.m_processes[i];

    BaseString tmp;
    const char *type = 0;
    switch (proc.m_type) {
      case atrt_process::AP_NDB_MGMD:
        type = (mask & proc.m_type) ? "ndb_mgmd" : 0;
        break;
      case atrt_process::AP_NDBD:
        type = (mask & proc.m_type) ? "ndbd" : 0;
        break;
      case atrt_process::AP_MYSQLD:
        type = (mask & proc.m_type) ? "mysqld" : 0;
        break;
      case atrt_process::AP_NDB_API:
        type = (mask & proc.m_type) ? "ndbapi" : 0;
        break;
      case atrt_process::AP_CLIENT:
        type = (mask & proc.m_type) ? "client" : 0;
        break;
      default:
        type = "<unknown>";
    }

    if (type == 0) continue;

#ifdef _WIN32
#define SYS_SSH                    \
  "bash '-c echo\"%s(%s) on %s\";" \
  "ssh -t %s sh %s/ssh-login.sh' &"
#else
#define SYS_SSH                   \
  "xterm -title \"%s(%s) on %s\"" \
  " -e 'ssh -t -X %s sh %s/ssh-login.sh' &"
#endif

    tmp.appfmt(SYS_SSH, type, proc.m_cluster->m_name.c_str(),
               proc.m_host->m_hostname.c_str(), proc.m_host->m_hostname.c_str(),
               proc.m_proc.m_cwd.c_str());

    g_logger.debug("system(%s)", tmp.c_str());
    const int r1 = sh(tmp.c_str());
    if (r1 != 0) {
      g_logger.critical("Failed sshx (%s)", tmp.c_str());
      return false;
    }
    NdbSleep_MilliSleep(300);  // To prevent xlock problem
  }

  return true;
}

bool reset_config(ProcessManagement &processManagement, atrt_config &config) {
  bool changed = false;
  for (unsigned i = 0; i < config.m_processes.size(); i++) {
    atrt_process &proc = *config.m_processes[i];
    if (proc.m_save.m_saved) {
      if (proc.m_proc.m_id != -1) {
        if (!processManagement.stopProcess(proc)) return false;
        if (!processManagement.waitForProcessToStop(proc)) return false;

        changed = true;
      }

      proc.m_save.m_saved = false;
      proc.m_proc = proc.m_save.m_proc;
    }
  }
  return changed;
}

bool find_scripts(const char *atrt_path) {
  g_logger.info("Locating scripts...");

  struct script_path {
    const char *name;
    const char **path;
  };
  std::vector<struct script_path> scripts = {
      {"atrt-gather-result.sh", &g_gather_progname},
      {"atrt-analyze-result.sh", &g_analyze_progname},
      {"atrt-backtrace.sh", nullptr},  // used by atrt-analyze-result.sh
      {"atrt-setup.sh", &g_setup_progname},
      {"atrt-analyze-coverage.sh", &g_analyze_coverage_progname},
      {"atrt-compute-coverage.sh", &g_compute_coverage_progname}};

  for (auto &script : scripts) {
    BaseString script_full_path;
    script_full_path.assfmt("%s/%s", atrt_path, script.name);

    if (!File_class::exists(script_full_path.c_str())) {
      g_logger.critical("atrt script %s could not be found in %s", script.name,
                        atrt_path);
      return false;
    }

    if (script.path != nullptr) {
      *script.path = strdup(script_full_path.c_str());
    }
  }
  return true;
}

static bool find_config_ini_files() {
  g_logger.info("Locating config.ini files...");

  BaseString tmp(g_clusters);
  Vector<BaseString> clusters;
  tmp.split(clusters, ",");

  bool found = true;
  for (unsigned int i = 0; i < clusters.size(); i++) {
    BaseString config_ini_path(g_cwd);
    const char *cluster_name = clusters[i].c_str();
    config_ini_path.appfmt("%sconfig%s.ini", PATH_SEPARATOR, cluster_name);
    to_native(config_ini_path);

    if (!exists_file(config_ini_path.c_str())) {
      g_logger.critical("Failed to locate '%s'", config_ini_path.c_str());
      found = false;
    }
  }

  return found;
}

BaseString get_atrt_path(const char *arg) {
  char fullPath[FN_REFLEN];
  int ret = my_realpath(fullPath, arg, 0);
  if (ret == -1) return {};

  BaseString path;
  char *last_folder_sep = strrchr(fullPath, '/');
  if (last_folder_sep != nullptr) {
    *last_folder_sep = '\0';
    path.assign(fullPath);
  }

  return path;
}

template class Vector<Vector<SimpleCpcClient::Process>>;
template class Vector<atrt_host *>;
template class Vector<atrt_cluster *>;
template class Vector<atrt_process *>;

int check_testcase_file_main(int argc, char **argv) {
  bool ok = true;
  int argi = 1;
  if (strcmp(argv[argi], "--check-testcase-files") == 0) {
    argi++;
  }
  if (argi == argc) {
    ok = false;
    g_logger.critical("Error: No files to check!\n");
  } else
    for (; argi < argc; argi++) {
      FILE *f = fopen(argv[argi], "r");
      if (f == NULL) {
        ok = false;
        g_logger.critical("Unable to open file: %s (%d: %s)", argv[argi], errno,
                          strerror(errno));
        continue;
      }
      atrt_testcase tc_dummy;
      int line_num = 0;
      int ntests = 0;
      int num_element_lines;
      while ((num_element_lines = read_test_case(f, line_num, tc_dummy)) > 0) {
        if (num_element_lines == ERR_CORRUPT_TESTCASE) break;
        ntests++;
      }
      // If line count is 0, it indicates end of file.
      if (num_element_lines == ERR_CORRUPT_TESTCASE) {
        ok = false;
        g_logger.critical("%s: Error at line %d (error %d)\n", argv[argi],
                          line_num, num_element_lines);
      } else {
        printf("%s: Contains %d tests in %d lines.\n", argv[argi], ntests,
               line_num);
      }
      fclose(f);
    }
  return ok ? 0 : 1;
}

void print_testcase_file_syntax() {
  printf(
      "\n"
      "Test cases to run are described in files passed with the\n"
      "--testcase-file (-f) option.\n"
      "\n"
      "A testcase is defined with some properties, one property per line,\n"
      "and terminated with exactly one empty line.  No other empty lines\n"
      "are allowed in the file.  Lines starting with # are comments and\n"
      "are ignored, note they are not counted as empty lines.\n"
      "\n"
      "The properties are:\n"
      "cmd      - Names the test executable.  The only mandatory property.\n"
      "args     - The arguments to test executable.\n"
      "max-time - Maximum run time for test in seconds (default 60000).\n"
      "type     - Declare the type of the test.  The only recognized value\n"
      "           is 'bench' which implies that results are stored also for\n"
      "           successful tests.  Normally if this option is not used\n"
      "           only results from failed tests will be stored.\n"
      "run-all  - If 'yes' atrt will start the same command for each defined\n"
      "           api/mysqld, normally it only starts one instance.\n"
      "mysqld   - Arguments that atrt will use when starting mysqld.\n"
      "cmd-type - If 'mysql' change test process type from ndbapi to client.\n"
      "name     - Change name of test.  Default is given by cmd and args.\n"
      "force-cluster-restart - If 'before', force restart the cluster before\n"
      "                        running the test case.\n"
      "                        If 'after', force restart the cluster after\n"
      "                        running the test case.\n"
      "                        If 'both', force restart the cluster before\n"
      "                        and after running the test case.\n"
      "                        If 'none', no forceful cluster restart.\n"
      "max-retries - Maximum number of retries after test failed.\n"
      ""
      "\n"
      "Example:\n"
      "# BASIC FUNCTIONALITY\n"
      "max-time: 500\n"
      "cmd: testBasic\n"
      "args: -n PkRead\n"
      "\n"
      "# 4k record DD\n"
      "max-time: 600\n"
      "cmd: flexAsynch\n"
      "args: -dd -temp -con 2 -t 8 -r 2 -p 64 -ndbrecord -a 25 -s 40\n"
      "type: bench\n"
      "\n"
      "# sql\n"
      "max-time: 600\n"
      "cmd: ndb-sql-perf.sh\n"
      "args: ndb-sql-perf-select.sh t1 1 64\n"
      "mysqld: --ndb-cluster-connection-pool=1\n"
      "type: bench\n"
      "cmd-type: mysql\n"
      "\n");
}
