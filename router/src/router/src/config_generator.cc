/*
  Copyright (c) 2016, 2019, Oracle and/or its affiliates. All rights reserved.

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
#include <sstream>
#include <stdexcept>

#include <rapidjson/rapidjson.h>
#include "common.h"
#include "dim.h"
#include "harness_assert.h"
#include "keyring/keyring_manager.h"
#include "mysql/harness/config_parser.h"
#include "mysql/harness/dynamic_state.h"
#include "mysql/harness/filesystem.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/vt100.h"
#include "mysqld_error.h"
#include "mysqlrouter/sha1.h"
#include "mysqlrouter/uri.h"
#include "random_generator.h"
#include "router_app.h"
#include "tcp_address.h"
#include "utils.h"
IMPORT_LOG_FUNCTIONS()

#include "cluster_metadata.h"
#include "cluster_metadata_dynamic_state.h"

static const int kDefaultRWPort = 6446;
static const int kDefaultROPort = 6447;
static const char *kRWSocketName = "mysql.sock";
static const char *kROSocketName = "mysqlro.sock";

static const int kDefaultRWXPort = 64460;
static const int kDefaultROXPort = 64470;
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
static constexpr uint32_t kMaxRouterId =
    999999;  // max router id is 6 digits due to username size constraints
static constexpr unsigned kNumRandomChars = 12;
static constexpr unsigned kDefaultPasswordRetries =
    20;  // number of the retries when generating random password
         // for the router user during the bootstrap
static constexpr unsigned kMaxPasswordRetries = 10000;

using mysql_harness::DIM;
using mysql_harness::get_strerror;
using mysql_harness::Path;
using mysql_harness::TCPAddress;
using mysql_harness::truncate_string;
using mysql_harness::UniquePtr;
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

// hint we offer to user when opening dir or file fails with "permission denied"
const char kAppArmorMsg[] =
    "This may be caused by insufficient rights or AppArmor settings.\n"
    "If you have AppArmor enabled try adding full path to the output directory "
    "in the mysqlrouter profile file:\n"
    "/etc/apparmor.d/usr.bin.mysqlrouter\n\n"
    "Example:\n\n"
    "  /path/to/your/output/dir rw,\n"
    "  /path/to/your/output/dir/** rw,";

static bool is_valid_name(const std::string &name) {
  if (!name.empty()) {
    for (char c : name) {
      if (c == '\n' || c == '\r') return false;
    }
  }
  return true;
}

class AutoCleaner {
 public:
  void add_file_delete(const std::string &f) {
    files_[f] = std::make_pair(File, "");
  }

  void add_directory_delete(const std::string &d, bool recursive = false) {
    files_[d] = std::make_pair(recursive ? DirectoryRecursive : Directory, "");
  }

  void add_file_revert(const std::string &file) {
    add_file_revert(file, file + ".bck");
  }

  void add_file_revert(const std::string &file,
                       const std::string &backup_file) {
    if (mysql_harness::Path(file).is_regular()) {
      copy_file(file, backup_file);
      files_[file] = std::make_pair(FileBackup, backup_file);
    } else {
      if (mysql_harness::Path(backup_file).exists())
        mysql_harness::delete_file(backup_file);
      files_[file] = std::make_pair(File, "");
    }
  }

  void add_cleanup_callback(const std::string &callback_name,
                            std::function<bool()> callback) noexcept {
    callbacks_.push_back(CallbackInfo{callback_name, true, callback});
  }

  void remove(const std::string &p) noexcept { files_.erase(p); }

  void clear() {
    for (auto f = files_.rbegin(); f != files_.rend(); ++f) {
      if (f->second.first == FileBackup)
        mysql_harness::delete_file(f->second.second);
    }
    files_.clear();

    for (auto &callback : callbacks_) {
      callback.should_be_called = false;
    }
  }

  ~AutoCleaner() {
    // remove in reverse order, so that files are deleted before their
    // contained directories
    for (auto f = files_.rbegin(); f != files_.rend(); ++f) {
      switch (f->second.first) {
        case File:
          mysql_harness::delete_file(f->first);
          break;

        case Directory:
          mysql_harness::delete_dir(f->first);
          break;

        case DirectoryRecursive:
          mysql_harness::delete_dir_recursive(f->first);
          break;

        case FileBackup:
          copy_file(f->second.second, f->first);
          mysql_harness::delete_file(f->second.second);
          break;
      }
    }

    for (const auto &callback_info : callbacks_) {
      if (callback_info.should_be_called)
        if (!callback_info.callback())
          log_warning("Failed to execute: %s",
                      callback_info.callback_name.c_str());
    }
  }

 private:
  enum Type { Directory, DirectoryRecursive, File, FileBackup };

  /**
   * @brief Contains callback related information: callback function, callback
   * name and information if it should be called.
   */
  struct CallbackInfo {
    /* text that is printed when function call fails */
    std::string callback_name;

    /* true if callback should be called then, false otherwise */
    bool should_be_called;

    /* function to call */
    std::function<bool()> callback;
  };

  /*
   * The map stores all the files that are scheduled to be auto-removed or
   * restored from backup if clean() wasn't called.
   * The key is a name of file to backup, and value is a pair of
   * backup's type and name of backup file (used only for FileBackup type).
   */
  std::map<std::string, std::pair<Type, std::string>> files_;

  /*
   * The vector stores callbacks that are scheduled to be called if clean()
   * wasn't called.
   */
  std::vector<CallbackInfo> callbacks_;
};

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

ConfigGenerator::~ConfigGenerator() {}

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
  // warninng applicable only if --ssl-mode=PREFERRED (or not specified, which
  // defaults to PREFERRED)
  std::string ssl_mode =
      get_opt(options, "ssl_mode", MySQLSession::kSslModePreferred);
  std::transform(ssl_mode.begin(), ssl_mode.end(), ssl_mode.begin(), toupper);

  if (ssl_mode != MySQLSession::kSslModePreferred) return true;

  // warn if the connection is unencrypted
  try {
    // example response
    //
    // > show status like "ssl_cipher"'
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
  if (bootstrap_options.find("base-port") != bootstrap_options.end()) {
    char *end = nullptr;
    const char *tmp = bootstrap_options.at("base-port").c_str();
    int base_port = static_cast<int>(std::strtol(tmp, &end, 10));
    int max_base_port = (kMaxTCPPortNumber - kAllocatedTCPPortCount + 1);
    if (base_port <= 0 || base_port > max_base_port ||
        end != tmp + strlen(tmp)) {
      throw std::runtime_error("Invalid base-port number " +
                               bootstrap_options.at("base-port") +
                               "; please pick a value between 1 and " +
                               std::to_string((max_base_port)));
    }
  }
  if (bootstrap_options.find("bind-address") != bootstrap_options.end()) {
    auto address = bootstrap_options.at("bind-address");
    TCPAddress tmp(address, 1);
    if (!tmp.is_valid()) {
      throw std::runtime_error("Invalid bind-address value " + address);
    }
  }
  if (bootstrap_options.find("connect-timeout") != bootstrap_options.end()) {
    char *end = nullptr;
    const char *tmp = bootstrap_options.at("connect-timeout").c_str();
    int connect_timeout = static_cast<int>(std::strtol(tmp, &end, 10));

    if (connect_timeout <= 0 || connect_timeout > 65535 ||
        end != tmp + strlen(tmp)) {
      throw std::runtime_error(
          "option connect-timeout needs value between 1 and 65535 inclusive, "
          "was " +
          std::to_string((connect_timeout)));
    }

    connect_timeout_ = connect_timeout;
  }
  if (bootstrap_options.find("read-timeout") != bootstrap_options.end()) {
    char *end = nullptr;
    const char *tmp = bootstrap_options.at("read-timeout").c_str();
    int read_timeout = static_cast<int>(std::strtol(tmp, &end, 10));

    if (read_timeout <= 0 || read_timeout > 65535 || end != tmp + strlen(tmp)) {
      throw std::runtime_error(
          "option read-timeout needs value between 1 and 65535 inclusive, "
          "was " +
          std::to_string((read_timeout)));
    }

    read_timeout_ = read_timeout;
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
  } else {
    // setup localhost address.
    u.host = (u.host == "localhost" ? "127.0.0.1" : u.host);
  }

  return u;
}

// throws std::runtime_error, std::logic_error
void ConfigGenerator::connect_to_metadata_server(
    const URI &u, const std::string &bootstrap_socket,
    const std::map<std::string, std::string> &bootstrap_options) {
  // connect to (what should be a) metadata server
  mysql_ = DIM::instance().new_MySQLSession();
  try {
    // throws std::logic_error, std::runtime_error, Error(runtime_error)
    set_ssl_options(mysql_.get(), bootstrap_options);
    mysql_->connect(u.host, u.port, u.username, u.password, bootstrap_socket,
                    "", connect_timeout_, read_timeout_);
  } catch (const MySQLSession::Error &e) {
    throw std::runtime_error("Unable to connect to the metadata server: "s +
                             e.what());
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
  auto schema_version = mysqlrouter::get_metadata_schema_version(mysql_.get());

  if (schema_version == mysqlrouter::kUpgradeInProgressMetadataVersion) {
    throw std::runtime_error(
        "Currently the cluster metadata update is in progress. Please rerun "
        "the bootstrap when it is finished.");
  }

  if (!metadata_schema_version_is_compatible(kRequiredBootstrapSchemaVersion,
                                             schema_version)) {
    throw std::runtime_error(mysqlrouter::string_format(
        "This version of MySQL Router is not compatible with the provided "
        "MySQL InnoDB cluster metadata. Expected metadata version %s, "
        "got %s",
        to_string(kRequiredBootstrapSchemaVersion).c_str(),
        to_string(schema_version).c_str()));
  }

  metadata_ = mysqlrouter::create_metadata(schema_version, mysql_.get());

  // at this point we know the cluster type so let's do additional verifications
  if (mysqlrouter::ClusterType::AR_V2 == metadata_->get_type()) {
    if (bootstrap_options.find("use-gr-notifications") !=
        bootstrap_options.end()) {
      throw std::runtime_error(
          "The parameter 'use-gr-notifications' is valid only for GR cluster "
          "type.");
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

void ConfigGenerator::bootstrap_system_deployment(
    const std::string &config_file_path, const std::string &state_file_path,
    const std::map<std::string, std::string> &user_options,
    const std::map<std::string, std::vector<std::string>> &multivalue_options,
    const std::map<std::string, std::string> &default_paths) {
  auto options(user_options);
  bool quiet = user_options.find("quiet") != user_options.end();
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
  std::vector<UniquePtr<Ofstream>> config_files;
  for (size_t i = 0; i < config_files_names.size(); ++i) {
    config_files.push_back(DIM::instance().new_Ofstream());
    auto &config_file = config_files[i];
    const auto &config_file_name = config_files_names[i];
    config_file->open(config_file_name + ".tmp");
    if (config_file->fail()) {
      throw std::runtime_error("Could not open " + config_file_name +
                               ".tmp for writing: " + get_strerror(errno));
    }
    auto_clean.add_file_delete(config_file_name + ".tmp");
  }

  // on bootstrap failure, DROP USER for all created accounts
  std::unique_ptr<void, std::function<void(void *)>> create_user_undo(
      (void *)1, [&](void *) { undo_create_user_for_new_accounts(); });

  bootstrap_deployment(*config_files[0], *config_files[1], config_file_path,
                       state_file_path, router_name, options,
                       multivalue_options, default_paths, false, auto_clean);

  for (size_t i = 0; i < config_files.size(); ++i) {
    config_files[i]->close();
    const std::string path = config_files_names[i];
    const bool is_static_conf = (i == 0);
    const std::string file_desc =
        is_static_conf ? "configuration" : "dynamic state";

    if (backup_config_file_if_different(path, path + ".tmp", options,
                                        &auto_clean)) {
      if (!quiet) {
        std::cout << "\nExisting " << file_desc << " backed up to '" << path
                  << ".bak'" << std::endl;
      }
      auto_clean.add_file_delete(path);
    }

    // rename the .tmp file to the final file
    if (mysqlrouter::rename_file((path + ".tmp").c_str(), path.c_str()) != 0) {
      // log_error("Error renaming %s.tmp to %s: %s", config_file_path.c_str(),
      //  config_file_path.c_str(), get_strerror(errno));
      throw std::runtime_error("Could not save " + file_desc +
                               " file to final location");
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
}

// throws std::system_error
static bool is_directory_empty(mysql_harness::Directory dir) {
  for (auto di = dir.begin(); di != dir.end(); ++di) {
    std::string name = (*di).basename().str();
    if (name != "." && name != "..") return false;
  }
  return true;
}

/**
 * Create a self-contained deployment of the Router in a directory.
 */
void ConfigGenerator::bootstrap_directory_deployment(
    const std::string &directory,
    const std::map<std::string, std::string> &user_options,
    const std::map<std::string, std::vector<std::string>> &multivalue_options,
    const std::map<std::string, std::string> &default_paths) {
  bool force = user_options.find("force") != user_options.end();
  bool quiet = user_options.find("quiet") != user_options.end();
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
      log_error("Cannot create directory '%s': %s",
                truncate_string(directory).c_str(), get_strerror(err).c_str());
#ifndef _WIN32
      if (err == EACCES || err == EPERM) log_error(kAppArmorMsg);
#endif
      throw std::runtime_error("Could not create deployment directory");
    }
    auto_clean.add_directory_delete(directory, true);
  }

  if (!Path(directory).is_directory()) {
    throw std::runtime_error("Can't use " + directory +
                             " for bootstrap, it is not directory.");
  }

  set_file_owner(user_options, directory);

  path = path.real_path();
  mysql_harness::Path config_file_path =
      path.join(mysql_harness::Path("mysqlrouter.conf"));
  if (!config_file_path.exists() && !force) {
    bool dir_empty;
    try {
      dir_empty = is_directory_empty(path);
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

    if (!dir_empty) {
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
        if (res != EEXIST) {
          log_error("Cannot create directory '%s': %s",
                    truncate_string(options[option_name]).c_str(),
                    get_strerror(errno).c_str());
          throw std::runtime_error("Could not create " + option_name +
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
  std::vector<UniquePtr<Ofstream>> config_files;
  for (size_t i = 0; i < config_files_names.size(); ++i) {
    config_files.emplace_back(DIM::instance().new_Ofstream());
    config_files[i]->open(config_files_names[i] + ".tmp");
    if (config_files[i]->fail()) {
#ifndef _WIN32
      if (errno == EACCES || errno == EPERM) log_error(kAppArmorMsg);
#endif
      throw std::runtime_error("Could not open " + config_files_names[i] +
                               ".tmp for writing: " + get_strerror(errno));
    }
    auto_clean.add_file_delete(config_files_names[i] + ".tmp");
  }

  set_keyring_info_real_paths(options, path);

  // on bootstrap failure, DROP USER for all created accounts
  std::unique_ptr<void, std::function<void(void *)>> create_user_undo(
      (void *)1, [&](void *) { undo_create_user_for_new_accounts(); });

  bootstrap_deployment(*config_files[0], *config_files[1],
                       config_files_names[0], config_files_names[1],
                       router_name, options, multivalue_options, default_paths,
                       true,
                       auto_clean);  // throws std::runtime_error, ?

  for (size_t i = 0; i < config_files_names.size(); ++i) {
    auto &config_file = config_files[i];
    const auto &config_file_name = config_files_names[i];
    const bool is_static_conf = (i == 0);
    config_file->close();
    if (backup_config_file_if_different(config_file_name,
                                        config_file_name + ".tmp", options)) {
      if (!quiet)
        std::cout << "\nExisting configurations backed up to '"
                  << config_file_name << ".bak'" << std::endl;
    }
    // rename the .tmp file to the final file
    if (mysqlrouter::rename_file((config_file_name + ".tmp").c_str(),
                                 config_file_name.c_str()) != 0) {
      // log_error("Error renaming %s.tmp to %s: %s", config_file_path.c_str(),
      //  config_file_path.c_str(), get_strerror(errno));
      throw std::runtime_error(
          "Could not move configuration file '" + config_file_name +
          ".tmp' to final location: " + mysqlrouter::get_last_error());
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
  create_start_script(path.str(), keyring_info_.get_master_key_file().empty(),
                      options);
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
      throw std::runtime_error("Could not access the config file as user '" +
                               user_name +
                               "' after the bootstrap in the directory " +
                               directory + " : " + get_strerror(errno));
    }
  }
#endif

  auto_clean.clear();
  create_user_undo.release();
}

ConfigGenerator::Options ConfigGenerator::fill_options(
    const std::map<std::string, std::string> &user_options) {
  std::string bind_address{"0.0.0.0"};
  bool use_sockets = false;
  bool skip_tcp = false;
  bool skip_classic_protocol = false;
  bool skip_x_protocol = false;
  int base_port = 0;
  if (user_options.find("base-port") != user_options.end()) {
    char *end = nullptr;
    const char *tmp = user_options.at("base-port").c_str();
    base_port = static_cast<int>(std::strtol(tmp, &end, 10));
    int max_base_port = (kMaxTCPPortNumber - kAllocatedTCPPortCount + 1);
    if (base_port <= 0 || base_port > max_base_port ||
        end != tmp + strlen(tmp)) {
      throw std::runtime_error("Invalid base-port number " +
                               user_options.at("base-port") +
                               "; please pick a value lower than " +
                               std::to_string((max_base_port)));
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
    auto address = user_options.at("bind-address");
    TCPAddress tmp(address, 1);
    if (!tmp.is_valid()) {
      throw std::runtime_error("Invalid bind-address value " + address);
    }
    options.bind_address = address;
  }
  if (!skip_classic_protocol) {
    if (use_sockets) {
      options.rw_endpoint.socket = kRWSocketName;
      options.ro_endpoint.socket = kROSocketName;
    }
    if (!skip_tcp) {
      options.rw_endpoint.port = base_port == 0 ? kDefaultRWPort : base_port++;
      options.ro_endpoint.port = base_port == 0 ? kDefaultROPort : base_port++;
    }
  }
  if (!skip_x_protocol) {
    if (use_sockets) {
      options.rw_x_endpoint.socket = kRWXSocketName;
      options.ro_x_endpoint.socket = kROXSocketName;
    }
    if (!skip_tcp) {
      options.rw_x_endpoint.port =
          base_port == 0 ? kDefaultRWXPort : base_port++;
      options.ro_x_endpoint.port =
          base_port == 0 ? kDefaultROXPort : base_port++;
    }
  }
  if (user_options.find("logdir") != user_options.end())
    options.override_logdir = user_options.at("logdir");
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

  options.use_gr_notifications =
      user_options.find("use-gr-notifications") != user_options.end();

  return options;
}

namespace {

unsigned get_password_retries(
    const std::map<std::string, std::string> &user_options) {
  if (user_options.find("password-retries") == user_options.end()) {
    return kDefaultPasswordRetries;
  }

  char *end = nullptr;
  const char *tmp = user_options.at("password-retries").c_str();
  unsigned result = static_cast<unsigned>(std::strtoul(tmp, &end, 10));
  if (result == 0 || result > kMaxPasswordRetries || end != tmp + strlen(tmp)) {
    throw std::runtime_error("Invalid password-retries value '" +
                             user_options.at("password-retries") +
                             "'; please pick a value from 1 to " +
                             std::to_string((kMaxPasswordRetries)));
  }

  return result;
}

std::string compute_password_hash(const std::string &password) {
  uint8_t hash_stage1[SHA1_HASH_SIZE];
  my_sha1::compute_sha1_hash(hash_stage1, password.c_str(), password.length());
  uint8_t hash_stage2[SHA1_HASH_SIZE];
  my_sha1::compute_sha1_hash(hash_stage2, (const char *)hash_stage1,
                             SHA1_HASH_SIZE);

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
      MySQLSession &sess, const std::string &cluster_initial_username,
      const std::string &cluster_initial_password,
      const std::string &cluster_initial_hostname,
      unsigned long cluster_initial_port,
      const std::string &cluster_initial_socket,
      unsigned long connection_timeout,
      std::set<MySQLErrorc> failure_codes = {MySQLErrorc::kSuperReadOnly,
                                             MySQLErrorc::kLostConnection})
      : mysql_(sess),
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
  virtual std::vector<std::tuple<std::string, unsigned long>>
  fetch_cluster_hosts() = 0;

  MySQLSession &mysql_;
  const std::string &cluster_initial_username_;
  const std::string &cluster_initial_password_;
  const std::string &cluster_initial_hostname_;
  unsigned long cluster_initial_port_;
  const std::string &cluster_initial_socket_;
  unsigned long connection_timeout_;
  std::set<MySQLErrorc> failure_codes_;
};

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

  // init it once, even though we'll never use it
  auto cluster_servers_it = cluster_servers.begin();

  do {
    try {
      return wrapped_func();
    } catch (const MySQLSession::Error &e) {
      MySQLErrorc ec = static_cast<MySQLErrorc>(e.code());

      log_info(
          "Executing statements failed with: '%s' (%d), trying to connect to "
          "another node",
          e.what(), e.code());

      // code not in failure-set
      if (failure_codes_.find(ec) == failure_codes_.end()) {
        throw;
      }

      do {
        if (!fetched_cluster_servers) {
          // lazy fetch the GR members
          //
          fetched_cluster_servers = true;

          log_info("Fetching Cluster Members");

          for (auto &gr_node : fetch_cluster_hosts()) {
            auto const &gr_host = std::get<0>(gr_node);
            auto gr_port = std::get<1>(gr_node);

            // if we connected through TCP/IP, ignore the initial host
            if (cluster_initial_socket_.size() == 0 &&
                (gr_host == cluster_initial_hostname_ &&
                 gr_port == cluster_initial_port_)) {
              continue;
            }

            log_debug("added cluster node: %s:%ld", gr_host.c_str(), gr_port);
            cluster_servers.emplace_back(gr_host, gr_port);
          }

          // get a new iterator as the old one is now invalid
          cluster_servers_it = cluster_servers.begin();
        } else {
          std::advance(cluster_servers_it, 1);
        }

        if (cluster_servers_it == cluster_servers.end()) {
          throw std::runtime_error(
              "no more nodes to fail-over too, giving up.");
        }

        if (mysql_.is_connected()) {
          log_info("%s", "disconnecting from mysql-server");
          mysql_.disconnect();
        }

        auto const &tp = *cluster_servers_it;

        auto const &host = std::get<0>(tp);
        auto port = std::get<1>(tp);

        log_info("trying to connecting to mysql-server at %s:%ld", host.c_str(),
                 port);

        try {
          mysql_.connect(host, port, cluster_initial_username_,
                         cluster_initial_password_, "", "",
                         connection_timeout_);
        } catch (const std::exception &inner_e) {
          log_info("Failed connecting to %s:%ld: %s, trying next", host.c_str(),
                   port, inner_e.what());
        }
        // if this fails, we should just skip it and go to the next
      } while (!mysql_.is_connected());
    }
  } while (true);
}

/**
 * GR-aware decorator for MySQL Sessions.
 */
class GRAwareDecorator : public ClusterAwareDecorator {
 public:
  using ClusterAwareDecorator::ClusterAwareDecorator;

 protected:
  virtual std::vector<std::tuple<std::string, unsigned long>>
  fetch_cluster_hosts() override;
};

std::vector<std::tuple<std::string, unsigned long>>
GRAwareDecorator::fetch_cluster_hosts() {
  // Query the name of the replicaset, the servers in the replicaset and the
  // router credentials using the URL of a server in the replicaset.
  //
  // order by member_role (in 8.0 and later) to sort PRIMARY over SECONDARY
  const std::string query =
      "SELECT member_host, member_port "
      "  FROM performance_schema.replication_group_members "
      " /*!80002 ORDER BY member_role */";

  try {
    std::vector<std::tuple<std::string, unsigned long>> gr_servers;

    mysql_.query(
        query, [&gr_servers](const std::vector<const char *> &row) -> bool {
          gr_servers.push_back(
              std::make_tuple(std::string(row[0]), std::stoul(row[1])));
          return true;  // don't stop
        });

    return gr_servers;
  } catch (const MySQLSession::Error &e) {
    // log_error("MySQL error: %s (%u)", e.what(), e.code());
    // log_error("    Failed query: %s", query.str().c_str());
    throw std::runtime_error("Error querying metadata: "s + e.what());
  }
}

/**
 * AsyncRepl-aware decorator for MySQL Sessions
 */
class AsyncReplAwareDecorator : public ClusterAwareDecorator {
 public:
  using ClusterAwareDecorator::ClusterAwareDecorator;

 protected:
  virtual std::vector<std::tuple<std::string, unsigned long>>
  fetch_cluster_hosts() override;
};

std::vector<std::tuple<std::string, unsigned long>>
AsyncReplAwareDecorator::fetch_cluster_hosts() {
  // Query the name of the cluster, and the instance addresses
  const std::string query =
      "select i.address from "
      "mysql_innodb_cluster_metadata.v2_instances i join "
      "mysql_innodb_cluster_metadata.v2_clusters c on c.cluster_id = "
      "i.cluster_id";

  try {
    std::vector<std::tuple<std::string, unsigned long>> ar_servers;

    mysql_.query(query,
                 [&ar_servers](const std::vector<const char *> &row) -> bool {
                   mysqlrouter::URI u("mysql://"s + row[0]);
                   ar_servers.push_back(std::make_tuple(u.host, u.port));
                   return true;  // don't stop
                 });

    return ar_servers;
  } catch (const MySQLSession::Error &e) {
    throw std::runtime_error("Error querying metadata: "s + e.what());
  }
}

std::unique_ptr<ClusterAwareDecorator> create_cluster_aware_decorator(
    const ClusterType cluster_type, MySQLSession &sess,
    const std::string &cluster_initial_username,
    const std::string &cluster_initial_password,
    const std::string &cluster_initial_hostname,
    unsigned long cluster_initial_port,
    const std::string &cluster_initial_socket, unsigned long connection_timeout,
    std::set<MySQLErrorc> failure_codes = {MySQLErrorc::kSuperReadOnly,
                                           MySQLErrorc::kLostConnection}) {
  if (cluster_type == ClusterType::AR_V2) {
    return std::make_unique<AsyncReplAwareDecorator>(
        sess, cluster_initial_username, cluster_initial_password,
        cluster_initial_hostname, cluster_initial_port, cluster_initial_socket,
        connection_timeout, failure_codes);
  } else {
    return std::make_unique<GRAwareDecorator>(
        sess, cluster_initial_username, cluster_initial_password,
        cluster_initial_hostname, cluster_initial_port, cluster_initial_socket,
        connection_timeout, failure_codes);
  }
}

void ConfigGenerator::set_log_file_permissions(
    const std::map<std::string, std::string> &default_paths,
    const std::map<std::string, std::string> &user_options,
    const Options &options) {
#ifndef _WIN32
  /* Currently at this point the logger is not yet initialized but while
   * bootstraping with the --user=<user> option we need to create a log file and
   * chown it to the <user>. Otherwise when the router gets launched later (not
   * bootstrap) with the same --user=<user> option, the user might not have
   * right to the logging directory.
   */
  out_stream_ << "- Adjusting permissions of generated files" << std::endl;
  // assert(default_paths.find("logging_folder") != default_paths.end());
  std::string logdir = (!options.override_logdir.empty())
                           ? options.override_logdir
                           : default_paths.at("logging_folder");
  if (!logdir.empty()) {
    auto log_path =
        mysql_harness::Path::make_path(logdir, "mysqlrouter", "log");
    auto log_file = log_path.str();
    std::fstream f;
    f.open(log_file, std::ios::out);
    set_file_owner(user_options, log_file);
  }
#endif
}

// get a value from the map if it exists, default-value otherwise
static std::string map_get(
    const std::map<std::string, std::string> &user_options,
    const std::string &key, const std::string &def_value) {
  const auto &it = user_options.find(key);
  return (it != user_options.end()) ? it->second : def_value;
}

void ConfigGenerator::bootstrap_deployment(
    std::ostream &config_file, std::ostream &state_file,
    const mysql_harness::Path &config_file_path,
    const mysql_harness::Path &state_file_path, const std::string &router_name,
    const std::map<std::string, std::string> &user_options,
    const std::map<std::string, std::vector<std::string>> &multivalue_options,
    const std::map<std::string, std::string> &default_paths,
    bool directory_deployment, AutoCleaner &auto_clean) {
  bool force = user_options.find("force") != user_options.end();
  bool quiet = user_options.find("quiet") != user_options.end();

  // get router_id and username from config and/or command-line
  uint32_t router_id;
  std::string username;
  auto cluster_info = metadata_->fetch_metadata_servers();

  std::tie(router_id, username) =
      get_router_id_and_username_from_config_if_it_exists(
          config_file_path.str(), cluster_info.metadata_cluster_name, force);

  // if user provided --account, override username with it
  username = map_get(user_options, "account", username);

  // If username is still empty at this point, it will be autogenerated
  // inside try_bootstrap_deployment().  It cannot be done here, because the
  // autogenerated name will contain router_id, and that is still subject to
  // change inside try_bootstrap_deployment()

  if (!quiet)
    print_bootstrap_start_msg(router_id, directory_deployment,
                              config_file_path);

  Options options(fill_options(user_options));
  // Prompt for the Router's runtime account that's used by metadata_cache and
  // specified by "--account".
  // If running in --account mode, the user provides the password (ALWAYS,
  // regarless of if it's already available from keyring or not.  This hard
  // rule exists to make automation easier by asking const number of questions).
  // OTOH, if running without --account, the password is NEVER prompted for
  // (it's autogenerated or taken from keyring (we throw an error if missing,
  // but never prompt for it - again, the goal is automation-friendliness)
  std::string password;
  if (user_options.count("account"))
    password = prompt_password("Please enter MySQL password for " + username);
  else if (!username.empty())
    password = fetch_password_from_keyring(username, router_id);

  // bootstrap
  // All SQL writes happen inside here
  {
    auto cluster_aware = create_cluster_aware_decorator(
        metadata_->get_type(), *mysql_, cluster_initial_username_,
        cluster_initial_password_, cluster_initial_hostname_,
        cluster_initial_port_, cluster_initial_socket_, connect_timeout_);

    // note: try_bootstrap_deployment() can update router_id, username and
    // password note: failover is performed only on specific errors (subset of
    // what
    //       appears in enum class MySQLErrorc)
    std::tie(password) =
        cluster_aware->failover_on_failure<std::tuple<std::string>>([&]() {
          return try_bootstrap_deployment(
              router_id, username, password, router_name,
              cluster_info.metadata_cluster_id, user_options,
              multivalue_options, options);
        });
  }

  // test out the connection that Router would use
  {
    bool strict = user_options.count("strict");
    verify_router_account(username, password,
                          cluster_info.metadata_cluster_name, strict);
  }

  store_credentials_in_keyring(auto_clean, user_options, router_id, username,
                               password, options);
  set_log_file_permissions(default_paths, user_options, options);

  // generate the new config file
  {
    out_stream_ << "- Creating configuration " << config_file_path.str()
                << std::endl;
    auto system_username = map_get(user_options, "user", "");
    create_config(config_file, state_file, router_id, router_name,
                  system_username, cluster_info.metadata_servers,
                  cluster_info.metadata_cluster_name,
                  cluster_info.metadata_replicaset, username, options,
                  state_file_path.str());
  }

  if (!quiet) {
    const std::string cluster_type_name =
        metadata_->get_type() == ClusterType::AR_V2 ? "Async Replicaset"
                                                    : "InnoDB";
    print_report(config_file_path.str(), router_name,
                 cluster_info.metadata_cluster_name, cluster_type_name,
                 map_get(user_options, "report-host", "localhost"),
                 !directory_deployment, options);
  }
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

void ConfigGenerator::verify_router_account(
    const std::string &username, const std::string &password,
    const std::string &primary_cluster_name, bool strict) {
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

  auto run_sql_queries = [&primary_cluster_name,
                          this](MySQLSession &rtr_acct_sess) {
    // no need to differentiate between SQL queries and statements, as both can
    // be called with mysql_real_query() (called inside MySQLSession::execute())
    const auto stmts =
        metadata_->get_routing_mode_queries(primary_cluster_name);

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
    MySQLSession::ConnectionParameters p = mysql_->get_connection_parameters();
    p.conn_opts.username = username;
    p.conn_opts.password = password;

    try {
      // will throw if logging in using Router's credentials fails
      rtr_acct_sess.connect_and_set_opts(p);
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

std::tuple<std::string> ConfigGenerator::try_bootstrap_deployment(
    uint32_t &router_id, std::string &username, std::string &password,
    const std::string &router_name, const std::string &cluster_id,
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
  metadata_->update_router_info(router_id, cluster_id, rw_endpoint, ro_endpoint,
                                rw_x_endpoint, ro_x_endpoint);

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
  } catch (const std::out_of_range &e) {
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
      auto_clean.add_cleanup_callback("master_key_writer", [keyring_info_copy] {
        return keyring_info_copy.write_master_key();
      });
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

// TODO This is very ugly, it should not be a global. It's set in main(), and
//      used in find_executable_path() below to provide path to Router binary
//      when generating start.sh.
std::string g_program_name;

#ifdef _WIN32
// This is only for Windows
static std::string find_plugin_path() {
  char szPath[MAX_PATH];
  if (GetModuleFileName(NULL, szPath, sizeof(szPath)) != 0) {
    mysql_harness::Path mypath(szPath);
    mysql_harness::Path mypath2(mypath.dirname().dirname());
    mypath2.append("lib");
    return std::string(mypath2.str());
  }
  throw std::logic_error("Could not find own installation directory");
}
#endif

static std::string find_executable_path() {
#ifdef _WIN32
  // the bin folder is not usually in the path, just the lib folder
  char szPath[MAX_PATH];
  if (GetModuleFileName(NULL, szPath, sizeof(szPath)) != 0) {
    char *pc = szPath - 1;
    while (*++pc)
      if (*pc == '\\') *pc = '/';
    return std::string(szPath);
  }
#else
  harness_assert(!g_program_name.empty());

  if (g_program_name.find('/') != std::string::npos) {
    char *tmp = realpath(g_program_name.c_str(), nullptr);
    harness_assert(tmp);  // will fail if g_program_name provides bogus path
    std::string path(tmp);
    free(tmp);
    return path;
  } else {
    std::string path(std::getenv("PATH"));
    char *last = nullptr;
    char *p = strtok_r(&path[0], ":", &last);
    while (p) {
      if (*p && p[strlen(p) - 1] == '/') p[strlen(p) - 1] = 0;
      std::string tmp(std::string(p) + "/" + g_program_name);
      if (access(tmp.c_str(), R_OK | X_OK) == 0) {
        return tmp;
      }
      p = strtok_r(nullptr, ":", &last);
    }
  }
#endif
  throw std::logic_error("Could not find own installation directory");
}

/*static*/ std::string ConfigGenerator::endpoint_option(
    const Options &options, const Options::Endpoint &ep) {
  std::string r;
  if (ep.port > 0) {
    auto bind_address =
        (!options.bind_address.empty()) ? options.bind_address : "0.0.0.0";
    r.append("bind_address=" + bind_address + "\n");
    r.append("bind_port=" + std::to_string(ep.port));
  }
  if (!ep.socket.empty()) {
    if (!r.empty()) r.append("\n");
    r.append("socket=" + options.socketsdir + "/" + ep.socket);
  }
  return r;
}

static std::string option_line(const std::string &key,
                               const std::string &value) {
  if (!value.empty()) {
    return key + "=" + value + "\n";
  }
  return "";
}

static void save_initial_dynamic_state(
    std::ostream &state_stream, ClusterMetadata &cluster_metadata,
    const std::string &cluster_type_specific_id,
    const std::vector<std::string> &metadata_server_addresses) {
  // create dynamic state object
  using DynamicState = mysql_harness::DynamicState;
  DynamicState dynamic_state{""};
  // put metadata-caches secion in it
  ClusterMetadataDynamicState mdc_dynamic_state(&dynamic_state,
                                                cluster_metadata.get_type());
  mdc_dynamic_state.set_cluster_type_specific_id(cluster_type_specific_id);
  mdc_dynamic_state.set_metadata_servers(metadata_server_addresses);
  if (cluster_metadata.get_type() == ClusterType::AR_V2) {
    auto view_id =
        dynamic_cast<mysqlrouter::ClusterMetadataAR &>(cluster_metadata)
            .get_view_id();
    mdc_dynamic_state.set_view_id(view_id);
  }
  // save to out stream
  mdc_dynamic_state.save(state_stream);
}

/*static*/ std::string ConfigGenerator::gen_metadata_cache_routing_section(
    bool is_classic, bool is_writable, const Options::Endpoint endpoint,
    const Options &options, const std::string &metadata_key,
    const std::string &metadata_replicaset,
    const std::string &fast_router_key) {
  if (!endpoint) return "";

  const std::string key_suffix =
      std::string(is_classic ? "" : "_x") + (is_writable ? "_rw" : "_ro");
  const std::string role = is_writable ? "PRIMARY" : "SECONDARY";
  const std::string strategy =
      is_writable ? "first-available" : "round-robin-with-fallback";
  const std::string protocol = is_classic ? "classic" : "x";

  // clang-format off
  return "[routing:" + fast_router_key + key_suffix + "]\n" +
         endpoint_option(options, endpoint) + "\n" +
         "destinations=metadata-cache://" + metadata_key + "/" +
             metadata_replicaset + "?role=" + role + "\n"
         "routing_strategy=" + strategy + "\n"
         "protocol=" + protocol + "\n"
         "\n";
  // clang-format on
}

void ConfigGenerator::create_config(
    std::ostream &config_file, std::ostream &state_file, uint32_t router_id,
    const std::string &router_name, const std::string &system_username,
    const std::vector<std::string> &metadata_server_addresses,
    const std::string &metadata_cluster, const std::string &metadata_replicaset,
    const std::string &username, const Options &options,
    const std::string &state_file_name) {
  config_file
      << "# File automatically generated during MySQL Router bootstrap\n";

  config_file << "[DEFAULT]\n";
  if (!router_name.empty()) config_file << "name=" << router_name << "\n";
  if (!system_username.empty())
    config_file << "user=" << system_username << "\n";
  if (!options.override_logdir.empty())
    config_file << "logging_folder=" << options.override_logdir << "\n";
  if (!options.override_rundir.empty())
    config_file << "runtime_folder=" << options.override_rundir << "\n";
  if (!options.override_datadir.empty())
    config_file << "data_folder=" << options.override_datadir << "\n";
  if (!options.keyring_file_path.empty())
    config_file << "keyring_path=" << options.keyring_file_path << "\n";
  if (!options.keyring_master_key_file_path.empty())
    config_file << "master_key_path=" << options.keyring_master_key_file_path
                << "\n";
  if (!keyring_info_.get_master_key_reader().empty())
    config_file << "master_key_reader=" << keyring_info_.get_master_key_reader()
                << "\n";
  if (!keyring_info_.get_master_key_writer().empty())
    config_file << "master_key_writer=" << keyring_info_.get_master_key_writer()
                << "\n";

  config_file << "connect_timeout=" << connect_timeout_ << "\n";
  config_file << "read_timeout=" << read_timeout_ << "\n";

  config_file << "dynamic_state=" << state_file_name << "\n";

  save_initial_dynamic_state(state_file, *metadata_.get(), cluster_specific_id_,
                             metadata_server_addresses);

  config_file << "\n"
              << "[" << mysql_harness::logging::kConfigSectionLogger << "]\n"
              << mysql_harness::logging::kConfigOptionLogLevel << " = INFO\n"
              << "\n";

  const auto &metadata_key = metadata_cluster;
  auto ttl = options.use_gr_notifications ? kDefaultMetadataTTLGRNotificationsON
                                          : kDefaultMetadataTTL;

  const std::string use_gr_notifications =
      mysqlrouter::ClusterType::AR_V2 == metadata_->get_type()
          ? ""
          : "use_gr_notifications="s +
                (options.use_gr_notifications ? "1" : "0") + "\n";

  config_file << "[metadata_cache:" << metadata_key << "]\n"
              << "cluster_type="
              << mysqlrouter::to_string(metadata_->get_type()) << "\n"
              << "router_id=" << router_id << "\n"
              << "user=" << username << "\n"
              << "metadata_cluster=" << metadata_cluster << "\n"
              << "ttl=" << mysqlrouter::ms_to_seconds_string(ttl) << "\n"
              << use_gr_notifications;

  // SSL options
  config_file << option_line("ssl_mode", options.ssl_options.mode);
  config_file << option_line("ssl_cipher", options.ssl_options.cipher);
  config_file << option_line("tls_version", options.ssl_options.tls_version);
  config_file << option_line("ssl_ca", options.ssl_options.ca);
  config_file << option_line("ssl_capath", options.ssl_options.capath);
  config_file << option_line("ssl_crl", options.ssl_options.crl);
  config_file << option_line("ssl_crlpath", options.ssl_options.crlpath);
  // Note: we don't write cert and key because
  // creating router accounts with REQUIRE X509 is not yet supported.
  // The cert and key options passed to bootstrap if for the bootstrap
  // connection itself.
  config_file << "\n";

  const std::string fast_router_key = metadata_key +
                                      (metadata_replicaset.empty() ? "" : "_") +
                                      metadata_replicaset;

  // proxy to save on typing the same long list of args
  auto gen_mdc_rt_sect = [&](bool is_classic, bool is_writable,
                             Options::Endpoint endpoint) {
    return gen_metadata_cache_routing_section(
        is_classic, is_writable, endpoint, options, metadata_key,
        metadata_replicaset, fast_router_key);
  };
  config_file << gen_mdc_rt_sect(true, true, options.rw_endpoint);
  config_file << gen_mdc_rt_sect(true, false, options.ro_endpoint);
  config_file << gen_mdc_rt_sect(false, true, options.rw_x_endpoint);
  config_file << gen_mdc_rt_sect(false, false, options.ro_x_endpoint);
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

void ConfigGenerator::print_report(const std::string &config_file_name,
                                   const std::string &router_name,
                                   const std::string &metadata_cluster,
                                   const std::string &cluster_type_name,
                                   const std::string &hostname,
                                   bool is_system_deployment,
                                   const Options &options) {
  constexpr const char kPromptPrefix[]{
#ifdef _WIN32
      "> "
#else
      "$ "
#endif
  };

  out_stream_ << "\n"
              << Vt100::foreground(Vt100::Color::Green) << "# MySQL Router "
              << ((router_name.empty() || router_name == kSystemRouterName)
                      ? ""
                      : "'" + router_name + "' ")
              << "configured for the " << cluster_type_name << " cluster '"
              << metadata_cluster.c_str() << "'"
              << Vt100::render(Vt100::Render::ForegroundDefault) << "\n"
              << std::endl;

  out_stream_ << "After this MySQL Router has been started with the generated "
                 "configuration"
              << "\n"
              << std::endl;
#ifdef _WIN32
  if (is_system_deployment) {
    out_stream_ << "    " << kPromptPrefix << "net start mysqlrouter"
                << "\n"
                << "or" << std::endl;
  }
#else
  if (is_system_deployment) {
    out_stream_ << "    " << kPromptPrefix << "/etc/init.d/mysqlrouter restart"
                << "\n"
                << "or" << std::endl;
    if (Path("/bin/systemctl").exists()) {
      out_stream_ << "    " << kPromptPrefix << "systemctl start mysqlrouter"
                  << "\n"
                  << "or" << std::endl;
    }
  }
#endif
  out_stream_ << "    " << kPromptPrefix << g_program_name << " -c "
              << config_file_name << "\n\n"
              << "the cluster '" << metadata_cluster
              << "' can be reached by connecting to:\n"
              << std::endl;

  auto dump_sockets = [this, &hostname](const std::string &section,
                                        const std::string &socketsdir,
                                        const Options::Endpoint &rw_endpoint,
                                        const Options::Endpoint &ro_endpoint) {
    if (rw_endpoint || ro_endpoint) {
      out_stream_ << "## " << section << "\n\n";
      if (rw_endpoint) {
        out_stream_ << "- Read/Write Connections: ";
        if (rw_endpoint.port > 0) {
          out_stream_ << hostname << ":" << rw_endpoint.port;
        }
        if (!rw_endpoint.socket.empty()) {
          if (rw_endpoint.port > 0) {
            out_stream_ << ", ";
          }
          out_stream_ << socketsdir << "/" << rw_endpoint.socket;
        }
        out_stream_ << std::endl;
      }

      if (ro_endpoint) {
        out_stream_ << "- Read/Only Connections:  ";
        if (ro_endpoint.port > 0) {
          out_stream_ << hostname << ":" << ro_endpoint.port;
        }
        if (!ro_endpoint.socket.empty()) {
          if (ro_endpoint.port > 0) {
            out_stream_ << ", ";
          }
          out_stream_ << socketsdir << "/" << ro_endpoint.socket;
        }

        out_stream_ << std::endl;
      }
      out_stream_ << std::endl;
    }
  };

  dump_sockets("MySQL Classic protocol", options.socketsdir,
               options.rw_endpoint, options.ro_endpoint);
  dump_sockets("MySQL X protocol", options.socketsdir, options.rw_x_endpoint,
               options.ro_x_endpoint);
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
  resolution availble
  - using IP (even if we can detect it correctly) will not work if IP is not
  static

  Summing up, '%' is the easy way to avoid these problems. But the decision
  ultimately belongs to the user.
  */

  bool if_not_exists;
  {
    const std::string ac =
        map_get(user_options, "account-create", "if-not-exists");
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
      get_password_retries(user_options);  // throws std::runtime_error
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
  for (const std::string a : accounts) {
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
      unsigned code;
      try {
        code = std::stoul(row[1]);
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
  try {  // need to guarrantee noexcept

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
      // accounts, but determinining which of them were new (via SHOW WARNINGS)
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
that we created.  Unfortuantely the cleanup failed with error:

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

// Unfortunately, there's no way to disable clang-format just for the
// pseudocode inside the Doxygen block, therefore it has to be done on the
// outside (affecting the entire block)
// clang-format off
/**
 * Get router_id name values associated with a metadata_cache configuration
 * for the given cluster_name.
 *
 * The lookup is done through the metadata_cluster option inside the
 * metadata_cache section.
 *
 * ---
 * This function returns Router's id and SQL user for cluster `cluster_name`,
 * if it finds them in config file and they seem valid.  If they're missing or
 * incomplete, function will return empty values for them (0 or ""), giving a
 * green light to generate new ones.  Finally, if it detects that the cluster
 * name is off or missing, it will throw or return {0, ""}, depending on
 * `forcing_overwrite`.
 *
 * The logic simplifies to this:
 *   if !exists config
 *     return {0, ""}
 *   elif !exists [metadata_cache]
 *     return {0, ""}
 *   elif exists >1 [metadata_cache]
 *     throw ">1 [metadata_cache] not supported"
 *   elif exists [metadata_cache].metadata_cluster &&
 *        [metadata_cache].metadata_cluster == {cluster_name reported by MD server}:
 *     if !exists [metadata_cache].router_id:
 *       [metadata_cache].router_id = 0
 *     elif invalid [metadata_cache].router_id:
 *       throw "invalid router_id"
 *     if !exits [metadata_cache].user:
 *       [metadata_cache].user = ""
 *     return {[metadata_cache].router_id, [metadata_cache].user}
 *   else
 *     // config exists, [metadata_cache] exists,
 *     // [metadata_cache].metadata_cluster does not exist or
 *     // [metadata_cache].metadata_cluster == some unexpected cluster name
 *     if !force
 *       throw "Router already configured for cluster <name from config>, use --force to replace"
 *     else
 *       return {0, ""}
 *
 * @param config_file_path /path/to/config/file
 * @param cluster_name Cluster name for which Router id and user should be
 *                     returned
 * @param forcing_overwrite Action to take on unexpected cluster in config, see
 *                          function description
 *
 * @throws std::runtime_error on invalid router_id or metadata_cluster
 *
 * @returns Router's id and (SQL) user if they're both valid, otherwise null
 *          values for both
 */
// clang-format on
std::pair<uint32_t, std::string>
ConfigGenerator::get_router_id_and_username_from_config_if_it_exists(
    const std::string &config_file_path, const std::string &cluster_name,
    bool forcing_overwrite) {
  const auto kFoundNoUsableAccount = std::make_pair(0, "");

  // no config -> return {0, ""}
  mysql_harness::Path path(config_file_path);
  if (!path.exists()) return kFoundNoUsableAccount;

  // no [metadata_cache] -> return {0, ""}
  mysql_harness::Config config(mysql_harness::Config::allow_keys);
  config.read(path);
  if (!config.has_any("metadata_cache")) return kFoundNoUsableAccount;

  // grab [metadata_cache], we only allow 1 for now
  mysql_harness::Config::SectionList sections = config.get("metadata_cache");
  if (sections.size() > 1) {
    throw std::runtime_error(
        "Bootstrapping of Router with multiple metadata_cache sections not "
        "supported");
  }

  // Thanks to the above limitation, this for() loop runs exactly once and
  // section == sections.front() always.  If section != `<cluster_name>`, the
  // code will fall back to if (!forcing_overwrite) {..} below
  std::string existing_cluster;
  for (auto const &section : sections) {
    if (section->has("metadata_cluster")) {
      existing_cluster = section->get("metadata_cluster");
      if (existing_cluster == cluster_name) {
        uint32_t router_id;
        std::string username;

        // get router_id
        if (section->has("router_id")) {
          std::string tmp = section->get("router_id");
          char *end;
          router_id = std::strtoul(tmp.c_str(), &end, 10);
          if (end == tmp.c_str() || errno == ERANGE) {
            throw std::runtime_error("Invalid router_id '" + tmp +
                                     "' for cluster '" + cluster_name +
                                     "' in " + config_file_path);
          }
        } else {
          router_id = 0;
          log_warning("WARNING: router_id not set for cluster '%s'",
                      cluster_name.c_str());
        }

        // get username, example: user=mysql_router4_kot8tcepf3kn
        if (section->has("user"))
          username = section->get("user");
        else
          log_warning("WARNING: user not set for cluster '%s'",
                      cluster_name.c_str());

        // return results
        return std::make_pair(router_id, username);
      }
    }
  }

  // If we made it here, it means that config exists, [metadata_cache] exists,
  // but [metadata_cache].metadata_cluster does not exist or it's different
  // from `cluster_name`.
  if (!forcing_overwrite) {
    std::string msg;
    msg +=
        "The given Router instance is already configured for a cluster named "
        "'" +
        existing_cluster + "'.\n";
    msg +=
        "If you'd like to replace it, please use the --force configuration "
        "option.";
    // XXX when multiple-clusters is supported, also suggest --add
    throw std::runtime_error(msg);
  } else {
    return kFoundNoUsableAccount;
  }
}

/* virtual */
void ConfigGenerator::set_script_permissions(
    const std::string &script_path,
    const std::map<std::string, std::string> &options) {
// we only call this method from unix-specific code
#ifndef _WIN32
  if (::chmod(script_path.c_str(), kStrictDirectoryPerm) < 0) {
    std::cerr << "Could not change permissions for " << script_path << ": "
              << get_strerror(errno) << "\n";
  }
  set_file_owner(options, script_path);
#endif
}

void ConfigGenerator::create_start_script(
    const std::string &directory, bool interactive_master_key,
    const std::map<std::string, std::string> &options) {
#ifdef _WIN32

  std::ofstream script;
  std::string script_path = directory + "/start.ps1";

  script.open(script_path);
  if (script.fail()) {
    throw std::runtime_error("Could not open " + script_path +
                             " for writing: " + get_strerror(errno));
  }
  script << "$env:path += \";" << find_plugin_path() << "\"" << std::endl;
  script << "[Environment]::SetEnvironmentVariable(\"ROUTER_PID\","
         << "\"" << directory << "\\"
         << "mysqlrouter.pid\", \"Process\")" << std::endl;
  script << "Start-Process \"" << find_executable_path() << "\" \" -c "
         << directory << "/mysqlrouter.conf\""
         << " -WindowStyle Hidden" << std::endl;
  script.close();

#else

  std::ofstream script;
  std::string script_path = directory + "/start.sh";

  script.open(script_path);
  if (script.fail()) {
    throw std::runtime_error("Could not open " + script_path +
                             " for writing: " + get_strerror(errno));
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
    std::string main_cmd = "ROUTER_PID=$basedir/mysqlrouter.pid " +
                           find_executable_path() +
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

  std::ofstream script;
  const std::string script_path = directory + "/stop.ps1";

  script.open(script_path);
  if (script.fail()) {
    throw std::runtime_error("Could not open " + script_path +
                             " for writing: " + get_strerror(errno));
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
    throw std::runtime_error("Could not open " + script_path +
                             " for writing: " + get_strerror(errno));
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
    const std::string &file_path) {
#ifndef _WIN32
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
