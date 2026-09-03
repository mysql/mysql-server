/*
   Copyright (c) 2022, 2026, Oracle and/or its affiliates.

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

#include <cstring>
#include <memory>
#include <string>
#include <unordered_set>

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <openssl/ssl.h>
#include <openssl/x509v3.h>

#include "typelib.h"

#include "ndb_global.h"
#include "ndb_opts.h"

#include "mgmapi.h"
#include "mgmcommon/Config.hpp"

#include "debugger/EventLogger.hpp"

#include "portlib/NdbDir.hpp"
#include "portlib/NdbProcess.hpp"
#include "portlib/NdbTCP.h"
#include "portlib/ndb_socket.h"
#include "portlib/ssl_applink.h"

#include "util/File.hpp"
#include "util/NdbOut.hpp"
#include "util/NodeCertificate.hpp"
#include "util/SocketServer.hpp"
#include "util/TlsKeyErrors.h"
#include "util/TlsKeyManager.hpp"
#include "util/ndb_openssl3_compat.h"

/* ndb_basename() is not declared in any header file: */
const char *ndb_basename(const char *path);

static constexpr size_t PASSPHRASE_BUFFER_SIZE = 1024;
static const char *default_groups[] = {"mysql_cluster", "ndb_sign_keys", 0};

const char *opt_ca_key = ClusterCertAuthority::KeyFile;
const char *opt_ca_cert = ClusterCertAuthority::CertFile;
const char *opt_ca_host = nullptr;
const char *opt_ca_search_path = nullptr;
const char *opt_ca_tool = nullptr;
const char *opt_ca_ordinal = "First";
const char *opt_ndb_config_file = nullptr;
const char *opt_bound_host = nullptr;
const char *opt_dest_dir = nullptr;
const char *opt_key_dest_dir = nullptr;
const char *opt_remote_path = nullptr;
const char *opt_curve = "P-256";
const char *opt_schedule = "120,10,130,10,150,0";
char *opt_cluster_key_pass = nullptr;
unsigned int opt_node_id = 0;
bool opt_create_ca = 0;
bool opt_create_key = 0;
bool opt_sign = 1;
bool opt_noconfig = 0;
bool opt_periodic = 0;
bool opt_pending = 0;
bool opt_promote = 0;
bool opt_rs_openssl = 0;
bool opt_stdio = 0;
int opt_replace_by = -10;
int opt_duration = 0;
int opt_ca_days = CertLifetime::CaDefaultDays;
unsigned long opt_trust = 0;
unsigned long long opt_bind_host = 0;
unsigned long long opt_node_types = 0;

enum {
  SIGN_LOCAL = 0,
  SIGN_SSH_SIGN_KEYS = 1,
  SIGN_SSH_OPENSSL = 2,
  SIGN_CO_PROCESS = 3
} signing_method = SIGN_LOCAL;

static inline bool signing_over_ssh() {
  switch (signing_method) {
    case SIGN_SSH_SIGN_KEYS:
    case SIGN_SSH_OPENSSL:
      return true;
    case SIGN_LOCAL:
    case SIGN_CO_PROCESS:
      return false;
  }
  return false;
}

short exp_schedule[6] = {0, 0, 0, 0, 0, 0};

const char *remote_ca_path = nullptr;

static const char *node_types[4] = {"mgmd", "db", "api", nullptr};
static struct TYPELIB node_types_lib = {3, "", node_types, nullptr};
static const char *trust_verbs[5] = {"(No default value)", "add", "remove",
                                     "show", nullptr};
static struct TYPELIB trust_verbs_lib = {4, "", trust_verbs, nullptr};

/* short option letters used: C K P T V X c f l n t ? */

static struct my_option sign_keys_options[] = {
    NdbStdOpt::usage,
    NdbStdOpt::help,
    NdbStdOpt::version,            // short form 'V'
    NdbStdOpt::ndb_connectstring,  // short form 'c'
    NdbStdOpt::connect_retries,
    NdbStdOpt::connect_retry_delay,
    NdbStdOpt::tls_search_path,
    NdbStdOpt::mgm_tls,
    {"config-file", 'f', "Read cluster configuration from file",
     &opt_ndb_config_file, nullptr, nullptr, GET_STR, REQUIRED_ARG, 0, 0, 0,
     nullptr, 0, nullptr},
    {"no-config", 'l',
     "Do not obtain cluster configuration; create a single certificate",
     &opt_noconfig, nullptr, nullptr, GET_BOOL, NO_ARG, 0, 0, 0, nullptr, 0,
     nullptr},
    {"CA-cert", 'C', "Cluster CA Certificate file name", &opt_ca_cert, nullptr,
     nullptr, GET_STR, REQUIRED_ARG, 0, 0, 0, nullptr, 0, nullptr},
    {"CA-key", 'K', "Cluster CA Private Key file name", &opt_ca_key, nullptr,
     nullptr, GET_STR, REQUIRED_ARG, 0, 0, 0, nullptr, 0, nullptr},
    {"CA-search-path", 'P', "Cluster CA file search path", &opt_ca_search_path,
     nullptr, nullptr, GET_STR, REQUIRED_ARG, 0, 0, 0, nullptr, 0, nullptr},
    {"remote-CA-host", NDB_OPT_NOSHORT, "address of remote CA host",
     &opt_ca_host, nullptr, nullptr, GET_STR, REQUIRED_ARG, 0, 0, 0, nullptr, 0,
     nullptr},
    {"CA-tool", 'X', "Path to local executable helper tool", &opt_ca_tool,
     nullptr, nullptr, GET_STR, REQUIRED_ARG, 0, 0, 0, nullptr, 0, nullptr},
    {"create-CA", NDB_OPT_NOSHORT, "Create Cluster CA", &opt_create_ca, nullptr,
     nullptr, GET_BOOL, NO_ARG, 0, 0, 0, nullptr, 0, nullptr},
    {"CA-ordinal", NDB_OPT_NOSHORT, "Ordinal CA name", &opt_ca_ordinal, nullptr,
     nullptr, GET_STR, REQUIRED_ARG, 0, 0, 0, nullptr, 0, nullptr},
    {"trust", 'T', "Manage trust store", &opt_trust, nullptr, &trust_verbs_lib,
     GET_ENUM, REQUIRED_ARG, 0, 0, 0, nullptr, 0, nullptr},
    {"CA-days", NDB_OPT_NOSHORT, "Set CA validity time in days", &opt_ca_days,
     nullptr, nullptr, GET_INT, REQUIRED_ARG, opt_ca_days, -1, 0, nullptr, 0,
     nullptr},
    {"passphrase", NDB_OPT_NOSHORT, "Cluster CA Key Pass Phrase",
     &opt_cluster_key_pass, nullptr, nullptr, GET_STR, REQUIRED_ARG, 0, 0, 0,
     nullptr, 0, nullptr},
    {"remote-openssl", NDB_OPT_NOSHORT,
     "Run openssl on CA host for key signing", &opt_rs_openssl, nullptr,
     nullptr, GET_BOOL, NO_ARG, 0, 0, 0, nullptr, 0, nullptr},
    {"remote-exec-path", NDB_OPT_NOSHORT,
     "Full path to executable on remote CA host", &opt_remote_path, nullptr,
     nullptr, GET_STR, REQUIRED_ARG, 0, 0, 0, nullptr, 0, nullptr},
    {"curve", NDB_OPT_NOSHORT, "Named curve to use for node keys", &opt_curve,
     nullptr, nullptr, GET_STR, REQUIRED_ARG, 0, 0, 0, nullptr, 0, nullptr},
    {"node-id", 'n', "Create or sign key for node n", &opt_node_id, nullptr,
     nullptr, GET_INT, REQUIRED_ARG, 0, 0, MAX_NODES_ID, nullptr, 0, nullptr},
    {"node-type", 't',
     "Create or sign keys for certain node types, "
     "from set (mgmd,db,api)",
     &opt_node_types, nullptr, &node_types_lib, GET_SET, REQUIRED_ARG, 7, 0, 7,
     nullptr, 0, nullptr},  // default 7 = all node types
    {"create-key", NDB_OPT_NOSHORT, "Create (or replace) private keys",
     &opt_create_key, nullptr, nullptr, GET_BOOL, NO_ARG, 0, 0, 0, nullptr, 0,
     nullptr},
    {"pending", NDB_OPT_NOSHORT,
     "Save keys and certificates as pending, rather than active", &opt_pending,
     nullptr, nullptr, GET_BOOL, NO_ARG, 0, 0, 0, nullptr, 0, nullptr},
    {"promote", NDB_OPT_NOSHORT, "Promote pending files to active, then exit",
     &opt_promote, nullptr, nullptr, GET_BOOL, NO_ARG, 0, 0, 0, nullptr, 0,
     nullptr},
    {"sign", NDB_OPT_NOSHORT,
     "Create signed certificates (with --skip-sign, "
     "create certificate signing requests)",
     &opt_sign, nullptr, nullptr, GET_BOOL, NO_ARG, 1, 0, 0, nullptr, 0,
     nullptr},
    {"check", NDB_OPT_NOSHORT, "Run periodic check of certificate expiry dates",
     &opt_periodic, nullptr, nullptr, GET_BOOL, NO_ARG, 0, 0, 0, nullptr, 0,
     nullptr},
    {"replace-by", NDB_OPT_NOSHORT,
     "Suggested certificate replacement date for periodic checks",
     &opt_replace_by, nullptr, nullptr, GET_INT, REQUIRED_ARG, opt_replace_by,
     -128, 127, nullptr, 0, nullptr},
    {"schedule", NDB_OPT_NOSHORT, "set certificate expiration schedule",
     &opt_schedule, nullptr, nullptr, GET_STR, REQUIRED_ARG, 0, 0, 0, nullptr,
     0, nullptr},
    {"duration", NDB_OPT_NOSHORT, "Set exact lifetime for CSR in seconds",
     &opt_duration, nullptr, nullptr, GET_INT, REQUIRED_ARG, 0, -500000, 0,
     nullptr, 0, nullptr},
    {"bound-hostname", NDB_OPT_NOSHORT, "Create certificate bound to hostname",
     &opt_bound_host, nullptr, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, nullptr, 0,
     nullptr},
    {"bind-host", NDB_OPT_NOSHORT,
     "list of node types that should have certificate hostname bindings, "
     "from set (mgmd,db,api)",
     &opt_bind_host, nullptr, &node_types_lib, GET_SET, REQUIRED_ARG, 5, 0, 7,
     nullptr, 0, nullptr},  // default 5 = true for MGMD and API
    {"to-dir", NDB_OPT_NOSHORT, "Specify output directory for created files",
     &opt_dest_dir, nullptr, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, nullptr, 0,
     nullptr},
    {"keys-to-dir", NDB_OPT_NOSHORT,
     "Specify output directory only for private keys (overrides --to-dir)",
     &opt_key_dest_dir, nullptr, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, nullptr, 0,
     nullptr},
    {"stdio", NDB_OPT_NOSHORT, "Read CSR on stdin and write X.509 on stdout",
     &opt_stdio, nullptr, nullptr, GET_BOOL, NO_ARG, 0, 0, 0, nullptr, 0,
     nullptr},
    NdbStdOpt::end_of_options,
};

enum class OperationMode {
  undefined,
  SignKeys,               /* Default mode. The CA can be local or remote. */
  CreateCa,               /* Create a local CA only */
  PromotePendingToActive, /* --promote */
  CheckExpirationDates,   /* --check */
  MaintainTrustStore      /* --trust=... */
};

class Arguments {
  int argc;
  char **argv;

 public:
  Arguments(int c, char **v) : argc(c), argv(v) {}
  bool supplied(int i) const { return (i < argc); }
  char *value(int i) const { return argv[i]; }
  int int_value(int i) const { return atoi(value(i)); }
  bool is_file(int i) const { return (access(value(i), F_OK) == 0); }
};

inline bool message(const char *m) {
  fputs(m, stderr);
  return false;
}

bool option_conflict(const char *opt1, const char *opt2) {
  fprintf(stderr, "Error: --%s is incompatible with --%s\n", opt1, opt2);
  return false;
}

bool option_invalid(const char *opt) {
  fprintf(stderr, "Error: option --%s is valid only in key-signing mode.\n",
          opt);
  return false;
}

bool parse_schedule() {
  int i = sscanf(opt_schedule, "%hd,%hd,%hd,%hd,%hd,%hd", &exp_schedule[0],
                 &exp_schedule[1], &exp_schedule[2], &exp_schedule[3],
                 &exp_schedule[4], &exp_schedule[5]);
  return (i == 6);
}

bool check_options() {
  if (!parse_schedule()) return message("Error: Invalid schedule string.\n");

  if (opt_rs_openssl && !opt_ca_host)
    return message("Error: --remote-openssl requires --remote-CA-host\n");

  if (opt_rs_openssl && opt_ca_tool)
    return option_conflict("remote-openssl", "CA-tool");

  if (opt_node_id && opt_noconfig)
    return message(
        "Error: --node-id cannot be used in --no-config mode;\n"
        "       use -t to specify a node type.\n");

  /* Begin determining mode of operation: */
  OperationMode operationMode = OperationMode::undefined;

  if (opt_trust)
    operationMode = OperationMode::MaintainTrustStore;
  else if (opt_promote)
    operationMode = OperationMode::PromotePendingToActive;
  else if (opt_create_ca)
    operationMode = OperationMode::CreateCa;
  else if (opt_periodic)
    operationMode = OperationMode::CheckExpirationDates;
  else
    operationMode = OperationMode::SignKeys;

  if (operationMode != OperationMode::SignKeys) {
    if (opt_ca_host) return option_invalid("remote-CA-host");
    if (opt_ca_tool) return option_invalid("CA-tool");
    if (opt_rs_openssl) return option_invalid("remote-openssl");
    if (opt_stdio) return option_invalid("stdio");
    opt_sign = false;
  }

  /* Set appropriate remote signing method for given options */
  if (opt_ca_tool)
    signing_method = SIGN_CO_PROCESS;
  else if (opt_rs_openssl)
    signing_method = SIGN_SSH_OPENSSL;
  else if (opt_ca_host)
    signing_method = SIGN_SSH_SIGN_KEYS;

  /* Check opt_remote_path */
  if (opt_remote_path) {
    const char *exe = ndb_basename(opt_remote_path);
    if (!((strcmp(exe, "ndb_sign_keys") == 0) ||
          (strcmp(exe, "ndb_sign_keys.exe") == 0) ||
          (strcmp(exe, "openssl") == 0) || (strcmp(exe, "openssl.exe") == 0)))
      return message("Error: invalid remote signing utility\n");
  }

  /* --stdio requires SignKeys mode; skip display of mode */
  if (opt_stdio) {
    if (!opt_sign) return message("Error: --stdio mode is only for signing\n");
    return true;
  }

  /* In trust mode, allow only a single node type */
  if (operationMode == OperationMode::MaintainTrustStore) {
    if (opt_node_types == 7)
      opt_node_types = 4;  // default to API
    else if (opt_node_types == 3 || opt_node_types > 4)
      return message("Only a single node type is allowed with --trust\n");
  }

  /* Run final check of mismatched options, and print operation mode */
  const char *mode = nullptr;
  switch (operationMode) {
    case OperationMode::SignKeys:
      if (opt_pending)
        mode = opt_create_key ? "create pending keys and certificates"
                              : "create pending certificates";
      else
        mode = opt_create_key ? "create active keys and certificates"
                              : "create active certificates";
      break;

    case OperationMode::MaintainTrustStore:
      if (opt_promote) return option_conflict("trust", "promote");
      if (opt_create_ca) return option_conflict("trust", "create-CA");
      if (opt_periodic) return option_conflict("trust", "check");
      mode = "maintain trust store (verbs: show, add, remove)";
      break;

    case OperationMode::PromotePendingToActive:
      if (opt_create_ca) return option_conflict("promote", "create-CA");
      if (opt_periodic) return option_conflict("promote", "check");
      mode = "promote files";
      break;

    case OperationMode::CreateCa:
      if (opt_periodic) return option_conflict("create-CA", "check");
      mode = "create CA";
      break;

    case OperationMode::CheckExpirationDates:
      mode = "check expiration dates";
      break;

    default:
      assert(false);
      return message("Unexpected operation mode");
  }

  fprintf(stderr, "Mode of operation: %s.\n", mode);
  return true;
}

static void sign_keys_usage_extra() {
  puts("");
  puts("    ndb_sign_keys: Generate TLS Keys and Certificates for NDB Cluster");
  puts("");
  puts("EXAMPLES:");
  puts("");
  puts("  Create a basic CA in the current directory:");
  puts("");
  puts("    ndb_sign_keys --create-CA");
  puts("");
  puts("  Create a key and certificate for ndb_mgmd, using a local cluster");
  puts("  configuration file and a local CA:");
  puts("");
  puts("    ndb_sign_keys -f cluster.ini --create-key -t mgmd \\");
  puts("       --CA-search-path=/var/ndb-ca/");
  puts("");
  puts("  Use a remote CA:");
  puts("");
  puts(
      "    ndb_sign_keys ... --CA-search-path=/remote/dir"
      " --remote-CA-host=name");
  puts("");
  puts("  Create updated certificates for all NDB nodes configured to");
  puts("  run on this host, using config obtained from ndb_mgmd:");
  puts("");
  puts("    ndb_sign_keys --connect-string=mgm-host:1186");
  puts("");
  puts("  Write private keys to directory x and certificates to directory y:");
  puts("");
  puts("    ndb_sign_keys --create-key --keys-to-dir=x --to-dir=y");
  puts("");
  puts("  Check for certificates set to expire within 15 days");
  puts("");
  puts("    ndb_sign_keys --no-config --check --replace-by=-15 \\");
#ifdef _WIN32
  puts("      --ndb-tls-search-path=\\path\\to\\keys;\\path\\to\\certs");
#else
  puts("      --ndb-tls-search-path=/path/to/keys:/path/to/certs");
  puts("");
  puts("");
  puts("ADVICE:");
  puts("");
  puts(" * Define ndb-tls-search-path in [mysql_cluster] section of my.cnf");
  puts(" * Define CA-search-path in [ndb_sign_keys] section of my.cnf");
  puts("");
#endif
}

inline const char *plural(int n) { return n == 1 ? "" : "s"; }

struct {
  int nodes{0};
  int matched{0};
  int keys_created{0};
  int certs_created{0};
  int promoted{0};

  void print() {
    if (nodes)
      fprintf(stderr, "Read %d node%s from cluster configuration.\n", nodes,
              plural(nodes));
    if (matched) {
      fprintf(stderr, "Found %d node%s configured to run on this host", matched,
              plural(matched));
      bool ft = (opt_node_types < (int)Node::Type::ANY);
      if (ft || opt_node_id) {
        fprintf(stderr, " matching ");
        if (opt_node_id) fprintf(stderr, "node id %d", opt_node_id);
        if (opt_node_id && ft) fprintf(stderr, " and ");
        if (ft) fprintf(stderr, "node type filters");
      }
      fprintf(stderr, ".\n");
    }
    if (keys_created || certs_created)
      fprintf(stderr, "Created %d key%s and %d certificate%s.\n", keys_created,
              plural(keys_created), certs_created, plural(certs_created));
    if (promoted)
      fprintf(stderr, "Promoted %d file%s.\n", promoted, plural(promoted));
  }
} stats;

/* sign_keys exits with code 0 on success.
   Exit codes less than 100 are codes from TlsKeyErrors.h.
   Exit codes greater than 100 are specific to sign_keys.
*/
int fatal_error(int code, const char *message) {
  fputs(message, stderr);
  return code;
}

inline int fatal_error_invalid_options() {
  return fatal_error(101, "Failed due to invalid command-line option.\n");
}

inline int fatal_error_cannot_read_config() {
  return fatal_error(102, "Failed to load cluster configuration.\n");
}

int fatal(int code) {
  assert(code > -1);
  if (code > 0 && code < 100)
    fprintf(stderr, "TLS key error: %s.\n", TlsKeyError::message(code));
  else
    stats.print();
  return code;
}

std::unordered_set<std::string> g_local_hostnames;

static constexpr Node::Type cfg_to_cert[3] = {
    Node::Type::DB, Node::Type::Client, Node::Type::MGMD};

bool hostname_is_local(const char *config_hostname) {
  if (strlen(config_hostname) == 0) return true;
  if (g_local_hostnames.count(config_hostname)) return true;  // already known

  char name_buffer[100];
  if (gethostname(name_buffer, sizeof(name_buffer)) == 0) {
    if (strcmp(name_buffer, config_hostname) == 0) {
      g_local_hostnames.emplace(config_hostname);
      return true;
    }
  }

  ndb_sockaddr localAddr;
  if (Ndb_getAddr(&localAddr, config_hostname) == 0) {
    if (SocketServer::tryBind(localAddr)) {
      g_local_hostnames.emplace(config_hostname);
      return true;
    }
  }

  return false;
}

bool g_keys_created[3] = {0, 0, 0};

inline void register_group_key_exists(int cfg_type) {
  g_keys_created[cfg_type] = true;
}

inline bool check_group_key_exists(int cfg_type) {
  return g_keys_created[cfg_type];
}

/**
 *    Configuration
 */

Config *read_configuration(const char *config_file) {
  InitConfigFileParser parser;
  return parser.parseConfig(config_file);
}

Config *fetch_configuration(SSL_CTX *ctx) {
  ndb_mgm_configuration *conf = 0;

  NdbMgmHandle mgm = ndb_mgm_create_handle();
  if (mgm == nullptr) {
    fprintf(stderr, "Cannot create handle to management server.\n");
    return nullptr;
  }

  ndb_mgm_set_ssl_ctx(mgm, ctx);
  ndb_mgm_set_error_stream(mgm, stderr);

  if (ndb_mgm_set_connectstring(mgm, opt_ndb_connectstring)) {
    fprintf(stderr, "* %5d: %s\n", ndb_mgm_get_latest_error(mgm),
            ndb_mgm_get_latest_error_msg(mgm));
    fprintf(stderr, "*        %s", ndb_mgm_get_latest_error_desc(mgm));
    goto noconnect;
  }

  if (ndb_mgm_connect_tls(mgm, opt_connect_retries - 1, opt_connect_retry_delay,
                          1, opt_mgm_tls)) {
    ndberr << "Connect failed, code: " << ndb_mgm_get_latest_error(mgm)
           << ", msg: " << ndb_mgm_get_latest_error_msg(mgm) << endl;
    goto noconnect;
  }

  conf = ndb_mgm_get_configuration(mgm, 0);

  if (conf == nullptr) {
    ndberr << "Could not get configuration, error code: "
           << ndb_mgm_get_latest_error(mgm)
           << ", error msg: " << ndb_mgm_get_latest_error_msg(mgm) << endl;
  }

  ndb_mgm_disconnect(mgm);
noconnect:
  ndb_mgm_destroy_handle(&mgm);

  return conf ? new Config(conf) : nullptr;
}

/* Returns true if a certificate should be replaced */
bool check_replace_time(X509 *cert) {
  CertLifetime certLifetime(cert);
  time_t current_time = time(nullptr);
  time_t replace_time = certLifetime.replace_time(opt_replace_by);
  return (replace_time <= current_time);
}

/**
 *  Certificate Authority
 */

static void print_creating(const char *t1, const char *t2, const char *dir) {
  require(dir);  // cannot store with nullptr
  bool hasDir = strlen(dir);
  fprintf(stderr, "Creating %s %s", t1, t2);
  if (hasDir)
    fprintf(stderr, " in directory %s.\n", dir);
  else
    fprintf(stderr, " in current directory.\n");
}

class ClusterCredentialFiles {
 public:
  static void get_passphrase();
  static int read_CA_key(const TlsSearchPath *, EVP_PKEY *&key,
                         PkiFile::PathName &path,
                         char *pass = opt_cluster_key_pass);
  static int read_CA_certs(const TlsSearchPath *, stack_st_X509 *,
                           PkiFile::PathName &);
  static int create(const char *key_dir, const char *cert_dir);
};

void ClusterCredentialFiles::get_passphrase() {
  if (opt_cluster_key_pass == nullptr) {
    char passphrase[PASSPHRASE_BUFFER_SIZE];
    PEM_def_callback(passphrase, PASSPHRASE_BUFFER_SIZE, 0, nullptr);
    opt_cluster_key_pass = strdup(passphrase);
  }
}

int ClusterCredentialFiles::read_CA_key(const TlsSearchPath *searchPath,
                                        EVP_PKEY *&key, PkiFile::PathName &path,
                                        char *pass) {
  if (searchPath->find(opt_ca_key, path)) {
    key = PrivateKey::open(path, pass);
    if (key) return 0;
    perror("PrivateKey::open()");
    return TlsKeyError::cannot_read_ca_key;
  }
  return TlsKeyError::ca_key_not_found;
}

int ClusterCredentialFiles::read_CA_certs(const TlsSearchPath *searchPath,
                                          stack_st_X509 *certs,
                                          PkiFile::PathName &path) {
  require(certs != nullptr);
  if (searchPath->find(opt_ca_cert, path)) {
    FILE *fp = fopen(path.c_str(), "r");
    if (fp) {
      bool r = Certificate::read(certs, fp);
      fclose(fp);
      if (r) return 0;
    }
    return TlsKeyError::cannot_read_ca_cert;
  }
  return TlsKeyError::ca_cert_not_found;
}

int ClusterCredentialFiles::create(const char *key_dir, const char *cert_dir) {
  fputs(
      "This utility will create a cluster CA private key and "
      "a public key certificate.\n",
      stderr);
  EVP_PKEY *key = EVP_RSA_gen(2048);
  if (!key) return TlsKeyError::openssl_error;
  CertLifetime days(opt_ca_days);
  X509 *cert = ClusterCertAuthority::create(key, days, opt_ca_ordinal, true);
  if (!cert) return TlsKeyError::failed_to_init_ca;

  if (!opt_cluster_key_pass)
    printf(
        "\n"
        "You will be prompted to supply a pass phrase to protect the\n"
        "cluster private key. This security of the cluster depends on this.\n\n"
        "Only the database administrator responsible for this cluster should\n"
        "have the pass phrase. Knowing the pass phrase would allow an "
        "attacker\n"
        "to gain full access to the database.\n\n"
        "The passphrase must be at least 4 characters in length.\n\n");

  print_creating("CA key file", opt_ca_key, key_dir);
  if (!PrivateKey::store(key, key_dir, opt_ca_key, opt_cluster_key_pass)) {
    perror("Error storing CA key");
    return TlsKeyError::cannot_store_ca_key;
  }

  print_creating("CA certificate", opt_ca_cert, cert_dir);
  if (!Certificate::store(cert, cert_dir, opt_ca_cert)) {
    perror("Error storing CA cert");
    return TlsKeyError::cannot_store_ca_cert;
  }
  return 0;
}

int create_CA(TlsSearchPath *CA_path) {
  /* Determine destination paths */
  const char *cert_dir = opt_dest_dir;
  if (!cert_dir) cert_dir = CA_path->first_writable();
  if (!cert_dir) return TlsKeyError::no_writable_dir;

  const char *key_dir = opt_key_dest_dir;
  if (!key_dir) key_dir = cert_dir;

  /* Check for existing CA that might get clobbered */
  PkiFile::PathName path;
  if (CA_path->find(opt_ca_cert, path)) {
    fprintf(stderr, "Failed to create CA: existing CA found in path.\n");
    fprintf(stderr, "Found existing CA at %s\n", path.c_str());
    return TlsKeyError::failed_to_init_ca;
  }

  return ClusterCredentialFiles::create(key_dir, cert_dir);
}

int find_cert_in_stack(const X509 *cert, STACK_OF(X509) * stack) {
  if (stack)
    for (int i = 0; i < sk_X509_num(stack); i++)
      if (X509_cmp(cert, sk_X509_value(stack, i)) == 0) return i;
  return -1;
}

void trust_show_cert(X509 *cert, bool skipDetails = false) {
  char buffer[128];
  CertLifetime lifetime(cert);

  Certificate::get_common_name(cert, buffer, sizeof(buffer));
  printf("     Name: %s\n", buffer);
  if (skipDetails) return;
  SerialNumber::print(buffer, sizeof(buffer), X509_get0_serialNumber(cert));
  printf("     Serial: %s\n", buffer);
  lifetime.print_expire_time(buffer, sizeof(buffer));
  printf("     Expires: %s\n", buffer);
}

const char *trust_show_flags(cstrbuf<64> &buffer, unsigned short flags) {
  if (flags & TlsKeyManager::CA_IN_TRUST_STORE)
    buffer.append("from_trust_file ");
  if (flags & TlsKeyManager::CA_IN_CERT_CHAIN)
    buffer.append("from_cert_chain ");
  if (flags & TlsKeyManager::CA_IS_ROOT) buffer.append("self-signed ");
  return buffer.c_str();
}

void trust_show_stack(STACK_OF(X509) * certs,
                      STACK_OF(X509) *trusted = nullptr) {
  for (int i = 0; i < sk_X509_num(certs); i++) {
    X509 *cert = sk_X509_value(certs, i);
    bool found = (find_cert_in_stack(cert, trusted) >= 0);
    printf("   %d %s\n", i + 1, found ? " ** IN TRUST STORE **" : "");
    trust_show_cert(cert, found);
  }
}

inline FILE *trust_file_open(const char *dir, const char *mode) {
  FILE *fp = TrustStore::open(dir, mode);
  if (fp == nullptr) perror("TrustStore::open()");
  return fp;
}

/* Initialize a TlsKeyManager for a single node (by default an API/client node)
   and display its actual trust store. */
void trust_show_context(TlsSearchPath *search_path) {
  TlsKeyManager keyManager;
  TlsKeyManager::CA_Table list(keyManager);
  cstrbuf<64> flags;
  char *path_string = search_path->expanded_path_string();

  Node::Type type = Node::Type::Client;
  const char *ntype = "api";
  if (opt_node_types < 3) {
    type = Node::Mask(opt_node_types);
    ntype = node_types[opt_node_types - 1];
  }
  printf("\n");
  printf("Current NDB trust store (node type: %4s)\n", ntype);
  printf("-----------------------------------------\n");
  printf("ndb-tls-search-path='%s'\n", path_string);
  {
    PkiFile::PathName pk;
    if (!ActivePrivateKey::find(search_path, 0, type, pk))
      printf("Private key file not found.\n");
  }

  /* Error or info messages from TlsKeyManager will appear here. */
  keyManager.init_mgm_client(opt_tls_search_path, type);
  printf("TLS is %s.\n", keyManager.ctx() ? "available" : "*NOT* available");

  for (cert_table_entry &cert : list) {
    flags.clear();
    puts("  Trusted CA");
    trust_show_cert(cert.cert);
    printf("     Flags: %s\n", trust_show_flags(flags, cert.flags));
    printf("     \n");
  }
  free(path_string);
}

inline int trust_try_read(const char *dir) {
  STACK_OF(X509) *stack = sk_X509_new_null();
  FILE *fp = trust_file_open(dir, "r");
  int n = fp ? TrustStore::read_all(stack, fp) : 0;
  sk_X509_pop_free(stack, X509_free);
  TrustStore::close(fp);
  return n;
}

/* Display full path names to all trusted cert files in path */
bool trust_show_files(TlsSearchPath *search_path) {
  PkiFile::PathName file_path;

  printf("\n");
  printf("Trusted Certificate files\n");
  printf("-------------------------\n");
  int d = TrustStore::find(search_path, file_path, 0);
  if (d < 0) {
    printf("  No files found.\n");
    return false;
  }
  while (d >= 0) {
    int n = trust_try_read(search_path->dir(d));
    if (n < 1)
      printf("  %s [%s]\n", file_path.c_str(), n ? "ERROR" : "EMPTY");
    else
      printf("  %s [%d certificate%s]\n", file_path.c_str(), n, plural(n));
    d = TrustStore::find(search_path, file_path, d + 1);
  }
  return true;
}

void trust_show_trusted_certs(STACK_OF(X509) * stack, TlsSearchPath *path) {
  printf("\n");
  printf("Contents of Trusted Certificate Files\n");
  printf("-------------------------------------\n");
  if (TrustStore::load(stack, path) < 1)
    printf("  ERROR\n");
  else
    trust_show_stack(stack);
}

void trust_show_ca_certs(STACK_OF(X509) * ca_certs, STACK_OF(X509) * trusted) {
  printf("\n");
  printf("Available CA certificates\n");
  printf("-------------------------\n");
  printf("CA-search-path='%s', CA-cert='%s'\n",
         opt_ca_search_path ? opt_ca_search_path : "", opt_ca_cert);
  if (sk_X509_num(ca_certs) < 1)
    printf("  No certificates.\n");
  else
    trust_show_stack(ca_certs, trusted);
  printf("\n");
}

int trust_show(const char *write_dir, TlsSearchPath *search_path,
               STACK_OF(X509) * ca_certs, const Arguments &) {
  STACK_OF(X509) *trusted = sk_X509_new_null();
  g_eventLogger->createConsoleHandler();

  trust_show_context(search_path);
  if (trust_show_files(search_path))
    trust_show_trusted_certs(trusted, search_path);
  trust_show_ca_certs(ca_certs, trusted);

  sk_X509_pop_free(trusted, X509_free);

  /* If write_dir is "", replace it with "." */
  assert(write_dir);
  if (!strlen(write_dir)) write_dir = ".";
  printf("Destination directory: '%s'\n\n", write_dir);

  return 0;
}

bool trust_match_serial(X509 *cert, const char *s) {
  char buffer[128];
  SerialNumber::print(buffer, sizeof(buffer), X509_get0_serialNumber(cert));
  return (strncmp(s, buffer, sizeof(buffer)) == 0);
}

int trust_select_cert_index(STACK_OF(X509) * stack, const Arguments &args) {
  /* When only one CA is in stack it can be implicitly selected with no arg */
  if (!args.supplied(0)) return (sk_X509_num(stack) == 1) ? 0 : -1;

  int d = args.int_value(0);
  if (d > 0 && d <= sk_X509_num(stack))
    return d - 1; /* If argument is an integer from 1 to size, use it */

  for (int i = 0; i < sk_X509_num(stack); i++)
    if (trust_match_serial(sk_X509_value(stack, i), args.value(0)))
      return i; /* Argument matches the serial number of an item in stack */
  return -1;
}

inline X509 *trust_select_cert(STACK_OF(X509) * stack, const Arguments &args) {
  int i = trust_select_cert_index(stack, args);
  return (i >= 0) ? sk_X509_value(stack, i) : nullptr;
}

int trust_add_cert(const TlsSearchPath *search_path, const char *dir,
                   STACK_OF(X509) * ca_certs, const Arguments &args) {
  /* arg0 could be an index into CA certs, a serial number, ... */
  X509 *cert = trust_select_cert(ca_certs, args);
  /* or a file name. */
  if (cert == nullptr && args.is_file(0))
    cert = Certificate::open_one(args.value(0));
  if (cert == nullptr) return TlsKeyError::trust_store_bad_indicator;

  /* Check for duplicate of an existing trusted cert */
  {
    STACK_OF(X509) *trusted = sk_X509_new_null();
    int dup = -1;
    if (TrustStore::load(trusted, search_path) > 0)
      dup = find_cert_in_stack(cert, trusted);
    sk_X509_pop_free(trusted, X509_free);
    if (dup >= 0) return TlsKeyError::trust_store_duplicate;
  }

  /* Append certificate to trust store */
  FILE *fp = trust_file_open(dir, "a");
  if (fp == nullptr) return TlsKeyError::trust_store_cannot_write;
  int rs = TrustStore::write(fp, cert);
  TrustStore::close(fp);
  if (rs == 0) {
    perror("TrustStore::write()");
    return TlsKeyError::trust_store_fs_error;
  }

  printf("Certificate:\n");
  trust_show_cert(cert);
  printf("Added to NDB-trusted-certs in directory '%s'.\n\n", dir);
  return 0;
}

/* Remove one certificate. An argument is required.
   The argument can be an integer or a serial number.
*/
int trust_remove_cert(const TlsSearchPath *path, const Arguments &args) {
  if (!args.supplied(0)) return TlsKeyError::trust_store_bad_indicator;

  STACK_OF(X509) *trusted = sk_X509_new_null();  // the trust store
  const X509 *cert = nullptr;  // the certificate to be removed
  {
    int idx = -1;
    int r = TrustStore::load(trusted, path);
    if (r > 0) idx = trust_select_cert_index(trusted, args);
    if (idx < 0) {
      sk_X509_free(trusted);
      return (r < 1) ? TlsKeyError::trust_store_fs_error
                     : TlsKeyError::trust_store_bad_indicator;
    }
    cert = sk_X509_value(trusted, idx);
  }

  /* We know which certificate to delete, but we don't yet know
     where to find it. */
  int c = -1;
  const char *dir = nullptr;
  STACK_OF(X509) *contents = nullptr;

  for (size_t i = 0; i < path->size(); i++) {
    dir = path->dir(i);
    contents = sk_X509_new_null();
    FILE *fp = TrustStore::open(dir, "r");
    if (fp == nullptr) continue;
    if (TrustStore::read_all(contents, fp) > 0)
      c = find_cert_in_stack(cert, contents);
    TrustStore::close(fp);
    if (c >= 0) break;
    sk_X509_pop_free(contents, X509_free);
  }
  sk_X509_pop_free(trusted, X509_free);
  cert = nullptr;

  assert(c >= 0);
  if (c < 0) {
    // cert was found using load() but not using read_all()
    sk_X509_pop_free(contents, X509_free);
    return TlsKeyError::trust_store_fs_error;
  }

  /* Remove cert from stack */
  sk_X509_delete(contents, c);

  /* If this was the only cert, delete the file */
  if (sk_X509_num(contents) == 0) {
    sk_X509_free(contents);
    if (TrustStore::remove(dir)) {
      printf("Removed NDB-trusted-certs file in directory '%s'.\n\n", dir);
      return 0;
    }
    perror("TrustStore::remove()");
    return TlsKeyError::trust_store_fs_error;
  }

  /* Rewrite the file */
  int rs = 0;
  FILE *fp = TrustStore::open(dir, "w");
  if (fp == nullptr)
    perror("TrustStore::open()");
  else {
    rs = TrustStore::write_all(fp, contents);
    if (rs == 0) perror("TrustStore::write()");
  }
  TrustStore::close(fp);

  sk_X509_pop_free(contents, X509_free);
  if (rs == 0) return TlsKeyError::trust_store_cannot_write;

  printf("Removed certificate from NDB-trusted-certs in directory '%s'.\n\n",
         dir);
  return 0;
}

int manage_trust(const char *write_dir, STACK_OF(X509) * ca_certs,
                 const Arguments &args) {
  TlsSearchPath spath1(opt_tls_search_path);
  std::unique_ptr<TlsSearchPath> search_path(spath1.dedup());

  switch (opt_trust) {
    case 1: /* add */
      return trust_add_cert(search_path.get(), write_dir, ca_certs, args);

    case 2: /* remove */
      return trust_remove_cert(search_path.get(), args);

    case 3: /* show */
      return trust_show(write_dir, search_path.get(), ca_certs, args);
  }
  assert(false);
  return 106;
}

void print_creating_object(const char *type, const char *dir) {
  print_creating(opt_pending ? "pending" : "active", type, dir);
}

int store_key(EVP_PKEY *key, const char *dir, const CertSubject &nc) {
  print_creating_object("private key", dir);

  if (!PendingPrivateKey::store(key, dir, nc))
    return TlsKeyError::cannot_store_pending_key;

  if (!opt_pending) {
    PkiFile::PathName buffer;
    nc.pathname(PkiFile::Type::PendingKey, dir, buffer);
    if (!PendingPrivateKey::promote(buffer))
      return TlsKeyError::cannot_promote_key;
  }
  stats.keys_created++;
  return 0;
}

int store_cert(const NodeCertificate *nc, const char *dir) {
  print_creating_object("certificate", dir);

  if (!PendingCertificate::store(nc, dir))
    return TlsKeyError::cannot_store_pending_cert;

  if (!opt_pending) {
    PkiFile::PathName buffer;
    nc->pathname(PkiFile::Type::PendingCert, dir, buffer);
    if (!PendingCertificate::promote(buffer))
      return TlsKeyError::cannot_promote_cert;
  }
  stats.certs_created++;
  return 0;
}

/* Open a key; prefer a pending key to an active key. */
EVP_PKEY *open_node_private_key(const TlsSearchPath *tlsPath, int id,
                                Node::Type type) {
  PkiFile::PathName key_file;
  short found = PendingPrivateKey::find(tlsPath, id, type, key_file);
  if (!found) found = ActivePrivateKey::find(tlsPath, id, type, key_file);
  return found ? PrivateKey::open(key_file, nullptr) : nullptr;
}

bool set_lifetime(SigningRequest *csr) {
  if (opt_duration) return csr->set_exact_duration(opt_duration);

  switch (csr->node_type()) {
    case Node::Type::Client:
      return csr->set_lifetime(exp_schedule[0], exp_schedule[1]);
    case Node::Type::DB:
      return csr->set_lifetime(exp_schedule[2], exp_schedule[3]);
    case Node::Type::MGMD:
      return csr->set_lifetime(exp_schedule[4], exp_schedule[5]);
    default:
      return message("set_lifetime(): Unexpected node type\n");
  }
}

bool promote_key(const TlsSearchPath *tlsPath, int id, Node::Type type) {
  PkiFile::PathName buffer;
  short a = ActivePrivateKey::find(tlsPath, id, type, buffer);
  short p = PendingPrivateKey::find(tlsPath, id, type, buffer);
  if (p && (a == 0 || a == p))  // "same specificity" requirement
    return PendingPrivateKey::promote(buffer);
  return false;
}

bool promote_cert(const TlsSearchPath *tlsPath, int id, Node::Type type) {
  PkiFile::PathName buffer;
  short a = ActiveCertificate::find(tlsPath, id, type, buffer);
  short p = PendingCertificate::find(tlsPath, id, type, buffer);
  if (p && (a == 0 || a == p))  // "same specificity" requirement
    return PendingCertificate::promote(buffer);
  return false;
}

int do_promote_files(const TlsSearchPath *tlsPath, int id, Node::Type type) {
  bool k = promote_key(tlsPath, id, type);
  if (k) stats.promoted++;
  bool c = promote_cert(tlsPath, id, type);
  if (c) stats.promoted++;
  int result = 0;
  if (!(k || c)) {
    result =
        c ? TlsKeyError::cannot_promote_key : TlsKeyError::cannot_promote_cert;
    fprintf(stderr, "Error: %s\n", TlsKeyError::message(result));
  }

  return result;
}

SigningRequest *create_csr(EVP_PKEY *key, Node::Type type, int id,
                           const char *hostname) {
  bool bind_host = (hostname && Node::And(type, opt_bind_host));
  if (bind_host && opt_bound_host) {
    if (strlen(hostname)) {
      if (strcmp(opt_bound_host, hostname))
        fprintf(stderr,
                "WARNING: Using host name '%s' where NDB configuration "
                "requires '%s'.\n",
                opt_bound_host, hostname);
    } else
      hostname = opt_bound_host;
  }

  SigningRequest *csr = SigningRequest::create(key, type);

  if (csr) {
    if (!set_lifetime(csr)) {
      fatal(TlsKeyError::lifetime_error);
      delete csr;
      return nullptr;
    }

    if (bind_host) csr->bind_hostname(hostname);
  }

  return csr;
}

int get_csr(SigningRequest *&csr, PkiFile::PathName &csr_file,
            const TlsSearchPath *tlsPath, int id, Node::Type type,
            const char *hostname = opt_bound_host) {
  EVP_PKEY *key = nullptr;
  int rs = 0;

  csr = nullptr;

  if (opt_create_key)
    key = PrivateKey::create(opt_curve);
  else if (SigningRequest::find(tlsPath, id, type, csr_file)) {
    csr = SigningRequest::open(csr_file);
    if (csr == nullptr) return TlsKeyError::cannot_read_signing_req;
    return csr->verify() ? 0 : TlsKeyError::verification_error;
  } else
    key = open_node_private_key(tlsPath, id, type);

  if (key == nullptr) return TlsKeyError::active_key_not_found;

  /* Check that we are creating a cert for a single type of node */
  if ((type != Node::Type::MGMD) && (type != Node::Type::DB) &&
      (type != Node::Type::Client)) {
    require(opt_noconfig);
    message("Missing node type. Use -t to specify a single type of node.\n");
    return TlsKeyError::cannot_store_signing_req;
  }

  /* Check for bound hostname */
  if (Node::And(type, opt_bind_host) && !hostname) {
    require(opt_noconfig);
    message(
        "Missing hostname. In no-config mode, either set bind-host=0 "
        "or use --bound-hostname to supply a hostname.\n");
    return TlsKeyError::cannot_store_signing_req;
  }

  /* Create the CSR */
  csr = create_csr(key, type, id, hostname);
  if (csr == nullptr) return TlsKeyError::openssl_error;

  /* finalise() has its own set of error numbers */
  rs = csr->finalise(key);
  if (rs) {
    fprintf(stderr, "SigningRequest::finalise() returned %d\n", rs);
    return TlsKeyError::openssl_error;
  }

  if (opt_create_key) {
    const char *dir = nullptr;
    if (opt_key_dest_dir)
      dir = opt_key_dest_dir;
    else if (opt_dest_dir)
      dir = opt_dest_dir;
    else
      dir = tlsPath->first_writable();
    if (dir == nullptr) return TlsKeyError::cannot_store_pending_key;
    rs = store_key(key, dir, *csr);
  }
  return rs;
}

int do_periodic_check(stack_st_X509 *certs, PkiFile::PathName &path) {
  X509 *cert = sk_X509_value(certs, 0);
  require(cert);
  if (check_replace_time(cert)) {
    char timestamp[30];
    struct tm *exp_time;
    CertLifetime lifetime(cert);
    lifetime.expire_time(&exp_time);
    strftime(timestamp, sizeof(timestamp), "%c", exp_time);
    fprintf(stderr, "Certificate '%s' will expire: %s\n", path.c_str(),
            timestamp);
    return 1;
  }
  return 0;
}

int do_periodic_check(const TlsSearchPath *tlsPath, Node::Type type) {
  PkiFile::PathName cert_path;
  int r = 0;

  if (ActiveCertificate::find(tlsPath, 0, type, cert_path)) {
    stack_st_X509 *certs = Certificate::open(cert_path);
    if (certs) {
      r = do_periodic_check(certs, cert_path);
      Certificate::free(certs);
    }
  }
  return r;
}

/* Periodic check: no-config mode */
int do_periodic_check(const TlsSearchPath *tlsPath) {
  int r = 0;
  if (Node::And(Node::Type::MGMD, opt_node_types))
    r = do_periodic_check(tlsPath, Node::Type::MGMD);
  if (Node::And(Node::Type::DB, opt_node_types))
    r += do_periodic_check(tlsPath, Node::Type::DB);
  if (Node::And(Node::Type::Client, opt_node_types))
    r += do_periodic_check(tlsPath, Node::Type::Client);
  return (r > 0) ? 1 : 0;
}

const NodeCertificate *sign_key(const SigningRequest *, stack_st_X509 *CA,
                                EVP_PKEY *CA_key);

// main

int main(int argc, char **argv) {
  PkiFile::PathName csr_file, ca_key_file, ca_cert_file;
  TlsKeyManager keyManager;
  SSL_CTX *ctx = nullptr;
  EVP_PKEY *ca_key = nullptr;
  stack_st_X509 *ca_certs = sk_X509_new_null();
  int rs = 0;

  g_local_hostnames.emplace("localhost");

  NDB_INIT(argv[0]);
  Ndb_opts opts(argc, argv, sign_keys_options, default_groups);
  opts.set_usage_funcs(sign_keys_usage_extra, nullptr);

  if (opts.handle_options()) return fatal_error_invalid_options();

  if (!check_options()) return fatal_error_invalid_options();

  /* Try to init TlsKeyManager. */
  keyManager.init_mgm_client(opt_tls_search_path);
  ctx = keyManager.ctx();

  /* Main search path and destination directory */
  TlsSearchPath *search_path = new TlsSearchPath(opt_tls_search_path);
  const char *write_dir = nullptr;
  if (opt_dest_dir)
    write_dir = opt_dest_dir;
  else {
    search_path->push_cwd();
    write_dir = search_path->first_writable();
  }

  if (!(write_dir || opt_key_dest_dir || opt_create_ca)) {
    return fatal(TlsKeyError::no_writable_dir);
  }

  /* CA search path
      + ndb_sign_keys uses the CA search path to find CA keys and certificates
      + --CA-search-path takes precedence over --ndb-tls-search-path
      + The final CA search path is passed to the remote key signing server
        as --ndb-tls-search-path, where it might be overriden by a
        --CA-search-path specified in my.cnf there
      + When creating a CA, if a destination directory is specified in --to-dir,
        it takes precedence over --ndb-tls-search-path
  */
  TlsSearchPath *CA_path = nullptr;
  if (opt_ca_search_path)
    CA_path = new TlsSearchPath(opt_ca_search_path);
  else if (opt_create_ca && opt_dest_dir)
    CA_path = new TlsSearchPath(opt_dest_dir);
  else
    CA_path = new TlsSearchPath(opt_tls_search_path);
  remote_ca_path = CA_path->expanded_path_string();

  /* (1) create-CA mode: Create CA and exit */
  if (opt_create_ca) return fatal(create_CA(CA_path));

  /* (2) Obtain CA credentials */
  if (!opt_rs_openssl) {
    if (opt_sign && !opt_stdio) {
      ClusterCredentialFiles::get_passphrase();
      rs = ClusterCredentialFiles::read_CA_key(CA_path, ca_key, ca_key_file);
      if (rs) return fatal(rs);
    }
    rs = ClusterCredentialFiles::read_CA_certs(CA_path, ca_certs, ca_cert_file);
    if (rs && opt_sign) return fatal(rs);
  }

  /* (3) Trust store mode: manage trust store and exit */
  if (opt_trust)
    return fatal(manage_trust(write_dir, ca_certs, Arguments(argc, argv)));

  /* (4) stdio mode: create a single certificate, then exit */
  if (opt_stdio) {
    /* Read the CSR from stdin */
    SigningRequest *csr = SigningRequest::read(stdin);
    if (!csr) return fatal(TlsKeyError::cannot_read_signing_req);
    if (!csr->verify()) return fatal(TlsKeyError::verification_error);
    if (!set_lifetime(csr)) return fatal(TlsKeyError::lifetime_error);

    /* Read CA private key now (deferred from step 2); passphrase on stdin */
    char passphrase[PASSPHRASE_BUFFER_SIZE];
    if (fgets(passphrase, PASSPHRASE_BUFFER_SIZE, stdin)) {
      size_t len = strlen(passphrase);
      if (len && (passphrase[len - 1] == '\n')) passphrase[len - 1] = '\0';
    }
    rs = ClusterCredentialFiles::read_CA_key(CA_path, ca_key, ca_key_file,
                                             passphrase);
    if (rs) return rs;

    /* Sign the certificate, and write the certificate chain to stdout */
    const NodeCertificate *nc = sign_key(csr, ca_certs, ca_key);
    if (!nc) return fatal(TlsKeyError::signing_error);
    Certificate::write(nc->all_certs(), stdout);
    fclose(stdout);
    return 0;
  }

  /* (5) no-config mode: create a single certificate, then exit. */
  if (opt_noconfig) {
    SigningRequest *csr = nullptr;
    Node::Type ntypes = Node::Mask(opt_node_types);

    if (opt_promote) return do_promote_files(search_path, opt_node_id, ntypes);

    if (opt_periodic) return do_periodic_check(search_path);

    rs = get_csr(csr, csr_file, search_path, opt_node_id, ntypes);
    if (rs) return fatal(rs);

    if (!opt_sign) {
      if (csr->store(write_dir)) return 0;
      return fatal(TlsKeyError::cannot_store_signing_req);
    }

    const NodeCertificate *nc = sign_key(csr, ca_certs, ca_key);
    if (nc) rs = store_cert(nc, write_dir);

    if (csr_file.length()) PkiFile::remove(csr_file);
    if (csr->key()) PrivateKey::free(csr->key());
    delete csr;
    delete nc;

    return fatal(nc ? rs : 105);
  }

  /* (6) Obtain cluster configuration from file or mgmd */
  Config *conf = opt_ndb_config_file ? read_configuration(opt_ndb_config_file)
                                     : fetch_configuration(ctx);
  if (conf == nullptr) return fatal_error_cannot_read_config();

  /* (7) Generate node keys and certificates for this host, per config */
  ConfigIter iter(conf, CFG_SECTION_NODE);
  for (iter.first(); iter.valid(); iter.next()) {
    stats.nodes++;
    unsigned int node_id;
    iter.get(CFG_NODE_ID, &node_id);
    if (opt_node_id == 0 || opt_node_id == node_id) {
      unsigned int cfg_node_type;
      const char *hostname = nullptr;
      SigningRequest *csr = nullptr;

      iter.get(CFG_NODE_HOST, &hostname);
      assert(hostname);

      if (!(opt_node_id || hostname_is_local(hostname)))
        continue;  // config is for some other host

      /* Get node type from configuration */
      iter.get(CFG_TYPE_OF_SECTION, &cfg_node_type);
      assert(cfg_node_type < 3);
      Node::Type node_type = cfg_to_cert[cfg_node_type];

      if (!Node::And(node_type, opt_node_types))
        continue;  // skip this node type

      stats.matched++;  // Node is for local host & matches id and type options

      if (check_group_key_exists(cfg_node_type)) continue;
      register_group_key_exists(cfg_node_type);

      if (opt_periodic) {
        if (do_periodic_check(search_path, node_type)) return 1;
        continue;
      }

      if (opt_promote) {
        do_promote_files(search_path, node_id, node_type);
        continue;
      }

      rs = get_csr(csr, csr_file, search_path, node_id, node_type, hostname);
      if (rs) return fatal(rs);

      /* Just create CSR; don't sign key */
      if (!opt_sign) {
        if (csr->store(write_dir)) continue;
        return fatal(TlsKeyError::cannot_store_signing_req);
      }

      /* Sign the key */
      const NodeCertificate *nc = sign_key(csr, ca_certs, ca_key);
      if (!nc) return fatal(TlsKeyError::signing_error);
      rs = store_cert(nc, write_dir);
      if (rs) return fatal(rs);

      /* Clean up */
      if (csr_file.length()) PkiFile::remove(csr_file);
      if (csr->key()) PrivateKey::free(csr->key());
      delete csr;
      delete nc;
    }
  }
  if (!opt_periodic) stats.print();
  if (stats.nodes && !stats.matched)
    return fatal_error(110, "No configured nodes matched filters.\n");

  if (ca_certs) Certificate::free(ca_certs);

  return 0;
}

/* Key signing
   remote_signing_method() returns a non-zero protocol number,
   or 0 if invalid.
*/
int remote_signing_method(BaseString &cmd, NdbProcess::Args &args,
                          const SigningRequest *csr, EVP_PKEY *key) {
  ASN1_STRING *serial = SerialNumber::random();
  SerialNumber::HexString hexSerial(serial);
  SerialNumber::free(serial);

  switch (signing_method) {
    case SIGN_SSH_SIGN_KEYS:  // 1: Run ndb_sign_keys on remote CA host via ssh
      cmd.assign(opt_remote_path ? opt_remote_path : "ndb_sign_keys");
      args.add("--stdio");
      args.add("--duration=", csr->duration());
      if (opt_ca_cert != ClusterCertAuthority::CertFile)
        args.add("--CA-cert=", opt_ca_cert);
      if (opt_ca_key != ClusterCertAuthority::KeyFile)
        args.add("--CA-key=", opt_ca_key);
      if (remote_ca_path) args.add("--ndb-tls-search-path=", remote_ca_path);
      return 1;
    case SIGN_SSH_OPENSSL:  // 2: Run openssl on remote CA host over ssh
      cmd.assign(opt_remote_path ? opt_remote_path : "openssl");
      args.add("x509");
      args.add("-req");
      args.add2("-CA", opt_ca_cert);    // full pathname on remote server
      args.add2("-CAkey", opt_ca_key);  // full pathname on remote server
      args.add2("-days", csr->duration() / CertLifetime::SecondsPerDay);
      args.add2("-set_serial", hexSerial.c_str());
      return 2;
    case SIGN_CO_PROCESS:  // 3: Run the signing utility co-process
      cmd.assign(opt_ca_tool);
      args.add("--duration=", csr->duration());
      args.add("--CA-cert=", opt_ca_cert);
      args.add("--CA-key=", opt_ca_key);
      if (opt_ca_host) args.add("--remote-CA-host=", opt_ca_host);
      /* Check whether ndb_sign_keys is also the utility */
      if (strstr(opt_ca_tool, "ndb_sign_keys")) {
        args.add("--stdio");
        if (remote_ca_path) args.add("--ndb-tls-search-path=", remote_ca_path);
      }
      return 1;

    default:
      fprintf(stderr, "Invalid key signing method %d \n", signing_method);
      return 0;
  }
}

/* The return values for fetch_CA_cert_from_remote_openssl() follow
   remote_key_signing() */
int fetch_CA_cert_from_remote_openssl(stack_st_X509 *CA_certs) {
  if (sk_X509_num(CA_certs) > 0) return 0;  // already fetched

  NdbProcess::Args args;
  NdbProcess::Pipes pipes;

  BaseString cmd(opt_remote_path ? opt_remote_path : "openssl");
  args.add("x509");
  args.add2("-in", opt_ca_cert);

  auto proc = NdbProcess::create_via_ssh("OpensslFetchCA", opt_ca_host, cmd,
                                         nullptr, args, &pipes);
  if (!proc) return 133;

  FILE *rfp = pipes.open(pipes.parentRead(), "r");
  if (!rfp) return 134;

  bool ok = Certificate::read(CA_certs, rfp);
  fclose(rfp);

  int r1 = 137;
  proc->wait(r1, 10000);
  return ok ? r1 : 138;
}

/* remote_key_signing() returns an internal error code between 130 and 140,
   or the exit code of the remote signing process.
*/
int remote_key_signing(const SigningRequest *csr, EVP_PKEY *key,
                       stack_st_X509 *ca_certs, stack_st_X509 *all_certs) {
  BaseString cmd;
  NdbProcess::Args args;
  NdbProcess::Pipes pipes;
  if (!pipes.connected()) {
    perror("Failed pipe");
    return fatal(131);
  }

  /* Prompt user for passphrase */
  if (opt_ca_tool)
    fprintf(stderr, "Using signing helper tool %s\n", opt_ca_tool);
  else
    fprintf(stderr, "Connecting to remote CA at %s.\n", opt_ca_host);

  ClusterCredentialFiles::get_passphrase();

  int r1;
  int protocol = remote_signing_method(cmd, args, csr, key);

  if (protocol == 0) return 132;
  if (protocol == 2) {
    r1 = fetch_CA_cert_from_remote_openssl(ca_certs);
    if (r1 != 0) {
      fprintf(stderr, "Error reading CA cert via openssl: %d.\n", r1);
      return r1;
    }
  }

  /* Create process */
  std::unique_ptr<NdbProcess> proc;
  if (signing_over_ssh())
    proc = NdbProcess::create_via_ssh("RemoteKeySigning", opt_ca_host, cmd,
                                      nullptr, args, &pipes);
  else
    proc = NdbProcess::create("RemoteKeySigning", cmd, nullptr, args, &pipes);
  if (!proc) return fatal_error(133, "Failed to create process.\n");

  /* Write CSR and passphrase to coprocess */
  FILE *wfp = pipes.open(pipes.parentWrite(), "w");
  FILE *rfp = pipes.open(pipes.parentRead(), "r");
  if (!(rfp && wfp)) {
    perror("Failed to open streams");
    return 134;
  }
  if (!csr->write(wfp)) {
    perror("Failed writing to pipe");
    return 135;
  }
  fprintf(wfp, "%s\n", opt_cluster_key_pass);
  fclose(wfp);

  /* Read certificate chain file from coprocess */
  bool read_certs_ok = Certificate::read(all_certs, rfp);
  fclose(rfp);

  /* Wait up to 10 seconds for coprocess to exit.
     Return value 137 will indicate that wait() has failed. */
  r1 = 137;
  proc->wait(r1, 10000);

  /* Check if any failure when certs were read */
  if (!read_certs_ok) return 138;

  /* Check if any certs were read */
  int ncerts = sk_X509_num(all_certs);
  if (ncerts == 0) return 136;

  /* If the signer did not return CA certs, attach them */
  if (ncerts == 1) {
    for (int i = 0; i < sk_X509_num(ca_certs); i++) {
      X509 *x = sk_X509_value(ca_certs, i);
      X509_up_ref(x);
      sk_X509_push(all_certs, x);
    }
  }

  return r1;
}

const NodeCertificate *sign_local(const SigningRequest *csr,
                                  stack_st_X509 *cluster_certs,
                                  EVP_PKEY *cluster_key) {
  X509 *cluster_cert = sk_X509_value(cluster_certs, 0);
  NodeCertificate *nc = new NodeCertificate(*csr, csr->key());
  int rs = nc->finalise(cluster_cert, cluster_key);
  if (rs == 0) {
    /* If there are extra CA certs, add them now */
    for (int i = 1; i < sk_X509_num(cluster_certs); i++)
      nc->push_extra_ca_cert(sk_X509_value(cluster_certs, i));
    return nc;
  }
  fprintf(stderr, "Local key signing error: %d\n", rs);
  delete nc;
  return nullptr;
}

const NodeCertificate *sign_remote(const SigningRequest *csr,
                                   stack_st_X509 *cluster_certs,
                                   EVP_PKEY *cluster_key) {
  STACK_OF(X509) *all_certs = sk_X509_new_null();
  int rs = remote_key_signing(csr, csr->key(), cluster_certs, all_certs);
  if (rs == 0) return NodeCertificate::from_credentials(all_certs, csr->key());
  sk_X509_pop_free(all_certs, X509_free);
  fprintf(stderr, "Remote key signing error: %d\n", rs);
  return nullptr;
}

const NodeCertificate *sign_key(const SigningRequest *csr,
                                stack_st_X509 *cluster_certs,
                                EVP_PKEY *cluster_key) {
  if (csr->key()) EVP_PKEY_up_ref(csr->key());  // For NodeCertificate
  switch (signing_method) {
    case SIGN_LOCAL:
      return sign_local(csr, cluster_certs, cluster_key);
    default:
      return sign_remote(csr, cluster_certs, cluster_key);
  }
}
