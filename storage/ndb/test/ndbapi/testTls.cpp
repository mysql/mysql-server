/*
   Copyright (c) 2026, Oracle and/or its affiliates.


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

#include "ConfigFactory.hpp"
#include "NDBT.hpp"
#include "NDBT_Find.hpp"
#include "NDBT_Test.hpp"
#include "NDBT_Workingdir.hpp"
#include "NdbEnv.h"
#include "NdbMgmd.hpp"
#include "NdbProcess.hpp"
#include "NdbRestarter.hpp"
#include "portlib/NdbDir.hpp"
#include "portlib/ssl_applink.h"
#include "util/TlsKeyManager.hpp"
#include "util/ndb_openssl3_compat.h"
#include "util/require.h"

#include "../../src/ndbapi/NdbInfo.hpp"
#include "ndb_cluster_connection.hpp"

#define CHECK(x)                                                     \
  if (!(x)) {                                                        \
    fprintf(stderr, "CHECK(" #x ") failed at line: %d\n", __LINE__); \
    return NDBT_FAILED;                                              \
  }

static const char *exe_valgrind = nullptr;
static const char *arg_valgrind = nullptr;

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

  Mgmd(const Mgmd &other) = delete;

 public:
  Mgmd(int nodeid) : m_nodeid(nodeid) {
    m_name.assfmt("ndb_mgmd_%d", nodeid);

    NDBT_find_ndb_mgmd(m_exe);
  }

  ~Mgmd() {
    if (m_proc) {
      stop();
    }
  }

  const char *name(void) const { return m_name.c_str(); }

  const char *exe(void) const { return m_exe.c_str(); }

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
    args.add("--ndb-nodeid=", m_nodeid);
    args.add("--nodaemon");
    args.add("--log-name=", name());
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

  template <class F>
  void list_trusted_certs(F func) {
    ndb_mgm_cert_table *list;
    int n = ndb_mgm_list_trusted_certs(handle(), &list);
    func(n, list);
    ndb_mgm_cert_table_free(&list);
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

  void set_connect_string(const BaseString &connect_string) {
    m_args.add("-c");
    m_args.add(connect_string.c_str());
  }

  bool start(const char *working_dir, const BaseString &connect_string) {
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

static struct Ndb_Sign_Keys {
  BaseString exe;
  int return_value{-1};

  bool run(const char *name, const NDBT_Workingdir &wd,
           const NdbProcess::Args &args) {
    if (exe.empty()) NDBT_find_sign_keys(exe);
    auto proc = NdbProcess::create(name, exe, wd.path(), args);
    bool r = proc->wait(return_value, 5000);
    if (!r) proc->stop();
    return (r && (return_value == 0));
  }
} sign_keys;

static bool create_CA(const NDBT_Workingdir &wd,
                      const char *cert_file = ClusterCertAuthority::CertFile,
                      const char *key_file = ClusterCertAuthority::KeyFile,
                      const char *ca_ordinal = "First") {
  NdbProcess::Args args;

  args.add("--passphrase=", "Trondheim");
  args.add("--create-CA");
  args.add("--CA-search-path=", wd.path());
  args.add("--CA-cert=", cert_file);
  args.add("--CA-key=", key_file);
  args.add("--CA-ordinal=", ca_ordinal);
  return sign_keys.run("Create CA", wd, args);
}

static X509 *get_CA(const NDBT_Workingdir &wd,
                    const char *name = ClusterCertAuthority::CertFile) {
  TlsSearchPath searchPath(wd.path());
  PkiFile::PathName ca_file;
  return (searchPath.find(name, ca_file)) ? Certificate::open_one(ca_file)
                                          : nullptr;
}

static EVP_PKEY *get_CA_key(const NDBT_Workingdir &wd,
                            const char *name = ClusterCertAuthority::KeyFile) {
  char password[16];
  snprintf(password, 16, "%s", "Trondheim");
  TlsSearchPath searchPath(wd.path());
  PkiFile::PathName key_file;
  return (searchPath.find(name, key_file))
             ? PrivateKey::open(key_file, password)
             : nullptr;
}

static bool sign(const NDBT_Workingdir &wd, const char *configFile,
                 const NdbProcess::Args *more = nullptr) {
  NdbProcess::Args args;
  if (configFile)
    args.add("--config-file=", configFile);
  else {
    args.add("-l");
    args.add("--bind-host=", 0);
  }
  args.add("--passphrase=", "Trondheim");
  args.add("--ndb-tls-search-path=", wd.path());
  if (more) args.add(*more);
  return sign_keys.run("Sign Keys", wd, args);
}

static bool create_and_sign_tls_keys(const NDBT_Workingdir &wd) {
  NdbProcess::Args args;
  args.add("--create-key");
  BaseString cfg_path = path(wd.path(), "config.ini", nullptr);
  return create_CA(wd) && sign(wd, cfg_path.c_str(), &args);
}

static bool manage_trust(const NDBT_Workingdir &wd, const char *verb,
                         const char *arg = nullptr) {
  NdbProcess::Args args;
  args.add("--ndb-tls-search-path=", wd.path());
  args.add("--trust=", verb);
  if (arg) args.add(arg);
  return sign_keys.run("manage_trust", wd, args);
}

/* Create a certificate for node_type that will expire after cert_duration
 */
static bool create_expiring_cert(const NDBT_Workingdir &wd,
                                 const BaseString node_type,
                                 const BaseString cert_duration) {
  NdbProcess::Args args;
  args.add("--create-key");
  args.add("--node-type=", node_type.c_str());
  args.add("--duration=", cert_duration.c_str());
  return sign(wd, nullptr, &args);
}

/* Create an expired certificate for a data node.
   Use a negative duration to create a cert that has already expired
*/
inline bool create_expired_cert(const NDBT_Workingdir &wd) {
  return create_CA(wd) && create_expiring_cert(wd, "db", "-50000");
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

class TlsCluster {
 public:
  static Properties configure(int nMgmd, int nDataNodes,
                              unsigned short require = 0xFF) {
    Properties config = ConfigFactory::create(nMgmd, nDataNodes);
    for (int i = 0; i < nMgmd + nDataNodes; i++)
      if (require & (1 << i))
        ConfigFactory::put(config, i < nMgmd ? "ndbd" : "ndb_mgmd", 1 + i,
                           "RequireTls", "true");
    return config;
  }

  NDBT_Workingdir wd;
  BaseString configFile;
  const Properties *config{nullptr};

  TlsCluster(const char *testName) : wd(testName) {
    configFile = path(wd.path(), "config.ini", nullptr);
  }

  TlsCluster(const NDBT_Workingdir &d) : wd(d) {
    configFile = path(wd.path(), "config.ini", nullptr);
  }

  bool write_config_ini(const Properties &cf) {
    config = &cf;
    return ConfigFactory::write_config_ini(cf, configFile.c_str());
  }

  bool mgmd_start(Mgmd &mgmd) {
    NdbProcess::Args args;
    mgmd.common_args(args, wd.path());
    return mgmd_start(mgmd, args);
  }

  bool mgmd_start(Mgmd &mgmd, NdbProcess::Args args) {
    args.add("--ndb-tls-search-path=", wd.path());
    return mgmd.start(wd.path(), args);
  }

  ndb_mgm_handle *connect_mgmd_client_tls(Mgmd &mgmd) {
    TlsKeyManager keyManager;
    assert(config);
    keyManager.init_mgm_client(wd.path());
    if (mgmd.connect(*config, 5, 2) &&
        (mgmd.client_start_tls(keyManager.ctx()) == 0))
      return mgmd.handle();
    return nullptr;
  }

  void configure_tls_search_path(Ndbd &ndb) {
    ndb.args().add("--ndb-tls-search-path=", wd.path());
  }

  bool ndbd_start(Ndbd &ndbd, Mgmd &mgmd) {
    return ndbd.start(wd.path(), mgmd.connectstring(*config));
  }
};

int runTestSshKeySigning(NDBT_Context *ctx, NDBT_Step *step) {
  /* Skip this test where "ssh localhost" can not be run without user
   * interaction. */
  {
    NdbProcess::Args args;
    auto exe = "ssh";
    args.add("-q");
    args.add("-oBatchMode=yes");
    args.add("localhost");
    args.add("exit");
    auto proc = NdbProcess::create(
        "Probe if `ssh localhost` need user interaction", exe, nullptr, args);
    int ret;
    bool r = proc->wait(ret, 1000);
    if (!r) proc->stop();
    if (r && ret == 255) {
      printf(
          "Skipping test SshKeySigning since `ssh localhost` may need user "
          "interaction.\n");
      return NDBT_SKIPPED;
    }
  }

  NDBT_Workingdir wd("test_mgmd");  // temporary working directory
  Properties config = ConfigFactory::create();
  ConfigFactory::put(config, "ndb_mgmd", 1, "RequireCertificate", "true");
  BaseString cfg_path = path(wd.path(), "config.ini", nullptr);
  CHECK(ConfigFactory::write_config_ini(config, cfg_path.c_str()));

  /* Create CA */
  if (!create_CA(wd)) return false;

  /* Create keys and certificates for all nodes, via ssh to localhost */
  /* There will be a parent ndb_sign_keys process plus 3 ssh invocations */
  NdbProcess::Args args;
  {
    args.add("--remote-exec-path=", sign_keys.exe.c_str());
    args.add("--remote-CA-host=", "localhost");
    args.add("--create-key");
    bool r = sign(wd, cfg_path.c_str(), &args);
    CHECK(r);
    CHECK(sign_keys.return_value == 0);
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
    args.add("--remote-openssl");
    args.add("--remote-CA-host=", "localhost");
    args.add("--CA-cert=", ca_cert.c_str());
    args.add("--CA-key=", ca_key.c_str());
    args.add("--create-key");
    bool r = sign(wd, cfg_path.c_str(), &args);
    CHECK(r);
    CHECK(sign_keys.return_value == 0);
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

  /* Create CA */
  if (!create_CA(wd)) return false;

  /* Create key and certificate for node 2 */
  NdbProcess::Args args;
  args.add("-n", 2);
  args.add("--create-key");
  args.add("--CA-tool=", sign_keys.exe.c_str());
  bool r = sign(wd, cfg_path.c_str(), &args);
  CHECK(r);
  CHECK(sign_keys.return_value == 0);
  return NDBT_OK;
}

int runTestApiWithoutCert(NDBT_Context *ctx, NDBT_Step *step) {
  NDBT_Workingdir wd("test_tls");  // temporary working directory

  BaseString cfg_path = path(wd.path(), "config.ini", nullptr);
  Properties config = ConfigFactory::create();
  CHECK(ConfigFactory::put(config, "ndbd", 2, "RequireTls", "true"));
  CHECK(ConfigFactory::write_config_ini(config, cfg_path.c_str()));

  CHECK(create_and_sign_tls_keys(wd));

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
  Properties config = TlsCluster::configure(1, 1, 01);
  Properties db;
  db.put("RequireCertificate", "true");
  config.put("DB Default", &db);

  TlsCluster cluster("testNdbdWithCert");
  Mgmd mgmd(1);
  Ndbd ndbd(2);

  CHECK(cluster.write_config_ini(config));
  CHECK(create_and_sign_tls_keys(cluster.wd));

  CHECK(cluster.mgmd_start(mgmd));  // Start management node
  NdbMgmHandle handle = cluster.connect_mgmd_client_tls(mgmd);
  CHECK(handle);
  CHECK(mgmd.wait_confirmed_config());  // Wait for configuration

  ndbd.args().add("--ndb-tls-search-path=", cluster.wd.path());
  ndbd.args().add("--ndb-mgm-tls=strict");
  CHECK(cluster.ndbd_start(ndbd, mgmd));
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

  create_and_sign_tls_keys(wd);

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
  free(state);

  /* Now convert the socket to a transporter */
  NdbSocket s = mgmd.convert_to_transporter();
  CHECK(s.is_valid());
  CHECK(s.close() == 0);

  return NDBT_OK;
}

/* Test the TLS INFO statistics after the TLS auth has failed due to an
   expired server certificate
*/
int runTestTlsStats1(NDBT_Context *ctx, NDBT_Step *step) {
  ndb_mgm_tls_stats stats[3];
  auto print_stats = [](const ndb_mgm_tls_stats &stats) {
    printf("TLS Stats -- accepted:%d upgraded:%d current:%d tls:%d\n",
           stats.accepted, stats.upgraded, stats.current, stats.tls);
  };
  NDBT_Workingdir wd("test_tls");  // temporary working directory

  /* Create a configuration */
  BaseString cfg_path = path(wd.path(), "config.ini", nullptr);
  Properties config = ConfigFactory::create();
  CHECK(ConfigFactory::write_config_ini(config, cfg_path.c_str()));

  /* Create certificates that will expire soon */
  CHECK(create_CA(wd));
  CHECK(create_expiring_cert(wd, "mgmd", "8"));  // expires in 8 seconds
  CHECK(create_expiring_cert(wd, "api", "120"));

  /* MGM server */
  Mgmd mgmd(1);
  NdbProcess::Args mgmdArgs;
  mgmd.common_args(mgmdArgs, wd.path());
  mgmdArgs.add("--ndb-tls-search-path=", wd.path());
  CHECK(mgmd.start(wd.path(), mgmdArgs));  // Start management node
  CHECK(mgmd.connect(config));             // Connect to management node
  CHECK(mgmd.wait_confirmed_config());     // Wait for configuration

  /* Get stats */
  ndb_mgm_get_tls_stats(mgmd.handle(), &stats[0]);
  print_stats(stats[0]);
  CHECK(ndb_mgm_has_tls(mgmd.handle()) == 0);  // Our handle does not use TLS,
  CHECK(stats[0].current > stats[0].tls);  // so current connections > TLS conns

  /* Now create a second client. It will use TLS */
  NdbMgmd client;
  client.use_tls(wd.path(), CLIENT_TLS_STRICT);
  CHECK(client.connect(mgmd.connectstring(config).c_str(), 1, 0));

  /* Get stats */
  ndb_mgm_get_tls_stats(mgmd.handle(), &stats[1]);
  print_stats(stats[1]);
  CHECK(stats[1].accepted > stats[0].accepted);
  CHECK(stats[1].upgraded > stats[0].upgraded);
  CHECK(stats[1].current > stats[0].current);
  CHECK(stats[1].tls > stats[0].tls);

  /* Wait for the MGMD cert to expire */
  client.disconnect();
  printf("Waiting 9 seconds for mgmd server certificate to expire.\n");
  sleep(9);

  /* Now a client will try to start TLS, and fail. */
  client.connect(mgmd.connectstring(config).c_str(), 1, 0);
  CHECK(client.last_error() == NDB_MGM_TLS_HANDSHAKE_FAILED);
  CHECK(client.is_connected() == false);
  client.close();

  /* The MGM server's TLS stats should reflect the failed attempt */
  ndb_mgm_get_tls_stats(mgmd.handle(), &stats[2]);
  print_stats(stats[2]);
  CHECK(stats[2].accepted > stats[1].accepted);
  CHECK(stats[2].upgraded == stats[1].upgraded);
  CHECK(stats[2].tls == stats[0].tls);
  CHECK(stats[2].current == stats[0].current);

  return NDBT_OK;
}

/* Test the TLS INFO statistics after the TLS auth has failed due to an
   expired client certificate
*/
int runTestTlsStats2(NDBT_Context *ctx, NDBT_Step *step) {
  ndb_mgm_tls_stats stats[2];
  auto print_stats = [](const ndb_mgm_tls_stats &stats) {
    printf("TLS Stats -- accepted:%d upgraded:%d current:%d tls:%d\n",
           stats.accepted, stats.upgraded, stats.current, stats.tls);
  };
  NDBT_Workingdir wd("test_tls");  // temporary working directory
  BaseString exe;
  NDBT_find_sign_keys(exe);

  /* Create a configuration */
  BaseString cfg_path = path(wd.path(), "config.ini", nullptr);
  Properties config = ConfigFactory::create();
  CHECK(ConfigFactory::write_config_ini(config, cfg_path.c_str()));

  /* Create certificates that will expire soon */
  CHECK(create_CA(wd));
  CHECK(create_expiring_cert(wd, "mgmd", "120"));
  CHECK(create_expiring_cert(wd, "api", "5"));  // expires in 5 seconds

  /* MGM server */
  Mgmd mgmd(1);
  NdbProcess::Args mgmdArgs;
  mgmd.common_args(mgmdArgs, wd.path());
  mgmdArgs.add("--ndb-tls-search-path=", wd.path());
  CHECK(mgmd.start(wd.path(), mgmdArgs));  // Start management node
  CHECK(mgmd.connect(config));             // Connect to management node
  CHECK(mgmd.wait_confirmed_config());     // Wait for configuration

  /* Get stats */
  ndb_mgm_get_tls_stats(mgmd.handle(), &stats[0]);
  print_stats(stats[0]);

  /* Create a client. Connect, but don't start TLS. */
  NdbMgmd client;
  TlsKeyManager tlsKeyManager;
  tlsKeyManager.init_mgm_client(wd.path());
  CHECK(client.connect(mgmd.connectstring(config).c_str(), 1, 0, false));
  CHECK(ndb_mgm_has_tls(client.handle()) == 0);

  /* Wait for the client cert to expire, then try to start TLS.
     The server's cert is valid, so the client will see auth as successful,
     but then it will fail on the next MGM call.
  */
  printf("Waiting 6 seconds for mgm client certificate to expire.\n");
  sleep(6);
  CHECK(client.start_tls(tlsKeyManager.ctx()) == 0);  // returns 0 on success
  CHECK(ndb_mgm_check_connection(client.handle()) == -1);

  /* Get stats */
  ndb_mgm_get_tls_stats(mgmd.handle(), &stats[1]);
  print_stats(stats[1]);
  CHECK(stats[1].accepted > stats[0].accepted);
  CHECK(stats[1].upgraded == stats[0].upgraded);
  CHECK(stats[1].tls == stats[0].tls);
  CHECK(stats[1].current == stats[0].current);

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
  bool k = create_and_sign_tls_keys(wd);
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

  ndb_mgm_destroy_handle(&handle);
  return NDBT_OK;
}

int runTestTrustedAuth(NDBT_Context *ctx, NDBT_Step *step) {
  /* Check that I can connect as an MGM client using a cert that was signed
     by a trusted CA. */
  TlsCluster cluster("TestTrustedAuth");
  Properties config = TlsCluster::configure(1, 2);
  CHECK(cluster.write_config_ini(config));

  /* Create a set of keys and certs */
  CHECK(create_and_sign_tls_keys(cluster.wd));

  /* Create a trusted CA. Add both CAs to the trust store */
  CHECK(create_CA(cluster.wd, "Trusted-CA-cert", "Trusted-CA-key", "Next"));
  CHECK(manage_trust(cluster.wd, "add", "NDB-Cluster-cert"));
  CHECK(manage_trust(cluster.wd, "add", "Trusted-CA-cert"));

  /* Create a client cert signed by the trusted CA */
  {
    NdbProcess::Args args;
    args.add("--CA-cert=", "Trusted-CA-cert");
    args.add("--CA-key=", "Trusted-CA-key");
    args.add("--node-type=", "api");
    CHECK(sign(cluster.wd, cluster.configFile.c_str(), &args));
  }

  /* List the trust store */
  {
    TlsKeyManager keyManager;
    keyManager.init_mgm_client(cluster.wd.path());
    TlsKeyManager::CA_Table trust_table(keyManager);
    for (cert_table_entry &cert : trust_table) {
      printf("Serial: %s  Flags: %d \n", cert.serial, cert.flags);
    }
  }

  /* Start the MGM server */
  Mgmd mgmd(1);
  CHECK(cluster.mgmd_start(mgmd));

  /* The client connection will use the new api cert */
  NdbMgmHandle handle = cluster.connect_mgmd_client_tls(mgmd);
  CHECK(handle);
  CHECK(mgmd.wait_confirmed_config());  // Wait for configuration

  mgmd.list_trusted_certs([](int n, ndb_mgm_cert_table *list) {
    printf("n: %d \n", n);
    while (list) {
      printf("%s, %s, use count: %d \n", list->cert_name, list->cert_serial,
             list->use_count);
      require(list->use_count > 0);
      list = list->next;
    }
  });

  return NDBT_OK;
}

int runMiscTrustTests(NDBT_Context *ctx, NDBT_Step *step) {
  EVP_PKEY *ca2_key = PrivateKey::create("P-256");
  CertLifetime lifetime(1);
  X509 *ca2 = ClusterCertAuthority::create(ca2_key, lifetime, "Second");

  {
    /* TESTS THAT USE A SINGLE DIRECTORY AND A NORMAL CHAINED ENTITY CERT */

    TlsCluster cluster("miscTests1");
    Properties config = TlsCluster::configure(1, 2, 0x00);
    CHECK(cluster.write_config_ini(config));

    {
      /* Zero Trusted Certs */
      TlsKeyManager km;
      TlsKeyManager::CA_Table trusted(km);
      km.init_mgm_client(cluster.wd.path());
      CHECK(trusted.end() == 0);
    }

    create_and_sign_tls_keys(cluster.wd);

    {
      /* Chained CA cert, but no NDB-trusted-certs file ==> 1 trusted cert */
      TlsKeyManager km;
      TlsKeyManager::CA_Table trusted(km);
      km.init_mgm_client(cluster.wd.path());
      CHECK(trusted.end() == 1);
    }

    {
      /* NDB-trusted-certs is empty ==> error message, TLS is not usable */
      FILE *fp = TrustStore::open(cluster.wd.path(), "w");
      CHECK(fp);
      TrustStore::close(fp);

      TlsKeyManager km;
      fprintf(stderr, " >> Empty trust file << \n");
      km.init_mgm_client(cluster.wd.path());
      CHECK(!km.ctx());
    }

    {
      /* NDB-trusted-certs is garbage ==> error message */
      FILE *fp = TrustStore::open(cluster.wd.path(), "w");
      fprintf(fp, "-----BEGIN TRUSTED CERTIFICATE-----\n");
      fprintf(fp, "MIIC8TCCAdmgAwIBAgIKdl13IKXqjNtDfzANBI0000000\n");
      fprintf(fp, "-----END TRUSTED CERTIFICATE-----\n");
      TrustStore::close(fp);

      TlsKeyManager km;
      fprintf(stderr, " >> 1st certificate in trust file is corrupted << \n");
      km.init_mgm_client(cluster.wd.path());
      CHECK(!km.ctx());
    }

    {
      /* One good cert and one truncated cert in NDB-trusted-certs ==> error */
      FILE *fp = TrustStore::open(cluster.wd.path(), "w");
      TrustStore::write(fp, ca2);
      fprintf(fp, "-----BEGIN TRUSTED CERTIFICATE-----\n");
      fprintf(fp, "MIIC8TCCAdmgAwIBAgIKdl13IKXqjNtDfzANBI0000000\n");
      TrustStore::close(fp);

      TlsKeyManager km;
      fprintf(stderr, " >> 2nd certificate in trust file is corrupted << \n");
      km.init_mgm_client(cluster.wd.path());
      CHECK(!km.ctx());
    }

    {
      /* Trusted cert is a duplicate of chained cert ==> 1 trusted cert */
      X509 *ca = get_CA(cluster.wd);
      CHECK(ca);
      FILE *fp = TrustStore::open(cluster.wd.path(), "w");
      TrustStore::write(fp, ca);
      TrustStore::close(fp);

      TlsKeyManager km;
      TlsKeyManager::CA_Table trusted(km);
      km.init_mgm_client(cluster.wd.path());
      CHECK(trusted.end() == 1);
      CHECK(km.ctx());
      for (cert_table_entry &cert : trusted) {
        CHECK(cert.flags == 7);  // IN_CHAIN + IN_TRUST_STORE + IS_ROOT
      }
      Certificate::free(ca);
    }

    fprintf(stderr, " === End of first test group === \n");
  }

  {
    /* TESTS THAT USE A BARE NODECERTIFICATE (NO CHAINED CA) */
    TlsCluster cluster("miscTests2");
    Properties config = TlsCluster::configure(1, 2, 0x00);
    TlsSearchPath searchPath(cluster.wd.path());

    CHECK(cluster.write_config_ini(config));
    CHECK(create_CA(cluster.wd));

    /* Open the CA key and certificate */
    X509 *CA_cert = get_CA(cluster.wd);
    EVP_PKEY *CA_key = get_CA_key(cluster.wd);
    CHECK(CA_cert);
    CHECK(CA_key);

    /* Create a bare node certifcate, sign it, and promote it to active */
    {
      PkiFile::PathName node_cert_file, node_key_file;
      NodeCertificate nc(Node::Type::Client, 1);
      nc.create_keys("P-256");
      CHECK(nc.finalise(CA_cert, CA_key, false) == 0);
      CHECK(PendingPrivateKey::store(nc.key(), cluster.wd.path(), nc));
      CHECK(PendingCertificate::store(&nc, cluster.wd.path()));
      CHECK(PendingCertificate::find(&searchPath, 1, Node::Type::Client,
                                     node_cert_file));
      CHECK(PendingCertificate::promote(node_cert_file));
      CHECK(PendingPrivateKey::find(&searchPath, 1, Node::Type::Client,
                                    node_key_file));
      CHECK(PendingPrivateKey::promote(node_key_file));
    }

    {
      /* NDB-trusted-certs does not exist ==> observable zero trusted certs */
      TlsKeyManager km;
      TlsKeyManager::CA_Table trusted(km);
      km.init_mgm_client(cluster.wd.path());
      fprintf(stderr, " >> Zero trusted certs << \n");
      CHECK(trusted.end() == 0);
    }

    {
      /* NDB-trusted-certs contains an X509, *not* an X509_AUX
          ==> no error, context is valid */
      Certificate::store(CA_cert, cluster.wd.path(), "NDB-trusted-certs");
      TlsKeyManager km;
      fprintf(stderr, " >> NDB-trusted-certs contains an ordinary X509 << \n");
      km.init_mgm_client(cluster.wd.path());
      TlsKeyManager::CA_Table trusted(km);
      CHECK(trusted.end() == 1);
      CHECK(km.ctx());
    }

    {
      /* One cert in NDB-trusted-certs ==> observable one trusted cert */
      /* A node has a bare entity certificate, no chained CA cert, and one
         trusted CA. The entity cert is signed by the trusted CA.
          ==> TLS context is valid
      */
      FILE *fp = TrustStore::open(cluster.wd.path(), "w");
      CHECK(TrustStore::write(fp, CA_cert));
      TrustStore::close(fp);

      TlsKeyManager km;
      TlsKeyManager::CA_Table trusted(km);
      km.init_mgm_client(cluster.wd.path());
      CHECK(km.ctx());
      CHECK(trusted.end() == 1);
    }

    {
      /* Two certs in NDB-trusted-certs ==> observable two trusted certs */
      FILE *fp = TrustStore::open(cluster.wd.path(), "a");
      CHECK(TrustStore::write(fp, ca2));
      TrustStore::close(fp);

      TlsKeyManager km;
      TlsKeyManager::CA_Table trusted(km);
      km.init_mgm_client(cluster.wd.path());
      CHECK(km.ctx());
      CHECK(trusted.end() == 2);
    }

    {
      /* A node has a bare entity certificate, no chained CA cert, and one
         trusted CA. The entity cert is _not_ signed by the trusted CA.
          ===> TLS context is _not_ valid
      */
      FILE *fp = TrustStore::open(cluster.wd.path(), "w");
      CHECK(TrustStore::write(fp, ca2));
      TrustStore::close(fp);

      TlsKeyManager km;
      TlsKeyManager::CA_Table trusted(km);
      fprintf(stderr, " >> Entity cert not signed by trusted CA << \n");
      km.init_mgm_client(cluster.wd.path());
      CHECK(trusted.end() == 1);
      CHECK(km.ctx() == nullptr);
    }
    Certificate::free(CA_cert);
    PrivateKey::free(CA_key);
    fprintf(stderr, " === End of second test group === \n");
  }

  {
    /* TESTS THAT USE TWO DIRECTORIES IN SEARCH PATH */
    NDBT_Workingdir wd1("miscTests3-wd1-");
    NDBT_Workingdir wd2("miscTests3-wd2-");
    BaseString pathString(wd1.path());
    pathString.append(TlsSearchPath::Separator);
    pathString.append(wd2.path());
    TlsSearchPath searchPath(pathString.c_str());
    CHECK(searchPath.size() == 2);

    TlsCluster cluster(wd1);
    Properties config = TlsCluster::configure(1, 2, 0x00);
    CHECK(cluster.write_config_ini(config));
    CHECK(create_and_sign_tls_keys(wd1));

    {
      /* Basic test */
      TlsKeyManager km;
      TlsKeyManager::CA_Table trusted(km);
      km.init_mgm_client(pathString.c_str());
      CHECK(km.ctx());
      CHECK(trusted.end() == 1);
    }

    {
      /* Chained cert plus one trusted cert in wd1 ==> 2 trusted certs */
      FILE *fp = TrustStore::open(wd1.path(), "w");
      CHECK(TrustStore::write(fp, ca2));
      TrustStore::close(fp);

      TlsKeyManager km;
      TlsKeyManager::CA_Table trusted(km);
      km.init_mgm_client(pathString.c_str());
      CHECK(km.ctx());
      CHECK(trusted.end() == 2);
    }

    {
      /* Two NDB-trusted-certs files; one valid + one with error ==> error */
      FILE *fp = TrustStore::open(wd2.path(), "w");
      fprintf(fp, "-----BEGIN TRUSTED CERTIFICATE-----\n");
      fprintf(fp, "MIIC8TCCAdmgAwIBAgIKdl13IKXqjNtDfzANBI0000000\n");
      TrustStore::close(fp);

      fprintf(stderr, " >> Two trust files, one with an error << \n");
      TlsKeyManager km;
      TlsKeyManager::CA_Table trusted(km);
      km.init_mgm_client(pathString.c_str());
      CHECK(!km.ctx());
    }

    {
      /* Chained cert plus trusted certs in wd1 and wd2 ==> 3 trusted certs  */
      EVP_PKEY *ca3_key = PrivateKey::create("P-256");
      X509 *ca3 = ClusterCertAuthority::create(ca3_key, lifetime, "Third");
      FILE *fp = TrustStore::open(wd2.path(), "w");
      CHECK(TrustStore::write(fp, ca3));
      TrustStore::close(fp);

      TlsKeyManager km;
      TlsKeyManager::CA_Table trusted(km);
      km.init_mgm_client(pathString.c_str());
      CHECK(km.ctx());
      CHECK(trusted.end() == 3);
      Certificate::free(ca3);
      PrivateKey::free(ca3_key);
    }

    fprintf(stderr, " === End of third test group === \n");
  }

  Certificate::free(ca2);
  PrivateKey::free(ca2_key);

  return NDBT_OK;
}

/* Test a full CA rotation lifecycle.
   The test requires three rolling restarts.
   The test cluster consists of just two management servers.

     - Start a cluster with TLS required
     - Create a new CA.
     - Create an on-disk trust store containing both the new and old CAs
     - Use the new CA to create a set of pending node certificates
     - Perform a rolling restart.
     - Confirm that the trust store contains old CA and new CA.
     - Promote the pending node certificates to make them active.
     - Perform a second rolling restart.
     - Confirm that the new CA is being used, and the old CA is not.
     - Remove the old CA from the trust store.
     - Perform a final rolling restart.
     - Confirm that the trust store now contains just the new CA.
*/
int runTestCaRotation(NDBT_Context *ctx, NDBT_Step *step) {
  TlsCluster cluster("runTestCaRotation");
  Properties config = TlsCluster::configure(2, 1);
  CHECK(cluster.write_config_ini(config));

  /* Create a set of keys and certs */
  CHECK(create_and_sign_tls_keys(cluster.wd));

  Mgmd mgm1(1);
  Mgmd mgm2(2);

  CHECK(cluster.mgmd_start(mgm1));
  CHECK(cluster.mgmd_start(mgm2));

  CHECK(cluster.connect_mgmd_client_tls(mgm1));
  CHECK(cluster.connect_mgmd_client_tls(mgm2));

  CHECK(mgm1.wait_confirmed_config());
  CHECK(mgm2.wait_confirmed_config());

  /* Create a second CA. Add both CAs to the on-disk trust store. */
  CHECK(create_CA(cluster.wd, "New-CA-cert", "New-CA-key", "Second"));
  CHECK(manage_trust(cluster.wd, "add", "NDB-Cluster-cert"));
  CHECK(manage_trust(cluster.wd, "add", "New-CA-cert"));

  /* Use the new CA to generate a new set of pending node certificates. */
  {
    NdbProcess::Args args;
    args.add("--CA-cert=", "New-CA-cert");
    args.add("--CA-key=", "New-CA-key");
    args.add("--pending");
    CHECK(sign(cluster.wd, cluster.configFile.c_str(), &args));
  }

  /* Restart one node */
  CHECK(mgm1.stop());
  CHECK(cluster.mgmd_start(mgm1));
  CHECK(cluster.connect_mgmd_client_tls(mgm1));
  CHECK(mgm1.wait_confirmed_config());

  BaseString oldCaSerial;
  int ncerts = 0;
  int use_counts[2];  // 0 = old CA ; 1 = new CA

  auto print_1 = [&](ndb_mgm_cert_table *c) {
    printf("Cert: %s, %s, flags: %d count: %d\n", c->cert_name, c->cert_serial,
           c->flags, c->use_count);
  };

  /* List certs, and note the serial number of the old CA */
  mgm1.list_trusted_certs([&](int n, ndb_mgm_cert_table *cert) {
    ncerts = n;
    while (cert) {
      print_1(cert);
      if (cert->use_count > 0) oldCaSerial.assign(cert->cert_serial);
      cert = cert->next;
    }
  });
  CHECK(ncerts == 2);

  auto check_certs = [&](int n, ndb_mgm_cert_table *cert) {
    ncerts = n;
    use_counts[0] = use_counts[1] = 0;
    while (cert) {
      if (oldCaSerial == cert->cert_serial)
        use_counts[0] = cert->use_count;
      else
        use_counts[1] = cert->use_count;
      print_1(cert);
      cert = cert->next;
    }
  };

  /* Restart the other node */
  CHECK(mgm2.stop());
  CHECK(cluster.mgmd_start(mgm2));
  CHECK(cluster.connect_mgmd_client_tls(mgm2));
  CHECK(mgm2.wait_confirmed_config());
  mgm2.list_trusted_certs(check_certs);
  CHECK(ncerts == 2);
  CHECK(use_counts[1] == 0);  // the new CA has not been used

  /* Promote the pending certificates to active */
  {
    NdbProcess::Args args;
    args.add("--ndb-tls-search-path=", cluster.wd.path());
    args.add("--config-file=", cluster.configFile.c_str());
    args.add("--promote");
    CHECK(sign_keys.run("Promote", cluster.wd, args));
  }

  /* Restart one node */
  CHECK(mgm1.stop());
  CHECK(cluster.mgmd_start(mgm1));
  CHECK(cluster.connect_mgmd_client_tls(mgm1));
  CHECK(mgm1.wait_confirmed_config());
  mgm1.list_trusted_certs(check_certs);
  CHECK(ncerts == 2);
  CHECK(use_counts[1] > 0);  // the new CA has been used

  /* Restart the other node */
  CHECK(mgm2.stop());
  CHECK(cluster.mgmd_start(mgm2));
  CHECK(cluster.connect_mgmd_client_tls(mgm2));
  CHECK(mgm2.wait_confirmed_config());
  mgm2.list_trusted_certs(check_certs);
  CHECK(ncerts == 2);
  CHECK(use_counts[0] == 0);  // the old CA has not been used

  /* Remove the old CA from the trust store */
  CHECK(manage_trust(cluster.wd, "remove", oldCaSerial.c_str()));

  /* Restart one node */
  CHECK(mgm1.stop());
  CHECK(cluster.mgmd_start(mgm1));
  CHECK(cluster.connect_mgmd_client_tls(mgm1));
  CHECK(mgm1.wait_confirmed_config());
  mgm1.list_trusted_certs(check_certs);
  CHECK(ncerts == 1);  // now we only know about the new CA
  CHECK(use_counts[1] > 0);

  /* Restart the other node */
  CHECK(mgm2.stop());
  CHECK(cluster.mgmd_start(mgm2));
  CHECK(cluster.connect_mgmd_client_tls(mgm2));
  CHECK(mgm2.wait_confirmed_config());
  mgm2.list_trusted_certs(check_certs);
  CHECK(ncerts == 1);  // now we only know about the new CA

  return NDBT_OK;
}

/* Test ndbinfo.trusted_certs */
int runTestNdbinfoTrustedCerts(NDBT_Context *ctx, NDBT_Step *step) {
  TlsCluster cluster("runTestNdbinfoTrustedCerts");
  Properties config = TlsCluster::configure(1, 2);
  CHECK(cluster.write_config_ini(config));

  /* Create a set of keys and certs */
  CHECK(create_and_sign_tls_keys(cluster.wd));

  /* Start the MGM server */
  Mgmd mgmd(1);
  CHECK(cluster.mgmd_start(mgmd));
  NdbMgmHandle handle = cluster.connect_mgmd_client_tls(mgmd);
  CHECK(mgmd.wait_confirmed_config());

  /* Start data node 2, but don't wait for it */
  Ndbd ndbd2(2);
  cluster.configure_tls_search_path(ndbd2);
  CHECK(cluster.ndbd_start(ndbd2, mgmd));
  ndbd2.wait_started(handle, 5);  // not checked

  /* Now add a cert to the trust store. Data node 2 has not seen this
     trusted cert, but node 3 will see it. */
  CHECK(create_CA(cluster.wd, "Trusted-CA-cert", "Trusted-CA-key", "Next"));
  CHECK(manage_trust(cluster.wd, "add", "Trusted-CA-cert"));

  /* Start data node 3 and wait for it */
  Ndbd ndbd3(3);
  cluster.configure_tls_search_path(ndbd3);
  CHECK(cluster.ndbd_start(ndbd3, mgmd));
  CHECK(ndbd3.wait_started(handle));

  /* Open an API connection */
  Ndb_cluster_connection apiConnection(mgmd.connectstring(config).c_str());
  apiConnection.configure_tls(cluster.wd.path(), 0);
  apiConnection.connect(5, 2, 0);
  apiConnection.wait_until_ready(5, 5);

  /* Scan ndbinfo.trusted_certs */
  NdbInfo ndbinfo(&apiConnection, "ndbinfo/");
  CHECK(ndbinfo.init());

  const NdbInfo::Table *table = nullptr;
  CHECK(ndbinfo.openTable("ndbinfo/trusted_certs", &table) == 0);

  NdbInfoScanOperation *scanOp = nullptr;
  CHECK(ndbinfo.createScanOperation(table, &scanOp) == 0);
  CHECK(scanOp->readTuples() == 0);
  const NdbInfoRecAttr *nodeId = scanOp->getValue("node_id");
  CHECK(nodeId);

  int certs[4]{0, 0, 0, 0};
  CHECK(scanOp->execute() == 0);
  while (scanOp->nextResult() == 1) {
    int node_id = nodeId->u_32_value();
    printf("Next result %d \n", node_id);
    CHECK(node_id < 4);
    certs[node_id] += 1;
  }
  CHECK(certs[0] == 0);
  CHECK(certs[1] == 0);
  CHECK(certs[2] == 1); /* Data node 2 has 1 trusted cert */
  CHECK(certs[3] == 2); /* Data node 3 has 2 trusted certs */

  /* Cleanup */
  ndbinfo.releaseScanOperation(scanOp);
  ndbinfo.closeTable(table);
  return NDBT_OK;
}

NDBT_TESTSUITE(testTls);
DRIVER(DummyDriver); /* turn off use of NdbApi */

TESTCASE("NdbdWithoutCertificate",
         "Test data node startup with TLS required but no certificate") {
  INITIALIZER(runTestNdbdWithoutCert);
}

TESTCASE("ApiWithoutCertificate",
         "Test API node without certificate where TRP TLS is required") {
  INITIALIZER(runTestApiWithoutCert);
}

TESTCASE("NdbdWithExpiredCertificate",
         "Test data node startup with expired certificate") {
  INITIALIZER(runTestNdbdWithExpiredCert);
}

TESTCASE("NdbdWithCertificate", "Test data node startup with certificate") {
  INITIALIZER(runTestNdbdWithCert);
}

TESTCASE("StartTls", "Test START TLS in MGM protocol") {
  INITIALIZER(runTestStartTls);
}

TESTCASE("RequireTls", "Test MGM server that requires TLS") {
  INITIALIZER(runTestRequireTls);
}

TESTCASE("TlsStats1", "Test TLS INFO statistics after server cert expires") {
  INITIALIZER(runTestTlsStats1);
}

TESTCASE("TlsStats2", "Test TLS INFO statistics after client cert expires") {
  INITIALIZER(runTestTlsStats2);
}

TESTCASE("KeySigningTool", "Test key signing using a co-process tool") {
  INITIALIZER(runTestKeySigningTool);
}

TESTCASE("SshKeySigning",
         "Test remote key signing over ssh using ndb_sign_keys") {
  INITIALIZER(runTestSshKeySigning);
}

TESTCASE("TrustedAuth", "Test TLS auth relying on a trusted CA") {
  INITIALIZER(runTestTrustedAuth);
}

TESTCASE("TrustStore", "Various tests of Trust Store") {
  INITIALIZER(runMiscTrustTests);
}

TESTCASE("CaRotation", "Test CA rotation using trust store") {
  INITIALIZER(runTestCaRotation);
}

TESTCASE("NdbinfoTrustedCerts", "Test ndbinfo.trusted_certs") {
  INITIALIZER(runTestNdbinfoTrustedCerts);
}

NDBT_TESTSUITE_END(testTls)

int main(int argc, const char **argv) {
  ndb_init();
  NDBT_TESTSUITE_INSTANCE(testTls);
  testTls.setCreateTable(false);
  testTls.setRunAllTables(true);
  testTls.setConnectCluster(false);
  testTls.setEnsureIndexStatTables(false);
  testTls.setCheckErrorInsert(false);

#ifdef NDB_USE_GET_ENV
  char buf1[255], buf2[255];
  if (NdbEnv_GetEnv("NDB_MGMD_VALGRIND_EXE", buf1, sizeof(buf1))) {
    exe_valgrind = buf1;
  }

  if (NdbEnv_GetEnv("NDB_MGMD_VALGRIND_ARG", buf2, sizeof(buf2))) {
    arg_valgrind = buf2;
  }
#endif

  return testTls.execute(argc, argv);
}
