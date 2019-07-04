/*
   Copyright (c) 2003, 2019, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef atrt_config_hpp
#define atrt_config_hpp

#include <NDBT_ReturnCodes.h>
#include <mgmapi.h>
#include <my_default.h>
#include <my_dir.h>
#include <my_getopt.h>
#include <my_sys.h>
#include <mysql.h>
#include <ndb_global.h>
#include <BaseString.hpp>
#include <CpcClient.hpp>
#include <Logger.hpp>
#include <NdbAutoPtr.hpp>
#include <Properties.hpp>
#include <Vector.hpp>

enum ErrorCodes {
  ERR_OK = 0,
  ERR_NDB_FAILED = 101,
  ERR_SERVERS_FAILED = 102,
  ERR_MAX_TIME_ELAPSED = 103,
  ERR_COMMAND_FAILED = 104,
  ERR_FAILED_TO_START = 105,
  ERR_NDB_AND_SERVERS_FAILED = 106,
  ERR_TEST_FAILED = NDBT_FAILED << 8,
  ERR_TEST_SKIPPED = NDBT_SKIPPED << 8
};

enum AtrtExitCodes {
  TESTSUITE_SUCCESS = 0,
  TESTSUITE_FAILURES = 1,
  ATRT_FAILURE = 2
};

struct atrt_host {
  unsigned m_index;
  BaseString m_user;
  BaseString m_basedir;
  BaseString m_hostname;
  SimpleCpcClient* m_cpcd;
  Vector<struct atrt_process*> m_processes;
};

struct atrt_options {
  enum Feature { AO_REPLICATION = 1, AO_NDBCLUSTER = 2 };

  int m_features;
  Properties m_loaded;
  Properties m_generated;
};

struct atrt_process {
  unsigned m_index;
  BaseString m_name;
  unsigned int m_procno;

  struct atrt_host* m_host;
  struct atrt_cluster* m_cluster;

  enum Type {
    AP_ALL = 255,
    AP_NDBD = 1,
    AP_NDB_API = 2,
    AP_NDB_MGMD = 4,
    AP_MYSQLD = 16,
    AP_CLIENT = 32,
    AP_CUSTOM = 64,
    AP_CLUSTER = 256  // Used for options parsing for "cluster" options
  } m_type;

  SimpleCpcClient::Process m_proc;
  bool m_atrt_stopped;

  NdbMgmHandle m_ndb_mgm_handle;    // if type == ndb_mgm
  atrt_process* m_mysqld;           // if type == client
  atrt_process* m_rep_src;          // if type == mysqld
  Vector<atrt_process*> m_rep_dst;  // if type == mysqld
  MYSQL m_mysql;                    // if type == mysqld
  atrt_options m_options;
  uint m_nodeid;  // if m_fix_nodeid

  struct {
    bool m_saved;
    SimpleCpcClient::Process m_proc;
  } m_save;
};

struct atrt_cluster {
  BaseString m_name;
  BaseString m_dir;
  Vector<atrt_process*> m_processes;
  atrt_options m_options;
  uint m_next_nodeid;  // if m_fix_nodeid
};

struct atrt_config {
  bool m_generated;
  enum { CNF, INI } m_config_type;
  BaseString m_key;
  BaseString m_replication;
  BaseString m_site;
  Vector<atrt_host*> m_hosts;
  Vector<atrt_cluster*> m_clusters;
  Vector<atrt_process*> m_processes;
};

struct atrt_testcase {
  bool m_report;
  bool m_run_all;
  time_t m_max_time;
  BaseString m_name;
  BaseString m_mysqld_options;
  int m_max_retries;
  bool m_force_cluster_restart;

  struct Command {
    atrt_process::Type m_cmd_type;
    BaseString m_exe;
    BaseString m_args;
  } m_cmd;  // Todo make array of these...
};

extern Logger g_logger;

bool parse_args(int argc, char** argv, MEM_ROOT *alloc);
bool setup_config(atrt_config&, const char* mysqld);
bool load_deployment_options(atrt_config&);
bool configure(atrt_config&, int setup);
bool setup_directories(atrt_config&, int setup);
bool setup_files(atrt_config&, int setup, int sshx);

bool deploy(int, atrt_config&);
bool sshx(atrt_config&, unsigned procmask);
bool start(atrt_config&, unsigned procmask);

bool remove_dir(const char*, bool incl = true);
bool exists_file(const char* path);
bool connect_hosts(atrt_config&);
bool connect_ndb_mgm(atrt_config&);
bool wait_ndb(atrt_config&, int ndb_mgm_node_status);
bool start_processes(atrt_config&, int);
bool stop_processes(atrt_config&, int);
bool update_status(atrt_config&, int types, bool check_for_missing = true);
bool wait_for_processes_to_stop(atrt_config& config,
                                int types = atrt_process::AP_ALL,
                                int retries = 5,
                                int wait_between_retries_s = 5);
bool wait_for_process_to_stop(atrt_config& config, atrt_process& proc,
                              int retries = 5, int wait_between_retries_s = 5);

int check_ndb_or_servers_failures(atrt_config& config);
bool is_client_running(atrt_config&);
bool gather_result(atrt_config&, int* result);

int read_test_case(FILE*, atrt_testcase&, int& line);
bool setup_test_case(atrt_config&, const atrt_testcase&);

bool setup_hosts(atrt_config&);

bool do_command(atrt_config& config);

bool start_process(atrt_process& proc, bool run_setup = true);
bool stop_process(atrt_process& proc);

bool connect_mysqld(atrt_process& proc);
bool disconnect_mysqld(atrt_process& proc);

/**
 * check configuration if any changes has been
 *   done for the duration of the latest running test
 *   if so, return true, and reset those changes
 *   (true, indicates that a restart is needed to actually
 *    reset the running processes)
 */
bool reset_config(atrt_config&);

NdbOut& operator<<(NdbOut& out, const atrt_process& proc);

/**
 * SQL
 */
bool setup_db(atrt_config&);

/**
 * Global variables...
 */
extern Logger g_logger;

extern const char* g_cwd;
extern const char* g_my_cnf;
extern const char* g_user;
extern const char* g_basedir;
extern const char* g_prefix;
extern const char* g_prefix0;
extern const char* g_prefix1;
extern int g_baseport;
extern int g_fqpn;
extern int g_fix_nodeid;
extern int g_default_ports;
extern int g_restart;

extern const char* g_site;
extern const char* g_clusters;

/**
 * Since binaries move location between 5.1 and 5.5
 *   we keep full path to them here
 */
char* find_bin_path(const char* basename);
char* find_bin_path(const char* prefix, const char* basename);
extern const char* g_ndb_mgmd_bin_path;
extern const char* g_ndbd_bin_path;
extern const char* g_ndbmtd_bin_path;
extern const char* g_mysqld_bin_path;
extern const char* g_mysql_install_db_bin_path;
extern const char* g_libmysqlclient_so_path;
extern const char* g_search_path[];

#ifdef _WIN32
#include <direct.h>

inline int lstat(const char* name, struct stat* buf) { return stat(name, buf); }

inline int S_ISREG(int x) { return x & _S_IFREG; }

inline int S_ISDIR(int x) { return x & _S_IFDIR; }

#endif

/* in-place replace */
static inline char* replace_chars(char* str, char from, char to) {
  int i;

  for (i = 0; str[i]; i++) {
    if (i && str[i] == from && str[i - 1] != ' ') {
      str[i] = to;
    }
  }
  return str;
}

static inline BaseString& replace_chars(BaseString& bs, char from, char to) {
  replace_chars((char*)bs.c_str(), from, to);
  return bs;
}
static inline BaseString& to_native(BaseString& bs) {
  return replace_chars(bs, DIR_SEPARATOR[0] == '/' ? '\\' : '/',
                       DIR_SEPARATOR[0]);
}
static inline BaseString& to_fwd_slashes(BaseString& bs) {
  return replace_chars(bs, '\\', '/');
}
static inline char* to_fwd_slashes(char* bs) {
  return replace_chars(bs, '\\', '/');
}

// you must free() the result
static inline char* replace_drive_letters(const char* path) {
  int i, j;
  int count;     // number of ':'s in path
  char* retval;  // return value
  const char cygdrive[] = "/cygdrive";
  size_t cyglen = strlen(cygdrive), retval_len;

  for (i = 0, count = 0; path[i]; i++) {
    count += path[i] == ':';
  }
  retval_len = strlen(path) + count * cyglen + 1;
  retval = (char*)malloc(retval_len);

  for (i = j = 0; path[i]; i++) {
    if (path[i] && path[i + 1]) {
      if ((!i || isspace(path[i - 1]) || ispunct(path[i - 1])) &&
          path[i + 1] == ':') {
        require(path[i + 2] == '/');
        j += BaseString::snprintf(retval + j, retval_len - 1, "%s/%c", cygdrive,
                                  path[i]);
        i++;
        continue;
      }
    }
    retval[j++] = path[i];
  }
  retval[j] = 0;

  return retval;
}

static inline int sh(const char* script) {
#ifdef _WIN32
  g_logger.debug("sh('%s')", script);

  /*
    Running sh script on Windows
    1) Write the command to run into temporary file
    2) Run the temporary file with 'sh <temp_file_name>'
  */

  char tmp_path[MAX_PATH];
  if (GetTempPath(sizeof(tmp_path), tmp_path) == 0) {
    g_logger.error("GetTempPath failed, error: %d", GetLastError());
    return -1;
  }

  char tmp_file[MAX_PATH];
  if (GetTempFileName(tmp_path, "sh_", 0, tmp_file) == 0) {
    g_logger.error("GetTempFileName failed, error: %d", GetLastError());
    return -1;
  }

  FILE* fp = fopen(tmp_file, "w");
  if (fp == NULL) {
    g_logger.error("Cannot open file '%s', error: %d", tmp_file, errno);
    return -1;
  }

  // cygwin'ify the script and write it to temp file
  {
    char* cygwin_script = replace_drive_letters(script);
    g_logger.debug(" - cygwin_script: '%s' ", cygwin_script);
    fprintf(fp, "%s", cygwin_script);
    free(cygwin_script);
  }

  fclose(fp);

  // Run the temp file with "sh"
  BaseString command;
  command.assfmt("sh %s", tmp_file);
  g_logger.debug(" - running '%s' ", command.c_str());

  int ret = system(command.c_str());
  if (ret == 0)
    g_logger.debug(" - OK!");
  else
    g_logger.warning("Running the command '%s' as '%s' failed, ret: %d", script,
                     command.c_str(), ret);

  // Remove the temp file
  unlink(tmp_file);

  return ret;

#else

  return system(script);

#endif
}
#endif
