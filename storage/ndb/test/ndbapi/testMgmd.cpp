/*
   Copyright (c) 2009, 2024, Oracle and/or its affiliates.


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

#include <fstream>
#include <iostream>
#include <string>

#include <NdbEnv.h>
#include <NDBT.hpp>
#include <NDBT_Find.hpp>
#include <NDBT_Test.hpp>
#include <NDBT_Workingdir.hpp>
#include <NdbMgmd.hpp>
#include <NdbProcess.hpp>
#include <portlib/NdbDir.hpp>
#include "ConfigFactory.hpp"
#include "portlib/ssl_applink.h"
#include "util/TlsKeyManager.hpp"
#include "util/ndb_openssl3_compat.h"
#include "util/require.h"

static const char *exe_valgrind = 0;
static const char *arg_valgrind = 0;
static int no_node_config = 0;
static bool with_nodeid = true;

static bool file_exists(const char *path, Uint32 timeout = 1) {
  g_info << "File '" << path << "' ";
  /**
   * ndb_mgmd does currently not fsync the directory
   *   after committing config-bin,
   *   which means that it can be on disk, wo/ being visible
   *   remedy this by retrying some
   */
  for (Uint32 i = 0; i < 10 * timeout; i++) {
    if (access(path, F_OK) == 0) {
      g_info << "exists" << endl;
      return true;
    }
    if (i == 0) {
      g_info << "does not exist, retrying...";
    }
    NdbSleep_MilliSleep(100);
  }
  g_info << "does not exist" << endl;
  return false;
}

// Util function that concatenate strings to form a path

static BaseString path(const char *first, ...) {
  BaseString path;
  path.assign(first);

  const char *str;
  va_list args;
  va_start(args, first);
  while ((str = va_arg(args, const char *)) != NULL) {
    path.appfmt("%s%s", DIR_SEPARATOR, str);
  }
  va_end(args);
  return path;
}

class Mgmd {
 protected:
  std::unique_ptr<NdbProcess> m_proc;
  int m_nodeid;
  BaseString m_name;
  BaseString m_exe;
  NdbMgmd m_mgmd_client;
  bool m_verbose{true};

  Mgmd(const Mgmd &other) = delete;

 public:
  Mgmd(int nodeid) : m_nodeid(nodeid) {
    m_name.assfmt("ndb_mgmd_%d", nodeid);

    NDBT_find_ndb_mgmd(m_exe);
  }

  Mgmd() {
    no_node_config = no_node_config + 1;
    m_name.assfmt("ndb_mgmd_autonode_%d", no_node_config);
    NDBT_find_ndb_mgmd(m_exe);
  }

  ~Mgmd() {
    if (m_proc) {
      stop();
    }
  }

  const char *name(void) const { return m_name.c_str(); }

  const char *exe(void) const { return m_exe.c_str(); }

  void verbose(bool f) { m_verbose = f; }

  bool start(const char *working_dir, NdbProcess::Args &args) {
    g_info << "Starting " << name() << " ";
    for (unsigned i = 0; i < args.args().size(); i++)
      g_info << args.args()[i].c_str() << " ";
    g_info << endl;

    if (exe_valgrind == 0) {
      m_proc = NdbProcess::create(name(), exe(), working_dir, args);
    } else {
      NdbProcess::Args copy;
      if (arg_valgrind) {
        copy.add(arg_valgrind);
      }
      copy.add(exe());
      copy.add(args);
      m_proc = NdbProcess::create(name(), BaseString(exe_valgrind), working_dir,
                                  copy);
    }
    return (bool)m_proc;
  }

  void common_args(NdbProcess::Args &args, const char *working_dir) {
    args.add("--no-defaults");
    args.add("--configdir=", working_dir);
    args.add("--config-file=", "config.ini");
    if (with_nodeid) {
      args.add("--ndb-nodeid=", m_nodeid);
    }
    args.add("--nodaemon");
    args.add("--log-name=", name());
    if (m_verbose) args.add("--verbose");
  }

  bool start_from_config_ini(const char *working_dir,
                             const char *first_extra_arg = NULL, ...) {
    NdbProcess::Args args;
    common_args(args, working_dir);

    if (first_extra_arg) {
      // Append any extra args
      va_list extra_args;
      const char *str = first_extra_arg;
      va_start(extra_args, first_extra_arg);
      do {
        args.add(str);
      } while ((str = va_arg(extra_args, const char *)) != NULL);
      va_end(extra_args);
    }

    return start(working_dir, args);
  }

  bool start(const char *working_dir, const char *first_extra_arg = NULL, ...) {
    NdbProcess::Args args;
    args.add("--no-defaults");
    args.add("--configdir=", working_dir);
    args.add("--ndb-nodeid=", m_nodeid);
    args.add("--nodaemon");
    args.add("--log-name=", name());
    if (m_verbose) args.add("--verbose");

    if (first_extra_arg) {
      // Append any extra args
      va_list extra_args;
      const char *str = first_extra_arg;
      va_start(extra_args, first_extra_arg);
      do {
        args.add(str);
      } while ((str = va_arg(extra_args, const char *)) != NULL);
      va_end(extra_args);
    }

    return start(working_dir, args);
  }

  bool stop(void) {
    g_info << "Stopping " << name() << endl;

    // Diconnect and close our "builtin" client
    m_mgmd_client.close();

    if (!m_proc || !m_proc->stop()) {
      fprintf(stderr, "Failed to stop process %s\n", name());
      return false;  // Can't kill with -9 -> fatal error
    }
    int ret;
    if (!m_proc->wait(ret, 30000)) {
      fprintf(stderr, "Failed to wait for process %s\n", name());
      return false;  // Can't wait after kill with -9 -> fatal error
    }

    if (ret != 9) {
      // The normal case after killing the process with -9 is that wait
      // returns 9, but other return codes may also be returned for example
      // when the process has already terminated itself.
      // The important thing is that the process has terminated, just log return
      // code and continue releasing resources.
      fprintf(stderr, "Process %s stopped with ret: %u\n", name(), ret);
    }

    m_proc.reset();
    return true;
  }

  bool wait(int &ret, int timeout = 30000) {
    g_info << "Waiting for " << name() << endl;

    if (!m_proc || !m_proc->wait(ret, timeout)) {
      fprintf(stderr, "Failed to wait for process %s\n", name());
      return false;
    }

    m_proc.reset();
    return true;
  }

  const BaseString connectstring(const Properties &config) {
    const char *hostname;
    require(get_section_string(config, m_name.c_str(), "HostName", &hostname));

    Uint32 port;
    require(get_section_uint32(config, m_name.c_str(), "PortNumber", &port));

    BaseString constr;
    constr.assfmt("%s:%d", hostname, port);
    return constr;
  }

  bool connect(const Properties &config, int num_retries = 60,
               int retry_delay_in_seconds = 1) {
    BaseString constr = connectstring(config);
    g_info << "Connecting to " << name() << " @ " << constr.c_str() << endl;

    return m_mgmd_client.connect(constr.c_str(), num_retries,
                                 retry_delay_in_seconds);
  }

  int client_start_tls(struct ssl_ctx_st *ctx) {
    return m_mgmd_client.start_tls(ctx);
  }

  bool wait_confirmed_config(int timeout = 30) {
    if (!m_mgmd_client.is_connected()) {
      g_err << "wait_confirmed_config: not connected!" << endl;
      return false;
    }

    int retries = 0;
    Config conf;
    while (!m_mgmd_client.get_config(conf)) {
      retries++;

      if (retries == timeout * 10) {
        g_err << "wait_confirmed_config: Failed to get config within "
              << timeout << " seconds" << endl;
        return false;
      }

      g_err << "Failed to get config, sleeping" << endl;
      NdbSleep_MilliSleep(100);
    }
    g_info << "wait_confirmed_config: ok" << endl;
    return true;
  }

  NdbMgmHandle handle() { return m_mgmd_client.handle(); }

  NdbSocket convert_to_transporter() {
    return m_mgmd_client.convert_to_transporter();
  }

 private:
  bool get_section_string(const Properties &config, const char *section_name,
                          const char *key, const char **value) const {
    const Properties *section;
    if (!config.get(section_name, &section)) return false;

    if (!section->get(key, value)) return false;
    return true;
  }

  bool get_section_uint32(const Properties &config, const char *section_name,
                          const char *key, Uint32 *value) const {
    const Properties *section;
    if (!config.get(section_name, &section)) return false;

    if (!section->get(key, value)) return false;
    return true;
  }
};

class MgmdProcessList : public Vector<Mgmd *> {
 public:
  ~MgmdProcessList() {
    // Delete and thus stop the mgmd(s)
    for (unsigned i = 0; i < size(); i++) {
      Mgmd *mgmd = this->operator[](i);
      delete mgmd;
    }
    clear();
  }
};

class Ndbd : public Mgmd {
 public:
  Ndbd(int nodeid) : Mgmd(nodeid), m_args() {
    m_args.add("--ndb-nodeid=", m_nodeid);
    m_args.add("--foreground");
    m_args.add("--loose-core-file=0");
    m_name.assfmt("ndbd_%d", nodeid);
    NDBT_find_ndbd(m_exe);
  }

  NdbProcess::Args &args() { return m_args; }

  void set_connect_string(BaseString connect_string) {
    m_args.add("-c");
    m_args.add(connect_string.c_str());
  }

  bool start(const char *working_dir, BaseString connect_string) {
    set_connect_string(connect_string);
    return Mgmd::start(working_dir, m_args);
  }

  bool wait_started(NdbMgmHandle &mgm_handle, int timeout = 30,
                    int node_index = 0) {
    ndb_mgm_node_type node_types[2] = {NDB_MGM_NODE_TYPE_NDB,
                                       NDB_MGM_NODE_TYPE_UNKNOWN};

    int retries = 0;
    while (retries++ < timeout) {
      ndb_mgm_cluster_state *cs = ndb_mgm_get_status2(mgm_handle, node_types);
      if (cs) {
        ndb_mgm_node_state *ndbd_status = cs->node_states + node_index;
        if (ndbd_status->node_status == NDB_MGM_NODE_STATUS_STARTED) {
          g_info << "Node: %d, get status Ok (NODE_STATUS_STARTED)" << m_nodeid
                 << endl;
          free(cs);
          return true;
        }
        free(cs);
      }
      NdbSleep_MilliSleep(1000);
    }
    g_info << "Node: %d, timeout waiting to reach status NODE_STATUS_STARTED"
           << m_nodeid << endl;
    return false;
  }

 private:
  NdbProcess::Args m_args;
};

static bool create_CA(NDBT_Workingdir &wd, const BaseString &exe) {
  int ret;
  NdbProcess::Args args;

  args.add("--passphrase=", "Trondheim");
  args.add("--create-CA");
  args.add("--CA-search-path=", wd.path());
  auto proc = NdbProcess::create("Create CA", exe, wd.path(), args);
  bool r = proc->wait(ret, 10000);

  return (r && (ret == 0));
}

static bool sign_tls_keys(NDBT_Workingdir &wd) {
  int ret;
  BaseString cfg_path = path(wd.path(), "config.ini", nullptr);

  /* Find executable */
  BaseString exe;
  NDBT_find_sign_keys(exe);

  /* Create CA */
  if (!create_CA(wd, exe)) return false;

  /* Create keys and certificates for all nodes in config */
  NdbProcess::Args args;
  args.add("--config-file=", cfg_path.c_str());
  args.add("--passphrase=", "Trondheim");
  args.add("--ndb-tls-search-path=", wd.path());
  args.add("--create-key");
  auto proc = NdbProcess::create("Create Keys", exe, wd.path(), args);
  bool r = proc->wait(ret, 10000);
  return (r && (ret == 0));
}

static bool create_expired_cert(NDBT_Workingdir &wd) {
  int ret;

  BaseString cfg_path = path(wd.path(), "config.ini", nullptr);

  /* Find executable */
  BaseString exe;
  NDBT_find_sign_keys(exe);

  /* Create CA */
  if (!create_CA(wd, exe)) return false;

  /* Create an expired certificate for a data node */
  NdbProcess::Args args;
  args.add("--create-key");
  args.add("--ndb-tls-search-path=", wd.path());
  args.add("--passphrase=", "Trondheim");
  args.add("-l");                     // no-config mode
  args.add("--node-type=", "db");     // type db
  args.add("--duration=", "-50000");  // negative seconds; already expired

  auto proc = NdbProcess::create("Create Keys", exe, wd.path(), args);
  bool r = proc->wait(ret, 10000);
  return (r && (ret == 0));
}

/* Print some information about a cert, and check that its validity is at
   least 120 days. Return true if ok.
*/
static bool check_cert(const NDBT_Workingdir &wd, Node::Type type) {
  static constexpr int MinDuration = 120 * CertLifetime::SecondsPerDay;

  int duration = 0;
  PkiFile::PathName certFile;
  TlsSearchPath searchPath(wd.path());
  if (ActiveCertificate::find(&searchPath, 0, type, certFile)) {
    fprintf(stderr, "Reading cert file: %s \n", certFile.c_str());
    X509 *cert = Certificate::open_one(certFile);
    if (cert) {
      char name[65];
      Certificate::get_common_name(cert, name, sizeof(name));
      const NodeCertificate *nc = NodeCertificate::for_peer(cert);
      if (nc) {
        duration = nc->duration();
        printf(" ... Cert CN:       %s\n", name);
        printf(" ... Cert Duration: %d\n", duration);
        printf(" ... Cert Serial:   %s\n", nc->serial_number().c_str());
        delete nc;
      }
      Certificate::free(cert);
    }
  }
  return (duration >= MinDuration);
}

bool Print_find_in_file(const char *path, Vector<BaseString> search_string) {
  std::ifstream indata;
  indata.open(path);
  Vector<bool> found;
  for (unsigned int i = 0; i < search_string.size(); i++) {
    found.push_back(false);
  }

  if (indata.is_open()) {
    std::string read_line;
    while (std::getline(indata, read_line)) {
      for (unsigned int i = 0; i < search_string.size(); i++) {
        if (found[i] == false) {
          if (read_line.find(search_string[i].c_str(), 0) !=
              std::string::npos) {
            {
              found[i] = true;
              break;
            }
          }
        }
      }
      printf("%s\n", read_line.c_str());
    }
  } else {
    return false;
  }
  indata.close();
  bool ret = true;
  for (unsigned int i = 0; i < search_string.size(); i++) {
    ret = ret && found[i];
  }
  return ret;
}

#define CHECK(x)                                                     \
  if (!(x)) {                                                        \
    fprintf(stderr, "CHECK(" #x ") failed at line: %d\n", __LINE__); \
    return NDBT_FAILED;                                              \
  }

int runTestBasic2Mgm(NDBT_Context *ctx, NDBT_Step *step) {
  NDBT_Workingdir wd("test_mgmd");  // temporary working directory

  // Create config.ini
  Properties config = ConfigFactory::create(2);
  CHECK(ConfigFactory::write_config_ini(
      config, path(wd.path(), "config.ini", NULL).c_str()));
  // Start ndb_mgmd(s)
  MgmdProcessList mgmds;
  for (int i = 1; i <= 2; i++) {
    Mgmd *mgmd = new Mgmd(i);
    mgmds.push_back(mgmd);
    CHECK(mgmd->start_from_config_ini(wd.path()));
  }

  // Connect the ndb_mgmd(s)
  for (unsigned i = 0; i < mgmds.size(); i++) CHECK(mgmds[i]->connect(config));

  // wait for confirmed config
  for (unsigned i = 0; i < mgmds.size(); i++)
    CHECK(mgmds[i]->wait_confirmed_config());

  // Check binary config files created
  CHECK(file_exists(path(wd.path(), "ndb_1_config.bin.1", NULL).c_str()));
  CHECK(file_exists(path(wd.path(), "ndb_2_config.bin.1", NULL).c_str()));

  // Stop the ndb_mgmd(s)
  for (unsigned i = 0; i < mgmds.size(); i++) CHECK(mgmds[i]->stop());

  // Start up the mgmd(s) again from config.bin
  for (unsigned i = 0; i < mgmds.size(); i++)
    CHECK(mgmds[i]->start_from_config_ini(wd.path()));

  // Connect the ndb_mgmd(s)
  for (unsigned i = 0; i < mgmds.size(); i++) CHECK(mgmds[i]->connect(config));

  // check ndb_X_config.bin.1 still exists but not ndb_X_config.bin.2
  CHECK(file_exists(path(wd.path(), "ndb_1_config.bin.1", NULL).c_str()));
  CHECK(file_exists(path(wd.path(), "ndb_2_config.bin.1", NULL).c_str()));

  CHECK(!file_exists(path(wd.path(), "ndb_1_config.bin.2", NULL).c_str()));
  CHECK(!file_exists(path(wd.path(), "ndb_2_config.bin.2", NULL).c_str()));

  return NDBT_OK;
}

int runTestBug45495(NDBT_Context *ctx, NDBT_Step *step) {
  NDBT_Workingdir wd("test_mgmd");  // temporary working directory

  g_err << "** Create config.ini" << endl;
  Properties config = ConfigFactory::create(2);
  CHECK(ConfigFactory::write_config_ini(
      config, path(wd.path(), "config.ini", NULL).c_str()));
  // Start ndb_mgmd(s)
  MgmdProcessList mgmds;
  for (int i = 1; i <= 2; i++) {
    Mgmd *mgmd = new Mgmd(i);
    mgmd->verbose(false);
    mgmds.push_back(mgmd);
    CHECK(mgmd->start_from_config_ini(wd.path()));
  }

  // Connect the ndb_mgmd(s)
  for (unsigned i = 0; i < mgmds.size(); i++) CHECK(mgmds[i]->connect(config));

  // wait for confirmed config
  for (unsigned i = 0; i < mgmds.size(); i++)
    CHECK(mgmds[i]->wait_confirmed_config());

  // Check binary config files created
  CHECK(file_exists(path(wd.path(), "ndb_1_config.bin.1", NULL).c_str()));
  CHECK(file_exists(path(wd.path(), "ndb_2_config.bin.1", NULL).c_str()));

  g_err << "** Restart one ndb_mgmd at a time --reload + --initial" << endl;
  for (unsigned i = 0; i < mgmds.size(); i++) {
    CHECK(mgmds[i]->stop());
    CHECK(mgmds[i]->start_from_config_ini(wd.path(), "--reload", "--initial",
                                          NULL));
    CHECK(mgmds[i]->connect(config));
    CHECK(mgmds[i]->wait_confirmed_config());

    // check ndb_X_config.bin.1 still exists but not ndb_X_config.bin.2
    CHECK(file_exists(path(wd.path(), "ndb_1_config.bin.1", NULL).c_str()));
    CHECK(file_exists(path(wd.path(), "ndb_2_config.bin.1", NULL).c_str()));

    CHECK(!file_exists(path(wd.path(), "ndb_1_config.bin.2", NULL).c_str()));
    CHECK(!file_exists(path(wd.path(), "ndb_2_config.bin.2", NULL).c_str()));
  }

  g_err << "** Restart one ndb_mgmd at a time --initial" << endl;
  for (unsigned i = 0; i < mgmds.size(); i++) {
    CHECK(mgmds[i]->stop());
    CHECK(mgmds[i]->start_from_config_ini(wd.path(), "--initial", NULL));
    CHECK(mgmds[i]->connect(config));
    CHECK(mgmds[i]->wait_confirmed_config());

    // check ndb_X_config.bin.1 still exists but not ndb_X_config.bin.2
    CHECK(file_exists(path(wd.path(), "ndb_1_config.bin.1", NULL).c_str()));
    CHECK(file_exists(path(wd.path(), "ndb_2_config.bin.1", NULL).c_str()));

    CHECK(!file_exists(path(wd.path(), "ndb_1_config.bin.2", NULL).c_str()));
    CHECK(!file_exists(path(wd.path(), "ndb_2_config.bin.2", NULL).c_str()));
  }

  g_err << "** Create config2.ini" << endl;
  CHECK(ConfigFactory::put(config, "ndb_mgmd", 1, "ArbitrationDelay", 100));
  CHECK(ConfigFactory::write_config_ini(
      config, path(wd.path(), "config2.ini", NULL).c_str()));

  g_err << "** Restart one ndb_mgmd at a time --initial should not work"
        << endl;
  for (unsigned i = 0; i < mgmds.size(); i++) {
    CHECK(mgmds[i]->stop());
    // Start from config2.ini
    CHECK(mgmds[i]->start_from_config_ini(wd.path(), "-f", "config2.ini",
                                          "--initial", NULL));

    // Wait for mgmd to exit and check return status
    int ret;
    CHECK(mgmds[i]->wait(ret));
    CHECK(ret == 1);

    // check config files exist only for the still running mgmd(s)
    for (unsigned j = 0; j < mgmds.size(); j++) {
      BaseString tmp;
      tmp.assfmt("ndb_%d_config.bin.1", j + 1);
      CHECK(file_exists(path(wd.path(), tmp.c_str(), NULL).c_str()) ==
            (j != i));
    }

    // Start from config.ini again
    CHECK(mgmds[i]->start_from_config_ini(wd.path(), "--initial", "--reload",
                                          NULL));
    CHECK(mgmds[i]->connect(config));
    CHECK(mgmds[i]->wait_confirmed_config());
  }

  g_err << "** Reload from config2.ini" << endl;
  for (unsigned i = 0; i < mgmds.size(); i++) {
    CHECK(mgmds[i]->stop());
    // Start from config2.ini
    CHECK(mgmds[i]->start_from_config_ini(wd.path(), "-f", "config2.ini",
                                          "--reload", NULL));
    CHECK(mgmds[i]->connect(config));
    CHECK(mgmds[i]->wait_confirmed_config());
  }

  CHECK(file_exists(path(wd.path(), "ndb_1_config.bin.1", NULL).c_str()));
  CHECK(file_exists(path(wd.path(), "ndb_2_config.bin.1", NULL).c_str()));

  Uint32 timeout = 30;
  CHECK(file_exists(path(wd.path(), "ndb_1_config.bin.2", NULL).c_str(),
                    timeout));
  CHECK(file_exists(path(wd.path(), "ndb_2_config.bin.2", NULL).c_str(),
                    timeout));

  g_err << "** Reload mgmd initial(from generation=2)" << endl;
  for (unsigned i = 0; i < mgmds.size(); i++) {
    CHECK(mgmds[i]->stop());
    CHECK(mgmds[i]->start_from_config_ini(wd.path(), "-f", "config2.ini",
                                          "--reload", "--initial", NULL));

    CHECK(mgmds[i]->connect(config));
    CHECK(mgmds[i]->wait_confirmed_config());

    // check config files exist
    for (unsigned j = 0; j < mgmds.size(); j++) {
      BaseString tmp;
      tmp.assfmt("ndb_%d_config.bin.1", j + 1);
      CHECK(file_exists(path(wd.path(), tmp.c_str(), NULL).c_str()) == (i < j));

      tmp.assfmt("ndb_%d_config.bin.2", j + 1);
      CHECK(file_exists(path(wd.path(), tmp.c_str(), NULL).c_str(), timeout));
    }
  }

  return NDBT_OK;
}

int runTestBug42015(NDBT_Context *ctx, NDBT_Step *step) {
  NDBT_Workingdir wd("test_mgmd");  // temporary working directory

  g_err << "** Create config.ini" << endl;
  Properties config = ConfigFactory::create(2);
  CHECK(ConfigFactory::write_config_ini(
      config, path(wd.path(), "config.ini", NULL).c_str()));

  MgmdProcessList mgmds;
  // Start ndb_mgmd 1 from config.ini
  Mgmd *mgmd = new Mgmd(1);
  mgmds.push_back(mgmd);
  CHECK(mgmd->start_from_config_ini(wd.path()));

  // Start ndb_mgmd 2 by fetching from first
  Mgmd *mgmd2 = new Mgmd(2);
  mgmds.push_back(mgmd2);
  CHECK(mgmd2->start(wd.path(), "--ndb-connectstring",
                     mgmd->connectstring(config).c_str(), NULL));

  // Connect the ndb_mgmd(s)
  for (unsigned i = 0; i < mgmds.size(); i++) CHECK(mgmds[i]->connect(config));

  // wait for confirmed config
  for (unsigned i = 0; i < mgmds.size(); i++)
    CHECK(mgmds[i]->wait_confirmed_config());

  // Check binary config files created
  CHECK(file_exists(path(wd.path(), "ndb_1_config.bin.1", NULL).c_str()));
  CHECK(file_exists(path(wd.path(), "ndb_2_config.bin.1", NULL).c_str()));

  return NDBT_OK;
}

/* Test for bug 53008:  --skip-config-cache */
int runTestNoConfigCache(NDBT_Context *ctx, NDBT_Step *step) {
  NDBT_Workingdir wd("test_mgmd");  // temporary working directory

  g_err << "** Create config.ini" << endl;
  Properties config = ConfigFactory::create();
  CHECK(ConfigFactory::write_config_ini(
      config, path(wd.path(), "config.ini", NULL).c_str()));

  MgmdProcessList mgmds;

  // Start ndb_mgmd  from config.ini
  Mgmd *mgmd = new Mgmd(1);
  mgmds.push_back(mgmd);

  CHECK(mgmd->start_from_config_ini(wd.path(), "--skip-config-cache", NULL));

  // Connect the ndb_mgmd(s)
  CHECK(mgmd->connect(config));

  // wait for confirmed config
  CHECK(mgmd->wait_confirmed_config());

  // Check binary config files *not* created
  bool bin_conf_file =
      file_exists(path(wd.path(), "ndb_1_config.bin.1", NULL).c_str());
  CHECK(bin_conf_file == false);

  mgmd->stop();
  return NDBT_OK;
}

/* Test for BUG#13428853 */
int runTestNoConfigCache_DontCreateConfigDir(NDBT_Context *ctx,
                                             NDBT_Step *step) {
  NDBT_Workingdir wd("test_mgmd");  // temporary working directory

  g_err << "** Create config.ini" << endl;
  Properties config = ConfigFactory::create();
  CHECK(ConfigFactory::write_config_ini(
      config, path(wd.path(), "config.ini", NULL).c_str()));

  MgmdProcessList mgmds;

  g_err << "Test no configdir is created with --skip-config-cache" << endl;
  Mgmd *mgmd = new Mgmd(1);
  mgmds.push_back(mgmd);

  CHECK(mgmd->start_from_config_ini(wd.path(), "--skip-config-cache",
                                    "--config-dir=dir37", NULL));

  // Connect the ndb_mgmd(s)
  CHECK(mgmd->connect(config));

  // wait for confirmed config
  CHECK(mgmd->wait_confirmed_config());

  // Check configdir not created
  CHECK(!file_exists(path(wd.path(), "dir37", NULL).c_str()));

  mgmd->stop();

  g_err << "Also test --initial --skip-config-cache" << endl;
  // Also test starting ndb_mgmd --initial --skip-config-cache
  CHECK(mgmd->start_from_config_ini(wd.path(), "--skip-config-cache",
                                    "--initial", "--config-dir=dir37", NULL));
  // Connect the ndb_mgmd(s)
  CHECK(mgmd->connect(config));

  // wait for confirmed config
  CHECK(mgmd->wait_confirmed_config());

  // Check configdir not created
  CHECK(!file_exists(path(wd.path(), "dir37", NULL).c_str()));

  mgmd->stop();
  return NDBT_OK;
}

int runTestNoConfigCache_Fetch(NDBT_Context *ctx, NDBT_Step *step) {
  NDBT_Workingdir wd("test_mgmd");  // temporary working directory

  Properties config = ConfigFactory::create(2);
  CHECK(ConfigFactory::write_config_ini(
      config, path(wd.path(), "config.ini", NULL).c_str()));

  MgmdProcessList mgmds;
  // Start ndb_mgmd 1 from config.ini without config cache
  Mgmd *mgmd = new Mgmd(1);
  mgmds.push_back(mgmd);
  CHECK(mgmd->start_from_config_ini(wd.path(), "--skip-config-cache", NULL));

  // Start ndb_mgmd 2 without config cache and by fetching from first
  Mgmd *mgmd2 = new Mgmd(2);
  mgmds.push_back(mgmd2);
  CHECK(mgmd2->start(wd.path(), "--ndb-connectstring",
                     mgmd->connectstring(config).c_str(), "--skip-config-cache",
                     NULL));

  // Connect the ndb_mgmd(s)
  for (unsigned i = 0; i < mgmds.size(); i++) CHECK(mgmds[i]->connect(config));

  // wait for confirmed config
  for (unsigned i = 0; i < mgmds.size(); i++)
    CHECK(mgmds[i]->wait_confirmed_config());

  return NDBT_OK;
}

int runTestNowaitNodes(NDBT_Context *ctx, NDBT_Step *step) {
  MgmdProcessList mgmds;
  NDBT_Workingdir wd("test_mgmd");  // temporary working directory

  // Create config.ini
  unsigned nodeids[] = {1, 2};
  Properties config = ConfigFactory::create(2, 1, 1, nodeids);
  CHECK(ConfigFactory::write_config_ini(
      config, path(wd.path(), "config.ini", NULL).c_str()));

  BaseString binfile[2];
  binfile[0].assfmt("ndb_%u_config.bin.1", nodeids[0]);
  binfile[1].assfmt("ndb_%u_config.bin.1", nodeids[1]);

  // Start first ndb_mgmd
  Mgmd *mgmd1 = new Mgmd(nodeids[0]);
  {
    mgmds.push_back(mgmd1);
    BaseString arg;
    arg.assfmt("--nowait-nodes=%u", nodeids[1]);
    CHECK(mgmd1->start_from_config_ini(wd.path(), "--initial", arg.c_str(),
                                       NULL));

    // Connect the ndb_mgmd
    CHECK(mgmd1->connect(config));

    // wait for confirmed config
    CHECK(mgmd1->wait_confirmed_config());

    // Check binary config file created
    CHECK(file_exists(path(wd.path(), binfile[0].c_str(), NULL).c_str()));
  }

  // Start second ndb_mgmd
  {
    Mgmd *mgmd2 = new Mgmd(nodeids[1]);
    mgmds.push_back(mgmd2);
    CHECK(mgmd2->start_from_config_ini(wd.path(), "--initial", NULL));

    // Connect the ndb_mgmd
    CHECK(mgmd2->connect(config));

    // wait for confirmed config
    CHECK(mgmd2->wait_confirmed_config());

    // Check binary config file created
    CHECK(file_exists(path(wd.path(), binfile[1].c_str(), NULL).c_str()));
  }

  // Create new config.ini
  g_err << "** Create config2.ini" << endl;
  CHECK(ConfigFactory::put(config, "ndb_mgmd", nodeids[0], "ArbitrationDelay",
                           100));
  CHECK(ConfigFactory::write_config_ini(
      config, path(wd.path(), "config2.ini", NULL).c_str()));

  g_err << "** Reload second mgmd from config2.ini" << endl;
  {
    Mgmd *mgmd2 = mgmds[1];
    CHECK(mgmd2->stop());
    // Start from config2.ini
    CHECK(mgmd2->start_from_config_ini(wd.path(), "-f", "config2.ini",
                                       "--reload", NULL));
    CHECK(mgmd2->connect(config));
    CHECK(mgmd1->wait_confirmed_config());
    CHECK(mgmd2->wait_confirmed_config());

    CHECK(file_exists(path(wd.path(), binfile[0].c_str(), NULL).c_str()));
    CHECK(file_exists(path(wd.path(), binfile[1].c_str(), NULL).c_str()));

    // Both ndb_mgmd(s) should have reloaded and new binary config exist
    binfile[0].assfmt("ndb_%u_config.bin.2", nodeids[0]);
    binfile[1].assfmt("ndb_%u_config.bin.2", nodeids[1]);
    CHECK(file_exists(path(wd.path(), binfile[0].c_str(), NULL).c_str()));
    CHECK(file_exists(path(wd.path(), binfile[1].c_str(), NULL).c_str()));
  }

  // Stop the ndb_mgmd(s)
  for (unsigned i = 0; i < mgmds.size(); i++) CHECK(mgmds[i]->stop());

  return NDBT_OK;
}

int runTestNowaitNodes2(NDBT_Context *ctx, NDBT_Step *step) {
  int ret;
  NDBT_Workingdir wd("test_mgmd");  // temporary working directory

  // Create config.ini
  Properties config = ConfigFactory::create(2);
  CHECK(ConfigFactory::write_config_ini(
      config, path(wd.path(), "config.ini", NULL).c_str()));

  g_err << "** Start mgmd1 from config.ini" << endl;
  MgmdProcessList mgmds;
  Mgmd *mgmd1 = new Mgmd(1);
  mgmds.push_back(mgmd1);
  CHECK(mgmd1->start_from_config_ini(wd.path(), "--initial",
                                     "--nowait-nodes=1-255", NULL));
  CHECK(mgmd1->connect(config));
  CHECK(mgmd1->wait_confirmed_config());

  // check config files exist
  CHECK(file_exists(path(wd.path(), "ndb_1_config.bin.1", NULL).c_str()));

  g_err << "** Create config2.ini" << endl;
  CHECK(ConfigFactory::put(config, "ndb_mgmd", 1, "ArbitrationDelay", 100));
  CHECK(ConfigFactory::write_config_ini(
      config, path(wd.path(), "config2.ini", NULL).c_str()));

  g_err << "** Start mgmd2 from config2.ini" << endl;
  Mgmd *mgmd2 = new Mgmd(2);
  mgmds.push_back(mgmd2);
  CHECK(mgmd2->start_from_config_ini(wd.path(), "-f", "config2.ini",
                                     "--initial", "--nowait-nodes=1-255",
                                     NULL));
  CHECK(mgmd2->wait(ret));
  CHECK(ret == 1);

  CHECK(mgmd1->stop());

  g_err << "** Start mgmd2 again from config2.ini" << endl;
  CHECK(mgmd2->start_from_config_ini(wd.path(), "-f", "config2.ini",
                                     "--initial", "--nowait-nodes=1-255",
                                     NULL));

  CHECK(mgmd2->connect(config));
  CHECK(mgmd2->wait_confirmed_config());

  // check config files exist
  CHECK(file_exists(path(wd.path(), "ndb_2_config.bin.1", NULL).c_str()));

  g_err << "** Start mgmd1 from config.ini, mgmd2 should shutdown" << endl;
  CHECK(mgmd1->start_from_config_ini(wd.path(), "--initial",
                                     "--nowait-nodes=1-255", NULL));
  CHECK(mgmd2->wait(ret));
  CHECK(ret == 1);

  CHECK(mgmd1->stop());

  return NDBT_OK;
}

int runBug56844(NDBT_Context *ctx, NDBT_Step *step) {
  NDBT_Workingdir wd("test_mgmd");  // temporary working directory

  g_err << "** Create config.ini" << endl;
  Properties config = ConfigFactory::create(2);
  CHECK(ConfigFactory::write_config_ini(
      config, path(wd.path(), "config.ini", NULL).c_str()));
  // Start ndb_mgmd(s)
  MgmdProcessList mgmds;
  for (int i = 1; i <= 2; i++) {
    Mgmd *mgmd = new Mgmd(i);
    mgmds.push_back(mgmd);
    CHECK(mgmd->start_from_config_ini(wd.path()));
  }

  // Connect the ndb_mgmd(s)
  for (unsigned i = 0; i < mgmds.size(); i++) {
    CHECK(mgmds[i]->connect(config));
  }

  // wait for confirmed config
  for (unsigned i = 0; i < mgmds.size(); i++) {
    CHECK(mgmds[i]->wait_confirmed_config());
  }

  // stop them
  for (unsigned i = 0; i < mgmds.size(); i++) {
    CHECK(mgmds[i]->stop());
  }

  // Check binary config files created
  CHECK(file_exists(path(wd.path(), "ndb_1_config.bin.1", NULL).c_str()));
  CHECK(file_exists(path(wd.path(), "ndb_2_config.bin.1", NULL).c_str()));

  CHECK(ConfigFactory::put(config, "ndb_mgmd", 1, "ArbitrationDelay", 100));
  CHECK(ConfigFactory::write_config_ini(
      config, path(wd.path(), "config2.ini", NULL).c_str()));
  Uint32 no = 2;
  int loops = ctx->getNumLoops();
  for (int l = 0; l < loops; l++, no++) {
    g_err << l << ": *** Reload from config.ini" << endl;
    for (unsigned i = 0; i < mgmds.size(); i++) {
      // Start from config2.ini
      CHECK(mgmds[i]->start_from_config_ini(
          wd.path(), "-f", (l & 1) == 1 ? "config.ini" : "config2.ini",
          "--reload", NULL));
    }
    for (unsigned i = 0; i < mgmds.size(); i++) {
      CHECK(mgmds[i]->connect(config));
      CHECK(mgmds[i]->wait_confirmed_config());
    }

    /**
     * Since it will first be confirmed...
     *   and then once connected to other ndb_nmgmd start a config
     *   change, it can take a bit until new config exists...
     *   allow 30s
     */
    Uint32 timeout = 30;
    for (unsigned i = 0; i < mgmds.size(); i++) {
      BaseString p = path(wd.path(), "", NULL);
      p.appfmt("ndb_%u_config.bin.%u", i + 1, no);
      g_err << "CHECK(" << p.c_str() << ")" << endl;
      CHECK(file_exists(p.c_str(), timeout));
    }

    for (unsigned i = 0; i < mgmds.size(); i++) {
      CHECK(mgmds[i]->stop());
    }
  }
  return NDBT_OK;
}

static bool get_status(const char *connectstring, Properties &status) {
  NdbMgmd ndbmgmd;
  if (!ndbmgmd.connect(connectstring)) return false;

  Properties args;
  if (!ndbmgmd.call("get status", args, "node status", status, NULL, true)) {
    g_err << "fetch_mgmd_status: mgmd.call failed" << endl;
    return false;
  }
  return true;
}

static bool value_equal(Properties &status, int nodeid, const char *name,
                        const char *expected_value) {
  const char *value;
  BaseString key;
  key.assfmt("node.%d.%s", nodeid, name);
  if (!status.get(key.c_str(), &value)) {
    g_err << "value_equal: no value found for '" << name << "." << nodeid << "'"
          << endl;
    return false;
  }

  if (strcmp(value, expected_value)) {
    g_err << "value_equal: found unexpected value: '" << value
          << "', expected: '" << expected_value << "'" << endl;
    return false;
  }
  g_info << "'" << value << "'=='" << expected_value << "'" << endl;
  return true;
}

#include <ndb_version.h>

int runTestBug12352191(NDBT_Context *ctx, NDBT_Step *step) {
  BaseString version;
  version.assfmt("%u", NDB_VERSION_D);
  BaseString mysql_version;
  mysql_version.assfmt("%u", NDB_MYSQL_VERSION_D);
  BaseString address_ipv4("127.0.0.1");
  BaseString address_ipv6("::1");

  NDBT_Workingdir wd("test_mgmd");  // temporary working directory

  g_err << "** Create config.ini" << endl;
  Properties config = ConfigFactory::create(2);
  CHECK(ConfigFactory::write_config_ini(
      config, path(wd.path(), "config.ini", NULL).c_str()));

  MgmdProcessList mgmds;
  const int nodeid1 = 1;
  Mgmd *mgmd1 = new Mgmd(nodeid1);
  mgmds.push_back(mgmd1);

  const int nodeid2 = 2;
  Mgmd *mgmd2 = new Mgmd(nodeid2);
  mgmds.push_back(mgmd2);

  // Start first mgmd
  CHECK(mgmd1->start_from_config_ini(wd.path()));
  CHECK(mgmd1->connect(config));

  Properties status1;
  CHECK(get_status(mgmd1->connectstring(config).c_str(), status1));
  // status1.print();
  // Check status for own mgm node, always CONNECTED
  CHECK(value_equal(status1, nodeid1, "type", "MGM"));
  CHECK(value_equal(status1, nodeid1, "status", "CONNECTED"));
  CHECK(value_equal(status1, nodeid1, "version", version.c_str()));
  CHECK(value_equal(status1, nodeid1, "mysql_version", mysql_version.c_str()));
  CHECK(value_equal(status1, nodeid1, "address", address_ipv4.c_str()) ||
        value_equal(status1, nodeid1, "address", address_ipv6.c_str()));
  CHECK(value_equal(status1, nodeid1, "startphase", "0"));
  CHECK(value_equal(status1, nodeid1, "dynamic_id", "0"));
  CHECK(value_equal(status1, nodeid1, "node_group", "0"));
  CHECK(value_equal(status1, nodeid1, "connect_count", "0"));

  // Check status for other mgm node
  // not started yet -> NO_CONTACT, no address, no versions
  CHECK(value_equal(status1, nodeid2, "type", "MGM"));
  CHECK(value_equal(status1, nodeid2, "status", "NO_CONTACT"));
  CHECK(value_equal(status1, nodeid2, "version", "0"));
  CHECK(value_equal(status1, nodeid2, "mysql_version", "0"));
  CHECK(value_equal(status1, nodeid2, "address", ""));
  CHECK(value_equal(status1, nodeid2, "startphase", "0"));
  CHECK(value_equal(status1, nodeid2, "dynamic_id", "0"));
  CHECK(value_equal(status1, nodeid2, "node_group", "0"));
  CHECK(value_equal(status1, nodeid2, "connect_count", "0"));

  // Start second mgmd
  CHECK(mgmd2->start_from_config_ini(wd.path()));
  CHECK(mgmd2->connect(config));

  // wait for confirmed config
  for (unsigned i = 0; i < mgmds.size(); i++)
    CHECK(mgmds[i]->wait_confirmed_config());

  Properties status2;
  CHECK(get_status(mgmd2->connectstring(config).c_str(), status2));
  // status2.print();
  // Check status for own mgm node, always CONNECTED
  CHECK(value_equal(status2, nodeid2, "type", "MGM"));
  CHECK(value_equal(status2, nodeid2, "status", "CONNECTED"));
  CHECK(value_equal(status2, nodeid2, "version", version.c_str()));
  CHECK(value_equal(status2, nodeid2, "mysql_version", mysql_version.c_str()));
  CHECK(value_equal(status2, nodeid2, "address", address_ipv4.c_str()) ||
        value_equal(status2, nodeid2, "address", address_ipv6.c_str()));
  CHECK(value_equal(status2, nodeid2, "startphase", "0"));
  CHECK(value_equal(status2, nodeid2, "dynamic_id", "0"));
  CHECK(value_equal(status2, nodeid2, "node_group", "0"));
  CHECK(value_equal(status2, nodeid2, "connect_count", "0"));

  // Check status for other mgm node
  // both started now -> CONNECTED, address and versions filled in
  CHECK(value_equal(status2, nodeid1, "type", "MGM"));
  CHECK(value_equal(status2, nodeid1, "status", "CONNECTED"));
  CHECK(value_equal(status2, nodeid1, "version", version.c_str()));
  CHECK(value_equal(status2, nodeid1, "mysql_version", mysql_version.c_str()));
  CHECK(value_equal(status2, nodeid1, "address", address_ipv4.c_str()) ||
        value_equal(status2, nodeid1, "address", address_ipv6.c_str()));
  CHECK(value_equal(status2, nodeid1, "startphase", "0"));
  CHECK(value_equal(status2, nodeid1, "dynamic_id", "0"));
  CHECK(value_equal(status2, nodeid1, "node_group", "0"));
  CHECK(value_equal(status2, nodeid1, "connect_count", "0"));

  Properties status3;
  CHECK(get_status(mgmd1->connectstring(config).c_str(), status3));
  // status3.print();
  // Check status for own mgm node, always CONNECTED
  CHECK(value_equal(status3, nodeid1, "type", "MGM"));
  CHECK(value_equal(status3, nodeid1, "status", "CONNECTED"));
  CHECK(value_equal(status3, nodeid1, "version", version.c_str()));
  CHECK(value_equal(status3, nodeid1, "mysql_version", mysql_version.c_str()));
  CHECK(value_equal(status3, nodeid1, "address", address_ipv4.c_str()) ||
        value_equal(status3, nodeid1, "address", address_ipv6.c_str()));
  CHECK(value_equal(status3, nodeid1, "startphase", "0"));
  CHECK(value_equal(status3, nodeid1, "dynamic_id", "0"));
  CHECK(value_equal(status3, nodeid1, "node_group", "0"));
  CHECK(value_equal(status3, nodeid1, "connect_count", "0"));

  // Check status for other mgm node
  // both started now -> CONNECTED, address and versions filled in
  CHECK(value_equal(status3, nodeid2, "type", "MGM"));
  CHECK(value_equal(status3, nodeid2, "status", "CONNECTED"));
  CHECK(value_equal(status3, nodeid2, "version", version.c_str()));
  CHECK(value_equal(status3, nodeid2, "mysql_version", mysql_version.c_str()));
  CHECK(value_equal(status3, nodeid2, "address", address_ipv4.c_str()) ||
        value_equal(status3, nodeid2, "address", address_ipv6.c_str()));
  CHECK(value_equal(status3, nodeid2, "startphase", "0"));
  CHECK(value_equal(status3, nodeid2, "dynamic_id", "0"));
  CHECK(value_equal(status3, nodeid2, "node_group", "0"));
  CHECK(value_equal(status3, nodeid2, "connect_count", "0"));

  return NDBT_OK;
}

int runBug61607(NDBT_Context *ctx, NDBT_Step *step) {
  NDBT_Workingdir wd("test_mgmd");  // temporary working directory

  // Create config.ini
  const int cnt_mgmd = 1;
  Properties config = ConfigFactory::create(cnt_mgmd);
  CHECK(ConfigFactory::write_config_ini(
      config, path(wd.path(), "config.ini", NULL).c_str()));
  // Start ndb_mgmd(s)
  MgmdProcessList mgmds;
  for (int i = 1; i <= cnt_mgmd; i++) {
    Mgmd *mgmd = new Mgmd(i);
    mgmds.push_back(mgmd);
    CHECK(mgmd->start_from_config_ini(wd.path()));
  }

  // Connect the ndb_mgmd(s)
  for (unsigned i = 0; i < mgmds.size(); i++) CHECK(mgmds[i]->connect(config));

  // wait for confirmed config
  for (unsigned i = 0; i < mgmds.size(); i++)
    CHECK(mgmds[i]->wait_confirmed_config());

  // Check binary config files created
  CHECK(file_exists(path(wd.path(), "ndb_1_config.bin.1", NULL).c_str()));

  int no_of_nodes = 0;
  int *node_ids = 0;
  int initialstart = 0;
  int nostart = 0;
  int abort = 0;
  int force = 0;
  int need_disconnect = 0;
  int res =
      ndb_mgm_restart4(mgmds[0]->handle(), no_of_nodes, node_ids, initialstart,
                       nostart, abort, force, &need_disconnect);

  return res == 0 ? NDBT_OK : NDBT_FAILED;
}

int runStopDuringStart(NDBT_Context *ctx, NDBT_Step *step) {
  MgmdProcessList mgmds;
  NDBT_Workingdir wd("test_mgmd");  // temporary working directory

  // Create config.ini
  unsigned nodeids[] = {251, 252};
  Properties config = ConfigFactory::create(2, 1, 1, nodeids);
  CHECK(ConfigFactory::write_config_ini(
      config, path(wd.path(), "config.ini", NULL).c_str()));

  for (unsigned i = 0; i < NDB_ARRAY_SIZE(nodeids); i++) {
    Mgmd *mgmd = new Mgmd(nodeids[i]);
    mgmds.push_back(mgmd);
    CHECK(mgmd->start_from_config_ini(wd.path()));
  }

  // Connect the ndb_mgmd(s)
  for (unsigned i = 0; i < mgmds.size(); i++) CHECK(mgmds[i]->connect(config));

  // wait for confirmed config
  for (unsigned i = 0; i < mgmds.size(); i++)
    CHECK(mgmds[i]->wait_confirmed_config());

  // Check binary config files created
  for (unsigned i = 0; i < mgmds.size(); i++) {
    BaseString file;
    file.assfmt("ndb_%u_config.bin.1", nodeids[i]);
    CHECK(file_exists(path(wd.path(), file.c_str(), NULL).c_str()));
  }

  // stop them
  for (unsigned i = 0; i < mgmds.size(); i++) {
    mgmds[i]->stop();
    int exitCode;
    mgmds[i]->wait(exitCode);
  }

  // restart one with error-insert 100
  // => it shall exit during start...
  mgmds[0]->start(wd.path(), "--error-insert=100", NULL);

  // restart rest normally
  for (unsigned i = 1; i < mgmds.size(); i++) {
    mgmds[i]->start(wd.path());
  }

  // wait first one to terminate
  int exitCode;
  mgmds[0]->wait(exitCode);
  NdbSleep_MilliSleep(3000);

  // check other OK
  for (unsigned i = 1; i < mgmds.size(); i++) {
    CHECK(mgmds[i]->connect(config));
    CHECK(mgmds[i]->wait_confirmed_config());
  }

  // now restart without error insert
  mgmds[0]->start(wd.path());

  // connect
  CHECK(mgmds[0]->connect(config));

  // all should be ok
  for (unsigned i = 0; i < mgmds.size(); i++)
    CHECK(mgmds[i]->wait_confirmed_config());

  return NDBT_OK;
}

/* WL#13860: AllowUnresolvedHostnames=false (the default)
   Check that MGM will not start up with unresolved hostname in configuration.
*/
int runTestUnresolvedHosts1(NDBT_Context *ctx, NDBT_Step *step) {
  NDBT_Workingdir wd("test_mgmd");

  char hostname[200];
  CHECK(gethostname(hostname, sizeof(hostname)) == 0);

  Properties config, mgm, ndb, api;
  mgm.put("NodeId", 1);
  mgm.put("HostName", hostname);
  mgm.put("PortNumber", ConfigFactory::get_ndbt_base_port() + /* mysqld */ 1);
  ndb.put("NodeId", 2);
  ndb.put("HostName", "xx-no-such-host.no.oracle.com.");
  ndb.put("NoOfReplicas", 1);
  api.put("NodeId", 3);
  config.put("ndb_mgmd", 1, &mgm);
  config.put("ndbd", 2, &ndb);
  config.put("mysqld", 3, &api);

  CHECK(ConfigFactory::write_config_ini(
      config, path(wd.path(), "config.ini", NULL).c_str()));

  Mgmd mgmd(1);
  int exit_value;
  CHECK(mgmd.start_from_config_ini(wd.path()));
  CHECK(mgmd.wait(exit_value));
  CHECK(exit_value == 1);
  return NDBT_OK;
}

/* WL#13860: AllowUnresolvedHostnames=true
   This test uses a configuration with 144 data nodes, of which 143 have
   unresolvable hostnames, and shows that the one data node with a usable
   hostname successfully connects, while a second data node with a bad hostname
   times out (within 40 seconds) with failure to allocate node id.
*/
int runTestUnresolvedHosts2(NDBT_Context *ctx, NDBT_Step *step) {
  NDBT_Workingdir wd("test_mgmd");

  char hostname[200];
  CHECK(gethostname(hostname, sizeof(hostname)) == 0);

  Properties config;
  {
    // 144 ndbds, nodeid 1 -> 144
    for (int i = 1; i <= 144; i++) {
      Properties ndbd;
      ndbd.put("NodeId", i);
      ndbd.put("NoOfReplicas", 4);
      if (i == 1) {
        // Node 1 has a good hostname.
        ndbd.put("HostName", hostname);
      } else {
        // The other have unresolvable hostnames.
        ndbd.put("HostName", "xx-no-such-host.no.oracle.com.");
      }
      config.put("ndbd", i, &ndbd);
    }
  }
  {
    // 1 ndb_mgmd, nodeid 145
    Properties mgmd;
    mgmd.put("NodeId", 145);
    mgmd.put("HostName", hostname);
    mgmd.put("PortNumber",
             ConfigFactory::get_ndbt_base_port() + /* mysqld */ 1);
    config.put("ndb_mgmd", 145, &mgmd);
  }
  {
    // 1 mysqld, nodeid 151
    Properties mysqld;
    mysqld.put("NodeId", 151);
    config.put("mysqld", 151, &mysqld);
  }
  {
    Properties tcp;
    tcp.put("AllowUnresolvedHostnames", "true");
    config.put("TCP DEFAULT", &tcp);
  }

  CHECK(ConfigFactory::write_config_ini(
      config, path(wd.path(), "config.ini", NULL).c_str()));

  /* Start the management node and data node 1 together, and expect this to
     succeed despite the unresolvable host names and large configuration.
  */
  Mgmd mgmd(145);
  Ndbd ndbd1(1);

  CHECK(
      ndbd1.start(wd.path(), mgmd.connectstring(config)));  // Start data node 1
  CHECK(mgmd.start_from_config_ini(wd.path()));  // Start management node
  CHECK(mgmd.connect(config));                   // Connect to management node
  CHECK(mgmd.wait_confirmed_config());           // Wait for configuration

  /* Start data node 2.
     Expect it to run for at least 20 seconds, trying to allocate a node id.
  */
  int ndbd_exit_code;
  Ndbd ndbd2(2);
  CHECK(ndbd2.start(wd.path(), mgmd.connectstring(config)));
  CHECK(ndbd2.wait(ndbd_exit_code, 20000) == 0);
  CHECK(ndbd2.wait(ndbd_exit_code, 40000) == 1);
  CHECK(ndbd1.stop());
  CHECK(mgmd.stop());

  BaseString mgmdlog = path(wd.path(), "ndb_145_cluster.log", nullptr);
  Vector<BaseString> search_list;
  search_list.push_back("Unable to allocate nodeid for NDB");
  CHECK(Print_find_in_file(mgmdlog.c_str(), search_list) == true);

  return NDBT_OK;
}

int runTestMgmdwithoutnodeid(NDBT_Context *ctx, NDBT_Step *step) {
  NDBT_Workingdir wd("test_mgmd");
  Vector<BaseString> search_list;

  Properties config, mgm, mgm2, mgm3, ndb, api;
  mgm.put("HostName", "190.10.10.4");
  ndb.put("HostName", "190.10.10.1");
  ndb.put("NoOfReplicas", 1);
  config.put("ndb_mgmd", 1, &mgm);
  config.put("ndbd", 2, &ndb);
  config.put("mysqld", 3, &api);

  CHECK(ConfigFactory::write_config_ini(
      config, path(wd.path(), "config.ini", NULL).c_str()));

  Mgmd mgmd;
  int exit_value;
  // write the stdout to temp file
  BaseString out_file = path(wd.path(), "out.txt", NULL);
  FILE *temp_file = fopen(out_file.c_str(), "w");
  int file_desc = open(out_file.c_str(), O_WRONLY | O_APPEND);
  int stdoutCopy = dup(1);
  if (dup2(file_desc, 1) < 0) return NDBT_FAILED;
  close(file_desc);

  // TEST 1: start mgmd without nodeid and unknown address
  with_nodeid = false;
  CHECK(mgmd.start_from_config_ini(wd.path()));
  CHECK(mgmd.wait(exit_value));
  CHECK(exit_value == 1);
  with_nodeid = true;
  search_list.push_back(
      "At least one hostname in the configuration does not match a local "
      "interface");

  // TEST 2:start mgmd without nodeid and config containing 2 mgmd
  // sections with same valid hostname
  char hostname[200];
  CHECK(gethostname(hostname, sizeof(hostname)) == 0);
  mgm2.put("HostName", hostname);
  mgm2.put("PortNumber", 1011);
  mgm3.put("HostName", hostname);
  config.put("ndb_mgmd", 4, &mgm2);
  config.put("ndb_mgmd", 5, &mgm3);
  CHECK(ConfigFactory::write_config_ini(
      config, path(wd.path(), "config2.ini", NULL).c_str()));
  with_nodeid = false;
  CHECK(mgmd.start_from_config_ini(wd.path(), "-f", "config2.ini", "--initial",
                                   NULL));
  CHECK(mgmd.wait(exit_value));
  CHECK(exit_value == 1);
  with_nodeid = true;
  search_list.push_back(
      "More than one hostname matches a local interface, including node ids");

  // TEST 3: Check if the error message truncate if the length of hostnames are
  // too big
  Properties config3, ndb3, api3;
  ndb3.put("HostName", "190.10.10.1");
  ndb3.put("NoOfReplicas", 1);
  for (int i = 1; i < 80; i++) {
    Properties p1;
    std::string host_generated = "190.100.100." + std::to_string(i);
    p1.put("HostName", host_generated.c_str());
    config3.put("ndb_mgmd", i, &p1);
  }
  config3.put("ndbd", 80, &ndb3);
  config3.put("mysqld", 81, &api3);
  CHECK(ConfigFactory::write_config_ini(
      config3, path(wd.path(), "config3.ini", NULL).c_str()));
  with_nodeid = false;
  CHECK(mgmd.start_from_config_ini(wd.path(), "-f", "config3.ini", "--initial",
                                   NULL));
  CHECK(mgmd.wait(exit_value));
  CHECK(exit_value == 1);
  with_nodeid = true;

  // Write the stdout back to the screen
  if (dup2(stdoutCopy, 1) < 0) return NDBT_FAILED;
  close(stdoutCopy);

  // Search output log for the matching error message
  CHECK(Print_find_in_file(out_file.c_str(), search_list) == true);
  fclose(temp_file);
  remove(out_file.c_str());
  return NDBT_OK;
}

/*
 * Check that when there are multiple MGMD nodes, if one MGMD connects and later
 * disconnects the other MGMD nodes are aware of the offline node and continue
 * their normal work.
 * Test case introduced as part of Bug #34582919 fix.
 */
int runTestMultiMGMDDisconnection(NDBT_Context *ctx, NDBT_Step *step) {
  NDBT_Workingdir wd("test_mgmd");
  char hostname[200];
  CHECK(gethostname(hostname, sizeof(hostname)) == 0);

  Properties config;
  // 3 MGMD nodes, nodeid 1, 2 and 3
  for (int i = 1; i <= 3; i++) {
    Properties mgmd;
    mgmd.put("NodeId", i);
    mgmd.put("HostName", hostname);
    mgmd.put("PortNumber", ConfigFactory::get_ndbt_base_port() + i);
    config.put("ndb_mgmd", i, &mgmd);
  }

  // 2 NDBD nodes, nodeid 10 and 11
  for (int i = 10; i <= 11; i++) {
    Properties ndbd;
    ndbd.put("NodeId", i);
    ndbd.put("NoOfReplicas", 2);
    ndbd.put("HostName", hostname);
    config.put("ndbd", i, &ndbd);
  }
  {
    // 1 mysqld, nodeid 21
    Properties mysqld;
    mysqld.put("NodeId", 21);
    config.put("mysqld", 21, &mysqld);
  }

  CHECK(ConfigFactory::write_config_ini(
      config, path(wd.path(), "config.ini", NULL).c_str()));

  // Start the 3 management nodes and the 2 data node together
  Mgmd mgmd1(1);
  Mgmd mgmd2(2);
  Mgmd mgmd3(3);
  Ndbd ndbd1(10);
  Ndbd ndbd2(11);

  CHECK(mgmd1.start_from_config_ini(wd.path(), "--initial", nullptr));
  CHECK(mgmd2.start_from_config_ini(wd.path(), "--initial", nullptr));
  CHECK(mgmd3.start_from_config_ini(wd.path(), "--initial", nullptr));

  CHECK(ndbd1.start(wd.path(), mgmd1.connectstring(config)));
  CHECK(ndbd2.start(wd.path(), mgmd1.connectstring(config)));

  CHECK(mgmd1.connect(config));
  CHECK(mgmd1.wait_confirmed_config());
  CHECK(mgmd2.connect(config));
  CHECK(mgmd2.wait_confirmed_config());
  CHECK(mgmd3.connect(config));
  CHECK(mgmd3.wait_confirmed_config());

  // Wait 15 secs for each data node to reach the started status
  NdbMgmHandle handle = mgmd1.handle();
  CHECK(ndbd1.wait_started(handle, 15, 0));
  CHECK(ndbd2.wait_started(handle, 15, 1));

  // Stop the ndb_mgmd(s)
  CHECK(mgmd3.stop());
  CHECK(mgmd2.stop());
  CHECK(mgmd1.stop());

  CHECK(mgmd3.wait_confirmed_config() == 0);
  CHECK(mgmd2.wait_confirmed_config() == 0);
  CHECK(mgmd1.wait_confirmed_config() == 0);

  // Start MGMD 1 again
  CHECK(mgmd1.start_from_config_ini(wd.path(), "--initial", nullptr));
  CHECK(mgmd1.connect(config));

  // Start MGMD 2 again
  CHECK(mgmd2.start_from_config_ini(wd.path(), "--initial", nullptr));
  CHECK(mgmd2.connect(config));

  // Stop MGMD 2
  CHECK(mgmd2.stop());
  CHECK(mgmd2.wait_confirmed_config() == 0);

  // Start MGMD 3 again (Without Bug#34582919 fix MGMD 1 should crash)
  CHECK(mgmd3.start_from_config_ini(wd.path(), "--initial", nullptr));
  CHECK(mgmd3.connect(config));

  // Start MGMD 2 again
  CHECK(mgmd2.start_from_config_ini(wd.path(), "--initial", nullptr));
  CHECK(mgmd2.connect(config));

  // All MGMDs are running
  CHECK(mgmd1.wait_confirmed_config());
  CHECK(mgmd2.wait_confirmed_config());
  CHECK(mgmd3.wait_confirmed_config());

  return NDBT_OK;
}

int runTestSshKeySigning(NDBT_Context *ctx, NDBT_Step *step) {
  /* Skip this test in PB2 environments, where "ssh localhost"
     does not necessarily work.
  */
  if (getenv("PB2WORKDIR")) {
    printf("Skipping test SshKeySigning\n");
    return NDBT_OK;
  }

  NDBT_Workingdir wd("test_mgmd");  // temporary working directory
  Properties config = ConfigFactory::create();
  ConfigFactory::put(config, "ndb_mgmd", 1, "RequireCertificate", "true");
  BaseString cfg_path = path(wd.path(), "config.ini", nullptr);
  CHECK(ConfigFactory::write_config_ini(config, cfg_path.c_str()));

  /* Find executable */
  BaseString exe;
  NDBT_find_sign_keys(exe);

  /* Create CA */
  if (!create_CA(wd, exe)) return false;

  /* Create keys and certificates for all nodes, via ssh to localhost */
  /* There will be a parent ndb_sign_keys process plus 3 ssh invocations */
  NdbProcess::Args args;
  int ret;
  {
    args.add("--config-file=", cfg_path.c_str());
    args.add("--passphrase=", "Trondheim");
    args.add("--ndb-tls-search-path=", wd.path());
    args.add("--create-key");
    args.add("--remote-exec-path=", exe.c_str());
    args.add("--remote-CA-host=", "localhost");
    auto proc = NdbProcess::create("Create Keys", exe, wd.path(), args);
    bool r = proc->wait(ret, 5000);
    if (!r) proc->stop();
    CHECK(r);
    CHECK(ret == 0);
  }
  CHECK(check_cert(wd, Node::Type::DB));

  /* Sign again, this time using openssl. ndb_sign_keys is called with
     the --remote-openssl option, and with --CA-cert and --CA-key holding
     the full paths to the CA PEM files on the remote server.
  */
  {
    BaseString ca_cert(wd.path());
    ca_cert.append(DIR_SEPARATOR).append(ClusterCertAuthority::CertFile);
    BaseString ca_key(wd.path());
    ca_key.append(DIR_SEPARATOR).append(ClusterCertAuthority::KeyFile);

    args.clear();
    args.add("--config-file=", cfg_path.c_str());
    args.add("--passphrase=", "Trondheim");
    args.add("--ndb-tls-search-path=", wd.path());
    args.add("--remote-openssl");
    args.add("--remote-CA-host=", "localhost");
    args.add("--CA-cert=", ca_cert.c_str());
    args.add("--CA-key=", ca_key.c_str());
    auto proc = NdbProcess::create("OpenSSL", exe, wd.path(), args);
    bool r = proc->wait(ret, 5000);
    if (!r) proc->stop();
    CHECK(r);
    CHECK(ret == 0);
  }
  CHECK(check_cert(wd, Node::Type::DB));

  /* Prove that the certificates created above are usable, by starting the mgmd.
   */
  args.clear();
  Mgmd mgmd(1);
  mgmd.common_args(args, wd.path());
  args.add("--ndb-tls-search-path=", wd.path());
  CHECK(mgmd.start(wd.path(), args));
  CHECK(mgmd.connect(config, 1, 5));
  CHECK(mgmd.wait_confirmed_config());
  CHECK(mgmd.stop());

  return NDBT_OK;
}

int runTestKeySigningTool(NDBT_Context *, NDBT_Step *) {
  NDBT_Workingdir wd("test_mgmd");  // temporary working directory
  Properties config = ConfigFactory::create();
  BaseString cfg_path = path(wd.path(), "config.ini", nullptr);
  CHECK(ConfigFactory::write_config_ini(config, cfg_path.c_str()));

  /* Find executable */
  BaseString exe;
  NDBT_find_sign_keys(exe);

  /* Create CA */
  if (!create_CA(wd, exe)) return false;

  /* Create key and certificate for node 2 */
  NdbProcess::Args args;
  int ret = -1;
  args.add("--config-file=", cfg_path.c_str());
  args.add("--passphrase=", "Trondheim");
  args.add("--ndb-tls-search-path=", wd.path());
  args.add("--create-key");
  args.add("-n", 2);
  args.add("--CA-tool=", exe.c_str());
  auto proc = NdbProcess::create("Create Keys", exe, wd.path(), args);
  bool r = proc->wait(ret, 10000);
  if (!r) proc->stop();
  CHECK(r);
  CHECK(ret == 0);
  return NDBT_OK;
}

int runTestMgmdWithoutCert(NDBT_Context *ctx, NDBT_Step *step) {
  NDBT_Workingdir wd("test_mgmd");  // temporary working directory
  BaseString cfg_path = path(wd.path(), "config.ini", nullptr);

  Properties config = ConfigFactory::create();
  ConfigFactory::put(config, "ndb_mgmd", 1, "RequireCertificate", "true");
  CHECK(ConfigFactory::write_config_ini(config, cfg_path.c_str()));

  Mgmd mgmd(1);
  int exitCode;
  CHECK(mgmd.start_from_config_ini(wd.path()));  // Start management node
  CHECK(mgmd.wait(exitCode));
  CHECK(exitCode == 1);
  return NDBT_OK;
}

int runTestApiWithoutCert(NDBT_Context *ctx, NDBT_Step *step) {
  NDBT_Workingdir wd("test_tls");  // temporary working directory

  BaseString cfg_path = path(wd.path(), "config.ini", nullptr);
  Properties config = ConfigFactory::create();
  CHECK(ConfigFactory::put(config, "ndbd", 2, "RequireTls", "true"));
  CHECK(ConfigFactory::write_config_ini(config, cfg_path.c_str()));

  CHECK(sign_tls_keys(wd));

  Mgmd mgmd(1);
  Ndbd ndbd(2);

  NdbProcess::Args mgmdArgs;
  mgmd.common_args(mgmdArgs, wd.path());

  CHECK(mgmd.start(wd.path(), mgmdArgs));  // Start management node
  CHECK(mgmd.connect(config));             // Connect to management node
  CHECK(mgmd.wait_confirmed_config());     // Wait for configuration

  ndbd.args().add("--ndb-tls-search-path=", wd.path());
  ndbd.start(wd.path(), mgmd.connectstring(config));  // Start data node
  NdbMgmHandle handle = mgmd.handle();
  CHECK(ndbd.wait_started(handle));

  /* API has no TLS context and should fail to connect */
  Ndb_cluster_connection con(mgmd.connectstring(config).c_str());
  con.set_name("api_without_cert");
  int r = con.connect(0, 0, 1);
  CHECK(r == -1);
  printf("ERROR %d: %s\n", con.get_latest_error(), con.get_latest_error_msg());

  ndbd.stop();
  mgmd.stop();
  return NDBT_OK;
}

int runTestNdbdWithoutCert(NDBT_Context *ctx, NDBT_Step *step) {
  NDBT_Workingdir wd("test_mgmd");  // temporary working directory
  BaseString cfg_path = path(wd.path(), "config.ini", nullptr);

  Properties config = ConfigFactory::create();
  Properties db;
  db.put("RequireCertificate", "true");
  config.put("DB Default", &db);

  CHECK(ConfigFactory::write_config_ini(config, cfg_path.c_str()));

  Mgmd mgmd(1);
  Ndbd ndbd(2);

  CHECK(mgmd.start_from_config_ini(wd.path()));  // Start management node
  CHECK(mgmd.connect(config));                   // Connect to management node
  CHECK(mgmd.wait_confirmed_config());           // Wait for configuration

  int exit_code;  // Start ndbd; it will fail
  CHECK(ndbd.start(wd.path(), mgmd.connectstring(config)));
  CHECK(ndbd.wait(exit_code, 5000));  // should fail quickly
  require(exit_code == 255);

  CHECK(mgmd.stop());
  return NDBT_OK;
}

int runTestNdbdWithExpiredCert(NDBT_Context *ctx, NDBT_Step *step) {
  NDBT_Workingdir wd("test_tls");  // temporary working directory

  BaseString cfg_path = path(wd.path(), "config.ini", nullptr);

  Properties config = ConfigFactory::create();
  Properties db;
  db.put("RequireCertificate", "true");
  config.put("DB Default", &db);
  CHECK(ConfigFactory::write_config_ini(config, cfg_path.c_str()));

  CHECK(create_expired_cert(wd));

  Mgmd mgmd(1);
  Ndbd ndbd(2);

  CHECK(mgmd.start_from_config_ini(wd.path()));  // Start management node
  CHECK(mgmd.connect(config));                   // Connect to management node
  CHECK(mgmd.wait_confirmed_config());           // Wait for configuration

  ndbd.args().add("--ndb-tls-search-path=", wd.path());
  ndbd.start(wd.path(), mgmd.connectstring(config));  // Start data node

  int exit_code;
  CHECK(ndbd.wait(exit_code, 5000));  // should fail quickly
  CHECK(exit_code == 255);

  CHECK(mgmd.stop());
  return NDBT_OK;
}

int runTestNdbdWithCert(NDBT_Context *ctx, NDBT_Step *step) {
  NDBT_Workingdir wd("test_tls");  // temporary working directory

  BaseString cfg_path = path(wd.path(), "config.ini", nullptr);
  Properties config = ConfigFactory::create();
  Properties db;
  db.put("RequireCertificate", "true");
  config.put("DB Default", &db);
  ConfigFactory::put(config, "ndb_mgmd", 1, "RequireTls", "true");
  CHECK(ConfigFactory::write_config_ini(config, cfg_path.c_str()));

  CHECK(sign_tls_keys(wd));

  Mgmd mgmd(1);
  Ndbd ndbd(2);

  NdbProcess::Args mgmdArgs;
  mgmd.common_args(mgmdArgs, wd.path());
  mgmdArgs.add("--ndb-tls-search-path=", wd.path());

  TlsKeyManager tls_km;
  tls_km.init_mgm_client(wd.path(), Node::Type::DB);

  CHECK(mgmd.start(wd.path(), mgmdArgs));  // Start management node
  CHECK(mgmd.connect(config));             // Connect to management node
  CHECK(mgmd.client_start_tls(tls_km.ctx()) == 0);  // Start TLS
  CHECK(mgmd.wait_confirmed_config());              // Wait for configuration

  ndbd.args().add("--ndb-tls-search-path=", wd.path());
  ndbd.args().add("--ndb-mgm-tls=strict");
  ndbd.start(wd.path(), mgmd.connectstring(config));  // Start data node
  NdbMgmHandle handle = mgmd.handle();
  CHECK(ndbd.wait_started(handle));

  CHECK(mgmd.stop());
  CHECK(ndbd.stop());
  return NDBT_OK;
}

int runTestStartTls(NDBT_Context *ctx, NDBT_Step *step) {
  NDBT_Workingdir wd("test_tls");  // temporary working directory
  TlsKeyManager tls_km;
  int major, minor, build, r;
  char ver[128];
  static constexpr int len = sizeof(ver);

  BaseString cfg_path = path(wd.path(), "config.ini", nullptr);
  Properties config = ConfigFactory::create();
  CHECK(ConfigFactory::write_config_ini(config, cfg_path.c_str()));

  sign_tls_keys(wd);

  Mgmd mgmd(1);

  NdbProcess::Args mgmdArgs;
  mgmd.common_args(mgmdArgs, wd.path());
  mgmdArgs.add("--ndb-tls-search-path=", wd.path());

  CHECK(mgmd.start(wd.path(), mgmdArgs));  // Start management node
  CHECK(mgmd.connect(config));             // Connect to management node
  CHECK(mgmd.wait_confirmed_config());     // Wait for configuration

  tls_km.init_mgm_client(wd.path());
  CHECK(tls_km.ctx());

  r = ndb_mgm_get_version(mgmd.handle(), &major, &minor, &build, len, ver);
  CHECK(r == 1);
  printf("Version: %d.%d.%d %s\n", major, minor, build, ver);

  r = ndb_mgm_start_tls(mgmd.handle());
  CHECK(r == -1);  // -1 is "SSL CTX required"
  CHECK(ndb_mgm_get_latest_error(mgmd.handle()) == NDB_MGM_TLS_ERROR);

  r = ndb_mgm_set_ssl_ctx(mgmd.handle(), tls_km.ctx());
  CHECK(r == 0);  // first time setting ctx succeeds
  r = ndb_mgm_set_ssl_ctx(mgmd.handle(), nullptr);
  CHECK(r == -1);  // second time setting ctx fails

  r = ndb_mgm_start_tls(mgmd.handle());
  printf("ndb_mgm_start_tls(): %d\n", r);
  CHECK(r == 0);

  r = ndb_mgm_start_tls(mgmd.handle());
  CHECK(r == -2);  // -2 is "Socket already has TLS"

  /* We have switched to TLS. Now run a command. */
  r = ndb_mgm_get_version(mgmd.handle(), &major, &minor, &build, len, ver);
  CHECK(r == 1);

  /* And run another command. */
  struct ndb_mgm_cluster_state *state = ndb_mgm_get_status(mgmd.handle());
  CHECK(state != nullptr);

  /* Now convert the socket to a transporter */
  NdbSocket s = mgmd.convert_to_transporter();
  CHECK(s.is_valid());
  CHECK(s.close() == 0);

  return NDBT_OK;
}

int runTestRequireTls(NDBT_Context *ctx, NDBT_Step *step) {
  /* Create a configuration file in the working directory */
  NDBT_Workingdir wd("test_tls");
  BaseString cfg_path = path(wd.path(), "config.ini", nullptr);
  Properties config = ConfigFactory::create();
  ConfigFactory::put(config, "ndb_mgmd", 1, "RequireTls", "true");
  CHECK(ConfigFactory::write_config_ini(config, cfg_path.c_str()));

  /* Create keys in test_tls, and initialize our own TLS context */
  TlsKeyManager tls_km;
  bool k = sign_tls_keys(wd);
  CHECK(k);
  tls_km.init_mgm_client(wd.path());
  CHECK(tls_km.ctx());

  /* Start a management server that will require TLS */
  Mgmd mgmd(1);
  NdbProcess::Args mgmdArgs;
  mgmd.common_args(mgmdArgs, wd.path());
  mgmdArgs.add("--ndb-tls-search-path=", wd.path());
  CHECK(mgmd.start(wd.path(), mgmdArgs));  // Start management node
  sleep(1);                                // Wait for confirmed config

  /* Our management client */
  NdbMgmHandle handle = ndb_mgm_create_handle();
  ndb_mgm_set_connectstring(handle, mgmd.connectstring(config).c_str());
  ndb_mgm_set_ssl_ctx(handle, tls_km.ctx());

  int r = ndb_mgm_connect(handle, 3, 5, 1);  // Connect to management node
  CHECK(r == 0);

  ndb_mgm_severity sev = {NDB_MGM_EVENT_SEVERITY_ON, 1};
  r = ndb_mgm_get_clusterlog_severity_filter(handle, &sev, 1);
  CHECK(r < 1);  // COMMAND IS NOT YET ALLOWED
  int err = ndb_mgm_get_latest_error(handle);
  CHECK(err == NDB_MGM_AUTH_REQUIRES_TLS);

  struct ndb_mgm_cluster_state *st = ndb_mgm_get_status(handle);
  CHECK(st == nullptr);  // COMMAND IS NOT YET ALLOWED
  err = ndb_mgm_get_latest_error(handle);
  CHECK(err == NDB_MGM_AUTH_REQUIRES_TLS);

  r = ndb_mgm_start_tls(handle);
  printf("ndb_mgm_start_tls(): %d\n", r);  // START TLS
  CHECK(r == 0);

  r = ndb_mgm_get_clusterlog_severity_filter(handle, &sev, 1);
  CHECK(r == 1);  // NOW COMMAND IS ALLOWED

  return NDBT_OK;
}

NDBT_TESTSUITE(testMgmd);
DRIVER(DummyDriver); /* turn off use of NdbApi */

TESTCASE("Basic2Mgm", "Basic test with two mgmd") {
  INITIALIZER(runTestBasic2Mgm);
}

TESTCASE("Bug42015",
         "Test that mgmd can fetch configuration from another mgmd") {
  INITIALIZER(runTestBug42015);
}
TESTCASE("NowaitNodes",
         "Test that one mgmd(of 2) can start alone with usage "
         "of --nowait-nodes, then start the second mgmd and it should join") {
  INITIALIZER(runTestNowaitNodes);
}
TESTCASE("NowaitNodes2",
         "Test that one mgmd(of 2) can start alone with usage "
         "of --nowait-nodes, then start the second mgmd from different "
         "configuration and the one with lowest nodeid should shutdown") {
  INITIALIZER(runTestNowaitNodes2);
}

TESTCASE("NoCfgCache",
         "Test that when an mgmd is started with --skip-config-cache, "
         "no ndb_xx_config.xx.bin file is created, but you can "
         "connect to the mgm node and retrieve the config.") {
  INITIALIZER(runTestNoConfigCache);
}
TESTCASE("NoCfgCacheOrConfigDir",
         "Test that when an mgmd is started with --skip-config-cache, "
         "no ndb_xx_config.xx.bin file is created, but you can "
         "connect to the mgm node and retrieve the config.") {
  INITIALIZER(runTestNoConfigCache_DontCreateConfigDir);
}
TESTCASE("NoCfgCacheFetch",
         "Test that when an mgmd is started with --skip-config-cache, "
         "it can still fetch config from another ndb_mgmd.") {
  INITIALIZER(runTestNoConfigCache_Fetch);
}
TESTCASE("Bug45495", "Test that mgmd can be restarted in any order") {
  INITIALIZER(runTestBug45495);
}

TESTCASE("Bug56844", "Test that mgmd can be reloaded in parallel") {
  INITIALIZER(runBug56844);
}
TESTCASE("Mgmdwithoutnodeid",
         "Test that mgmd reports proper error message "
         "when configuration contains unresolvable ip address "
         " and does not include node ids") {
  INITIALIZER(runTestMgmdwithoutnodeid);
}
TESTCASE("Bug12352191", "Test mgmd status for other mgmd") {
  INITIALIZER(runTestBug12352191);
}
TESTCASE(
    "Bug61607",
    "ndb_mgmd incorrectly reports failure when there are no ndbds to stop") {
  INITIALIZER(runBug61607);
}
TESTCASE("StopDuringStart", "") { INITIALIZER(runStopDuringStart); }
TESTCASE("UnresolvedHosts1", "Test mgmd failure due to unresolvable hostname") {
  INITIALIZER(runTestUnresolvedHosts1);
}
TESTCASE("UnresolvedHosts2", "Test mgmd with AllowUnresolvedHostnames=true") {
  INITIALIZER(runTestUnresolvedHosts2);
}
TESTCASE("MultiMGMDDisconnection",
         "Test multi mgmd robustness against other mgmd disconnections") {
  INITIALIZER(runTestMultiMGMDDisconnection);
}

#if OPENSSL_VERSION_NUMBER >= NDB_TLS_MINIMUM_OPENSSL

TESTCASE("MgmdWithoutCertificate",
         "Test MGM server startup with TLS required but no certificate"){
    INITIALIZER(runTestMgmdWithoutCert)}

TESTCASE("NdbdWithoutCertificate",
         "Test data node startup with TLS required but no certificate"){
    INITIALIZER(runTestNdbdWithoutCert)}

TESTCASE("ApiWithoutCertificate",
         "Test API node without certificate where TRP TLS is required"){
    INITIALIZER(runTestApiWithoutCert)}

TESTCASE("NdbdWithExpiredCertificate",
         "Test data node startup with expired certificate"){
    INITIALIZER(runTestNdbdWithExpiredCert)}

TESTCASE("NdbdWithCertificate", "Test data node startup with certificate"){
    INITIALIZER(runTestNdbdWithCert)}

TESTCASE("StartTls", "Test START TLS in MGM protocol") {
  INITIALIZER(runTestStartTls);
}

TESTCASE("RequireTls", "Test MGM server that requires TLS") {
  INITIALIZER(runTestRequireTls);
}

TESTCASE("KeySigningTool", "Test key signing using a co-process tool") {
  INITIALIZER(runTestKeySigningTool);
}

TESTCASE("SshKeySigning",
         "Test remote key signing over ssh using ndb_sign_keys") {
  INITIALIZER(runTestSshKeySigning);
}

#endif

NDBT_TESTSUITE_END(testMgmd)

int main(int argc, const char **argv) {
  ndb_init();
  NDBT_TESTSUITE_INSTANCE(testMgmd);
  testMgmd.setCreateTable(false);
  testMgmd.setRunAllTables(true);
  testMgmd.setConnectCluster(false);
  testMgmd.setEnsureIndexStatTables(false);

#ifdef NDB_USE_GET_ENV
  char buf1[255], buf2[255];
  if (NdbEnv_GetEnv("NDB_MGMD_VALGRIND_EXE", buf1, sizeof(buf1))) {
    exe_valgrind = buf1;
  }

  if (NdbEnv_GetEnv("NDB_MGMD_VALGRIND_ARG", buf2, sizeof(buf2))) {
    arg_valgrind = buf2;
  }
#endif

  return testMgmd.execute(argc, argv);
}

template class Vector<Mgmd *>;
