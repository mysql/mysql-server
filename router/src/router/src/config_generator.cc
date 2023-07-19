/*
  Copyright (c) 2016, 2023, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#define MYSQL_ROUTER_LOG_DOMAIN \
  ::mysql_harness::logging::kMainLogger  // must precede #include "logging.h"

#include "config_generator.h"

#ifdef _WIN32
#include <Windows.h>
#define strcasecmp _stricmp
#else
#include <sys/stat.h>
#endif

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <random>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <system_error>

#include <rapidjson/rapidjson.h>

#include "certificate_handler.h"
#include "common.h"  // truncate_string
#include "config_builder.h"
#include "dim.h"
#include "harness_assert.h"
#include "hostname_validator.h"
#include "keyring/keyring_manager.h"
#include "mysql/harness/config_option.h"
#include "mysql/harness/config_parser.h"
#include "mysql/harness/dynamic_state.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/logging/supported_logger_options.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/supported_config_options.h"
#include "mysql/harness/utility/string.h"
#include "mysql/harness/vt100.h"
#include "mysqld_error.h"
#include "mysqlrouter/default_paths.h"
#include "mysqlrouter/supported_http_options.h"
#include "mysqlrouter/supported_metadata_cache_options.h"
#include "mysqlrouter/supported_rest_options.h"
#include "mysqlrouter/supported_router_options.h"
#include "mysqlrouter/supported_routing_options.h"
#include "mysqlrouter/uri.h"
#include "mysqlrouter/utils.h"
#include "random_generator.h"
#include "router_app.h"
#include "sha1.h"  // compute_sha1_hash() from mysql's include/
IMPORT_LOG_FUNCTIONS()

#include "cluster_metadata.h"
#include "mysqlrouter/cluster_metadata_dynamic_state.h"

static const int kDefaultRWPort = 6446;
static const int kDefaultROPort = 6447;
static const char *kRWSocketName = "mysql.sock";
static const char *kROSocketName = "mysqlro.sock";

// these were defaults for the pre-8.0.24, we still use them for compatibility
// (--conf-base-port=0 or bootstrapping over the existing configuration)
static const int kLegacyDefaultRWXPort = 64460;
static const int kLegacyDefaultROXPort = 64470;
static const int kBasePortDefault = -1;       // use 8.0.24+ defaults
static const int kBasePortLegacyDefault = 0;  // use pre-8.0.24 defaults
static const char *kRWXSocketName = "mysqlx.sock";
static const char *kROXSocketName = "mysqlxro.sock";

static const int kMaxTCPPortNumber = 65535;
static const int kAllocatedTCPPortCount = 4;  // 2 for classic, 2 for X

static const std::string kSystemRouterName = "system";

static const int kMetadataServerPasswordLength = 16;
static const int kMaxRouterNameLength =
    255;  // must match metadata router.name column

static const char *kKeyringAttributePassword = "password";

static const std::chrono::milliseconds kDefaultMetadataTTL =
    std::chrono::milliseconds(500);
static const std::chrono::milliseconds kDefaultMetadataTTLGRNotificationsON =
    std::chrono::milliseconds(60 * 1000);
static const std::chrono::milliseconds kDefaultMetadataTTLClusterSet =
    std::chrono::milliseconds(
        5 * 1000);  // default TTL for ClusterSet is 5 seconds regardless if GR
                    // Notifications are used or not
static const std::chrono::milliseconds kDefaultAuthCacheTTL =
    std::chrono::seconds(-1);
static const std::chrono::milliseconds kDefaultAuthCacheRefreshInterval =
    std::chrono::milliseconds(2000);
static constexpr uint32_t kMaxRouterId =
    999999;  // max router id is 6 digits due to username size constraints
static constexpr unsigned kNumRandomChars = 12;
static constexpr unsigned kDefaultPasswordRetries =
    20;  // number of the retries when generating random password
         // for the router user during the bootstrap
static constexpr unsigned kMaxPasswordRetries = 10000;

static const std::string kDefaultMetadataCacheSectionKey = "bootstrap";

using mysql_harness::DIM;
using mysql_harness::get_from_map;
using mysql_harness::Path;
using mysql_harness::truncate_string;
using namespace mysqlrouter;
using namespace std::string_literals;

namespace {
struct password_too_weak : public std::runtime_error {
  using std::runtime_error::runtime_error;
};

struct plugin_not_loaded : public std::runtime_error {
  using std::runtime_error::runtime_error;
};
struct account_exists : public std::runtime_error {
  using std::runtime_error::runtime_error;
};
}  // namespace

#ifndef _WIN32
// hint we offer to user when opening dir or file fails with "permission denied"
const char kAppArmorMsg[] =
    "This may be caused by insufficient rights or AppArmor settings.\n"
    "If you have AppArmor enabled try adding full path to the output directory "
    "in the mysqlrouter profile file:\n"
    "/etc/apparmor.d/usr.bin.mysqlrouter\n\n"
    "Example:\n\n"
    "  /path/to/your/output/dir rw,\n"
    "  /path/to/your/output/dir/** rw,";
#endif

static bool is_valid_name(const std::string &name) {
  if (!name.empty()) {
    for (char c : name) {
      if (c == '\n' || c == '\r') return false;
    }
  }
  return true;
}

inline std::string get_opt(const std::map<std::string, std::string> &map,
                           const std::string &key,
                           const std::string &default_value) {
  auto iter = map.find(key);
  if (iter == map.end()) return default_value;
  return iter->second;
}

ConfigGenerator::ConfigGenerator(std::ostream &out_stream,
                                 std::ostream &err_stream
#ifndef _WIN32
                                 ,
                                 SysUserOperationsBase *sys_user_operations
#endif
                                 )
    : connect_timeout_(MySQLSession::kDefaultConnectTimeout),
      read_timeout_(MySQLSession::kDefaultReadTimeout),
      out_stream_(out_stream),
      err_stream_(err_stream)
#ifndef _WIN32
      ,
      sys_user_operations_(sys_user_operations)
#endif
{
}

ConfigGenerator::~ConfigGenerator() = default;

// throws std::logic_error, std::runtime_error, Error(runtime_error)
/*static*/
void ConfigGenerator::set_ssl_options(
    MySQLSession *sess, const std::map<std::string, std::string> &options) {
  std::string ssl_mode =
      get_opt(options, "ssl_mode", MySQLSession::kSslModePreferred);
  std::string ssl_cipher = get_opt(options, "ssl_cipher", "");
  std::string tls_version = get_opt(options, "tls_version", "");
  std::string ssl_ca = get_opt(options, "ssl_ca", "");
  std::string ssl_capath = get_opt(options, "ssl_capath", "");
  std::string ssl_crl = get_opt(options, "ssl_crl", "");
  std::string ssl_crlpath = get_opt(options, "ssl_crlpath", "");

  std::string ssl_cert = get_opt(options, "ssl_cert", "");
  std::string ssl_key = get_opt(options, "ssl_key", "");

  // parse ssl_mode option (already validated in cmdline option handling)
  mysql_ssl_mode ssl_enum =
      MySQLSession::parse_ssl_mode(ssl_mode);  // throws std::logic_error

  // set ssl mode + server authentication options
  sess->set_ssl_options(
      ssl_enum, tls_version, ssl_cipher, ssl_ca, ssl_capath, ssl_crl,
      ssl_crlpath);  // throws Error, std::invalid_argument(logic_error)

  // set client authentication options
  if (!ssl_cert.empty() || !ssl_key.empty()) {
    sess->set_ssl_cert(ssl_cert, ssl_key);  // throws Error(runtime_error)
  }
}

bool ConfigGenerator::warn_on_no_ssl(
    const std::map<std::string, std::string> &options) {
  // warning applicable only if --ssl-mode=PREFERRED (or not specified, which
  // defaults to PREFERRED)
  std::string ssl_mode =
      get_opt(options, "ssl_mode", MySQLSession::kSslModePreferred);
  std::transform(ssl_mode.begin(), ssl_mode.end(), ssl_mode.begin(), toupper);

  if (ssl_mode != MySQLSession::kSslModePreferred) return true;

  // warn if the connection is unencrypted
  try {
    // example response
    //
    // > show status like 'ssl_cipher'
    // +---------------+--------------------+
    // | Variable_name | Value              |
    // +---------------+--------------------+
    // | Ssl_cipher    | DHE-RSA-AES256-SHA | (or null)
    // +---------------+--------------------+

    std::unique_ptr<MySQLSession::ResultRow> result(
        mysql_->query_one("show status like 'ssl_cipher'"));
    if (!result || result->size() != 2 ||
        strcasecmp((*result)[0], "ssl_cipher"))
      throw std::runtime_error("Error reading 'ssl_cipher' status variable");

    // if ssl_cipher is empty, it means the connection is unencrypted
    if ((*result)[1] && (*result)[1][0]) {
      return true;  // connection is encrypted
    } else {
      log_warning(
          "WARNING: The MySQL server does not have SSL configured and "
          "metadata used by the router may be transmitted unencrypted.");
      return false;  // connection is unencrypted
    }
  } catch (const std::exception &e) {
    log_error("Failed determining if metadata connection uses SSL: %s",
              e.what());
    throw std::runtime_error(e.what());
  }
}

// throws std::runtime_error on invalid option value
void ConfigGenerator::parse_bootstrap_options(
    const std::map<std::string, std::string> &bootstrap_options) {
  {
    const auto it = bootstrap_options.find("base-port");

    if (it != bootstrap_options.end()) {
      const int max_base_port =
          (kMaxTCPPortNumber - kAllocatedTCPPortCount + 1);

      // verify only.
      mysql_harness::option_as_uint<uint16_t>(it->second, "--conf-base-port", 0,
                                              max_base_port);
    }
  }

  {
    const auto it = bootstrap_options.find("bind-address");

    if (it != bootstrap_options.end()) {
      const auto address = it->second;
      if (!mysql_harness::is_valid_domainname(address)) {
        throw std::runtime_error("Invalid --conf-bind-address value '" +
                                 address + "'");
      }
    }
  }

  {
    const auto it = bootstrap_options.find("connect-timeout");
    if (it != bootstrap_options.end()) {
      connect_timeout_ = mysql_harness::option_as_uint<uint16_t>(
          it->second, "--connect-timeout", 1);
    }
  }

  {
    const auto it = bootstrap_options.find("read-timeout");
    if (it != bootstrap_options.end()) {
      read_timeout_ = mysql_harness::option_as_uint<uint16_t>(
          it->second, "--read-timeout", 1);
    }
  }
}

// throws std::runtime_error on invalid data
URI ConfigGenerator::parse_server_uri(const std::string &server_uri,
                                      const std::string &bootstrap_socket) {
  // Extract connection information from the bootstrap server URI.
  std::string uri;
  const std::string default_schema = "mysql://";
  if (server_uri.compare(0, default_schema.size(), default_schema) != 0) {
    uri = default_schema + server_uri;
  } else {
    uri = server_uri;
  }

  URI u;
  try {
    // don't allow rootless URIs (mailto:foo@...) which would collide with the
    // schema-less URIs are allow too: root:pw@host
    u = URIParser::parse(uri, false);
  } catch (const mysqlrouter::URIError &e) {
    throw std::runtime_error(e.what());
  }

  // query, fragment and path should all be empty
  if (!u.fragment.empty()) {
    throw std::runtime_error(
        "the bootstrap URI contains a #fragement, but shouldn't");
  }
  if (!u.query.empty()) {
    throw std::runtime_error(
        "the bootstrap URI contains a ?query, but shouldn't");
  }
  if (!u.path.empty()) {
    throw std::runtime_error(
        "the bootstrap URI contains a /path, but shouldn't");
  }

  if (u.username.empty()) {
    u.username = "root";
  }
  // we need to prompt for the password
  if (u.password.empty()) {
    u.password =
        prompt_password("Please enter MySQL password for " + u.username);
  }

  if (bootstrap_socket.size() > 0) {
    // enforce host == "localhost" if a socket is used to avoid ambiguity with
    // the possible hostname
    if (u.host != "localhost") {
      throw std::runtime_error(
          "--bootstrap-socket given, but --bootstrap option contains a "
          "non-'localhost' hostname: " +
          u.host);
    }
  }

  return u;
}

// throws std::runtime_error, std::logic_error
void ConfigGenerator::connect_to_metadata_server(
    const URI &u, const std::string &bootstrap_socket,
    const std::map<std::string, std::string> &bootstrap_options) {
  // connect to (what should be a) metadata server
  mysql_ = std::make_unique<MySQLSession>(
      std::make_unique<MySQLSession::LoggingStrategyDebugLogger>());
  try {
    // throws std::logic_error, std::runtime_error, Error(runtime_error)
    set_ssl_options(mysql_.get(), bootstrap_options);
    mysql_->connect(u.host, u.port, u.username, u.password, bootstrap_socket,
                    "", connect_timeout_, read_timeout_);
  } catch (const MySQLSession::Error &e) {
    throw std::runtime_error("Unable to connect to the metadata server: "s +
                             e.what());
  }

  const auto result = mysqlrouter::setup_metadata_session(*mysql_);
  if (!result) {
    throw std::runtime_error("Failed setting up a metadata session: "s +
                             result.error().c_str());
  }
}

void ConfigGenerator::init_gr_data(const URI &u,
                                   const std::string &bootstrap_socket) {
  cluster_specific_id_ = metadata_->get_cluster_type_specific_id();

  cluster_initial_username_ = u.username;
  cluster_initial_password_ = u.password;
  cluster_initial_hostname_ = u.host;
  cluster_initial_port_ = u.port;
  cluster_initial_socket_ = bootstrap_socket;
}

// throws std::runtime_error, std::logic_error
void ConfigGenerator::init(
    const std::string &server_url,
    const std::map<std::string, std::string> &bootstrap_options) {
  // ensure defaults are in place in case ::init() gets called again
  // and they aren't overwritten by bootstrap_options
  connect_timeout_ = MySQLSession::kDefaultConnectTimeout;
  read_timeout_ = MySQLSession::kDefaultReadTimeout;

  parse_bootstrap_options(bootstrap_options);  // throws std::runtime_error

  const auto &it = bootstrap_options.find("bootstrap_socket");
  std::string bootstrap_socket =
      (it != bootstrap_options.end()) ? it->second : "";
  URI u = parse_server_uri(server_url,
                           bootstrap_socket);  // throws std::runtime_error

  // throws std::runtime_error, std::logic_error,
  connect_to_metadata_server(u, bootstrap_socket, bootstrap_options);
  schema_version_ = mysqlrouter::get_metadata_schema_version(mysql_.get());

  if (schema_version_ == mysqlrouter::kUpgradeInProgressMetadataVersion) {
    throw std::runtime_error(
        "Currently the cluster metadata update is in progress. Please rerun "
        "the bootstrap when it is finished.");
  }

  if (!metadata_schema_version_is_compatible(kRequiredBootstrapSchemaVersion,
                                             schema_version_)) {
    throw std::runtime_error(mysql_harness::utility::string_format(
        "This version of MySQL Router is not compatible with the provided "
        "MySQL InnoDB cluster metadata. Expected metadata version %s, "
        "got %s",
        to_string(kRequiredBootstrapSchemaVersion).c_str(),
        to_string(schema_version_).c_str()));
  }

  metadata_ = mysqlrouter::create_metadata(schema_version_, mysql_.get(),
                                           bootstrap_options);

  // at this point we know the cluster type so let's do additional verifications
  if (mysqlrouter::ClusterType::RS_V2 == metadata_->get_type()) {
    if (bootstrap_options.find("use-gr-notifications") !=
        bootstrap_options.end()) {
      throw std::runtime_error(
          "The parameter 'use-gr-notifications' is valid only for GR cluster "
          "type.");
    }
  }

  if (mysqlrouter::ClusterType::GR_CS != metadata_->get_type()) {
    if (bootstrap_options.find("target-cluster") != bootstrap_options.end()) {
      throw std::runtime_error(
          "The parameter 'target-cluster' is valid only for Cluster that is "
          "part of the ClusterSet.");
    }

    if (bootstrap_options.find("target-cluster-by-name") !=
        bootstrap_options.end()) {
      throw std::runtime_error(
          "The parameter 'target-cluster-by-name' is valid only for Cluster "
          "that is part of the ClusterSet.");
    }
  }

  // check if the current server is meta-data server
  metadata_->require_metadata_is_ok();  // throws MySQLSession::Error,
                                        // std::runtime_error,
                                        // std::out_of_range, std::logic_error
  metadata_->require_cluster_is_ok();   // throws MySQLSession::Error,
                                       // std::runtime_error, std::out_of_range,
                                       // std::logic_error

  init_gr_data(u, bootstrap_socket);
}

static stdx::expected<std::ofstream, std::error_code> open_ofstream(
    const std::string &file_name) {
  std::ofstream of;

  of.open(file_name);

  if (of.fail()) {
    return stdx::make_unexpected(
        std::error_code{errno, std::generic_category()});
  }

  return of;
}

void ConfigGenerator::bootstrap_system_deployment(
    const std::string &program_name, const std::string &config_file_path,
    const std::string &state_file_path,
    const std::map<std::string, std::string> &user_options,
    const std::map<std::string, std::vector<std::string>> &multivalue_options,
    const std::map<std::string, std::string> &default_paths) {
  auto options(user_options);
  mysql_harness::Path _config_file_path(config_file_path);
  AutoCleaner auto_clean;

  std::string router_name;
  if (user_options.find("name") != user_options.end()) {
    router_name = user_options.at("name");
    if (!is_valid_name(router_name))
      throw std::runtime_error("Router name '" + truncate_string(router_name) +
                               "' contains invalid characters.");
    if (router_name.length() > kMaxRouterNameLength)
      throw std::runtime_error("Router name '" + truncate_string(router_name) +
                               "' too long (max " +
                               std::to_string(kMaxRouterNameLength) + ").");
  }
  if (router_name.empty()) router_name = kSystemRouterName;

  if (user_options.find("socketsdir") == user_options.end())
    options["socketsdir"] = "/tmp";

  // (re-)bootstrap the instance
  std::vector<std::string> config_files_names{config_file_path,
                                              state_file_path};
  std::vector<std::ofstream> config_files;
  for (const auto &config_file_name : config_files_names) {
    const std::string tmp_file_name = config_file_name + ".tmp";

    auto open_res = open_ofstream(tmp_file_name);
    if (!open_res) {
      throw std::system_error(
          open_res.error(),
          "Could not open " + tmp_file_name + " for writing: ");
    }
    auto_clean.add_file_delete(tmp_file_name);

    config_files.push_back(std::move(open_res.value()));
  }

  // on bootstrap failure, DROP USER for all created accounts
  std::unique_ptr<void, std::function<void(void *)>> create_user_undo(
      (void *)1, [&](void *) { undo_create_user_for_new_accounts(); });

  const std::string bootstrap_report_text = bootstrap_deployment(
      program_name, config_files[0], config_files[1], config_file_path,
      state_file_path, router_name, options, multivalue_options, default_paths,
      false, auto_clean);

  for (size_t i = 0; i < config_files.size(); ++i) {
    config_files[i].close();
    const std::string path = config_files_names[i];
    const bool is_static_conf = (i == 0);
    const std::string file_desc =
        is_static_conf ? "configuration" : "dynamic state";

    if (backup_config_file_if_different(path, path + ".tmp", options,
                                        &auto_clean)) {
      std::cout << "\nExisting " << file_desc << " backed up to '" << path
                << ".bak'" << std::endl;
      auto_clean.add_file_delete(path);
    }

    // rename the .tmp file to the final file
    auto rename_res = mysqlrouter::rename_file((path + ".tmp"), path);

    if (!rename_res) {
      auto ec = rename_res.error();

      throw std::system_error(
          ec, "Could not save " + file_desc + " file to final location");
    }
    try {
      // for dynamic config file we need to grant the write access too
      mysql_harness::make_file_private(path, /* read_only= */ is_static_conf);
    } catch (const std::system_error &e) {
#ifdef _WIN32
      if (e.code() !=
          std::error_code(ERROR_INVALID_FUNCTION, std::system_category()))
      // if the filesystem can't set permissions, the test later would fail
#endif
        throw;
    }
    set_file_owner(options, path);
  }
  auto_clean.clear();
  create_user_undo.release();
  out_stream_ << bootstrap_report_text;
}

bool ConfigGenerator::datadir_contains_allowed_files(
    const mysql_harness::Directory &dir) const {
  const std::set<mysql_harness::Path> allowed_paths{
      mysql_harness::Path{"data"}.join("ca-key.pem"),
      mysql_harness::Path{"data"}.join("ca.pem"),
      mysql_harness::Path{"data"}.join("router-key.pem"),
      mysql_harness::Path{"data"}.join("router-cert.pem")};

  auto existing_paths = dir.list_recursive();
  std::sort(std::begin(existing_paths), std::end(existing_paths));
  return std::includes(std::cbegin(allowed_paths), std::cend(allowed_paths),
                       std::cbegin(existing_paths), std::cend(existing_paths));
}

/**
 * Create a self-contained deployment of the Router in a directory.
 */
void ConfigGenerator::bootstrap_directory_deployment(
    const std::string &program_name, const std::string &directory,
    const std::map<std::string, std::string> &user_options,
    const std::map<std::string, std::vector<std::string>> &multivalue_options,
    const std::map<std::string, std::string> &default_paths) {
  bool force = user_options.find("force") != user_options.end();
  mysql_harness::Path path(directory);
  std::string router_name;
  AutoCleaner auto_clean;

  if (user_options.find("name") != user_options.end()) {
    if ((router_name = user_options.at("name")) == kSystemRouterName)
      throw std::runtime_error("Router name '" + kSystemRouterName +
                               "' is reserved");
    if (!is_valid_name(router_name))
      throw std::runtime_error("Router name '" + truncate_string(router_name) +
                               "' contains invalid characters.");
    if (router_name.length() > kMaxRouterNameLength)
      throw std::runtime_error("Router name '" + truncate_string(router_name) +
                               "' too long (max " +
                               std::to_string(kMaxRouterNameLength) + ").");
  }

  if (!path.exists()) {
    int err = mysql_harness::mkdir(directory, kStrictDirectoryPerm);
    if (err != 0) {
      auto ec = std::error_code{err, std::generic_category()};
      log_error("Cannot create directory '%s': %s",
                truncate_string(directory).c_str(), ec.message().c_str());
#ifndef _WIN32
      if (err == EACCES || err == EPERM) log_error(kAppArmorMsg);
#endif
      throw std::system_error(ec, "Could not create deployment directory");
    }
    auto_clean.add_directory_delete(directory, true);
  }

  if (!Path(directory).is_directory()) {
    throw std::runtime_error("Expected bootstrap directory '" + directory +
                             "' to be a directory, but its type is: " +
                             mysqlrouter::to_string(Path(directory).type()));
  }

  set_file_owner(user_options, directory);

  path = path.real_path();
  mysql_harness::Path config_file_path =
      path.join(mysql_harness::Path("mysqlrouter.conf"));
  if (!config_file_path.exists() && !force) {
    bool dir_empty;
    try {
      dir_empty = mysql_harness::Directory{path}.is_empty();
    } catch (const std::system_error &e) {
      log_error("%s", e.what());
#ifndef _WIN32
      if (e.code().value() == EACCES || e.code().value() == EPERM)
        log_error(kAppArmorMsg);
#endif
      throw std::runtime_error(
          "Could not check contents of existing deployment directory");
    } catch (...) {
      harness_assert_this_should_not_execute();
    }

    if (!dir_empty && !datadir_contains_allowed_files(path)) {
      log_error("Directory '%s' already contains files", directory.c_str());
      throw std::runtime_error("Directory already exits");
    }
  }

  std::map<std::string, std::string> options(user_options);

  const std::vector<std::tuple<std::string, std::string, bool>> directories{
      //              option name   dir_name      mkdir
      std::make_tuple("logdir", "log", true),
      std::make_tuple("rundir", "run", true),
      std::make_tuple("datadir", "data", true),
      std::make_tuple("socketsdir", "", false),
  };

  for (const auto &dir : directories) {
    const auto &option_name = std::get<0>(dir);
    const auto &dir_name = std::get<1>(dir);
    const auto &do_mkdir = std::get<2>(dir);

    if (user_options.find(option_name) == user_options.end()) {
      if (dir_name.empty()) {
        options[option_name] = path.str();
      } else {
        options[option_name] = path.join(dir_name).str();
      }
    }
    if (do_mkdir) {
      int res =
          mysql_harness::mkdir(options[option_name], kStrictDirectoryPerm);
      if (res != 0) {
        std::error_code ec{res, std::generic_category()};
        if (res != EEXIST) {
          log_error("Cannot create directory '%s': %s",
                    truncate_string(options[option_name]).c_str(),
                    ec.message().c_str());
          throw std::system_error(ec,
                                  "Could not create " + option_name +
                                      " directory: " + options[option_name]);
        }
      } else {
        auto_clean.add_directory_delete(options[option_name]);
      }
    }

    // sets the directory owner if the directory exists and --user provided
    set_file_owner(options, options[option_name]);
  }

  // (re-)bootstrap the instance
  std::vector<std::string> config_files_names{
      config_file_path.str(), path.join("data").join("state.json").str()};
  std::vector<std::ofstream> config_files;
  for (const auto &config_file_name : config_files_names) {
    const std::string tmp_file_name = config_file_name + ".tmp";

    auto open_res = open_ofstream(tmp_file_name);
    if (!open_res) {
      const auto ec = open_res.error();
#ifndef _WIN32
      // on Linux give the hint about AppArmor
      if (ec == make_error_condition(std::errc::permission_denied)) {
        log_error(kAppArmorMsg);
      }
#endif
      throw std::system_error(
          ec, "Could not open " + tmp_file_name + " for writing");
    }
    auto_clean.add_file_delete(tmp_file_name);

    config_files.push_back(std::move(open_res.value()));
  }

  set_keyring_info_real_paths(options, path);

  // on bootstrap failure, DROP USER for all created accounts
  std::unique_ptr<void, std::function<void(void *)>> create_user_undo(
      (void *)1, [&](void *) { undo_create_user_for_new_accounts(); });

  const std::string bootstrap_report_text = bootstrap_deployment(
      program_name, config_files[0], config_files[1], config_files_names[0],
      config_files_names[1], router_name, options, multivalue_options,
      default_paths, true,
      auto_clean);  // throws std::runtime_error, ?

  for (size_t i = 0; i < config_files_names.size(); ++i) {
    auto &config_file = config_files[i];
    const auto &config_file_name = config_files_names[i];
    const bool is_static_conf = (i == 0);
    config_file.close();
    if (backup_config_file_if_different(config_file_name,
                                        config_file_name + ".tmp", options)) {
      std::cout << "\nExisting configurations backed up to '"
                << config_file_name << ".bak'" << std::endl;
    }
    // rename the .tmp file to the final file
    auto rename_res = mysqlrouter::rename_file(
        (config_file_name + ".tmp").c_str(), config_file_name.c_str());

    if (!rename_res) {
      const auto ec = rename_res.error();

      throw std::system_error(ec, "Could not move configuration file '" +
                                      config_file_name +
                                      ".tmp' to final location");
    }

    try {
      mysql_harness::make_file_private(
          config_file_name,
          /* read_only_for_local_service= */ is_static_conf);
    } catch (const std::system_error &e) {
#ifdef _WIN32
      if (e.code() !=
          std::error_code(ERROR_INVALID_FUNCTION, std::system_category()))
      // if the filesystem can't set permissions, the test later would fail
#endif
        throw;
    }
    set_file_owner(options, config_file_name);
  }

  // create start/stop scripts
  create_start_script(program_name, path.str(),
                      keyring_info_.get_master_key_file().empty(), options);
  create_stop_script(path.str(), options);

#ifndef _WIN32
  // If we are running with --user option we need to check if the user will have
  // access to the directory where the bootstrap output files were created. It
  // may not have access if it does not have search right to any of the
  // directories on the path. We do this by switching to the --user and trying
  // to open the config file.
  if (options.find("user") != options.end()) {
    std::string &user_name = options.at("user");

    // remember the current user as we want to switch back to it
    std::string orig_user_name{std::to_string(geteuid())};

    set_user(user_name);
    bool user_has_access{false};
    {
      std::ifstream conf_file;
      conf_file.open(config_file_path.str());
      user_has_access = !conf_file.fail();
    }
    // switch back to original user.
    //
    // this is needed to clean up the files in case the user can't access them
    // and we are failing the bootstrap
    set_user(orig_user_name);

    if (!user_has_access) {
      std::error_code ec{errno, std::generic_category()};
      throw std::system_error(
          ec, "Could not access the config file as user '" + user_name +
                  "' after the bootstrap in the directory " + directory);
    }
  }
#endif

  auto_clean.clear();
  create_user_undo.release();
  out_stream_ << bootstrap_report_text;
}

ConfigGenerator::Options ConfigGenerator::fill_options(
    const std::map<std::string, std::string> &user_options,
    const std::map<std::string, std::string> &default_paths,
    const ExistingConfigOptions &existing_config_options) {
  std::string bind_address{"0.0.0.0"};
  bool use_sockets = false;
  bool skip_tcp = false;
  int base_port = kBasePortDefault;

  {
    const auto it = user_options.find("base-port");

    if (it != user_options.end()) {
      const int max_base_port =
          (kMaxTCPPortNumber - kAllocatedTCPPortCount + 1);
      base_port = mysql_harness::option_as_uint<uint16_t>(
          it->second, "--conf-base-port", 0, max_base_port);
    }
  }

  if (user_options.find("use-sockets") != user_options.end()) {
    use_sockets = true;
  }
  if (user_options.find("skip-tcp") != user_options.end()) {
    skip_tcp = true;
  }
  ConfigGenerator::Options options;
  if (user_options.find("bind-address") != user_options.end()) {
    // the address was already validated in parse_bootstrap_options()
    options.bind_address = user_options.at("bind-address");
  }

  // if not given as a parameter we want consecutive numbers starting with 6446
  bool use_default_ports{false};
  if (base_port == kBasePortDefault) {
    base_port = kDefaultRWPort;
    use_default_ports = true;
  }

  // classic protocol endpoints
  if (use_sockets) {
    options.rw_endpoint.socket = kRWSocketName;
    options.ro_endpoint.socket = kROSocketName;
  }
  if (!skip_tcp) {
    options.rw_endpoint.port =
        base_port == kBasePortLegacyDefault ? kDefaultRWPort : base_port;
    options.ro_endpoint.port =
        base_port == kBasePortLegacyDefault ? kDefaultROPort : base_port + 1;
  }

  // x protocol endpoints
  if (use_sockets) {
    options.rw_x_endpoint.socket = kRWXSocketName;
    options.ro_x_endpoint.socket = kROXSocketName;
  }
  if (!skip_tcp) {
    // if "base-port" param was not provided AND we are overwriting an
    // existing config AND the RW X bind_port in the existing config was
    // legacy default (64460) we want to keep it
    if (use_default_ports && existing_config_options.valid &&
        existing_config_options.rw_x_port == kLegacyDefaultRWXPort) {
      options.rw_x_endpoint.port = kLegacyDefaultRWXPort;
    } else {
      options.rw_x_endpoint.port = base_port == kBasePortLegacyDefault
                                       ? kLegacyDefaultRWXPort
                                       : base_port + 2;
    }

    // if "base-port" param was not provided AND we are overwriting an
    // existing config AND the RO X bind_port in the existing config was
    // legacy default (64470) we want to keep it
    if (use_default_ports && existing_config_options.valid &&
        existing_config_options.ro_x_port == kLegacyDefaultROXPort) {
      options.ro_x_endpoint.port = kLegacyDefaultROXPort;
    } else {
      options.ro_x_endpoint.port = base_port == kBasePortLegacyDefault
                                       ? kLegacyDefaultROXPort
                                       : base_port + 3;
    }
  }

  if (user_options.find("logdir") != user_options.end())
    options.override_logdir = user_options.at("logdir");
  if (user_options.find("filename") != user_options.end())
    options.override_logfilename = user_options.at("filename");
  if (user_options.find("rundir") != user_options.end())
    options.override_rundir = user_options.at("rundir");
  if (user_options.find("datadir") != user_options.end())
    options.override_datadir = user_options.at("datadir");
  if (user_options.find("socketsdir") != user_options.end())
    options.socketsdir = user_options.at("socketsdir");

  options.ssl_options.mode = get_opt(user_options, "ssl_mode", "");
  options.ssl_options.cipher = get_opt(user_options, "ssl_cipher", "");
  options.ssl_options.tls_version = get_opt(user_options, "tls_version", "");
  options.ssl_options.ca = get_opt(user_options, "ssl_ca", "");
  options.ssl_options.capath = get_opt(user_options, "ssl_capath", "");
  options.ssl_options.crl = get_opt(user_options, "ssl_crl", "");
  options.ssl_options.crlpath = get_opt(user_options, "ssl_crlpath", "");

  if (user_options.find("use-gr-notifications") != user_options.end())
    options.use_gr_notifications =
        user_options.at("use-gr-notifications") == "1";
  else {
    // default for ClusterSet is use to GR Notifications, for single cluster it
    // is not use them
    options.use_gr_notifications =
        mysqlrouter::ClusterType::GR_CS == metadata_->get_type();
  }

  if (mysqlrouter::ClusterType::GR_CS == metadata_->get_type()) {
    options.ttl = kDefaultMetadataTTLClusterSet;
  } else {
    options.ttl = options.use_gr_notifications
                      ? kDefaultMetadataTTLGRNotificationsON
                      : kDefaultMetadataTTL;
  }

  if (user_options.find("disable-rest") != user_options.end())
    options.disable_rest = true;

  options.https_port_str = get_opt(user_options, "https-port", "8443");

  options.client_ssl_mode =
      get_opt(user_options, "client_ssl_mode", "PREFERRED");
  options.server_ssl_mode =
      get_opt(user_options, "server_ssl_mode", "AS_CLIENT");

  // default depends on client-ssl-mode and server-ssl-mode
  std::string default_client_ssl_cert;
  std::string default_client_ssl_key;

  if (!((options.client_ssl_mode == "PASSTHROUGH") ||
        (options.client_ssl_mode == "DISABLED"))) {
    mysql_harness::Path datadir_path =
        (user_options.find("datadir") != std::end(user_options))
            ? mysql_harness::Path(user_options.at("datadir"))
            : mysql_harness::Path(default_paths.at("data_folder"));

    default_client_ssl_cert =
        datadir_path.real_path().join(tls_filenames_.router_cert).str();
    default_client_ssl_key =
        datadir_path.real_path().join(tls_filenames_.router_key).str();
  }

  options.client_ssl_cert =
      get_opt(user_options, "client_ssl_cert", default_client_ssl_cert);
  options.client_ssl_key =
      get_opt(user_options, "client_ssl_key", default_client_ssl_key);
  options.client_ssl_cipher = get_opt(user_options, "client_ssl_cipher", "");
  options.client_ssl_curves = get_opt(user_options, "client_ssl_curves", "");
  options.client_ssl_dh_params =
      get_opt(user_options, "client_ssl_dh_params", "");

  options.server_ssl_ca = get_opt(user_options, "server_ssl_ca", "");
  options.server_ssl_capath = get_opt(user_options, "server_ssl_capath", "");
  options.server_ssl_crl = get_opt(user_options, "server_ssl_crl", "");
  options.server_ssl_crlpath = get_opt(user_options, "server_ssl_crlpath", "");
  options.server_ssl_cipher = get_opt(user_options, "server_ssl_cipher", "");
  options.server_ssl_curves = get_opt(user_options, "server_ssl_curves", "");
  options.server_ssl_verify =
      get_opt(user_options, "server_ssl_verify", "DISABLED");

  options.target_cluster = get_opt(user_options, "target-cluster", "default");

  options.target_cluster = get_opt(user_options, "target-cluster", "");
  options.target_cluster_by_name =
      get_opt(user_options, "target-cluster-by-name", "");

  return options;
}

namespace {

unsigned get_password_retries(
    const std::map<std::string, std::string> &user_options) {
  const auto it = user_options.find("password-retries");
  if (it == user_options.end()) {
    return kDefaultPasswordRetries;
  }

  if (it->second.empty()) {
    throw std::invalid_argument(
        "--password-retries needs value between 1 and " +
        std::to_string(kMaxPasswordRetries) + " inclusive, was ''");
  }

  return mysql_harness::option_as_uint<unsigned long>(
      it->second, "--password-retries", 1, kMaxPasswordRetries);
}

std::string compute_password_hash(const std::string &password) {
  uint8_t hash_stage1[SHA1_HASH_SIZE];
  compute_sha1_hash(hash_stage1, password.c_str(), password.length());
  uint8_t hash_stage2[SHA1_HASH_SIZE];
  compute_sha1_hash(hash_stage2, (const char *)hash_stage1, SHA1_HASH_SIZE);

  std::stringstream ss;
  ss << "*";
  ss << std::hex << std::setfill('0') << std::uppercase;
  for (unsigned i = 0; i < SHA1_HASH_SIZE; ++i) {
    ss << std::setw(2) << (int)hash_stage2[i];
  }

  return ss.str();
}

inline std::string str(
    const mysqlrouter::ConfigGenerator::Options::Endpoint &ep) {
  if (ep.port > 0)
    return std::to_string(ep.port);
  else if (!ep.socket.empty())
    return ep.socket;
  else
    return "null";
}

}  // namespace

/**
 * Error codes for MySQL Errors that we handle specifically
 *
 * @todo extend to other MySQL Error codes that need to be handled specifically
 *       and move into a place where other can access it too
 */
enum class MySQLErrorc {
  kSyntaxError = ER_PARSE_ERROR,                  // 1064
  kSuperReadOnly = ER_OPTION_PREVENTS_STATEMENT,  // 1290
  kLostConnection = CR_SERVER_LOST,               // 2013
};

/**
 * Cluster (GR or AR)-aware decorator for MySQL Sessions.
 */
class ClusterAwareDecorator {
 public:
  ClusterAwareDecorator(
      ClusterMetadata &metadata, const std::string &cluster_initial_username,
      const std::string &cluster_initial_password,
      const std::string &cluster_initial_hostname,
      unsigned long cluster_initial_port,
      const std::string &cluster_initial_socket,
      unsigned long connection_timeout,
      std::set<MySQLErrorc> failure_codes = {MySQLErrorc::kSuperReadOnly,
                                             MySQLErrorc::kLostConnection})
      : metadata_(metadata),
        cluster_initial_username_(cluster_initial_username),
        cluster_initial_password_(cluster_initial_password),
        cluster_initial_hostname_(cluster_initial_hostname),
        cluster_initial_port_(cluster_initial_port),
        cluster_initial_socket_(cluster_initial_socket),
        connection_timeout_(connection_timeout),
        failure_codes_(std::move(failure_codes)) {}

  template <class R>
  R failover_on_failure(std::function<R()> wrapped_func);

  virtual ~ClusterAwareDecorator() = default;

 protected:
  void connect(MySQLSession &session, const std::string &host,
               const unsigned port);

  ClusterMetadata &metadata_;
  const std::string &cluster_initial_username_;
  const std::string &cluster_initial_password_;
  const std::string &cluster_initial_hostname_;
  unsigned long cluster_initial_port_;
  const std::string &cluster_initial_socket_;
  unsigned long connection_timeout_;
  std::set<MySQLErrorc> failure_codes_;
};

void ClusterAwareDecorator::connect(MySQLSession &session,
                                    const std::string &host,
                                    const unsigned port) {
  try {
    session.connect(host, port, cluster_initial_username_,
                    cluster_initial_password_, "", "", connection_timeout_);
  } catch (const std::exception &) {
    if (session.is_connected()) session.disconnect();
    throw;
  }
}

/**
 * Cluster (GR or AR) aware failover.
 *
 * @param wrapped_func function will be called
 *
 * assumes:
 *
 * - actively connected mysql_ session
 * - all nodes in the group have the same user/pass combination
 * - wrapped_func throws MySQLSession::Error with .code in .failure_codes
 */
template <class R>
R ClusterAwareDecorator::failover_on_failure(std::function<R()> wrapped_func) {
  bool fetched_cluster_servers = false;
  std::vector<std::tuple<std::string, unsigned long>> cluster_servers;

  auto cluster_servers_it = cluster_servers.begin();
  const auto cluster_specific_initial_id =
      metadata_.get_cluster_type_specific_id();

  bool initial_node = true;
  do {
    bool skip_node = false;
    if (!initial_node) {
      // let's check if the node we failed over to belongs to the same cluster
      // the user is bootstaping against
      const auto cluster_specific_id = metadata_.get_cluster_type_specific_id();

      if (cluster_specific_id != cluster_specific_initial_id) {
        log_warning(
            "Node on '%s' that the bootstrap failed over to, seems to belong "
            "to different cluster(%s != %s), skipping...",
            metadata_.get_session().get_address().c_str(),
            cluster_specific_initial_id.c_str(), cluster_specific_id.c_str());
        skip_node = true;
      }
    } else {
      initial_node = false;
    }

    if (!skip_node) {
      try {
        return wrapped_func();
      } catch (const MySQLSession::Error &e) {
        MySQLErrorc ec = static_cast<MySQLErrorc>(e.code());

        log_debug(
            "Executing statements failed with: '%s' (%d), trying to connect to "
            "another node",
            e.what(), e.code());

        // code not in failure-set
        if (failure_codes_.find(ec) == failure_codes_.end()) {
          throw;
        }
      }
    }

    // bootstrap not successful, checking next node to fail over to
    do {
      if (!fetched_cluster_servers) {
        // lazy fetch the GR members
        //
        fetched_cluster_servers = true;

        log_info("Fetching Cluster Members");

        for (auto &cluster_node : metadata_.fetch_cluster_hosts()) {
          auto const &node_host = std::get<0>(cluster_node);
          auto node_port = std::get<1>(cluster_node);

          // if we connected through TCP/IP, ignore the initial host
          if (cluster_initial_socket_.size() == 0 &&
              (node_host == cluster_initial_hostname_ &&
               node_port == cluster_initial_port_)) {
            continue;
          }

          log_debug("added cluster node: %s:%ld", node_host.c_str(), node_port);
          cluster_servers.emplace_back(node_host, node_port);
        }

        // get a new iterator as the old one is now invalid
        cluster_servers_it = cluster_servers.begin();
      } else {
        std::advance(cluster_servers_it, 1);
      }

      if (cluster_servers_it == cluster_servers.end()) {
        throw std::runtime_error("no more nodes to fail-over too, giving up.");
      }

      if (metadata_.get_session().is_connected()) {
        log_debug("%s", "disconnecting from mysql-server");
        metadata_.get_session().disconnect();
      }

      auto const &tp = *cluster_servers_it;

      auto const &host = std::get<0>(tp);
      auto port = std::get<1>(tp);

      log_info("trying to connect to mysql-server at %s:%ld", host.c_str(),
               port);

      try {
        connect(metadata_.get_session(), host, port);
      } catch (const std::exception &inner_e) {
        log_info("Failed connecting to %s:%ld: %s, trying next", host.c_str(),
                 port, inner_e.what());
        continue;
      }

      const auto result =
          mysqlrouter::setup_metadata_session(metadata_.get_session());
      if (!result) {
        metadata_.get_session().disconnect();
        log_info("Failed setting up a metadata session %s:%ld: %s, trying next",
                 host.c_str(), port, result.error().c_str());
      }

      // if this fails, we should just skip it and go to the next
    } while (!metadata_.get_session().is_connected());
  } while (true);
}

void ConfigGenerator::set_log_file_permissions(
    const std::map<std::string, std::string> &default_paths,
    const std::map<std::string, std::string> &user_options,
    const Options &options) {
#ifdef _WIN32
  UNREFERENCED_PARAMETER(default_paths);
  UNREFERENCED_PARAMETER(user_options);
  UNREFERENCED_PARAMETER(options);
#else
  /* Currently at this point the logger is not yet initialized but while
   * bootstrapping with the --user=<user> option we need to create a log file
   * and chown it to the <user>. Otherwise when the router gets launched later
   * (not bootstrap) with the same --user=<user> option, the user might not have
   * right to the logging directory.
   */
  out_stream_ << "- Adjusting permissions of generated files" << std::endl;
  // assert(default_paths.find("logging_folder") != default_paths.end());
  std::string logdir = (!options.override_logdir.empty())
                           ? options.override_logdir
                           : default_paths.at("logging_folder");
  std::string logfilename = (!options.override_logfilename.empty())
                                ? options.override_logfilename
                                : mysql_harness::logging::kDefaultLogFilename;
  if (!logdir.empty()) {
    auto log_path = mysql_harness::Path(logdir).join(logfilename);
    auto log_file = log_path.str();
    std::fstream f;
    f.open(log_file, std::ios::out);
    set_file_owner(user_options, log_file);
  }
#endif
}

// create certificates in default locations if key and cert don't exist.
void ConfigGenerator::prepare_ssl_certificate_files(
    const std::map<std::string, std::string> &user_options,
    const std::map<std::string, std::string> &default_paths,
    AutoCleaner *auto_cleaner) const {
  mysql_harness::Path datadir_path;
  if (user_options.find("datadir") != std::end(user_options))
    datadir_path = mysql_harness::Path(user_options.at("datadir"));
  else
    datadir_path = mysql_harness::Path(default_paths.at("data_folder"));

  mysql_harness::Path ca_key_path =
      datadir_path.real_path().join(tls_filenames_.ca_key);
  mysql_harness::Path ca_cert_path =
      datadir_path.real_path().join(tls_filenames_.ca_cert);
  mysql_harness::Path router_key_path =
      datadir_path.real_path().join(tls_filenames_.router_key);
  mysql_harness::Path router_cert_path =
      datadir_path.real_path().join(tls_filenames_.router_cert);

  CertificateHandler cert_handler{ca_key_path, ca_cert_path, router_key_path,
                                  router_cert_path};

  if (cert_handler.no_cert_files_exists()) {
    auto_cleaner->add_file_delete(ca_key_path.str());
    auto_cleaner->add_file_delete(ca_cert_path.str());
    auto_cleaner->add_file_delete(router_key_path.str());
    auto_cleaner->add_file_delete(router_cert_path.str());

    auto res = cert_handler.create();
    if (!res) {
      const auto &error = res.error();
      throw std::runtime_error{error.message() + " (" +
                               std::to_string(error.value()) + ")"};
    }

    for (const auto &file :
         {ca_key_path, ca_cert_path, router_key_path, router_cert_path}) {
      mysql_harness::make_file_private(file.str());
      set_file_owner(user_options, file.str());
    }
  } else if (cert_handler.router_cert_files_exists()) {
    out_stream_ << "- Using existing certificates from the '"
                << datadir_path.real_path().str().c_str() << "' directory\n";
    return;
  } else {
    std::string missing_files;
    if (!router_key_path.exists()) missing_files += tls_filenames_.router_key;
    if (!missing_files.empty()) missing_files += ", ";
    if (!router_cert_path.exists()) missing_files += tls_filenames_.router_cert;
    throw std::runtime_error{mysql_harness::utility::string_format(
        "Missing certificate files in %s: '%s'. Please provide them or erase "
        "the existing certificate files and re-run bootstrap.",
        datadir_path.c_str(), missing_files.c_str())};
  }
}

namespace {

bool is_valid_conf_ident(const std::string &name) {
  if (name.empty()) return false;

  return std::all_of(name.begin(), name.end(),
                     mysql_harness::is_valid_conf_ident_char);
}

std::map<std::string, std::string> get_config_cmdln_options(
    const std::map<std::string, std::vector<std::string>> &multivalue_options) {
  if (multivalue_options.count("conf-set-option") == 0) return {};

  std::map<std::string, std::string> result;

  const auto &conf_options = multivalue_options.at("conf-set-option");
  for (const auto &option : conf_options) {
    const auto eq_pos = option.find_first_of('=');
    if (eq_pos == std::string::npos) {
      throw std::runtime_error("conf-set-option: invalid option '" + option +
                               "', should be section.option_name=value");
    }

    std::string option_id = option.substr(0, eq_pos);
    const std::string option_value = option.substr(eq_pos + 1);

    const auto dot = option_id.find('.');
    if (dot == std::string::npos) {
      throw std::runtime_error("conf-set-option: invalid option '" + option +
                               "', should be section.option_name=value");
    }

    const std::string section_name = option_id.substr(0, dot);
    bool section_name_valid{false};
    const auto colon = section_name.find(':');
    if (colon != std::string::npos) {
      std::string section = section_name.substr(0, colon);
      const std::string label =
          section_name.substr(colon + 1, section_name.length() - colon - 1);

      section_name_valid =
          is_valid_conf_ident(section) && is_valid_conf_ident(label);

      // label part is not allowed for DEFAULT section
      std::transform(section.begin(), section.end(), section.begin(),
                     ::tolower);
      if (section == "default") {
        throw std::runtime_error(
            "conf-set-option: DEFAULT section is not allowed to have a key: '" +
            section_name + "'");
      }
    } else {
      section_name_valid = is_valid_conf_ident(section_name);
    }

    if (!section_name_valid) {
      throw std::runtime_error("conf-set-option: invalid section name '" +
                               section_name + "'");
    }

    const std::string option_name =
        option_id.substr(dot + 1, option_id.length() - dot - 1);

    if (!is_valid_conf_ident(option_name)) {
      throw std::runtime_error("conf-set-option: invalid option name '" +
                               option_name + "'");
    }

    std::transform(option_id.begin(), option_id.end(), option_id.begin(),
                   ::tolower);

    if (result.count(option_id) > 0) {
      throw std::runtime_error("conf-set-option: duplicate value for option '" +
                               option_id + "'");
    }

    result[option_id] = option_value;
  }

  return result;
}

}  // namespace

std::string ConfigGenerator::bootstrap_deployment(
    const std::string &program_name, std::ostream &config_file,
    std::ostream &state_file, const mysql_harness::Path &config_file_path,
    const mysql_harness::Path &state_file_path, const std::string &router_name,
    const std::map<std::string, std::string> &user_options,
    const std::map<std::string, std::vector<std::string>> &multivalue_options,
    const std::map<std::string, std::string> &default_paths,
    bool directory_deployment, AutoCleaner &auto_clean) {
  bool force = user_options.find("force") != user_options.end();

  auto cluster_info = metadata_->fetch_metadata_servers();

  // get router_id and username from config and/or command-line
  auto conf_options = get_options_from_config_if_it_exists(
      config_file_path.str(), cluster_info, force);

  // if user provided --account, override username with it
  conf_options.username =
      get_from_map(user_options, "account"s, conf_options.username);

  // If username is still empty at this point, it will be autogenerated
  // inside try_bootstrap_deployment().  It cannot be done here, because the
  // autogenerated name will contain router_id, and that is still subject to
  // change inside try_bootstrap_deployment()
  print_bootstrap_start_msg(conf_options.router_id, directory_deployment,
                            config_file_path);

  Options options(fill_options(user_options, default_paths, conf_options));
  // Prompt for the Router's runtime account that's used by metadata_cache and
  // specified by "--account".
  // If running in --account mode, the user provides the password (ALWAYS,
  // regardless of if it's already available from keyring or not.  This hard
  // rule exists to make automation easier by asking const number of questions).
  // OTOH, if running without --account, the password is NEVER prompted for
  // (it's autogenerated or taken from keyring (we throw an error if missing,
  // but never prompt for it - again, the goal is automation-friendliness)
  std::string password;
  if (user_options.count("account"))
    password = prompt_password("Please enter MySQL password for " +
                               conf_options.username);
  else if (!conf_options.username.empty())
    password = fetch_password_from_keyring(conf_options.username,
                                           conf_options.router_id);

  // bootstrap
  // All SQL writes happen inside here
  {
    ClusterAwareDecorator cluster_aware(
        *metadata_, cluster_initial_username_, cluster_initial_password_,
        cluster_initial_hostname_, cluster_initial_port_,
        cluster_initial_socket_, connect_timeout_);

    // note: try_bootstrap_deployment() can update router_id, username and
    // password note: failover is performed only on specific errors (subset of
    // what appears in enum class MySQLErrorc)
    std::tie(password) =
        cluster_aware.failover_on_failure<std::tuple<std::string>>([&]() {
          return try_bootstrap_deployment(
              conf_options.router_id, conf_options.username, password,
              router_name, cluster_info, user_options, multivalue_options,
              options);
        });
  }

  // check if self-signed certs need to be generated:
  //
  // - REST API is enabled
  // - client-ssl-mode is neither PASSTHROUGH nor DISABLED
  // - client-ssl-key and client-cert are both not specified
  const bool with_rest_api = user_options.count("disable-rest") == 0;

  if (with_rest_api || ((options.client_ssl_mode != "PASSTHROUGH") &&
                        (options.client_ssl_mode != "DISABLED") &&
                        (get_opt(user_options, "client_ssl_cert", "") == "") &&
                        (get_opt(user_options, "client_ssl_key", "") == ""))) {
    prepare_ssl_certificate_files(user_options, default_paths, &auto_clean);
  }

  // test out the connection that Router would use
  {
    bool strict = user_options.count("strict");
    verify_router_account(conf_options.username, password, strict);
  }

  store_credentials_in_keyring(auto_clean, user_options, conf_options.router_id,
                               conf_options.username, password, options);
  set_log_file_permissions(default_paths, user_options, options);

  // generate the new config file
  {
    out_stream_ << "- Creating configuration " << config_file_path.str()
                << std::endl;
    auto system_username = get_from_map(user_options, "user"s, ""s);
    create_config(config_file, state_file, conf_options.router_id, router_name,
                  system_username, cluster_info, conf_options.username, options,
                  default_paths, get_config_cmdln_options(multivalue_options),
                  state_file_path.str());
  }

  // return bootstrap report (several lines of human-readable text)
  const std::string cluster_type_name = [](auto cluster_type) {
    switch (cluster_type) {
      case ClusterType::RS_V2:
        return "InnoDB ReplicaSet";
      case ClusterType::GR_CS:
        return "ClusterSet";
      default:
        return "InnoDB Cluster";
    }
  }(metadata_->get_type());

  return get_bootstrap_report_text(
      program_name, config_file_path.str(), router_name, cluster_info.name,
      cluster_type_name,
      get_from_map(user_options, "report-host"s, "localhost"s),
      !directory_deployment, options);
}

void ConfigGenerator::ensure_router_id_is_ours(
    uint32_t &router_id, const std::string &hostname_override) {
  // if router data is valid
  try {
    metadata_->verify_router_id_is_ours(router_id, hostname_override);
  } catch (
      const mysql_harness::SocketOperationsBase::LocalHostnameResolutionError
          &e) {
    throw std::runtime_error(
        "Could not verify if this Router instance is already "
        "registered with the "
        "cluster because querying this host's hostname from OS "
        "failed:\n  "s +
        e.what() +
        "\nYou may want to try --report-host option to manually supply this "
        "hostname.");
  } catch (const std::exception &e) {
    log_warning("WARNING: %s", e.what());
    // TODO: abort here and suggest --force to force reconfiguration?
    router_id = 0;
  }
}

uint32_t ConfigGenerator::register_router(const std::string &router_name,
                                          const std::string &hostname_override,
                                          bool force) {
  // register router
  uint32_t router_id;
  try {
    router_id =
        metadata_->register_router(router_name, force, hostname_override);
  } catch (
      const mysql_harness::SocketOperationsBase::LocalHostnameResolutionError
          &e) {
    throw std::runtime_error(
        "Could not register this Router instance with the cluster because "
        "querying this host's hostname from OS failed:\n  "s +
        e.what() +
        "\nYou may want to try --report-host option to manually supply this "
        "hostname.");
  } catch (const MySQLSession::Error &e) {
    if (e.code() == ER_DUP_ENTRY) {  // duplicate key
      throw std::runtime_error(
          "It appears that a router instance named '" + router_name +
          "' has been previously configured in this host. If that instance"
          " no longer exists, use the --force option to overwrite it.");
    }

    throw;
  }

  if (router_id > kMaxRouterId) {
    throw std::runtime_error("router_id (" + std::to_string(router_id) +
                             ") exceeded max allowable value (" +
                             std::to_string(kMaxRouterId) + ")");
  }

  return router_id;
}

static std::string generate_username(
    uint32_t router_id, mysql_harness::RandomGeneratorInterface &rg) {
  using RandomGen = mysql_harness::RandomGeneratorInterface;
  return "mysql_router" + std::to_string(router_id) + "_" +
         rg.generate_identifier(
             kNumRandomChars,
             RandomGen::AlphabetDigits | RandomGen::AlphabetLowercase);
}

/*static*/
std::set<std::string> ConfigGenerator::get_account_host_args(
    const std::map<std::string, std::vector<std::string>>
        &multivalue_options) noexcept {
  // We need to eliminate any non-unique --account-host entries (to ensure
  // CREATE USER does not get called twice for the same user@host).
  // Repackaging into std::set elegantly takes care of this need.
  std::set<std::string> account_hosts;
  constexpr const char kAccountHost[] = "account-host";
  if (multivalue_options.count(kAccountHost))
    for (const std::string &h : multivalue_options.at(kAccountHost))
      account_hosts.emplace(h);

  // if no --account-host was given, default to just one: '%'
  if (account_hosts.empty()) account_hosts.emplace("%");

  return account_hosts;
}

void ConfigGenerator::verify_router_account(const std::string &username,
                                            const std::string &password,
                                            bool strict) {
  out_stream_ << "- Verifying account (using it to run SQL queries that would "
                 "be run by Router)"
              << std::endl;

  auto failed_verification_handler = [this,
                                      strict](const std::runtime_error &e) {
    const std::string msg = R"(Account verification failed with error:
  )"s + e.what() +
                            R"(

This means that we were unable to log in using the accounts that were created
and run SQL queries that Router needs to run during its operation.
It means this Router instance may be inoperable and user intervention is
required to correct the issue and/or bootstrap again.

See https://dev.mysql.com/doc/mysql-router/8.0/en/ for more information.)";

    if (strict) {
      throw std::runtime_error(msg);
    } else {
      err_stream_ << "\n"
                  << Vt100::foreground(Vt100::Color::Red)
                  << "***** WARNING *****\n"
                  << Vt100::render(Vt100::Render::ForegroundDefault) << msg
                  << std::endl;
    }
  };

  auto run_sql_queries = [this](MySQLSession &rtr_acct_sess) {
    // no need to differentiate between SQL queries and statements, as both can
    // be called with mysql_real_query() (called inside MySQLSession::execute())
    const auto stmts = metadata_->get_routing_mode_queries();

    // we just call them (ignore the resultset) - all we care about is whether
    // they execute without error
    for (const std::string &s : stmts) {
      // throws MySQLSession::Error (std::runtime_error)
      rtr_acct_sess.execute(s);
    }
  };

  // Create a new connection that's just like the one we currently have, but
  // using Router's credentials, i.e. using the new account we just set up.
  // More precisely, we use new account's username - we have no control over
  // which hostname is used.  Therefore it may well be that the account with
  // appropriate hostname may not exist and we will not be able to log in.
  // That's ok, that's the main reason why we do this verification - to alert
  // the user when such condition exists.
  MySQLSession rtr_acct_sess;
  {
    try {
      // will throw if logging in using Router's credentials fails
      rtr_acct_sess.connect(*mysql_.get(), username, password);
    } catch (const MySQLSession::Error &e) {
      failed_verification_handler(e);
      return;
    }
  }

  // run through all the queries Router could run
  try {
    run_sql_queries(rtr_acct_sess);
  } catch (const MySQLSession::Error &e) {
    failed_verification_handler(e);
  }
}

static std::string get_target_cluster_value(
    const std::string &target_cluster_option,
    const std::string &target_cluster_by_name_option,
    const ClusterInfo &cluster_info) {
  if (!target_cluster_by_name_option.empty()) {
    return cluster_info.get_cluster_type_specific_id();
  }

  std::string option_lowercase{target_cluster_option};
  std::transform(option_lowercase.begin(), option_lowercase.end(),
                 option_lowercase.begin(), ::tolower);

  if (option_lowercase == "primary") return "primary";

  if (option_lowercase == "current") {
    if (cluster_info.is_primary) {
      // user wants current on the Primary cluster, let's issue an warning
      log_warning(
          "WARNING: Option --conf-target-cluster=current was used to bootstrap "
          "against the Primary Cluster. Note that the Router will not follow "
          "the new Primary Cluster in case of the Primary Cluster change in "
          "the ClusterSet");
    }

    return cluster_info.get_cluster_type_specific_id();
  }

  if (option_lowercase.empty()) {
    // neither --conf-target-cluster nor --conf-target-cluster-by-name was used
    return "";
  }

  harness_assert(option_lowercase == "primary");
  return "primary";
}

std::tuple<std::string> ConfigGenerator::try_bootstrap_deployment(
    uint32_t &router_id, std::string &username, std::string &password,
    const std::string &router_name, const ClusterInfo &cluster_info,
    const std::map<std::string, std::string> &user_options,
    const std::map<std::string, std::vector<std::string>> &multivalue_options,
    const Options &options) {
  MySQLSession::Transaction transaction(mysql_.get());

  // set hostname override if provided
  const auto &it = user_options.find("report-host");
  const std::string &hostname_override =
      (it != user_options.end()) ? it->second : "";

  // if reconfiguration
  if (router_id > 0) {
    // throws std::runtime if our hostname couldn't be queried,
    // resets router_id to 0 if router_id doesn't exist or
    // belongs to a different host.
    // NOTE that these were passed by reference to us, thus they are stored
    //      outside of this function and will be persisted to the next call.
    ensure_router_id_is_ours(router_id, hostname_override);
  }

  // if router not registered yet (or router_id was invalid)
  if (router_id == 0) {
    bool force = user_options.find("force") != user_options.end();
    router_id = register_router(router_name, hostname_override, force);
  }
  harness_assert(router_id > 0);

  // now that we got our router_id, we can finally autogenerate the username
  if (username.empty()) {
    // --account sets username
    harness_assert(!user_options.count("account"));

    using RandomGen = mysql_harness::RandomGeneratorInterface;
    RandomGen &rg = mysql_harness::DIM::instance().get_RandomGenerator();
    username = generate_username(router_id, rg);
  }

  const std::set<std::string> hostnames_cmd =
      get_account_host_args(multivalue_options);
  // If password is empty and running without --account, it will be
  // autogenerated and returned
  bool password_change_ok = !user_options.count("account");
  password = create_router_accounts(user_options, hostnames_cmd, username,
                                    password, password_change_ok);

  const std::string rw_endpoint = str(options.rw_endpoint);
  const std::string ro_endpoint = str(options.ro_endpoint);
  const std::string rw_x_endpoint = str(options.rw_x_endpoint);
  const std::string ro_x_endpoint = str(options.ro_x_endpoint);
  const std::string target_cluster =
      mysqlrouter::ClusterType::GR_CS == metadata_->get_type()
          ? get_target_cluster_value(options.target_cluster,
                                     options.target_cluster_by_name,
                                     cluster_info)
          : "";
  const std::string cluster_id =
      mysqlrouter::ClusterType::GR_CS == metadata_->get_type()
          ? cluster_specific_id_
          : cluster_info.cluster_id;

  metadata_->update_router_info(router_id, cluster_id, target_cluster,
                                rw_endpoint, ro_endpoint, rw_x_endpoint,
                                ro_x_endpoint, username);

  transaction.commit();

  return std::make_tuple(password);
}

void ConfigGenerator::set_keyring_info_real_paths(
    std::map<std::string, std::string> &options,
    const mysql_harness::Path &path) {
  keyring_info_.set_keyring_file(mysql_harness::Path(options["datadir"])
                                     .real_path()
                                     .join(keyring_info_.get_keyring_file())
                                     .str());
  keyring_info_.set_master_key_file(
      keyring_info_.get_master_key_file().empty()
          ? ""
          : path.real_path().join(keyring_info_.get_master_key_file()).str());
}

std::string ConfigGenerator::fetch_password_from_keyring(
    const std::string &username, uint32_t router_id) {
  out_stream_ << "- Fetching password for current account (" + username +
                     ") from keyring"
              << std::endl;

  try {
    init_keyring_file(router_id, false);
  } catch (const std::runtime_error &e) {
    throw std::runtime_error("Failed retrieving password for user '" +
                             username + "' from keyring: " + e.what());
  }

  try {
    return mysql_harness::get_keyring()->fetch(username,
                                               kKeyringAttributePassword);
  } catch (const std::out_of_range &) {
    throw std::runtime_error(R"(Failed retrieving password for user ')"s +
                             username + R"(' from keyring:

  Keyring was opened successfully, but it doesn't contain the password for
  user ')"s + username + R"('

See https://dev.mysql.com/doc/mysql-router/8.0/en/ for more information.
)"s);
  }
}

void ConfigGenerator::store_credentials_in_keyring(
    AutoCleaner &auto_clean,
    const std::map<std::string, std::string> &user_options, uint32_t router_id,
    const std::string &username, const std::string &password,
    Options &options) {
  out_stream_ << "- Storing account in keyring" << std::endl;
  init_keyring_and_master_key(auto_clean, user_options, router_id);

  mysql_harness::Keyring *keyring = mysql_harness::get_keyring();

  keyring->store(username, kKeyringAttributePassword, password);
  try {
    mysql_harness::flush_keyring();
  } catch (const std::exception &e) {
    throw std::runtime_error(
        std::string("Error storing encrypted password to disk: ") + e.what());
  }

  options.keyring_file_path = keyring_info_.get_keyring_file();
  if (keyring_info_.use_master_key_file())
    options.keyring_master_key_file_path = keyring_info_.get_master_key_file();
}

void ConfigGenerator::init_keyring_and_master_key(
    AutoCleaner &auto_clean,
    const std::map<std::string, std::string> &user_options,
    uint32_t router_id) {
  // buffer original master key file, it will be restored when bootstrap fails
  if (!keyring_info_.get_master_key_file().empty())
    auto_clean.add_file_revert(keyring_info_.get_master_key_file());

  // buffer original master key from external facility, it will be restored when
  // bootstrap fails
  if (keyring_info_.use_master_key_external_facility()) {
    // add ROUTER_ID to ENV
    keyring_info_.add_router_id_to_env(router_id);
    KeyringInfo keyring_info_copy(false);
    keyring_info_copy.set_master_key_reader(
        keyring_info_.get_master_key_reader());
    keyring_info_copy.set_master_key_writer(
        keyring_info_.get_master_key_writer());
    if (keyring_info_copy.read_master_key()) {
      auto_clean.add_cleanup_callback(
          [keyring_info_copy] { return keyring_info_copy.write_master_key(); });
    }
  }
  init_keyring_file(router_id);
  set_file_owner(user_options, keyring_info_.get_keyring_file());
  set_file_owner(user_options, keyring_info_.get_master_key_file());
}

void ConfigGenerator::init_keyring_file(uint32_t router_id,
                                        bool create_if_needed /*= true*/) {
  if (keyring_initialized_) return;

  if (keyring_info_.use_master_key_external_facility()) {
    if (!keyring_info_.read_master_key()) {
      throw MasterKeyWriteError(
          "Cannot fetch master key file using master key reader:" +
          keyring_info_.get_master_key_reader());
    }

    if (keyring_info_.get_master_key().empty()) {
      keyring_info_.add_router_id_to_env(router_id);
      keyring_info_.generate_master_key();
      if (!keyring_info_.write_master_key()) {
        throw MasterKeyWriteError(
            "Cannot write master key file using master key writer:" +
            keyring_info_.get_master_key_writer());
      }
    }
    mysql_harness::init_keyring_with_key(keyring_info_.get_keyring_file(),
                                         keyring_info_.get_master_key(),
                                         create_if_needed);
  } else if (keyring_info_.use_master_key_file()) {
    try {
      mysql_harness::init_keyring(keyring_info_.get_keyring_file(),
                                  keyring_info_.get_master_key_file(),
                                  create_if_needed);
    } catch (const mysql_harness::invalid_master_keyfile &) {
      throw mysql_harness::invalid_master_keyfile(
          "Invalid master key file " + keyring_info_.get_master_key_file());
    }
  } else {  // prompt for password
    std::string master_key;
#ifdef _WIN32
    // When no master key file is provided, console interaction is required to
    // provide a master password. Since console interaction is not available
    // when run as service, throw an error to abort.
    if (mysqlrouter::is_running_as_service()) {
      std::string msg =
          "Cannot run router in Windows a service without a master key file. "
          "Please run MySQL Router from the command line (instead of as a "
          "service) to create a master keyring file.";
      mysqlrouter::write_windows_event_log(msg);
      throw std::runtime_error(msg);
    }
#endif
    if (mysql_harness::Path(keyring_info_.get_keyring_file()).exists()) {
      master_key =
          prompt_password("Please provide the encryption key for key file at " +
                          keyring_info_.get_keyring_file());
      if (master_key.length() > mysql_harness::kMaxKeyringKeyLength)
        throw std::runtime_error("Encryption key is too long");
    } else {
      std::cout << "MySQL Router needs to create a InnoDB cluster metadata "
                   "client account.\n"
                   "To allow secure storage of its password, please provide an "
                   "encryption key.\n"
                << std::endl;
    again:
      master_key = prompt_password("Please provide an encryption key");
      if (master_key.empty()) {
        throw std::runtime_error("Keyring encryption key must not be blank");
      } else if (master_key.length() > mysql_harness::kMaxKeyringKeyLength) {
        throw std::runtime_error("Encryption key is too long");
      } else {
        std::string confirm = prompt_password("Please confirm encryption key");
        if (confirm != master_key) {
          std::cout << "Entered keys do not match. Please try again."
                    << std::endl;
          goto again;
        }
      }
    }
    mysql_harness::init_keyring_with_key(keyring_info_.get_keyring_file(),
                                         master_key, create_if_needed);
  }

  keyring_initialized_ = true;
}

#ifdef _WIN32
// This is only for Windows
static std::string find_plugin_path() {
  std::array<char, MAX_PATH> szPath;
  if (GetModuleFileName(NULL, szPath.data(), szPath.size()) != 0) {
    // bin/mysqlrouter/../../lib/
    auto p = mysql_harness::Path(szPath.data()).dirname().dirname();

    p.append("lib");

    return p.str();
  }
  throw std::logic_error("Could not find own installation directory");
}
#endif

namespace {

using mysql_harness::ConfigBuilder;
class ConfigSectionPrinter {
 public:
  ConfigSectionPrinter(
      std::ostream &ostream,
      const std::map<std::string, std::string> &config_cmdln_options,
      const std::string &section_name)
      : ostream_(ostream),
        config_cmdln_options_(config_cmdln_options),
        section_name_(section_name) {
    auto section_name_lc = section_name;
    std::transform(section_name_lc.begin(), section_name_lc.end(),
                   section_name_lc.begin(), ::tolower);

    used_sections_.insert(section_name_lc);
  }

  ConfigSectionPrinter &add_line(const std::string &key,
                                 const std::string &value) {
    std::string cmdln_option_key = section_name_ + "." + key;
    std::transform(cmdln_option_key.begin(), cmdln_option_key.end(),
                   cmdln_option_key.begin(), ::tolower);
    if (config_cmdln_options_.count(cmdln_option_key) > 0) {
      section_options_.push_back(
          {key, config_cmdln_options_.at(cmdln_option_key)});

      used_cmdln_options_.insert(key);
    } else if (!value.empty()) {
      section_options_.push_back({key, value});
    }

    return *this;
  }

  ~ConfigSectionPrinter() {
    // got through all the command line options for this section and see if
    // there are some that user provided and we did not use them yet, now is
    // time to add them to our section
    for (const auto &cmdln_option : config_cmdln_options_) {
      const auto &cmdln_option_key = cmdln_option.first;
      const auto dot = cmdln_option_key.find('.');
      if (dot == std::string::npos) continue;
      const std::string section = cmdln_option_key.substr(0, dot);

      std::string section_name_lowerc = section_name_;
      std::transform(section_name_lowerc.begin(), section_name_lowerc.end(),
                     section_name_lowerc.begin(), ::tolower);

      if (section != section_name_lowerc) continue;

      const std::string option =
          cmdln_option_key.substr(dot + 1, cmdln_option_key.length() - dot - 1);

      if (used_cmdln_options_.count(option) == 0)
        section_options_.push_back({option, cmdln_option.second});
    }

    ostream_ << ConfigBuilder::build_section(section_name_, section_options_);
  }

  static void add_remaining_sections(
      std::ostream &ostream,
      const std::map<std::string, std::string> &config_cmdln_options) {
    std::string current_section;
    std::vector<ConfigBuilder::kv_type> section_options;

    for (const auto &cmdln_option : config_cmdln_options) {
      const auto &cmdln_option_key = cmdln_option.first;
      const auto dot = cmdln_option_key.find('.');
      // that should be checked before
      assert(dot != std::string::npos);
      const std::string section_name = cmdln_option_key.substr(0, dot);
      if (used_sections_.count(section_name) > 0) {
        continue;
      }

      if (section_name != current_section) {
        if (!current_section.empty()) {
          ostream << ConfigBuilder::build_section(current_section,
                                                  section_options);
        }
        current_section = section_name;
        section_options.clear();
      }

      const std::string option =
          cmdln_option_key.substr(dot + 1, cmdln_option_key.length() - dot - 1);

      section_options.push_back({option, cmdln_option.second});
    }

    if (!current_section.empty()) {
      ostream << ConfigBuilder::build_section(current_section, section_options);
    }
  }

 private:
  std::ostream &ostream_;
  const std::map<std::string, std::string> &config_cmdln_options_;
  const std::string section_name_;

  std::vector<ConfigBuilder::kv_type> section_options_;

  std::set<std::string> used_cmdln_options_;
  static std::set<std::string> used_sections_;
};

/*static*/ std::set<std::string> ConfigSectionPrinter::used_sections_;

using mysql_harness::loader_supported_options;

#define ADD_CONFIG_LINE_CHECKED(section, option, value, supported_options)    \
  static_assert(mysql_harness::str_in_collection(supported_options, option)); \
  section.add_line(option, value);

void add_endpoint_option(ConfigSectionPrinter &routing_section,
                         const ConfigGenerator::Options &options,
                         const ConfigGenerator::Options::Endpoint &ep) {
  if (ep.port > 0) {
    const auto bind_address =
        (!options.bind_address.empty()) ? options.bind_address : "0.0.0.0";

    ADD_CONFIG_LINE_CHECKED(routing_section, "bind_address", bind_address,
                            routing_supported_options);
    ADD_CONFIG_LINE_CHECKED(routing_section, "bind_port",
                            std::to_string(ep.port), routing_supported_options);
  }

  if (!ep.socket.empty()) {
    const auto socket = options.socketsdir + "/" + ep.socket;
    ADD_CONFIG_LINE_CHECKED(routing_section, "socket", socket,
                            routing_supported_options);
  }
}

void add_metadata_cache_routing_section(
    std::ostream &config_file, bool is_classic, bool is_writable,
    const ConfigGenerator::Options::Endpoint endpoint,
    const ConfigGenerator::Options &options, const std::string &metadata_key,
    const std::string &cluster_name,
    const std::map<std::string, std::string> &config_cmdln_options) {
  if (!endpoint) return;

  const std::string key_suffix =
      std::string(is_classic ? "" : "_x") + (is_writable ? "_rw" : "_ro");
  const std::string role = is_writable ? "PRIMARY" : "SECONDARY";
  const std::string strategy =
      is_writable ? "first-available" : "round-robin-with-fallback";
  const std::string protocol = is_classic ? "classic" : "x";
  // kept for backward compatibility, always empty
  const std::string metadata_replicaset{""};

  ConfigSectionPrinter routing_section(config_file, config_cmdln_options,
                                       "routing:" + metadata_key + key_suffix);
  add_endpoint_option(routing_section, options, endpoint);
  const auto destinations = "metadata-cache://" + cluster_name + "/" +
                            metadata_replicaset + "?role=" + role;
  ADD_CONFIG_LINE_CHECKED(routing_section, "destinations", destinations,
                          routing_supported_options);
  ADD_CONFIG_LINE_CHECKED(routing_section, "routing_strategy", strategy,
                          routing_supported_options);
  ADD_CONFIG_LINE_CHECKED(routing_section, "protocol", protocol,
                          routing_supported_options);
}

/**
 * Add proper authentication backend section to the config based on the
 * metadata version. If needed it creates an empty authentication password
 * file used in the config.
 *
 * @param[in] config_file - output configuration file stream
 * @param[in] datadir - path of a router data directory
 * @param[in] auth_backend_name - authentication backend section name
 * @param[in] schema_version - metadata schema version
 * @param[in] config_cmdln_options - config options overwrites from the command
 * line
 *
 */
static void add_http_auth_backend_section(
    std::ostream &config_file, const mysql_harness::Path &datadir,
    const std::string_view auth_backend_name,
    const mysqlrouter::MetadataSchemaVersion schema_version,
    const std::map<std::string, std::string> &config_cmdln_options) {
  ConfigSectionPrinter http_backend_section{
      config_file, config_cmdln_options,
      "http_auth_backend:" + std::string(auth_backend_name)};
  if (metadata_schema_version_is_compatible(kNewMetadataVersion,
                                            schema_version)) {
    ADD_CONFIG_LINE_CHECKED(http_backend_section, "backend", "metadata_cache",
                            http_backend_supported_options);
  } else {
    const auto auth_backend_passwd_file =
        datadir.join("auth_backend_passwd_file").str();
    const auto open_res = open_ofstream(auth_backend_passwd_file);
    if (!open_res) {
      log_warning("Cannot create file '%s': %s",
                  auth_backend_passwd_file.c_str(),
                  open_res.error().message().c_str());
    }
    ADD_CONFIG_LINE_CHECKED(http_backend_section, "backend", "file",
                            http_backend_supported_options);
    ADD_CONFIG_LINE_CHECKED(http_backend_section, "filename",
                            auth_backend_passwd_file,
                            http_backend_supported_options);
  }
}

void add_rest_section(
    std::ostream &config_file, const ConfigGenerator::Options &options,
    const std::map<std::string, std::string> &default_paths,
    const std::map<std::string, std::string> &config_cmdln_options,
    const std::string &ssl_cert, const std::string &ssl_key,
    const mysqlrouter::MetadataSchemaVersion &schema_version) {
  std::stringstream config;
  const std::string auth_realm_name{"default_auth_realm"};
  const std::string auth_backend_name{"default_auth_backend"};

  mysql_harness::Path datadir_path;
  if (!options.override_datadir.empty())
    datadir_path = mysql_harness::Path(options.override_datadir);
  else
    datadir_path = mysql_harness::Path(default_paths.at("data_folder"));

  {
    ConfigSectionPrinter http_server_section(config_file, config_cmdln_options,
                                             "http_server");
    ADD_CONFIG_LINE_CHECKED(http_server_section, "port", options.https_port_str,
                            http_server_supported_options);
    ADD_CONFIG_LINE_CHECKED(http_server_section, "ssl", "1",
                            http_server_supported_options);
    ADD_CONFIG_LINE_CHECKED(http_server_section, "ssl_cert",
                            datadir_path.real_path().join(ssl_cert).str(),
                            http_server_supported_options);
    ADD_CONFIG_LINE_CHECKED(http_server_section, "ssl_key",
                            datadir_path.real_path().join(ssl_key).str(),
                            http_server_supported_options);
  }

  {
    ConfigSectionPrinter http_auth_realm_section(
        config_file, config_cmdln_options,
        "http_auth_realm:" + auth_realm_name);
    ADD_CONFIG_LINE_CHECKED(http_auth_realm_section, "backend",
                            auth_backend_name,
                            http_auth_realm_suported_options);
    ADD_CONFIG_LINE_CHECKED(http_auth_realm_section, "method", "basic",
                            http_auth_realm_suported_options);
    ADD_CONFIG_LINE_CHECKED(http_auth_realm_section, "name", "default_realm",
                            http_auth_realm_suported_options);
  }

  {
    ConfigSectionPrinter rest_router_section(config_file, config_cmdln_options,
                                             "rest_router");
    ADD_CONFIG_LINE_CHECKED(rest_router_section, "require_realm",
                            auth_realm_name, rest_plugin_supported_options);
  }

  {
    ConfigSectionPrinter rest_api_section(config_file, config_cmdln_options,
                                          "rest_api");
  }

  add_http_auth_backend_section(config_file, datadir_path, auth_backend_name,
                                schema_version, config_cmdln_options);

  {
    ConfigSectionPrinter rest_routing_section(config_file, config_cmdln_options,
                                              "rest_routing");
    ADD_CONFIG_LINE_CHECKED(rest_routing_section, "require_realm",
                            auth_realm_name, rest_plugin_supported_options);
  }

  {
    ConfigSectionPrinter rest_metadata_cache_section(
        config_file, config_cmdln_options, "rest_metadata_cache");
    ADD_CONFIG_LINE_CHECKED(rest_metadata_cache_section, "require_realm",
                            auth_realm_name, rest_plugin_supported_options);
  }
}

}  // namespace

static void save_initial_dynamic_state(
    std::ostream &state_stream, ClusterMetadata &cluster_metadata,
    const std::string &cluster_type_specific_id,
    const std::vector<std::string> &metadata_server_addresses) {
  // create dynamic state object
  using DynamicState = mysql_harness::DynamicState;
  DynamicState dynamic_state{""};
  // put metadata-caches section in it
  ClusterMetadataDynamicState mdc_dynamic_state(&dynamic_state,
                                                cluster_metadata.get_type());

  if (cluster_metadata.get_type() == mysqlrouter::ClusterType::GR_CS) {
    mdc_dynamic_state.set_clusterset_id(cluster_type_specific_id);
  } else {
    mdc_dynamic_state.set_cluster_type_specific_id(cluster_type_specific_id);
  }
  mdc_dynamic_state.set_metadata_servers(metadata_server_addresses);
  const auto view_id = cluster_metadata.get_view_id(cluster_type_specific_id);
  mdc_dynamic_state.set_view_id(view_id);
  // save to out stream
  mdc_dynamic_state.save(state_stream);
}

void ConfigGenerator::create_config(
    std::ostream &config_file, std::ostream &state_file, uint32_t router_id,
    const std::string &router_name, const std::string &system_username,
    const ClusterInfo &cluster_info, const std::string &username,
    const Options &options,
    const std::map<std::string, std::string> &default_paths,
    const std::map<std::string, std::string> &config_cmdln_options,
    const std::string &state_file_name) {
  config_file << "# File automatically generated during MySQL Router bootstrap"
              << "\n";

  {
    // Add the DEFAULT section options. For each check if Loader's checker
    // has them registered as allowed options.
    ConfigSectionPrinter default_section(config_file, config_cmdln_options,
                                         "DEFAULT");
    ADD_CONFIG_LINE_CHECKED(default_section, "name", router_name,
                            router_supported_options);
    ADD_CONFIG_LINE_CHECKED(default_section, "user", system_username,
                            router_supported_options);
    ADD_CONFIG_LINE_CHECKED(default_section, "logging_folder",
                            options.override_logdir, loader_supported_options);
    ADD_CONFIG_LINE_CHECKED(default_section, "runtime_folder",
                            options.override_rundir, loader_supported_options);
    ADD_CONFIG_LINE_CHECKED(default_section, "data_folder",
                            options.override_datadir, loader_supported_options);
    ADD_CONFIG_LINE_CHECKED(default_section, "keyring_path",
                            options.keyring_file_path,
                            router_supported_options);
    ADD_CONFIG_LINE_CHECKED(default_section, "master_key_path",
                            options.keyring_master_key_file_path,
                            router_supported_options);
    ADD_CONFIG_LINE_CHECKED(default_section, "master_key_reader",
                            keyring_info_.get_master_key_reader(),
                            router_supported_options);
    ADD_CONFIG_LINE_CHECKED(default_section, "master_key_writer",
                            keyring_info_.get_master_key_writer(),
                            router_supported_options);
    ADD_CONFIG_LINE_CHECKED(default_section, "connect_timeout",
                            std::to_string(connect_timeout_),
                            metadata_cache_supported_options);
    ADD_CONFIG_LINE_CHECKED(default_section, "read_timeout",
                            std::to_string(read_timeout_),
                            metadata_cache_supported_options);
    ADD_CONFIG_LINE_CHECKED(default_section, "dynamic_state", state_file_name,
                            router_supported_options);
    ADD_CONFIG_LINE_CHECKED(default_section, "client_ssl_cert",
                            options.client_ssl_cert, routing_supported_options);
    ADD_CONFIG_LINE_CHECKED(default_section, "client_ssl_key",
                            options.client_ssl_key, routing_supported_options);
    ADD_CONFIG_LINE_CHECKED(default_section, "client_ssl_cipher",
                            options.client_ssl_cipher,
                            routing_supported_options);
    ADD_CONFIG_LINE_CHECKED(default_section, "client_ssl_curves",
                            options.client_ssl_curves,
                            routing_supported_options);
    ADD_CONFIG_LINE_CHECKED(default_section, "client_ssl_mode",
                            options.client_ssl_mode, routing_supported_options);
    ADD_CONFIG_LINE_CHECKED(default_section, "client_ssl_dh_params",
                            options.client_ssl_dh_params,
                            routing_supported_options);
    ADD_CONFIG_LINE_CHECKED(default_section, "server_ssl_ca",
                            options.server_ssl_ca, routing_supported_options);
    ADD_CONFIG_LINE_CHECKED(default_section, "server_ssl_capath",
                            options.server_ssl_capath,
                            routing_supported_options);
    ADD_CONFIG_LINE_CHECKED(default_section, "server_ssl_crl",
                            options.server_ssl_crl, routing_supported_options);
    ADD_CONFIG_LINE_CHECKED(default_section, "server_ssl_crlpath",
                            options.server_ssl_crlpath,
                            routing_supported_options);
    ADD_CONFIG_LINE_CHECKED(default_section, "server_ssl_cipher",
                            options.server_ssl_cipher,
                            routing_supported_options);
    ADD_CONFIG_LINE_CHECKED(default_section, "server_ssl_curves",
                            options.server_ssl_curves,
                            routing_supported_options);
    ADD_CONFIG_LINE_CHECKED(default_section, "server_ssl_mode",
                            options.server_ssl_mode, routing_supported_options);
    ADD_CONFIG_LINE_CHECKED(default_section, "server_ssl_verify",
                            options.server_ssl_verify,
                            routing_supported_options);
    ADD_CONFIG_LINE_CHECKED(default_section, "unknown_config_option", "error",
                            loader_supported_options);
  }

  save_initial_dynamic_state(state_file, *metadata_.get(), cluster_specific_id_,
                             cluster_info.metadata_servers);

  {
    ConfigSectionPrinter logger_section(
        config_file, config_cmdln_options,
        mysql_harness::logging::kConfigSectionLogger);
    ADD_CONFIG_LINE_CHECKED(logger_section, "level", "INFO",
                            logger_supported_options);
    ADD_CONFIG_LINE_CHECKED(logger_section, "filename",
                            options.override_logfilename,
                            logger_supported_options);
  }

  {
    ConfigSectionPrinter metadata_cache_section(
        config_file, config_cmdln_options,
        "metadata_cache:" + kDefaultMetadataCacheSectionKey);

    ADD_CONFIG_LINE_CHECKED(metadata_cache_section, "cluster_type",
                            mysqlrouter::to_string(metadata_->get_type()),
                            metadata_cache_supported_options);
    ADD_CONFIG_LINE_CHECKED(metadata_cache_section, "router_id",
                            std::to_string(router_id),
                            metadata_cache_supported_options);
    ADD_CONFIG_LINE_CHECKED(metadata_cache_section, "user", username,
                            metadata_cache_supported_options);

    if (mysqlrouter::ClusterType::GR_CS != metadata_->get_type()) {
      ADD_CONFIG_LINE_CHECKED(metadata_cache_section, "metadata_cluster",
                              cluster_info.name,
                              metadata_cache_supported_options);
    }

    ADD_CONFIG_LINE_CHECKED(metadata_cache_section, "ttl",
                            mysqlrouter::ms_to_seconds_string(options.ttl),
                            metadata_cache_supported_options);
    ADD_CONFIG_LINE_CHECKED(
        metadata_cache_section, "auth_cache_ttl",
        mysqlrouter::ms_to_seconds_string(kDefaultAuthCacheTTL),
        metadata_cache_supported_options);

    const auto auth_cache_refresh_interval = mysqlrouter::ms_to_seconds_string(
        kDefaultAuthCacheRefreshInterval > options.ttl
            ? kDefaultAuthCacheRefreshInterval
            : options.ttl);
    ADD_CONFIG_LINE_CHECKED(
        metadata_cache_section, "auth_cache_refresh_interval",
        auth_cache_refresh_interval, metadata_cache_supported_options);

    if (mysqlrouter::ClusterType::RS_V2 != metadata_->get_type()) {
      const auto use_gr_notifications =
          options.use_gr_notifications ? "1" : "0";
      ADD_CONFIG_LINE_CHECKED(metadata_cache_section, "use_gr_notifications",
                              use_gr_notifications,
                              metadata_cache_supported_options);
    }

    ADD_CONFIG_LINE_CHECKED(metadata_cache_section, "ssl_mode",
                            options.ssl_options.mode,
                            metadata_cache_supported_options);
    ADD_CONFIG_LINE_CHECKED(metadata_cache_section, "ssl_cipher",
                            options.ssl_options.cipher,
                            metadata_cache_supported_options);
    ADD_CONFIG_LINE_CHECKED(metadata_cache_section, "tls_version",
                            options.ssl_options.tls_version,
                            metadata_cache_supported_options);
    ADD_CONFIG_LINE_CHECKED(metadata_cache_section, "ssl_ca",
                            options.ssl_options.ca,
                            metadata_cache_supported_options);
    ADD_CONFIG_LINE_CHECKED(metadata_cache_section, "ssl_capath",
                            options.ssl_options.capath,
                            metadata_cache_supported_options);
    ADD_CONFIG_LINE_CHECKED(metadata_cache_section, "ssl_crl",
                            options.ssl_options.crl,
                            metadata_cache_supported_options);
    ADD_CONFIG_LINE_CHECKED(metadata_cache_section, "ssl_crlpath",
                            options.ssl_options.crlpath,
                            metadata_cache_supported_options);
  }

  // Note: we don't write cert and key because
  // creating router accounts with REQUIRE X509 is not yet supported.
  // The cert and key options passed to bootstrap if for the bootstrap
  // connection itself.

  auto add_mdc_rt_sect = [&](bool is_classic, bool is_writable,
                             Options::Endpoint endpoint) {
    add_metadata_cache_routing_section(config_file, is_classic, is_writable,
                                       endpoint, options,
                                       kDefaultMetadataCacheSectionKey,
                                       cluster_info.name, config_cmdln_options);
  };
  add_mdc_rt_sect(true, true, options.rw_endpoint);
  add_mdc_rt_sect(true, false, options.ro_endpoint);
  add_mdc_rt_sect(false, true, options.rw_x_endpoint);
  add_mdc_rt_sect(false, false, options.ro_x_endpoint);

  if (!options.disable_rest) {
    add_rest_section(config_file, options, default_paths, config_cmdln_options,
                     tls_filenames_.router_cert, tls_filenames_.router_key,
                     schema_version_);
  }

  ConfigSectionPrinter::add_remaining_sections(config_file,
                                               config_cmdln_options);
  config_file.flush();
}

void ConfigGenerator::print_bootstrap_start_msg(
    uint32_t router_id, bool directory_deployment,
    const mysql_harness::Path &config_file_path) {
  std::string prefix;
  if (router_id > 0) {
    prefix = "# Reconfiguring";
  } else {
    prefix = "# Bootstrapping";
  }
  out_stream_ << Vt100::foreground(Vt100::Color::Yellow) << prefix;
  if (directory_deployment) {
    out_stream_ << " MySQL Router instance at '" << config_file_path.dirname()
                << "'...";
  } else {
    out_stream_ << " system MySQL Router instance...";
  }
  out_stream_ << Vt100::render(Vt100::Render::ForegroundDefault) << "\n"
              << std::endl;
}

std::string ConfigGenerator::get_bootstrap_report_text(
    const std::string &program_name, const std::string &config_file_name,
    const std::string &router_name, const std::string &metadata_cluster,
    const std::string &cluster_type_name, const std::string &hostname,
    bool is_system_deployment, const Options &options) {
  constexpr const char kPromptPrefix[]{
#ifdef _WIN32
      "> "
#else
      "$ "
#endif
  };

  std::stringstream ss;

  ss << "\n"
     << Vt100::foreground(Vt100::Color::Green) << "# MySQL Router "
     << ((router_name.empty() || router_name == kSystemRouterName)
             ? ""
             : "'" + router_name + "' ")
     << "configured for the " << cluster_type_name << " '"
     << metadata_cluster.c_str() << "'"
     << Vt100::render(Vt100::Render::ForegroundDefault) << "\n"
     << std::endl;

  ss << "After this MySQL Router has been started with the generated "
        "configuration"
     << "\n"
     << std::endl;
#ifdef _WIN32
  if (is_system_deployment) {
    ss << "    " << kPromptPrefix << "net start mysqlrouter"
       << "\n"
       << "or" << std::endl;
  }
#else
  if (is_system_deployment) {
    ss << "    " << kPromptPrefix << "/etc/init.d/mysqlrouter restart"
       << "\n"
       << "or" << std::endl;
    if (Path("/bin/systemctl").exists()) {
      ss << "    " << kPromptPrefix << "systemctl start mysqlrouter"
         << "\n"
         << "or" << std::endl;
    }
  }
#endif
  ss << "    " << kPromptPrefix << program_name << " -c " << config_file_name
     << "\n\n"
     << cluster_type_name << " '" << metadata_cluster
     << "' can be reached by connecting to:\n"
     << std::endl;

  auto dump_sockets = [&ss, &hostname](const std::string &section,
                                       const std::string &socketsdir,
                                       const Options::Endpoint &rw_endpoint,
                                       const Options::Endpoint &ro_endpoint) {
    if (rw_endpoint || ro_endpoint) {
      ss << "## " << section << "\n\n";
      if (rw_endpoint) {
        ss << "- Read/Write Connections: ";
        if (rw_endpoint.port > 0) {
          ss << hostname << ":" << rw_endpoint.port;
        }
        if (!rw_endpoint.socket.empty()) {
          if (rw_endpoint.port > 0) {
            ss << ", ";
          }
          ss << socketsdir << "/" << rw_endpoint.socket;
        }
        ss << std::endl;
      }

      if (ro_endpoint) {
        ss << "- Read/Only Connections:  ";
        if (ro_endpoint.port > 0) {
          ss << hostname << ":" << ro_endpoint.port;
        }
        if (!ro_endpoint.socket.empty()) {
          if (ro_endpoint.port > 0) {
            ss << ", ";
          }
          ss << socketsdir << "/" << ro_endpoint.socket;
        }

        ss << std::endl;
      }
      ss << std::endl;
    }
  };

  dump_sockets("MySQL Classic protocol", options.socketsdir,
               options.rw_endpoint, options.ro_endpoint);
  dump_sockets("MySQL X protocol", options.socketsdir, options.rw_x_endpoint,
               options.ro_x_endpoint);

  return ss.str();
}

/**
 * create account to be used by Router.
 *
 * `<host>` part of `<user>@<host>` will be %, unless user specified otherwise
 * using --account-host switch. Multiple --account-host switches are allowed.
 */
std::string ConfigGenerator::create_router_accounts(
    const std::map<std::string, std::string> &user_options,
    const std::set<std::string> &hostnames, const std::string &username,
    const std::string &password, bool password_change_ok) {
  /*
  Ideally, we create a single account for the specific host that the router is
  running on. But that has several problems in real world, including:
  - if you're configuring on localhost ref to metadata server, the router will
  think it's in localhost and thus it will need 2 accounts: user@localhost
  and user@public_ip... further, there could be more than 1 IP for the host,
  which (like lan IP, localhost, internet IP, VPN IP, IPv6 etc). We don't know
  which ones are needed, so either we need to automatically create all of
  those or have some very complicated and unreliable logic.
  - using hostname is not reliable, because not every place will have name
  resolution available
  - using IP (even if we can detect it correctly) will not work if IP is not
  static

  Summing up, '%' is the easy way to avoid these problems. But the decision
  ultimately belongs to the user.
  */

  bool if_not_exists;
  {
    const std::string ac =
        get_from_map(user_options, "account-create"s, "if-not-exists"s);
    if (ac == "never")
      return password;
    else if (ac == "if-not-exists" || !user_options.count("account"))
      if_not_exists = true;
    else if (ac == "always")
      if_not_exists = false;
    else
      harness_assert_this_should_not_execute();
  }

  // NOTE ON EXCEPTIONS:
  // create_accounts*() functions throw many things (see their descriptions)
  // - we let the higher-level logic deal with them when that happens.

  if (hostnames.size()) {
    // NOTE: it may update the password
    return create_accounts_with_compliant_password(
        user_options, username, hostnames, password, password_change_ok,
        if_not_exists);
  }

  return password;
}

std::string ConfigGenerator::create_accounts_with_compliant_password(
    const std::map<std::string, std::string> &user_options,
    const std::string &username, const std::set<std::string> &hostnames,
    const std::string &password, bool password_change_ok,
    const bool if_not_exists) {
  using RandomGen = mysql_harness::RandomGeneratorInterface;
  RandomGen &rg = mysql_harness::DIM::instance().get_RandomGenerator();

  const bool force_password_validation =
      user_options.find("force-password-validation") != user_options.end();
  std::string password_candidate;
  unsigned retries =
      get_password_retries(user_options);  // throws std::invalid_argument
  if (!force_password_validation) {
    // 1) Try to create an account using mysql_native_password with the hashed
    // password to avoid validate_password verification (hashing is done inside
    // create_accounts())
    password_candidate =
        password.empty() && password_change_ok
            ? rg.generate_strong_password(kMetadataServerPasswordLength)
            : password;
    try {
      // create_accounts() throws many things, see its description
      create_accounts(username, hostnames, password_candidate,
                      true /*hash password*/, if_not_exists);
      return password_candidate;
    } catch (const plugin_not_loaded &) {
      // fallback to 2)
    }
  }

  // 2) If 1) failed because of the missing mysql_native_password plugin,
  //    or "-force-password-validation" parameter has being used
  //    try to create an account using the password directly
  while (true) {
    password_candidate =
        password.empty() && password_change_ok
            ? rg.generate_strong_password(kMetadataServerPasswordLength)
            : password;

    try {
      // create_accounts() throws many things, see its description
      create_accounts(username, hostnames, password_candidate,
                      false /*hash password*/, if_not_exists);
      return password_candidate;
    } catch (const password_too_weak &e) {
      if (--retries == 0          // retries used up
          || !password.empty()    // \_ retrying is pointless b/c the password
          || !password_change_ok  // /  will be the same every time
      ) {
        // 3) If 2) failed issue an error suggesting the change to
        // validate_password rules
        std::stringstream err_msg;
        err_msg << "Error creating user account: " << e.what() << std::endl
                << " Try to decrease the validate_password rules and try the "
                   "operation again.";
        throw std::runtime_error(err_msg.str());
      }
      // generated password does not satisfy the current policy requirements.
      // we do our best to generate strong password but with the
      // validate_password plugin, the user can set very strong or unusual
      // requirements that we are not able to predict so we just retry several
      // times hoping to meet the requirements with the next generated
      // password.
      continue;
    }
  }

  harness_assert_this_should_not_execute();
}

/*static*/
void ConfigGenerator::throw_account_exists(const MySQLSession::Error &e,
                                           const std::string &username) {
  // clang-format off
  // Extract a list of accounts that are reported to already exist.
  //
  // We do this by parsing error message we got back from the Server.  In
  // English form, it looks like this:
  //
  //   ERROR 1396 (HY000): Operation CREATE USER failed for 'foo'@'host1','foo'@'host2'
  //
  // The message contains only the accounts that already exist, so it could
  // have been a result of:
  //
  //   CREATE USER 'foo'@'host1', 'foo'@'host2', 'foo'@'host3', 'foo'@'host4'
  //
  // if host3 and host4 did not exist yet.
  //
  // Note that on such failure, CREATE USER will not create host3 and host4.
  // clang-format on
  std::set<std::string> accounts;
  {
    std::string text = e.message();
    const std::regex re{mysql_->quote(username) + "@'.*?'"};
    std::smatch m;

    while (std::regex_search(text, m, re)) {
      accounts.insert(m[0]);
      text = m.suffix().str();
    }
  }
  if (accounts.empty())
    throw std::runtime_error(
        "Failed to parse error message returned by CREATE USER command: "s +
        e.what());

  // Build error message informing of existing accounts
  std::string msg = "Account(s) ";

  bool is_first{true};
  for (const std::string &a : accounts) {
    if (is_first) {
      is_first = false;
    } else {
      msg += ",";
    }
    msg += a;
  }
  msg +=
      " already exist(s). If this is expected, please rerun without "
      "`--account-create always`.";

  throw account_exists(msg);
}

void ConfigGenerator::create_users(const std::string &username,
                                   const std::set<std::string> &hostnames,
                                   const std::string &password,
                                   bool hash_password /*=false*/,
                                   bool if_not_exists /*=false*/) {
  harness_assert(hostnames.size());

  // build string containing account/auth list
  std::string accounts_with_auth;
  {
    const std::string auth_part =
        " IDENTIFIED "s +
        (hash_password ? "WITH mysql_native_password AS " : "BY ") +
        mysql_->quote(hash_password ? compute_password_hash(password)
                                    : password);

    const std::string quoted_username = mysql_->quote(username);
    bool is_first{true};
    for (const std::string &h : hostnames) {
      if (is_first) {
        is_first = false;
      } else {
        accounts_with_auth += ",";
      }
      accounts_with_auth +=
          quoted_username + "@" + mysql_->quote(h) + auth_part;
    }
  }

  try {
    mysql_->execute(
        "CREATE USER "s + (if_not_exists ? "IF NOT EXISTS " : "") +
        accounts_with_auth);  // throws MySQLSession::Error, std::logic_error
  } catch (const MySQLSession::Error &e) {
    // log_error("%s: executing query: %s", e.what(), s.c_str());
    try {
      mysql_->execute("ROLLBACK");
    } catch (...) {
      // log_error("Could not rollback transaction explicitly.");
    }
    std::string err_msg =
        std::string(
            "Error creating MySQL account for router (CREATE USER stage): ") +
        e.what();
    if (e.code() == ER_NOT_VALID_PASSWORD) {  // password does not satisfy the
                                              // current policy requirements
      throw password_too_weak(err_msg);
    }
    if (e.code() == ER_PLUGIN_IS_NOT_LOADED) {  // auth plugin not loaded
      throw plugin_not_loaded(err_msg);
    }
    if (e.code() == ER_CANNOT_USER) {  // user already exists
      // // this should only happen when running with --account-create always,
      // // which sets if_not_exists to false
      // harness_assert(!if_not_exists);

      throw_account_exists(e, username);
    }

    // it shouldn't have failed, let the upper layers try to handle it
    throw MySQLSession::Error(err_msg, e.code());
  }
}

std::set<std::string> ConfigGenerator::get_hostnames_of_created_accounts(
    const std::string &username, const std::set<std::string> &hostnames,
    bool if_not_exists) {
  harness_assert(hostnames.size());

  // when running with IF NOT EXISTS, a warning will be produced for every
  // account that already exists.  We want to continue setup only for those
  // that don't.  Also, we need to save this list in case we need to revert
  // setup due to some errors later on.
  std::set<std::string> new_hostnames;  // if/else blocks will populate it
  if (if_not_exists && mysql_->warning_count() > 0) {
    // example response
    // clang-format off
    // +-------+------+---------------------------------------------+
    // | Level | Code | Message                                     |
    // +-------+------+---------------------------------------------+
    // | Note  | 3163 | Authorization ID 'bla'@'h1' already exists. |
    // | Note  | 3163 | Authorization ID 'bla'@'h3' already exists. |
    // +-------+------+---------------------------------------------+
    // clang-format on

    auto validator = [](unsigned num_fields, MYSQL_FIELD *fields) {
      if (num_fields != 3) {
        throw std::runtime_error(
            "SHOW WARNINGS: Unexpected number of fields in the resultset. "
            "Expected = 3, got = " +
            std::to_string(num_fields));
      }

      auto verify_column_name = [fields](unsigned idx,
                                         const std::string &expected) {
        if (fields[idx].name != expected)
          throw std::runtime_error(
              "SHOW WARNINGS: Unexpected column " + std::to_string(idx + 1) +
              " name '" + fields[idx].name + "', expected '" + expected + "'");
      };
      verify_column_name(0, "Level");
      verify_column_name(1, "Code");
      verify_column_name(2, "Message");
    };

    // start off with a full list, and we'll subtract existing hostnames from it
    new_hostnames = hostnames;

    const std::regex re{" '" + username + "'@'(.*?)' "};
    auto processor = [&](const MySQLSession::Row &row) -> bool {
      // we ignore warnings we're not expecting
      unsigned long code;
      try {
        size_t end_pos{};
        code = std::stoul(row[1], &end_pos);

        if (end_pos != strlen(row[1])) {
          throw std::invalid_argument(std::string(row[1]) +
                                      " is expected to be an positive integer");
        }
      } catch (const std::exception &e) {
        throw std::runtime_error(
            "SHOW WARNINGS: Failed to parse error code from error code column (column content = '"s +
            row[1] + "'): " + e.what());
      }
      if (code != ER_USER_ALREADY_EXISTS) {
        return true;  // true = give me another row
      }

      // extract the hostname from the warning message, and erase it from
      // new_hostnames
      const char *msg = row[2];
      std::cmatch m;
      if (std::regex_search(msg, m, re)) {
        if (!new_hostnames.erase(m[1].str())) {
          throw std::runtime_error("SHOW WARNINGS: Unexpected account name '" +
                                   username + "'@'" + m[1].str() +
                                   "' in message \""s + msg + "\"");
        }
      } else {
        throw std::runtime_error(
            "SHOW WARNINGS: Failed to extract account name ('" + username +
            "'@'<anything>') from message \""s + msg + "\"");
      }

      return true;  // true = give me another row
    };

    try {
      mysql_->query("SHOW WARNINGS", processor, validator);
    } catch (const MySQLSession::Error &e) {
      // log_error("%s: executing query: %s", e.what(), s.c_str());
      try {
        mysql_->execute("ROLLBACK");
      } catch (...) {
        // log_error("Could not rollback transaction explicitly.");
      }

      // it shouldn't have failed, let the upper layers try to handle it
      std::string err_msg = std::string(
                                "Error creating MySQL account for router (SHOW "
                                "WARNINGS stage): ") +
                            e.what();
      throw std::runtime_error(err_msg);
    }
  } else {
    // nothing special here - accounts for all hostnames were created
    // successfully, therefore all are new
    new_hostnames = hostnames;
  }

  return new_hostnames;
}

void ConfigGenerator::give_grants_to_users(const std::string &new_accounts) {
  // give GRANTs to new accounts
  if (!new_accounts.empty()) {
    // run GRANT stantements
    const std::vector<std::string> statements =
        metadata_->get_grant_statements(new_accounts);
    for (const auto &s : statements) {
      try {
        mysql_->execute(s);  // throws MySQLSession::Error, std::logic_error
      } catch (const MySQLSession::Error &e) {
        // log_error("%s: executing query: %s", e.what(), s.c_str());
        try {
          mysql_->execute("ROLLBACK");
        } catch (...) {
          // log_error("Could not rollback transaction explicitly.");
        }

        // we throw such that fail-over WILL NOT work.  Since CREATE USER
        // already succeeded, we can't simply go over to next node and start
        // over because the state of the next node is uncertain due to
        // replication syncing the effect of CREATE USER that already succeeded.
        std::string err_msg =
            std::string(
                "Error creating MySQL account for router (GRANTs stage): ") +
            e.what();
        throw std::runtime_error(err_msg);
      }
    }
  }
}

std::string ConfigGenerator::make_account_list(
    const std::string username, const std::set<std::string> &hostnames) {
  std::string account_list;
  for (const std::string &h : hostnames) {
    if (!account_list.empty()) {
      account_list += ",";
    }
    account_list += mysql_->quote(username) + "@" + mysql_->quote(h);
  }
  return account_list;
}

/*
  Create MySQL account for this instance of the router in the target cluster.

  The account will have access to the cluster metadata and to the
  replication_group_members table of the performance_schema.
  Note that this assumes that the metadata schema is stored in the
  destinations cluster and that there is only one replicaset in it.
 */
void ConfigGenerator::create_accounts(const std::string &username,
                                      const std::set<std::string> &hostnames,
                                      const std::string &password,
                                      bool hash_password /*=false*/,
                                      bool if_not_exists /*=false*/) {
  harness_assert(hostnames.size());
  harness_assert(undo_create_account_list_.type ==
                 UndoCreateAccountList::kNotSet);

  out_stream_ << "- Creating account(s) "
              << (if_not_exists ? "(only those that are needed, if any)" : "")
              << std::endl;

  // when this throws, it may trigger failover (depends on what exception it
  // throws)
  create_users(username, hostnames, password, hash_password, if_not_exists);

  // Now that we created users, we can no longer fail-over on subsequent
  // errors, because that write operation may automatically get propagated to
  // other nodes.  If we were to fail-over to another node and start over from
  // scratch, our writes (CREATE USER in this case) would be in conflict with
  // the writes coming through database replication mechanism.
  // All subsequent failures bypass fail-over and trigger bootstrap exit for
  // this reason.

  // save the list of all accounts, so it can be used to clean up the accounts
  // we just created, in case something later fails.  Saving the list of JUST
  // NEW accounts would be better (and we do that later), but in the meantime if
  // determining new accounts fails, at least we'll have a list of all accounts
  // that went into CREATE USER [IF NOT EXISTS] statement
  undo_create_account_list_ = {UndoCreateAccountList::kAllAccounts,
                               make_account_list(username, hostnames)};

  // determine which of the accounts we ran in CREATE USER... statement did not
  // exist before
  const std::set<std::string> new_hostnames =
      get_hostnames_of_created_accounts(username, hostnames, if_not_exists);
  const std::string new_accounts =
      new_hostnames.empty() ? "" : make_account_list(username, new_hostnames);

  // if we made it here, we managed to get a list of JUST NEW accounts that got
  // created.  This is more useful than the previous list of ALL accounts, so
  // let's replace it with this new better list.
  undo_create_account_list_ = {UndoCreateAccountList::kNewAccounts,
                               new_accounts};

  // proceed to giving grants
  give_grants_to_users(new_accounts);
}

void ConfigGenerator::undo_create_user_for_new_accounts() noexcept {
  try {  // need to guarantee noexcept

    switch (undo_create_account_list_.type) {
      case UndoCreateAccountList::kNotSet:
        // we didn't get around to creating accounts yet -> nothing to do
        return;
      case UndoCreateAccountList::kAllAccounts:
        // fallthrough
      case UndoCreateAccountList::kNewAccounts:
        if (undo_create_account_list_.accounts.empty()) {
          // even if we created some accounts, none of them were new -> nothing
          // to do
          return;
        }
    };

    err_stream_ << "FATAL ERROR ENCOUNTERED, attempting to undo new accounts "
                   "that were created"
                << std::endl;

    // shorter name
    const std::string &account_list = undo_create_account_list_.accounts;

    if (undo_create_account_list_.type == UndoCreateAccountList::kAllAccounts) {
      // we successfully ran CREATE USER [IF NOT EXISTS] on requested
      // accounts, but determining which of them were new (via SHOW WARNINGS)
      // failed.

      err_stream_
          << "\n"
          << Vt100::foreground(Vt100::Color::Red)
          << "ERROR: " << Vt100::render(Vt100::Render::ForegroundDefault)
          << R"(We created account(s), of which at least one already existed.
A fatal error occurred while we tried to determine which account(s) were new,
therefore to be safe, we did not erase any accounts while cleaning-up before
exiting.
You may want to clean those up yourself, if you deem it appropriate.
Here's a full list of accounts that bootstrap tried to create (some of which
might have already existed before bootstrapping):

  )"s << account_list
          << std::endl;
    } else {
      harness_assert(undo_create_account_list_.type ==
                     UndoCreateAccountList::kNewAccounts);
      // we successfully ran CREATES USER [IF NOT EXISTS] on requested
      // accounts, and we have the (undo) list of which ones were new

      // build DROP USER statement to erase all existing accounts
      std::string query = "DROP USER IF EXISTS " + account_list;

      auto handle_error = [this, &account_list](const std::exception &e) {
        err_stream_ << "\n"
                    << Vt100::foreground(Vt100::Color::Red) << "ERROR: "
                    << Vt100::render(Vt100::Render::ForegroundDefault) <<
            R"(As part of cleanup after bootstrap failure, we tried to erase account(s)
that we created.  Unfortunately the cleanup failed with error:

  )"s << e.what() << R"(
You may want to clean up the accounts yourself, here is the full list of
accounts that were created:
  )"s << account_list
                    << std::endl;

        log_error("Undoing creating new users failed: %s", e.what());
      };

      // since we're running this code as result of prior errors, we can't
      // really do anything about new exceptions, except to advise user.
      try {
        mysql_->execute(query);
        err_stream_ << "- New accounts cleaned up successfully" << std::endl;
      } catch (const MySQLSession::Error &e) {
        handle_error(e);
      } catch (const std::logic_error &e) {
        handle_error(e);
      }
    }
  } catch (...) {
  }
}

uint16_t get_x_protocol_port(const mysql_harness::Config &conf,
                             const std::string &role) {
  if (!conf.has_any("routing")) return 0;

  const auto &routing_sections = conf.get("routing");
  for (const auto &section : routing_sections) {
    if (!section->has("protocol")) continue;
    if (section->get("protocol") != "x") continue;

    if (!section->has("destinations")) continue;
    try {
      URI uri(section->get("destinations"), false);
      const auto &conf_role = uri.query.find("role");
      if (conf_role == uri.query.end()) continue;

      if (conf_role->second != role) continue;
    } catch (const URIError &) {
      continue;
    }

    if (!section->has("bind_port")) continue;
    const std::string bind_port_str = section->get("bind_port");

    try {
      return get_tcp_port(bind_port_str);
    } catch (const std::exception &) {
      log_warning("get_x_protocol_port: invalid x port = '%s' in section '%s'",
                  bind_port_str.c_str(), section->get_section_name().c_str());
      continue;
    }
  }

  return 0;
}

namespace {

std::string get_cluster_type_specific_uuid(
    const mysql_harness::Config &conf, mysqlrouter::ClusterType cluster_type) {
  if (!conf.has_default("dynamic_state")) {
    return "";
  }
  const std::string dynamic_state_file = conf.get_default("dynamic_state");

  mysql_harness::DynamicState dynamic_state{dynamic_state_file};
  ClusterMetadataDynamicState mdc_dynamic_state(&dynamic_state, cluster_type);
  try {
    mdc_dynamic_state.load();
  } catch (...) {
    return "";
  }

  if (cluster_type == mysqlrouter::ClusterType::GR_CS) {
    return mdc_dynamic_state.get_clusterset_id();
  } else {
    return mdc_dynamic_state.get_cluster_type_specific_id();
  }
}

}  // namespace

/**
 * Get selected configuration options from the existing Router configuration
 * file.
 **
 * ---
 * This function returns Router's id and SQL user for cluster `cluster_name`,
 * if it finds them in config file and they seem valid.  If they're missing or
 * incomplete, function will return empty values for them (0 or ""), giving a
 * green light to generate new ones.  Finally, if it detects that the cluster
 * name is off or missing, it will throw or return {0, ""}, depending on
 * `forcing_overwrite`.
 *
 * @param config_file_path /path/to/config/file
 * @param cluster_info Information about the Cluster for which Router id and
 *                     user should be returned
 * @param forcing_overwrite Action to take on unexpected cluster in config, see
 *                          function description
 *
 * @throws std::runtime_error on invalid router_id or metadata_cluster
 *
 * @returns Struct with options from the existing configuration file
 */
ConfigGenerator::ExistingConfigOptions
ConfigGenerator::get_options_from_config_if_it_exists(
    const std::string &config_file_path,
    const mysqlrouter::ClusterInfo &cluster_info, bool forcing_overwrite) {
  ConfigGenerator::ExistingConfigOptions result;

  // no config
  mysql_harness::Path path(config_file_path);
  if (!path.exists()) return result;

  // no [metadata_cache]
  mysql_harness::Config config(mysql_harness::Config::allow_keys);
  config.read(path);
  if (!config.has_any("metadata_cache")) return result;

  // grab [metadata_cache], we only allow 1 for now
  mysql_harness::Config::SectionList sections = config.get("metadata_cache");
  if (sections.size() > 1) {
    throw std::runtime_error(
        "Bootstrapping of Router with multiple metadata_cache sections not "
        "supported");
  }

  bool cluster_verified{false};
  // get uuid from the dynamic configuration file
  const auto uuid =
      get_cluster_type_specific_uuid(config, metadata_->get_type());
  if (!uuid.empty()) {
    if (!forcing_overwrite && cluster_info.cluster_type_specific_id != uuid) {
      throw std::runtime_error(
          "The given Router instance is already configured for a cluster with "
          "UUID '" +
          uuid + "'.\n" +
          "If you'd like to replace it, please use the --force configuration "
          "option.");
    }
    cluster_verified = true;
  }

  // We already checked that there is a single metadata_cache secion in the
  // configuration file so this loop runs exactly once
  for (auto const &section : sections) {
    if (!section->has("metadata_cluster")) {
      break;
    }

    if (!cluster_verified) {
      const std::string existing_cluster_name =
          section->get("metadata_cluster");
      if (existing_cluster_name != cluster_info.name) {
        if (!result.valid && !forcing_overwrite) {
          // it means that config exists, [metadata_cache] exists,
          // but [metadata_cache].metadata_cluster does not exist or it's
          // different from `cluster_name`.
          throw std::runtime_error(
              "The given Router instance is already configured for a cluster "
              "named '" +
              existing_cluster_name +
              "'.\nIf you'd like to replace it, please use the --force "
              "configuration option.");
        }
      }
    }

    // get router_id
    if (section->has("router_id")) {
      std::string tmp = section->get("router_id");
      char *end;
      result.router_id = std::strtoul(tmp.c_str(), &end, 10);
      if (end == tmp.c_str() || errno == ERANGE) {
        throw std::runtime_error(
            "Invalid router_id in the existing config file " +
            config_file_path);
      }
    } else {
      result.router_id = 0;
      log_warning("WARNING: router_id not set in the existing config file: %s",
                  config_file_path.c_str());
    }

    // get username, example: user=mysql_router4_kot8tcepf3kn
    if (section->has("user"))
      result.username = section->get("user");
    else
      log_warning(
          "WARNING: No metadata user found in the existinf config file %s",
          config_file_path.c_str());

    result.valid = true;
  }

  result.rw_x_port = get_x_protocol_port(config, "PRIMARY");
  result.ro_x_port = get_x_protocol_port(config, "SECONDARY");

  return result;
}

/* virtual */
void ConfigGenerator::set_script_permissions(
    const std::string &script_path,
    const std::map<std::string, std::string> &options) {
// we only call this method from unix-specific code
#ifdef _WIN32
  UNREFERENCED_PARAMETER(script_path);
  UNREFERENCED_PARAMETER(options);
#else
  if (::chmod(script_path.c_str(), kStrictDirectoryPerm) < 0) {
    std::error_code ec{errno, std::generic_category()};

    std::cerr << "Could not change permissions for " << script_path << ": "
              << ec.message() << "\n";
  }
  set_file_owner(options, script_path);
#endif
}

void ConfigGenerator::create_start_script(
    const std::string &program_name, const std::string &directory,
    bool interactive_master_key,
    const std::map<std::string, std::string> &options) {
#ifdef _WIN32
  UNREFERENCED_PARAMETER(interactive_master_key);
  UNREFERENCED_PARAMETER(options);

  std::ofstream script;
  std::string script_path = directory + "/start.ps1";

  script.open(script_path);
  if (script.fail()) {
    std::error_code ec{errno, std::generic_category()};
    throw std::system_error(ec,
                            "Could not open " + script_path + " for writing");
  }
  script << "$env:path += \";" << find_plugin_path() << "\"" << std::endl;
  script << "[Environment]::SetEnvironmentVariable(\"ROUTER_PID\","
         << "\"" << directory << "\\"
         << "mysqlrouter.pid\", \"Process\")" << std::endl;
  script << "Start-Process \""
         << mysqlrouter::find_full_executable_path(program_name) << "\" \" -c "
         << directory << "/mysqlrouter.conf\""
         << " -WindowStyle Hidden" << std::endl;
  script.close();

#else

  std::ofstream script;
  std::string script_path = directory + "/start.sh";

  script.open(script_path);
  if (script.fail()) {
    std::error_code ec{errno, std::generic_category()};
    throw std::system_error(ec,
                            "Could not open " + script_path + " for writing");
  }
  script << "#!/bin/bash\n";
  script << "basedir=" << directory << "\n";
  if (interactive_master_key) {
    // prompt for password if master_key_path is not set
    script << "old_stty=`stty -g`\n";
    script << "stty -echo\n";
    script << "echo -n 'Encryption key for router keyring:'\n";
    script << "read password\n";
    script << "stty $old_stty\n";
    script << "echo $password | ";
  }

  // Router launch command
  {
    std::string main_cmd =
        "ROUTER_PID=$basedir/mysqlrouter.pid " +
        mysqlrouter::find_full_executable_path(program_name) +
        " -c $basedir/mysqlrouter.conf ";

    if (options.find("user") != options.end()) {
      // if --user was given, we use it to generate shell code that works for
      // both cases:
      //   - when owner runs this script -> run without sudo and --user
      //   - when someone else runs it   -> run with    sudo and --user

      const std::string owner_name = options.at("user");
      harness_assert(!owner_name.empty());

      script << "if [ `whoami` == '" << owner_name
             << "' ]; then"
                "\n"
             << "  " << main_cmd
             << "&"
                "\n"
             << "else"
                "\n"
             << "  "
             << "sudo " << main_cmd << "--user=" << owner_name
             << " &"
                "\n"
             << "fi"
                "\n";
    } else {
      // if --user was not given, we have no choice but to only provide the
      // code for that case
      script << main_cmd << "&\n";
    }
  }

  script << "disown %-\n";
  script.close();

  set_script_permissions(script_path, options);

#endif  // #ifdef _WIN32
}

void ConfigGenerator::create_stop_script(
    const std::string &directory,
    const std::map<std::string, std::string> &options) {
#ifdef _WIN32
  UNREFERENCED_PARAMETER(options);

  std::ofstream script;
  const std::string script_path = directory + "/stop.ps1";

  script.open(script_path);
  if (script.fail()) {
    std::error_code ec{errno, std::generic_category()};
    throw std::system_error(ec,
                            "Could not open " + script_path + " for writing");
  }
  script << "$filename = [Environment]::GetEnvironmentVariable(\"ROUTER_PID\", "
            "\"Process\")"
         << std::endl;
  script << "If(Test-Path $filename) {" << std::endl;
  script << "  $mypid = [IO.File]::ReadAllText($filename)" << std::endl;
  script << "  Stop-Process -Id $mypid" << std::endl;
  script << "  [IO.File]::Delete($filename)" << std::endl;
  script << "}" << std::endl;
  script << "else { Write-Host \"Error when trying to stop mysqlrouter "
            "process\" }"
         << std::endl;
  script.close();

#else

  std::ofstream script;
  const std::string script_path = directory + "/stop.sh";

  script.open(script_path);
  if (script.fail()) {
    std::error_code ec{errno, std::generic_category()};
    throw std::system_error(ec,
                            "Could not open " + script_path + " for writing");
  }
  script << "#!/bin/bash\n";
  script << "if [ -f " + directory + "/mysqlrouter.pid ]; then\n";
  script << "  kill -TERM `cat " + directory + "/mysqlrouter.pid` && rm -f "
         << directory + "/mysqlrouter.pid\n";
  script << "fi\n";
  script.close();

  set_script_permissions(script_path, options);

#endif  // #ifdef _WIN32
}

static bool files_equal(const std::string &f1, const std::string &f2) {
  std::ifstream if1(f1);
  std::ifstream if2(f2);

  if1.seekg(0, if1.end);
  std::streamoff fsize = if1.tellg();
  if1.seekg(0, if1.beg);

  if2.seekg(0, if2.end);
  if (fsize != if2.tellg()) return false;
  if2.seekg(0, if2.beg);

  std::string data1, data2;
  data1.resize(static_cast<size_t>(fsize));
  data2.resize(static_cast<size_t>(fsize));

  if1.read(&data1[0], static_cast<std::streamsize>(fsize));
  if2.read(&data2[0], static_cast<std::streamsize>(fsize));

  return data1 == data2;
}

bool ConfigGenerator::backup_config_file_if_different(
    const mysql_harness::Path &config_path, const std::string &new_file_path,
    const std::map<std::string, std::string> &options,
    AutoCleaner *auto_cleaner) {
  if (config_path.exists() && config_path.is_regular()) {
    // if the old and new config files are the same, don't bother with a
    // backup
    if (!files_equal(config_path.str(), new_file_path)) {
      std::string backup_file_name = config_path.str() + ".bak";
      if (auto_cleaner) {
        auto_cleaner->add_file_revert(config_path.str(), backup_file_name);
      } else {
        copy_file(config_path.str(), backup_file_name);
      }
      try {
        mysql_harness::make_file_private(backup_file_name);
      } catch (const std::system_error &e) {
#ifdef _WIN32
        if (e.code() !=
            std::error_code(ERROR_INVALID_FUNCTION, std::system_category()))
        // if the filesystem can't set permissions, the test later would fail
#endif
          throw;
      }
      set_file_owner(options, backup_file_name);
      return true;
    }
  }
  return false;
}

void ConfigGenerator::set_file_owner(
    const std::map<std::string, std::string> &options,
    const std::string &file_path) const {
#ifdef _WIN32
  UNREFERENCED_PARAMETER(options);
  UNREFERENCED_PARAMETER(file_path);
#else
  bool change_owner =
      (options.count("user") != 0) && (!options.at("user").empty());
  if (change_owner) {
    auto username = options.at("user");
    auto user_info = check_user(username, true, sys_user_operations_);
    if (user_info != nullptr) {
      mysqlrouter::set_owner_if_file_exists(file_path, username, user_info,
                                            sys_user_operations_);
    }
  }
#endif
}
