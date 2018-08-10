/*
  Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.

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
#include "common.h"
#include "dim.h"
#include "harness_assert.h"
#include "keyring/keyring_manager.h"
#include "mysql/harness/config_parser.h"
#include "mysql/harness/filesystem.h"
#include "mysql/harness/logging/logging.h"
#include "mysqlrouter/sha1.h"
#include "mysqlrouter/uri.h"
#include "random_generator.h"
#include "rapidjson/rapidjson.h"
#include "router_app.h"
#include "tcp_address.h"
#include "utils.h"
IMPORT_LOG_FUNCTIONS()

#ifdef _WIN32
#include <Windows.h>
#define strcasecmp _stricmp
#else
#include <sys/stat.h>
#endif

#include <algorithm>
#include <cstring>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>

#include "cluster_metadata.h"

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
static constexpr uint32_t kMaxRouterId =
    999999;  // max router id is 6 digits due to username size constraints
static constexpr unsigned kNumRandomChars = 12;
static constexpr unsigned kDefaultPasswordRetries =
    20;  // number of the retries when generating random password
         // for the router user during the bootstrap
static constexpr unsigned kMaxPasswordRetries = 10000;

using mysql_harness::DIM;
using mysql_harness::Path;
using mysql_harness::TCPAddress;
using mysql_harness::UniquePtr;
using mysql_harness::get_strerror;
using mysql_harness::truncate_string;
using namespace mysqlrouter;

namespace {
struct password_too_weak : public std::runtime_error {
  using std::runtime_error::runtime_error;
};

struct plugin_not_loaded : public std::runtime_error {
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

/**
 * Return a string representation of the input character string.
 *
 * @param input_str A character string.
 *
 * @return A string object encapsulation of the input character string. An empty
 *         string if input string is nullptr.
 */
static std::string get_string(const char *input_str) {
  if (input_str == nullptr) {
    return "";
  }
  return std::string(input_str);
}

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

ConfigGenerator::ConfigGenerator(
#ifndef _WIN32
    SysUserOperationsBase *sys_user_operations
#endif
    )
    : connect_timeout_(MySQLSession::kDefaultConnectTimeout),
      read_timeout_(MySQLSession::kDefaultReadTimeout)
#ifndef _WIN32
      ,
      sys_user_operations_(sys_user_operations)
#endif
{
}

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
  mysql_ssl_mode ssl_enum = MySQLSession::parse_ssl_mode(ssl_mode);

  // set ssl mode + server authentication options
  sess->set_ssl_options(ssl_enum, tls_version, ssl_cipher, ssl_ca, ssl_capath,
                        ssl_crl, ssl_crlpath);

  // set client authentication options
  if (!ssl_cert.empty() || !ssl_key.empty()) {
    sess->set_ssl_cert(ssl_cert, ssl_key);
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
  } catch (std::exception &e) {
    log_error("Failed determining if metadata connection uses SSL: %s",
              e.what());
    throw std::runtime_error(e.what());
  }
}

// throws MySQLSession::Error, std::runtime_error, std::out_of_range,
// std::logic_error
void ConfigGenerator::init(
    const std::string &server_url,
    const std::map<std::string, std::string> &bootstrap_options) {
  // TODO: This function is quite lengthy and seems to be doing several things.
  //       It should probably be broken up into smaller parts.

  // ensure defaults are in place in case ::init() gets called again
  // and they aren't overwritten by bootstrap_options
  connect_timeout_ = MySQLSession::kDefaultConnectTimeout;
  read_timeout_ = MySQLSession::kDefaultReadTimeout;
  std::string uri;

  // check options
  if (bootstrap_options.find("base-port") != bootstrap_options.end()) {
    char *end = NULL;
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

  const std::string default_schema = "mysql://";
  // Extract connection information from the bootstrap server URL.
  if (server_url.compare(0, default_schema.size(), default_schema) != 0) {
    uri = default_schema + server_url;
  } else {
    uri = server_url;
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

  const auto &it = bootstrap_options.find("bootstrap_socket");
  std::string socket_name = (it != bootstrap_options.end()) ? it->second : "";

  if (socket_name.size() > 0) {
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

  mysql_ = DIM::instance().new_MySQLSession();
  try {
    set_ssl_options(mysql_.get(), bootstrap_options);
    mysql_->connect(u.host, u.port, u.username, u.password, socket_name, "",
                    connect_timeout_, read_timeout_);
  } catch (const MySQLSession::Error &e) {
    std::stringstream err;
    err << "Unable to connect to the metadata server: " << e.what();
    throw std::runtime_error(err.str());
  }

  // check if the current server meta-data server
  require_innodb_metadata_is_ok(
      mysql_.get());  // throws MySQLSession::Error, std::runtime_error,
                      // std::out_of_range, std::logic_error
  require_innodb_group_replication_is_ok(
      mysql_.get());  // throws MySQLSession::Error, std::runtime_error,
                      // std::out_of_range, std::logic_error

  gr_initial_username_ = u.username;
  gr_initial_password_ = u.password;
  gr_initial_hostname_ = u.host;
  gr_initial_port_ = u.port;
  gr_initial_socket_ = socket_name;
}

void ConfigGenerator::bootstrap_system_deployment(
    const std::string &config_file_path,
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
  UniquePtr<Ofstream> config_file = DIM::instance().new_Ofstream();
  config_file->open(config_file_path + ".tmp");
  if (config_file->fail()) {
    throw std::runtime_error("Could not open " + config_file_path +
                             ".tmp for writing: " + get_strerror(errno));
  }
  auto_clean.add_file_delete(config_file_path + ".tmp");
  bootstrap_deployment(*config_file, _config_file_path, router_name, options,
                       multivalue_options, default_paths, false, auto_clean);
  config_file->close();

  if (backup_config_file_if_different(
          config_file_path, config_file_path + ".tmp", options, &auto_clean)) {
    if (!quiet)
      std::cout << "\nExisting configurations backed up to '"
                << config_file_path << ".bak'" << std::endl;
    auto_clean.add_file_delete(config_file_path);
  }

  // rename the .tmp file to the final file
  if (mysqlrouter::rename_file((config_file_path + ".tmp").c_str(),
                               config_file_path.c_str()) != 0) {
    // log_error("Error renaming %s.tmp to %s: %s", config_file_path.c_str(),
    //  config_file_path.c_str(), get_strerror(errno));
    throw std::runtime_error(
        "Could not save configuration file to final location");
  }
  try {
    mysql_harness::make_file_private(config_file_path);
  } catch (const std::system_error &e) {
#ifdef _WIN32
    if (e.code() !=
        std::error_code(ERROR_INVALID_FUNCTION, std::system_category()))
    // if the filesystem can't set permissions, the test later would fail
#endif
      throw;
  }
  set_file_owner(options, config_file_path);
  auto_clean.clear();
}

// throws std::system_error
static bool is_directory_empty(mysql_harness::Directory dir) {
  for (auto di = dir.begin(); di != dir.end(); ++di) {
    std::string name = (*di).basename().str();
    if (name != "." && name != "..") return false;
  }
  return true;
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
  mysql_harness::Path config_file_path;
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
    if (mkdir(directory.c_str(), kStrictDirectoryPerm) < 0) {
      log_error("Cannot create directory '%s': %s",
                truncate_string(directory).c_str(),
                get_strerror(errno).c_str());
#ifndef _WIN32
      if (errno == EACCES || errno == EPERM) log_error(kAppArmorMsg);
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
  config_file_path = path.join(mysql_harness::Path("mysqlrouter.conf"));
  if (!config_file_path.exists() && !force) {
    bool dir_empty;
    try {
      dir_empty = is_directory_empty(path);
    } catch (std::system_error &e) {
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
      if (mkdir(options[option_name].c_str(), kStrictDirectoryPerm) < 0) {
        if (errno != EEXIST) {
          log_error("Cannot create directory '%s': %s",
                    truncate_string(options[option_name]).c_str(),
                    get_strerror(errno).c_str());
          throw std::runtime_error("Could not create " + option_name +
                                   "directory");
        }
      } else {
        auto_clean.add_directory_delete(options[option_name]);
      }
    }

    // sets the directory owner if the directory exists and --user provided
    set_file_owner(options, options[option_name]);
  }

  // (re-)bootstrap the instance
  std::ofstream config_file;
  config_file.open(config_file_path.str() + ".tmp");
  if (config_file.fail()) {
    log_error("Cannot open config file '%s.tmp' for writing: %s",
              config_file_path.c_str(), get_strerror(errno).c_str());
#ifndef _WIN32
    if (errno == EACCES || errno == EPERM) log_error(kAppArmorMsg);
#endif
    throw std::runtime_error("Could not open " + config_file_path.str() +
                             ".tmp for writing: " + get_strerror(errno));
  }
  auto_clean.add_file_delete(config_file_path.str() + ".tmp");
  set_keyring_info_real_paths(options, path);

  bootstrap_deployment(config_file, config_file_path, router_name, options,
                       multivalue_options, default_paths, true,
                       auto_clean);  // throws std::runtime_error, ?
  config_file.close();

  if (backup_config_file_if_different(
          config_file_path, config_file_path.str() + ".tmp", options)) {
    if (!quiet)
      std::cout << "\nExisting configurations backed up to '"
                << config_file_path << ".bak'" << std::endl;
  }

  // rename the .tmp file to the final file
  if (mysqlrouter::rename_file((config_file_path.str() + ".tmp").c_str(),
                               config_file_path.c_str()) != 0) {
    // log_error("Error renaming %s.tmp to %s: %s", config_file_path.c_str(),
    //  config_file_path.c_str(), get_strerror(errno));
    throw std::runtime_error(
        "Could not move configuration file '" + config_file_path.str() +
        ".tmp' to final location: " + mysqlrouter::get_last_error());
  }

  try {
    mysql_harness::make_file_private(config_file_path.str());
  } catch (const std::system_error &e) {
#ifdef _WIN32
    if (e.code() !=
        std::error_code(ERROR_INVALID_FUNCTION, std::system_category()))
    // if the filesystem can't set permissions, the test later would fail
#endif
      throw;
  }
  set_file_owner(options, config_file_path.str());

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
}

ConfigGenerator::Options ConfigGenerator::fill_options(
    bool multi_master, const std::map<std::string, std::string> &user_options) {
  std::string bind_address{"0.0.0.0"};
  bool use_sockets = false;
  bool skip_tcp = false;
  bool skip_classic_protocol = false;
  bool skip_x_protocol = false;
  int base_port = 0;
  if (user_options.find("base-port") != user_options.end()) {
    char *end = NULL;
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
  options.multi_master = multi_master;
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
      if (!multi_master) options.ro_endpoint.socket = kROSocketName;
    }
    if (!skip_tcp) {
      options.rw_endpoint.port = base_port == 0 ? kDefaultRWPort : base_port++;
      if (!multi_master)
        options.ro_endpoint.port =
            base_port == 0 ? kDefaultROPort : base_port++;
    }
  }
  if (!skip_x_protocol) {
    if (use_sockets) {
      options.rw_x_endpoint.socket = kRWXSocketName;
      if (!multi_master) options.ro_x_endpoint.socket = kROXSocketName;
    }
    if (!skip_tcp) {
      options.rw_x_endpoint.port =
          base_port == 0 ? kDefaultRWXPort : base_port++;
      if (!multi_master)
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

  return options;
}

namespace {

unsigned get_password_retries(
    const std::map<std::string, std::string> &user_options) {
  if (user_options.find("password-retries") == user_options.end()) {
    return kDefaultPasswordRetries;
  }

  char *end = NULL;
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
  kSyntaxError = 1064,
  kSuperReadOnly = 1290,
  kLostConnection = 2013,
};

/**
 * Group Replication-aware decorator for MySQL Sessions
 */
class GrAwareDecorator {
 public:
  GrAwareDecorator(
      MySQLSession &sess, const std::string &gr_initial_username,
      const std::string &gr_initial_password,
      const std::string &gr_initial_hostname, unsigned long gr_initial_port,
      const std::string &gr_initial_socket, unsigned long connection_timeout,
      std::set<MySQLErrorc> failure_codes = {MySQLErrorc::kSuperReadOnly,
                                             MySQLErrorc::kLostConnection})
      : mysql_(sess),
        gr_initial_username_(gr_initial_username),
        gr_initial_password_(gr_initial_password),
        gr_initial_hostname_(gr_initial_hostname),
        gr_initial_port_(gr_initial_port),
        gr_initial_socket_(gr_initial_socket),
        connection_timeout_(connection_timeout),
        failure_codes_(failure_codes) {}

  template <class R>
  R failover_on_failure(std::function<R()> wrapped_func);

 private:
  std::vector<std::tuple<std::string, unsigned long>>
  fetch_group_replication_hosts();

  MySQLSession &mysql_;
  const std::string &gr_initial_username_;
  const std::string &gr_initial_password_;
  const std::string &gr_initial_hostname_;
  unsigned long gr_initial_port_;
  const std::string &gr_initial_socket_;
  unsigned long connection_timeout_;
  std::set<MySQLErrorc> failure_codes_;
};

/**
 * group replication aware failover
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
R GrAwareDecorator::failover_on_failure(std::function<R()> wrapped_func) {
  bool fetched_gr_servers = false;
  std::vector<std::tuple<std::string, unsigned long>> gr_servers;

  // init it once, even though we'll never use it
  auto gr_servers_it = gr_servers.begin();

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
        if (!fetched_gr_servers) {
          // lazy fetch the GR members
          //
          fetched_gr_servers = true;

          log_info("Fetching Group Replication Members");

          for (auto &gr_node : fetch_group_replication_hosts()) {
            auto const &gr_host = std::get<0>(gr_node);
            auto gr_port = std::get<1>(gr_node);

            // if we connected through TCP/IP, ignore the initial host
            if (gr_initial_socket_.size() == 0 &&
                (gr_host == gr_initial_hostname_ &&
                 gr_port == gr_initial_port_)) {
              continue;
            }

            log_debug("added GR node: %s:%ld", gr_host.c_str(), gr_port);
            gr_servers.emplace_back(gr_host, gr_port);
          }

          // get a new iterator as the old one is now invalid
          gr_servers_it = gr_servers.begin();
        } else {
          std::advance(gr_servers_it, 1);
        }

        if (gr_servers_it == gr_servers.end()) {
          throw std::runtime_error(
              "no more nodes to fail-over too, giving up.");
        }

        if (mysql_.is_connected()) {
          log_info("%s", "disconnecting from mysql-server");
          mysql_.disconnect();
        }

        auto const &tp = *gr_servers_it;

        auto const &gr_host = std::get<0>(tp);
        auto gr_port = std::get<1>(tp);

        log_info("trying to connecting to mysql-server at %s:%ld",
                 gr_host.c_str(), gr_port);

        try {
          mysql_.connect(gr_host, gr_port, gr_initial_username_,
                         gr_initial_password_, "", "", connection_timeout_);
        } catch (const std::exception &inner_e) {
          log_info("Failed connecting to %s:%ld: %s, trying next",
                   gr_host.c_str(), gr_port, inner_e.what());
        }
        // if this fails, we should just skip it and go to the next
      } while (!mysql_.is_connected());
    }
  } while (true);
}

std::vector<std::tuple<std::string, unsigned long>>
GrAwareDecorator::fetch_group_replication_hosts() {
  std::ostringstream query;

  // Query the name of the replicaset, the servers in the replicaset and the
  // router credentials using the URL of a server in the replicaset.
  //
  // order by member_role (in 8.0 and later) to sort PRIMARY over SECONDARY
  query << "SELECT member_host, member_port "
           "  FROM performance_schema.replication_group_members "
           " /*!80002 ORDER BY member_role */";

  try {
    std::vector<std::tuple<std::string, unsigned long>> gr_servers;

    mysql_.query(query.str(),
                 [&gr_servers](const std::vector<const char *> &row) -> bool {
                   gr_servers.push_back(std::make_tuple(std::string(row[0]),
                                                        std::stoul(row[1])));
                   return true;  // don't stop
                 });

    return gr_servers;
  } catch (MySQLSession::Error &e) {
    // log_error("MySQL error: %s (%u)", e.what(), e.code());
    // log_error("    Failed query: %s", query.str().c_str());
    throw std::runtime_error(std::string("Error querying metadata: ") +
                             e.what());
  }
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

void ConfigGenerator::bootstrap_deployment(
    std::ostream &config_file, const mysql_harness::Path &config_file_path,
    const std::string &router_name,
    const std::map<std::string, std::string> &user_options,
    const std::map<std::string, std::vector<std::string>> &multivalue_options,
    const std::map<std::string, std::string> &default_paths,
    bool directory_deployment, AutoCleaner &auto_clean) {
  std::string primary_cluster_name;
  std::string primary_replicaset_servers;
  std::string primary_replicaset_name;
  bool multi_master = false;
  bool force = user_options.find("force") != user_options.end();
  bool quiet = user_options.find("quiet") != user_options.end();
  uint32_t router_id = 0;
  std::string username;

  using RandomGen = mysql_harness::RandomGeneratorInterface;
  RandomGen &rg = mysql_harness::DIM::instance().get_RandomGenerator();

  fetch_bootstrap_servers(primary_replicaset_servers, primary_cluster_name,
                          primary_replicaset_name, multi_master);

  if (config_file_path.exists()) {
    std::tie(router_id, username) = get_router_id_and_name_from_config(
        config_file_path.str(), primary_cluster_name, force);
  }

  if (!quiet) {
    std::string prefix;
    if (router_id > 0) {
      prefix = "\nReconfiguring";
    } else {
      prefix = "\nBootstrapping";
    }
    if (directory_deployment) {
      std::cout << prefix << " MySQL Router instance at '"
                << config_file_path.dirname() << "'..." << std::endl;
    } else {
      std::cout << prefix << " system MySQL Router instance..." << std::endl;
    }
  }

  std::string password;

  Options options(fill_options(multi_master, user_options));

  GrAwareDecorator gr_aware(*mysql_, gr_initial_username_, gr_initial_password_,
                            gr_initial_hostname_, gr_initial_port_,
                            gr_initial_socket_, connect_timeout_);

  {
    const std::string rw_endpoint = str(options.rw_endpoint);
    const std::string ro_endpoint = str(options.ro_endpoint);
    const std::string rw_x_endpoint = str(options.rw_x_endpoint);
    const std::string ro_x_endpoint = str(options.ro_x_endpoint);

    std::tie(password) =
        gr_aware.failover_on_failure<std::tuple<std::string>>([&]() {
          return try_bootstrap_deployment(router_id, username, router_name, rg,
                                          user_options, multivalue_options,
                                          rw_endpoint, ro_endpoint,
                                          rw_x_endpoint, ro_x_endpoint);
        });
  }
  init_keyring_and_master_key(auto_clean, user_options, router_id);

  {
    mysql_harness::Keyring *keyring = mysql_harness::get_keyring();

    keyring->store(username, kKeyringAttributePassword, password);
    try {
      mysql_harness::flush_keyring();
    } catch (std::exception &e) {
      throw std::runtime_error(
          std::string("Error storing encrypted password to disk: ") + e.what());
    }
  }

  options.keyring_file_path = keyring_info_.get_keyring_file();
  if (keyring_info_.use_master_key_file())
    options.keyring_master_key_file_path = keyring_info_.get_master_key_file();

#ifndef _WIN32
  /* Currently at this point the logger is not yet initialized but while
   * bootstraping with the --user=<user> option we need to create a log file and
   * chown it to the <user>. Otherwise when the router gets launched later (not
   * bootstrap) with the same --user=<user> option, the user might not have
   * right to the logging directory.
   */
  assert(default_paths.find("logging_folder") != default_paths.end());
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

  auto system_username = (user_options.find("user") != user_options.end())
                             ? user_options.at("user")
                             : "";

  // generate the new config file
  create_config(config_file, router_id, router_name, system_username,
                primary_replicaset_servers, primary_cluster_name,
                primary_replicaset_name, username, options, !quiet);
}

void ConfigGenerator::ensure_router_id_is_ours(
    uint32_t &router_id, std::string &username,
    const std::string &hostname_override,
    MySQLInnoDBClusterMetadata &metadata) {
  // if router data is valid
  try {
    metadata.check_router_id(router_id, hostname_override);
  } catch (
      const mysql_harness::SocketOperationsBase::LocalHostnameResolutionError
          &e) {
    throw std::runtime_error(
        std::string("Could not verify if this Router instance is already "
                    "registered with the "
                    "cluster because querying this host's hostname from OS "
                    "failed:\n  ") +
        e.what() +
        "\nYou may want to try --report-host option to manually supply this "
        "hostname.");
  } catch (const std::exception &e) {
    log_warning("WARNING: %s", e.what());
    // TODO: abort here and suggest --force to force reconfiguration?
    router_id = 0;
    username.clear();
  }
}

void ConfigGenerator::register_router_and_set_username(
    uint32_t &router_id, const std::string &router_name, std::string &username,
    const std::string &hostname_override, bool force,
    MySQLInnoDBClusterMetadata &metadata,
    mysql_harness::RandomGeneratorInterface &rg) {
  try {
    router_id = metadata.register_router(router_name, force, hostname_override);
  } catch (
      const mysql_harness::SocketOperationsBase::LocalHostnameResolutionError
          &e) {
    throw std::runtime_error(
        std::string(
            "Could not register this Router instance with the cluster because "
            "querying this host's hostname from OS failed:\n  ") +
        e.what() +
        "\nYou may want to try --report-host option to manually supply this "
        "hostname.");
  } catch (MySQLSession::Error &e) {
    if (e.code() == 1062) {  // duplicate key
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

  using RandomGen = mysql_harness::RandomGeneratorInterface;
  username =
      "mysql_router" + std::to_string(router_id) + "_" +
      rg.generate_identifier(kNumRandomChars, RandomGen::AlphabetDigits |
                                                  RandomGen::AlphabetLowercase);
}

std::tuple<std::string> ConfigGenerator::try_bootstrap_deployment(
    uint32_t &router_id, std::string &username, const std::string &router_name,
    mysql_harness::RandomGeneratorInterface &rg,
    const std::map<std::string, std::string> &user_options,
    const std::map<std::string, std::vector<std::string>> &multivalue_options,
    const std::string &rw_endpoint, const std::string &ro_endpoint,
    const std::string &rw_x_endpoint, const std::string &ro_x_endpoint) {
  MySQLSession::Transaction transaction(mysql_.get());
  MySQLInnoDBClusterMetadata metadata(mysql_.get());

  // set hostname override if provided
  const auto &it = user_options.find("report-host");
  const std::string &hostname_override =
      (it != user_options.end()) ? it->second : "";
  // if reconfiguration;
  if (router_id > 0) {
    // throws std::runtime if our hostname couldn't be queried,
    // resets router_id to 0 and clears username if router_id doesn't exist or
    // belongs to a different host.
    // NOTE that these were passed by reference to us, thus they are stored
    //      outside of this function and will be persisted to the next call.
    ensure_router_id_is_ours(router_id, username, hostname_override, metadata);
  }

  // if router not registered yet (or router_id was invalid)
  if (router_id == 0) {
    assert(username.empty());
    bool force = user_options.find("force") != user_options.end();

    // throws std::runtime on failure
    register_router_and_set_username(router_id, router_name, username,
                                     hostname_override, force, metadata, rg);
  }

  assert(router_id);
  assert(!username.empty());

  // we erase any old accounts with the same name, if they exist
  delete_account_for_all_hosts(
      username);  // throws MySQLSession::Error, std::logic_error

  // create_router_accounts() throws many things, see its description
  std::string password =
      create_router_accounts(user_options, multivalue_options, username);

  metadata.update_router_info(router_id, rw_endpoint, ro_endpoint,
                              rw_x_endpoint, ro_x_endpoint);

  transaction.commit();

  return std::make_tuple(password);
}

void ConfigGenerator::init_keyring_file(uint32_t router_id) {
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
                                         keyring_info_.get_master_key(), true);
  } else if (keyring_info_.use_master_key_file()) {
    try {
      mysql_harness::init_keyring(keyring_info_.get_keyring_file(),
                                  keyring_info_.get_master_key_file(), true);
    } catch (mysql_harness::invalid_master_keyfile &) {
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
                                         master_key, true);
  }
}

void ConfigGenerator::fetch_bootstrap_servers(std::string &bootstrap_servers,
                                              std::string &metadata_cluster,
                                              std::string &metadata_replicaset,
                                              bool &multi_master) {
  std::ostringstream query;

  // Query the name of the replicaset, the servers in the replicaset and the
  // router credentials using the URL of a server in the replicaset.
  query << "SELECT "
           "F.cluster_name, "
           "R.replicaset_name, "
           "R.topology_type, "
           "JSON_UNQUOTE(JSON_EXTRACT(I.addresses, '$.mysqlClassic')) "
           "FROM "
           "mysql_innodb_cluster_metadata.clusters AS F, "
           "mysql_innodb_cluster_metadata.instances AS I, "
           "mysql_innodb_cluster_metadata.replicasets AS R "
           "WHERE "
           "R.replicaset_id = "
           "(SELECT replicaset_id FROM mysql_innodb_cluster_metadata.instances "
           "WHERE "
           "mysql_server_uuid = @@server_uuid)"
           "AND "
           "I.replicaset_id = R.replicaset_id "
           "AND "
           "R.cluster_id = F.cluster_id";

  metadata_cluster = "";
  metadata_replicaset = "";
  bootstrap_servers = "";
  try {
    mysql_->query(
        query.str(),
        [&metadata_cluster, &metadata_replicaset, &bootstrap_servers,
         &multi_master](const std::vector<const char *> &row) -> bool {
          if (metadata_cluster == "") {
            metadata_cluster = get_string(row[0]);
          } else if (metadata_cluster != get_string(row[0])) {
            // metadata with more than 1 replicaset not currently supported
            throw std::runtime_error("Metadata contains more than one cluster");
          }
          if (metadata_replicaset == "") {
            metadata_replicaset = get_string(row[1]);
          } else if (metadata_replicaset != get_string(row[1])) {
            // metadata with more than 1 replicaset not currently supported
            throw std::runtime_error(
                "Metadata contains more than one replica-set");
          }
          if (bootstrap_servers != "") bootstrap_servers += ",";
          if (row[2]) {
            if (strcmp(row[2], "mm") == 0)
              multi_master = true;
            else if (strcmp(row[2], "pm") == 0)
              multi_master = false;
            else
              throw std::runtime_error("Unknown topology type in metadata: " +
                                       std::string(row[2]));
          }
          bootstrap_servers += "mysql://" + get_string(row[3]);
          return true;
        });
  } catch (MySQLSession::Error &e) {
    // log_error("MySQL error: %s (%u)", e.what(), e.code());
    // log_error("    Failed query: %s", query.str().c_str());
    throw std::runtime_error(std::string("Error querying metadata: ") +
                             e.what());
  }
  if (metadata_cluster.empty())
    throw std::runtime_error("No clusters defined in metadata server");
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
    char *tmp = realpath(g_program_name.c_str(), NULL);
    harness_assert(tmp);  // will fail if g_program_name provides bogus path
    std::string path(tmp);
    free(tmp);
    return path;
  } else {
    std::string path(std::getenv("PATH"));
    char *last = NULL;
    char *p = strtok_r(&path[0], ":", &last);
    while (p) {
      if (*p && p[strlen(p) - 1] == '/') p[strlen(p) - 1] = 0;
      std::string tmp(std::string(p) + "/" + g_program_name);
      if (access(tmp.c_str(), R_OK | X_OK) == 0) {
        return tmp;
      }
      p = strtok_r(NULL, ":", &last);
    }
  }
#endif
  throw std::logic_error("Could not find own installation directory");
}

std::string ConfigGenerator::endpoint_option(const Options &options,
                                             const Options::Endpoint &ep) {
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

void ConfigGenerator::create_config(
    std::ostream &cfp, uint32_t router_id, const std::string &router_name,
    const std::string &system_username,
    const std::string &bootstrap_server_addresses,
    const std::string &metadata_cluster, const std::string &metadata_replicaset,
    const std::string &username, const Options &options, bool print_configs) {
  cfp << "# File automatically generated during MySQL Router bootstrap\n";

  cfp << "[DEFAULT]\n";
  if (!router_name.empty()) cfp << "name=" << router_name << "\n";
  if (!system_username.empty()) cfp << "user=" << system_username << "\n";
  if (!options.override_logdir.empty())
    cfp << "logging_folder=" << options.override_logdir << "\n";
  if (!options.override_rundir.empty())
    cfp << "runtime_folder=" << options.override_rundir << "\n";
  if (!options.override_datadir.empty())
    cfp << "data_folder=" << options.override_datadir << "\n";
  if (!options.keyring_file_path.empty())
    cfp << "keyring_path=" << options.keyring_file_path << "\n";
  if (!options.keyring_master_key_file_path.empty())
    cfp << "master_key_path=" << options.keyring_master_key_file_path << "\n";
  if (!keyring_info_.get_master_key_reader().empty())
    cfp << "master_key_reader=" << keyring_info_.get_master_key_reader()
        << "\n";
  if (!keyring_info_.get_master_key_writer().empty())
    cfp << "master_key_writer=" << keyring_info_.get_master_key_writer()
        << "\n";

  cfp << "connect_timeout=" << connect_timeout_ << "\n";
  cfp << "read_timeout=" << read_timeout_ << "\n";

  const std::string metadata_key = metadata_cluster;
  cfp << "\n"
      << "[" << mysql_harness::logging::kConfigSectionLogger << "]\n"
      << mysql_harness::logging::kConfigOptionLogLevel << " = INFO\n"
      << "\n"
      << "[metadata_cache:" << metadata_key << "]\n"
      << "router_id=" << router_id << "\n"
      << "bootstrap_server_addresses=" << bootstrap_server_addresses << "\n"
      << "user=" << username << "\n"
      << "metadata_cluster=" << metadata_cluster << "\n"
      << "ttl=" << mysqlrouter::ms_to_seconds_string(kDefaultMetadataTTL)
      << "\n";

  // SSL options
  cfp << option_line("ssl_mode", options.ssl_options.mode);
  cfp << option_line("ssl_cipher", options.ssl_options.cipher);
  cfp << option_line("tls_version", options.ssl_options.tls_version);
  cfp << option_line("ssl_ca", options.ssl_options.ca);
  cfp << option_line("ssl_capath", options.ssl_options.capath);
  cfp << option_line("ssl_crl", options.ssl_options.crl);
  cfp << option_line("ssl_crlpath", options.ssl_options.crlpath);
  // Note: we don't write cert and key because
  // creating router accounts with REQUIRE X509 is not yet supported.
  // The cert and key options passed to bootstrap if for the bootstrap
  // connection itself.
  cfp << "\n";

  const std::string fast_router_key = metadata_key + "_" + metadata_replicaset;
  if (options.rw_endpoint) {
    cfp << "[routing:" << fast_router_key << "_rw]\n"
        << endpoint_option(options, options.rw_endpoint) << "\n"
        << "destinations=metadata-cache://" << metadata_key << "/"
        << metadata_replicaset << "?role=PRIMARY\n"
        << "routing_strategy=round-robin\n"
        << "protocol=classic\n"
        << "\n";
  }
  if (options.ro_endpoint) {
    cfp << "[routing:" << fast_router_key << "_ro]\n"
        << endpoint_option(options, options.ro_endpoint) << "\n"
        << "destinations=metadata-cache://" << metadata_key << "/"
        << metadata_replicaset << "?role=SECONDARY\n"
        << "routing_strategy=round-robin\n"
        << "protocol=classic\n"
        << "\n";
  }
  if (options.rw_x_endpoint) {
    cfp << "[routing:" << fast_router_key << "_x_rw]\n"
        << endpoint_option(options, options.rw_x_endpoint) << "\n"
        << "destinations=metadata-cache://" << metadata_key << "/"
        << metadata_replicaset << "?role=PRIMARY\n"
        << "routing_strategy=round-robin\n"
        << "protocol=x\n"
        << "\n";
  }
  if (options.ro_x_endpoint) {
    cfp << "[routing:" << fast_router_key << "_x_ro]\n"
        << endpoint_option(options, options.ro_x_endpoint) << "\n"
        << "destinations=metadata-cache://" << metadata_key << "/"
        << metadata_replicaset << "?role=SECONDARY\n"
        << "routing_strategy=round-robin\n"
        << "protocol=x\n"
        << "\n";
  }
  cfp.flush();

  if (print_configs) {
    std::cout << "MySQL Router "
              << ((router_name.empty() || router_name == kSystemRouterName)
                      ? ""
                      : "'" + router_name + "'")
              << " has now been configured for the InnoDB cluster '"
              << metadata_cluster.c_str() << "'"
              << (options.multi_master ? " (multi-master)" : "") << ".\n"
              << std::endl;

    std::cout << "The following connection information can be used to connect "
                 "to the cluster after MySQL Router has been started with "
                 "generated configuration..\n"
              << std::endl;
    if (options.rw_endpoint || options.ro_endpoint) {
      std::cout << "Classic MySQL protocol connections to cluster '"
                << metadata_cluster << "':" << std::endl;
      if (options.rw_endpoint.port > 0)
        std::cout << "- Read/Write Connections: localhost:"
                  << options.rw_endpoint.port << std::endl;
      if (!options.rw_endpoint.socket.empty())
        std::cout << "- Read/Write Connections: " << options.socketsdir << "/"
                  << options.rw_endpoint.socket << std::endl;
      if (options.ro_endpoint.port > 0)
        std::cout << "- Read/Only Connections: localhost:"
                  << options.ro_endpoint.port << std::endl;
      if (!options.ro_endpoint.socket.empty())
        std::cout << "- Read/Only Connections: " << options.socketsdir << "/"
                  << options.ro_endpoint.socket << "\n"
                  << std::endl;
    }
    if (options.rw_x_endpoint || options.ro_x_endpoint) {
      std::cout << "X protocol connections to cluster '" << metadata_cluster
                << "':" << std::endl;
      if (options.rw_x_endpoint.port > 0)
        std::cout << "- Read/Write Connections: localhost:"
                  << options.rw_x_endpoint.port << std::endl;
      if (!options.rw_x_endpoint.socket.empty())
        std::cout << "- Read/Write Connections: " << options.socketsdir << "/"
                  << options.rw_x_endpoint.socket << std::endl;
      if (options.ro_x_endpoint.port > 0)
        std::cout << "- Read/Only Connections: localhost:"
                  << options.ro_x_endpoint.port << std::endl;
      if (!options.ro_x_endpoint.socket.empty())
        std::cout << "- Read/Only Connections: " << options.socketsdir.c_str()
                  << "/" << options.ro_x_endpoint.socket << std::endl;
    }
  }
}

/**
 * create account to be used by Router.
 *
 * `<host>` part of `<user>@<host>` will be %, unless user specified otherwise
 * using --account-host switch. Multiple --account-host switches are allowed.
 */
std::string ConfigGenerator::create_router_accounts(
    const std::map<std::string, std::string> &user_options,
    const std::map<std::string, std::vector<std::string>> &multivalue_options,
    const std::string &username) {
  /*
  Ideally, we create a single account for the specific host that the router is
  running on. But that has several problems in real world, including:
  - if you're configuring on localhost ref to metadata server, the router will
  think it's in localhost and thus it will need 2 accounts: user@localhost
  and user@public_ip... further, there could be more than 1 IP for the host,
  which (like lan IP, localhost, internet IP, VPN IP, IPv6 etc). We don't know
  which ones are needed, so either we need to automatically create all of those
  or have some very complicated and unreliable logic.
  - using hostname is not reliable, because not every place will have name
  resolution availble
  - using IP (even if we can detect it correctly) will not work if IP is not
  static

  Summing up, '%' is the easy way to avoid these problems. But the decision
  ultimately belongs to the user.
  */

  // extract --account-host args; if none were given, default to just one: "%"
  // NOTE: By the time we call this function, all --account-host entries should
  //       be sorted and any non-unique entries eliminated (to ensure CREATE
  //       USER does not get called twice for the same user@host). This happens
  //       at the commandline parsing level during --account-host processing.
  constexpr const char kAccountHost[] = "account-host";
  const std::vector<std::string> &hostnames =
      multivalue_options.count(kAccountHost)
          ? multivalue_options.at(kAccountHost)
          : std::vector<std::string>{"%"};

  // NOTE ON EXCEPTIONS:
  // create_account*() functions throw many things (see their descriptions)
  // - we let the higher-level logic deal with them when that happens.

  // create first account and save password info that got generated in the
  // process
  std::pair<std::string, bool> password_and_is_hashed =
      create_account_with_compliant_password(user_options, username,
                                             hostnames.front());

  // and now we use that password info for creation of remaining accounts
  for (auto it = hostnames.begin() + 1; it != hostnames.end(); ++it) {
    try {
      create_account(username, *it, password_and_is_hashed.first /*password*/,
                     password_and_is_hashed.second /*hash password*/);
    }

    // create_account_with_compliant_password() should have caught these (and
    // dealt with them accordingly), since these occur either always or never.
    // The only way these could occur here is if the Server's responses changed
    // for some reason (reconfigured in the meantime?). Anyhow, probably an
    // unlikely event.
    catch (const plugin_not_loaded &) {
      throw std::runtime_error(
          "Error creating user account: unexpected error: "
          "plugin not loaded (it seems Server changed its password policy, "
          "has it been reconfigured in the meantime?)");
    } catch (const password_too_weak &) {
      throw std::runtime_error(
          "Error creating user account: unexpected error: "
          "password too weak (it seems Server changed its password policy, "
          "has it been reconfigured in the meantime?)");
    } catch (...) {
      throw;  // all others we pass
    }
  }

  return password_and_is_hashed.first;
}

std::pair<std::string, bool>
ConfigGenerator::create_account_with_compliant_password(
    const std::map<std::string, std::string> &user_options,
    const std::string &username, const std::string &hostname) {
  using RandomGen = mysql_harness::RandomGeneratorInterface;
  RandomGen &rg = mysql_harness::DIM::instance().get_RandomGenerator();

  const bool force_password_validation =
      user_options.find("force-password-validation") != user_options.end();
  std::string password;
  unsigned retries =
      get_password_retries(user_options);  // throws std::runtime_error
  if (!force_password_validation) {
    // 1) Try to create an account using mysql_native_password with the hashed
    // password
    //    to avoid validate_password verification (hashing is done inside
    //    create_account())
    password = rg.generate_strong_password(kMetadataServerPasswordLength);
    try {
      // create_account() throws many things, see its description
      create_account(username, hostname, password, true /*hash password*/);
      return std::make_pair(password, true);
    } catch (const plugin_not_loaded &) {
      // fallback to 2)
    }
  }

  // 2) If 1) failed because of the missing mysql_native_password plugin,
  //    or "-force-password-validation" parameter has being used
  //    try to create an account using the password directly
  while (true) {
    password = rg.generate_strong_password(kMetadataServerPasswordLength);

    try {
      // create_account() throws many things, see its description
      create_account(username, hostname, password, false /*hash password*/);
      return std::make_pair(password, false);
    } catch (const password_too_weak &e) {
      if (--retries == 0) {
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
      // times hoping to meet the requirements with the next generated password.
      continue;
    }
  }

  harness_assert_this_should_not_execute();
}

/*
  Create MySQL account for this instance of the router in the target cluster.

  The account will have access to the cluster metadata and to the
  replication_group_members table of the performance_schema.
  Note that this assumes that the metadata schema is stored in the destinations
  cluster and that there is only one replicaset in it.
 */
void ConfigGenerator::create_account(const std::string &username,
                                     const std::string &hostname,
                                     const std::string &password,
                                     bool hash_password) {
  const std::string account = username + "@" + mysql_->quote(hostname);
  log_info("Creating account %s", account.c_str());

  const std::string create_user =
      "CREATE USER " + account + " IDENTIFIED " +
      (hash_password ? "WITH mysql_native_password AS " : "BY ") +
      mysql_->quote(hash_password ? compute_password_hash(password) : password);
  //    + mysql_->quote(password) + " REQUIRE X509";

  const std::vector<std::string> queries{
      create_user,
      "GRANT SELECT ON mysql_innodb_cluster_metadata.* TO " + account,
      "GRANT SELECT ON performance_schema.replication_group_members TO " +
          account,
      "GRANT SELECT ON performance_schema.replication_group_member_stats TO " +
          account};

  for (auto &q : queries) {
    try {
      mysql_->execute(q);  // throws MySQLSession::Error, std::logic_error
    } catch (MySQLSession::Error &e) {
      // log_error("%s: executing query: %s", e.what(), q.c_str());
      try {
        mysql_->execute("ROLLBACK");
      } catch (...) {
        // log_error("Could not rollback transaction explicitly.");
      }
      std::string err_msg =
          std::string("Error creating MySQL account for router: ") + e.what();
      if (e.code() ==
          1819) {  // password does not satisfy the current policy requirements
        throw password_too_weak(err_msg);
      }
      if (e.code() == 1524) {  // plugin not loaded
        throw plugin_not_loaded(err_msg);
      }

      // it shouldn't have failed, let the upper layers try to handle it
      throw MySQLSession::Error(err_msg, e.code());
    }
  }
}

void ConfigGenerator::delete_account_for_all_hosts(
    const std::string &username) {
  std::vector<std::string> hostnames;

  log_info("Checking for old Router accounts");
  {
    // throws MySQLSession::Error, should be handled by caller
    mysql_->query("SELECT host FROM mysql.user WHERE user = '" + username + "'",
                  [&hostnames](const std::vector<const char *> &row) -> bool {
                    harness_assert(row.size() == 1);  // expect just 1 column
                    hostnames.push_back(row[0]);
                    return true;  // don't stop
                  });

    if (hostnames.size() < 1) {
      log_debug("No prior Router accounts found");
      return;
    }
  }

  log_info("Found old Router accounts, removing");
  {
    // build DROP USER statement to erase all existing accounts
    std::string query = "DROP USER ";
    for (std::string &host : hostnames)
      query += username + "@" + mysql_->quote(host) + ",";
    query.resize(query.size() - 1);  // erase last ','

    // throws MySQLSession::Error and std::logic_error, both should be handled
    // by caller
    mysql_->execute(query);
  }
}

/**
 * Get router_id name values associated with a metadata_cache configuration for
 * the given cluster_name.
 *
 * The lookup is done through the metadata_cluster option inside the
 * metadata_cache section.
 */
std::pair<uint32_t, std::string>
ConfigGenerator::get_router_id_and_name_from_config(
    const std::string &config_file_path, const std::string &cluster_name,
    bool forcing_overwrite) {
  mysql_harness::Path path(config_file_path);
  std::string existing_cluster;
  if (path.exists()) {
    mysql_harness::Config config(mysql_harness::Config::allow_keys);
    config.read(path);
    mysql_harness::Config::SectionList sections;
    if (config.has_any("metadata_cache")) {
      sections = config.get("metadata_cache");
    } else {
      return std::make_pair(0, "");
    }
    if (sections.size() > 1) {
      throw std::runtime_error(
          "Bootstrapping of Router with multiple metadata_cache sections not "
          "supported");
    }
    for (auto const &section : sections) {
      if (section->has("metadata_cluster")) {
        existing_cluster = section->get("metadata_cluster");
        if (existing_cluster == cluster_name) {
          // get router_id
          if (!section->has("router_id")) {
            log_warning("WARNING: router_id not set for cluster '%s'",
                        cluster_name.c_str());
            return std::make_pair(0, "");
          }
          std::string tmp = section->get("router_id");
          char *end;
          unsigned long r = std::strtoul(tmp.c_str(), &end, 10);
          if (end == tmp.c_str() || errno == ERANGE) {
            throw std::runtime_error("Invalid router_id '" + tmp +
                                     "' for cluster '" + cluster_name +
                                     "' in " + config_file_path);
          }

          // get username, example: user=mysql_router4_kot8tcepf3kn
          if (!section->has("user")) {
            log_warning("WARNING: user not set for cluster '%s'",
                        cluster_name.c_str());
            return std::make_pair(0, "");
          }
          std::string user = section->get("user");

          // return results
          return std::make_pair(static_cast<uint32_t>(r), user);
        }
      }
    }
  }
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
  }
  return std::make_pair(0, "");
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
      // if --user was not given, we have no choice but to only provide the code
      // for that case
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
  script
      << "else { Write-Host \"Error when trying to stop mysqlrouter process\" }"
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
    // if the old and new config files are the same, don't bother with a backup
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
