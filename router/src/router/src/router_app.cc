/*
  Copyright (c) 2015, 2019, Oracle and/or its affiliates. All rights reserved.

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

#include "router_app.h"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "common.h"
#include "config_files.h"
#include "config_generator.h"
#include "dim.h"
#include "harness_assert.h"
#include "hostname_validator.h"
#include "keyring/keyring_manager.h"
#include "mysql/harness/config_parser.h"
#include "mysql/harness/dynamic_state.h"
#include "mysql/harness/filesystem.h"
#include "mysql/harness/logging/logger_plugin.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/logging/registry.h"
#include "mysql/harness/vt100.h"
#include "mysql_session.h"
#include "print_version.h"
#include "welcome_copyright_notice.h"

#ifndef _WIN32
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
const char dir_sep = '/';
const std::string path_sep = ":";
#else
#include <process.h>
#include <windows.h>
#define getpid _getpid
#include <io.h>
#include <string.h>
#include "mysqlrouter/windows/password_vault.h"
#define strtok_r strtok_s
const char dir_sep = '\\';
const std::string path_sep = ";";
#endif

IMPORT_LOG_FUNCTIONS()

using mysql_harness::DIM;
using mysql_harness::get_strerror;
using mysql_harness::truncate_string;
using mysqlrouter::SysUserOperations;
using mysqlrouter::SysUserOperationsBase;
using mysqlrouter::string_format;
using mysqlrouter::substitute_envvar;
using mysqlrouter::wrap_string;
using std::string;
using std::vector;

static const char *kDefaultKeyringFileName = "keyring";
static const char kProgramName[] = "mysqlrouter";

// throws std::runtime_error, ...?
/*static*/
std::string MySQLRouter::find_full_path(const std::string &argv0) {
#ifdef _WIN32
  // the bin folder is not usually in the path, just the lib folder
  char szPath[MAX_PATH];
  if (GetModuleFileName(NULL, szPath, sizeof(szPath)) != 0)
    return std::string(szPath);
#else
  mysql_harness::Path p_argv0(argv0);
  // Path normalizes '\' to '/'
  if (p_argv0.str().find('/') != std::string::npos) {
    // Path is either absolute or relative to the current working dir, so
    // we can use realpath() to find the full absolute path
    mysql_harness::Path path2(p_argv0.real_path());
    const char *tmp = path2.c_str();
    std::string path(tmp);
    return path;
  } else {
    // Program was found via PATH lookup by the shell, so we
    // try to find the program in one of the PATH dirs
    std::string path(std::getenv("PATH"));
    char *last = NULL;
    char *p = strtok_r(&path[0], path_sep.c_str(), &last);
    while (p) {
      std::string tmp(std::string(p) + dir_sep + argv0);
      if (mysqlrouter::my_check_access(tmp)) {
        mysql_harness::Path path1(tmp.c_str());
        mysql_harness::Path path2(path1.real_path());
        return path2.str();
      }
      p = strtok_r(NULL, path_sep.c_str(), &last);
    }
  }
#endif
  throw std::logic_error("Could not find own installation directory");
}

static inline void set_signal_handlers() {
#ifndef _WIN32
  // until we have proper signal handling we need at least
  // mask out broken pipe to prevent terminating the router
  // if the receiving end closes the socket while the router
  // writes to it
  signal(SIGPIPE, SIG_IGN);
#endif
}

// Check if the value is valid regular filename and if it is add to the vector,
// if it is not throw an exception
static void check_and_add_conf(std::vector<string> &configs,
                               const std::string &value) {
  mysql_harness::Path cfg_file_path;
  try {
    cfg_file_path = mysql_harness::Path(value);
  } catch (const std::invalid_argument &exc) {
    throw std::runtime_error(
        string_format("Failed reading configuration file: %s", exc.what()));
  }

  if (cfg_file_path.is_regular()) {
    configs.push_back(cfg_file_path.real_path().str());
  } else if (cfg_file_path.is_directory()) {
    throw std::runtime_error(string_format(
        "Expected configuration file, got directory name: %s", value.c_str()));
  } else {
    throw std::runtime_error(
        string_format("Failed reading configuration file: %s", value.c_str()));
  }
}

// throws MySQLSession::Error, std::runtime_error, std::out_of_range,
// std::logic_error, ...?
MySQLRouter::MySQLRouter(const mysql_harness::Path &origin,
                         const std::vector<std::string> &arguments,
                         std::ostream &out_stream, std::ostream &err_stream
#ifndef _WIN32
                         ,
                         SysUserOperationsBase *sys_user_operations
#endif
                         )
    : version_(MYSQL_ROUTER_VERSION_MAJOR, MYSQL_ROUTER_VERSION_MINOR,
               MYSQL_ROUTER_VERSION_PATCH),
      arg_handler_(),
      can_start_(false),
      showing_info_(false),
      origin_(origin),
      out_stream_(out_stream),
      err_stream_(err_stream)
#ifndef _WIN32
      ,
      sys_user_operations_(sys_user_operations)
#endif
{
  set_signal_handlers();
  init(arguments);  // throws MySQLSession::Error, std::runtime_error,
                    // std::out_of_range, std::logic_error, ...?
}

// throws MySQLSession::Error, std::runtime_error, std::out_of_range,
// std::logic_error, ...?
MySQLRouter::MySQLRouter(const int argc, char **argv, std::ostream &out_stream,
                         std::ostream &err_stream
#ifndef _WIN32
                         ,
                         SysUserOperationsBase *sys_user_operations
#endif
                         )
    : MySQLRouter(mysql_harness::Path(find_full_path(argv[0])).dirname(),
                  vector<string>({argv + 1, argv + argc}), out_stream,
                  err_stream
#ifndef _WIN32
                  ,
                  sys_user_operations
#endif
      ) {
}

// throws std::runtime_error
void MySQLRouter::parse_command_options(const vector<string> &arguments) {
  prepare_command_options();
  try {
    arg_handler_.process(arguments);
  } catch (const std::invalid_argument &exc) {
    throw std::runtime_error(exc.what());
  }
}

// throws MySQLSession::Error, std::runtime_error, std::out_of_range,
// std::logic_error, ...?
void MySQLRouter::init(const vector<string> &arguments) {
  set_default_config_files(CONFIG_FILES);

  parse_command_options(arguments);  // throws std::runtime_error

  if (showing_info_) {
    return;
  }

  if (!bootstrap_uri_.empty()) {
#ifndef _WIN32
    // If the user does the bootstrap with superuser (uid==0) but did not
    // provide
    // --user option let's encourage her/him to do so.
    // Otherwise [s]he will end up with the files (config, log, etc.) owned
    // by the root user and not accessible by others, which is likely not what
    // was expected. The user still can use --user=root to force using
    // superuser.
    bool user_option = this->bootstrap_options_.count("user") != 0;
    bool superuser = sys_user_operations_->geteuid() == 0;

    if (superuser && !user_option) {
      std::string msg(
          "You are bootstraping as a superuser.\n"
          "This will make all the result files (config etc.) privately owned "
          "by the superuser.\n"
          "Please use --user=username option to specify the user that will be "
          "running the router.\n"
          "Use --user=root if this really should be the superuser.");

      throw std::runtime_error(msg);
    }
#endif

    // default configuration for boostrap is not supported
    // extra configuration for bootstrap is not supported
    ConfigFiles config_files({}, config_files_, {});

    if (!config_files.empty()) {
      DIM::instance().reset_Config();  // simplifies unit tests
      DIM::instance().set_Config(
          [this, &config_files]() { return make_config({}, config_files); },
          std::default_delete<mysql_harness::LoaderConfig>());
      mysql_harness::LoaderConfig &config = DIM::instance().get_Config();

      // reinit logger (right now the logger is configured to log to STDERR,
      // here we re-configure it with settings from config file)
      init_main_logger(config, true);  // true = raw logging mode
    }

    bootstrap(
        bootstrap_uri_);  // throws MySQLSession::Error, std::runtime_error,
                          // std::out_of_range, std::logic_error, ...?
    return;
  }

  check_config_files();
  can_start_ = true;
}

uint32_t MySQLRouter::get_router_id(mysql_harness::Config &config) {
  uint32_t result = 0;

  if (config.has_any("metadata_cache")) {
    const auto &metadata_caches = config.get("metadata_cache");
    for (const auto &section : metadata_caches) {
      if (section->has("router_id")) {
        std::istringstream iss(section->get("router_id"));
        iss >> result;
        break;
      }
    }
  }
  return result;
}

void MySQLRouter::init_keyring(mysql_harness::Config &config) {
  bool needs_keyring = false;

  if (config.has_any("metadata_cache")) {
    auto metadata_caches = config.get("metadata_cache");
    for (auto &section : metadata_caches) {
      if (section->has("user")) {
        needs_keyring = true;
        break;
      }
    }
  }
  if (needs_keyring) {
    // Initialize keyring
    keyring_info_.init(config, origin_.str());

    if (keyring_info_.use_master_key_external_facility()) {
      init_keyring_using_external_facility(config);
    } else if (keyring_info_.use_master_key_file()) {
      init_keyring_using_master_key_file();
    } else {  // prompt password
      init_keyring_using_prompted_password();
    }
  }
}

void MySQLRouter::init_dynamic_state(mysql_harness::Config &config) {
  if (config.has_default("dynamic_state")) {
    using mysql_harness::DynamicState;

    const std::string dynamic_state_file = config.get_default("dynamic_state");
    DIM::instance().set_DynamicState(
        [=]() { return new DynamicState(dynamic_state_file); },
        std::default_delete<mysql_harness::DynamicState>());
    // force object creation, the further code relies on it's existence
    DIM::instance().get_DynamicState();
  }
}

void MySQLRouter::init_keyring_using_external_facility(
    mysql_harness::Config &config) {
  keyring_info_.add_router_id_to_env(get_router_id(config));
  if (!keyring_info_.read_master_key()) {
    throw MasterKeyReadError(
        "Cannot fetch master key using master key reader:" +
        keyring_info_.get_master_key_reader());
  }
  keyring_info_.validate_master_key();
  mysql_harness::init_keyring_with_key(keyring_info_.get_keyring_file(),
                                       keyring_info_.get_master_key(), false);
}

void MySQLRouter::init_keyring_using_master_key_file() {
  mysql_harness::init_keyring(keyring_info_.get_keyring_file(),
                              keyring_info_.get_master_key_file(), false);
}

void MySQLRouter::init_keyring_using_prompted_password() {
#ifdef _WIN32
  // When no master key file is provided, console interaction is required to
  // provide a master password. Since console interaction is not available when
  // run as service, throw an error to abort.
  if (mysqlrouter::is_running_as_service()) {
    std::string msg =
        "Cannot run router in Windows a service without a master key file.";
    mysqlrouter::write_windows_event_log(msg);
    throw std::runtime_error(msg);
  }
#endif
  std::string master_key =
      mysqlrouter::prompt_password("Encryption key for router keyring");
  if (master_key.length() > mysql_harness::kMaxKeyringKeyLength)
    throw std::runtime_error("Encryption key is too long");
  mysql_harness::init_keyring_with_key(keyring_info_.get_keyring_file(),
                                       master_key, false);
}

static string fixpath(const string &path, const std::string &basedir) {
  if (path.empty()) return basedir;
  if (path.compare(0, strlen("{origin}"), "{origin}") == 0) return path;
  if (path.find("ENV{") != std::string::npos) return path;
#ifdef _WIN32
  if (path[0] == '\\' || path[0] == '/' || path[1] == ':') return path;
  // if the path is not absolute, it must be relative to the origin
  return basedir + "\\" + path;
#else
  if (path[0] == '/') return path;
  // if the path is not absolute, it must be relative to the origin
  return basedir + "/" + path;
#endif
}

/*static*/
std::map<std::string, std::string> MySQLRouter::get_default_paths(
    const mysql_harness::Path &origin) {
  std::string basedir = mysql_harness::Path(origin)
                            .dirname()
                            .str();  // throws std::invalid_argument

  std::map<std::string, std::string> params = {
      {"program", kProgramName},
      {"origin", origin.str()},
      {"logging_folder", fixpath(MYSQL_ROUTER_LOGGING_FOLDER, basedir)},
      {"plugin_folder", fixpath(MYSQL_ROUTER_PLUGIN_FOLDER, basedir)},
      {"runtime_folder", fixpath(MYSQL_ROUTER_RUNTIME_FOLDER, basedir)},
      {"config_folder", fixpath(MYSQL_ROUTER_CONFIG_FOLDER, basedir)},
      {"data_folder", fixpath(MYSQL_ROUTER_DATA_FOLDER, basedir)}};
  // check if the executable is being ran from the install location and if not
  // set the plugin dir to a path relative to it
#ifndef _WIN32
  {
    mysql_harness::Path install_origin(
        fixpath(MYSQL_ROUTER_BINARY_FOLDER, basedir));
    if (!install_origin.exists() || !(install_origin.real_path() == origin)) {
      params["plugin_folder"] = fixpath(MYSQL_ROUTER_PLUGIN_FOLDER, basedir);
    }
  }
#else
  {
    mysql_harness::Path install_origin(
        fixpath(MYSQL_ROUTER_BINARY_FOLDER, basedir));
    if (!install_origin.exists() || !(install_origin.real_path() == origin)) {
      params["plugin_folder"] = origin.dirname().join("lib").str();
    }
  }
#endif

  // resolve environment variables & relative paths
  for (auto it : params) {
    std::string &param = params.at(it.first);
    param.assign(
        mysqlrouter::substitute_variable(param, "{origin}", origin.str()));
  }
  return params;
}

std::map<std::string, std::string> MySQLRouter::get_default_paths() const {
  return get_default_paths(origin_);  // throws std::invalid_argument
}

/*static*/
void MySQLRouter::init_main_logger(mysql_harness::LoaderConfig &config,
                                   bool raw_mode /*= false*/,
                                   bool use_os_log /*= false*/) {
// currently logging to OS log is only supported on Windows
#ifndef _WIN32
  harness_assert(use_os_log == false);
#endif

  if (!config.has_default("logging_folder"))
    config.set_default("logging_folder", "");

  const std::string logging_folder = config.get_default("logging_folder");

  // setup logging
  {
    // REMINDER: If something threw beyond this point, but before we managed to
    //           re-initialize the logger (registry), we would be in a world of
    //           pain: throwing with a non-functioning logger may cascade to a
    //           place where the error is logged and... BOOM!) So we deal with
    //           the above problem by working on a new logger registry object,
    //           and only if nothing throws, we replace the current registry
    //           with the new one at the very end.

    // our new logger registry, it will replace the current one if all goes well
    std::unique_ptr<mysql_harness::logging::Registry> registry(
        new mysql_harness::logging::Registry());

    const auto level = mysql_harness::logging::get_default_log_level(
        config, raw_mode);  // throws std::invalid_argument

    // register loggers for all modules + main exec (throws std::logic_error,
    // std::invalid_argument)
    mysql_harness::logging::create_module_loggers(
        *registry, level, {MYSQL_ROUTER_LOG_DOMAIN}, MYSQL_ROUTER_LOG_DOMAIN);

    // register logger for sql domain
    mysql_harness::logging::create_logger(*registry, level, "sql");

    // attach all loggers to main handler (throws std::runtime_error)
    mysql_harness::logging::create_main_log_handler(
        *registry, kProgramName, logging_folder, !raw_mode, use_os_log);

    // nothing threw - we're good. Now let's replace the new registry with the
    // old one
    DIM::instance().set_LoggingRegistry(
        [&registry]() { return registry.release(); },
        std::default_delete<mysql_harness::logging::Registry>());
    DIM::instance().reset_LoggingRegistry();

    // flag that the new loggers are ready for use
    DIM::instance().get_LoggingRegistry().set_ready();
  }

  // and give it a first spin
  if (config.logging_to_file())
    log_debug("Main logger initialized, logging to '%s'",
              config.get_log_file().c_str());
#ifdef _WIN32
  else if (use_os_log)
    log_debug("Main logger initialized, logging to Windows EventLog");
#endif
  else
    log_debug("Main logger initialized, logging to STDERR");
}

// throws std::runtime_error
mysql_harness::LoaderConfig *MySQLRouter::make_config(
    const std::map<std::string, std::string> params, ConfigFiles config_files) {
  constexpr const char *err_msg = "Configuration error: %s.";

  try {
    // LoaderConfig ctor throws bad_option (std::runtime_error)
    std::unique_ptr<mysql_harness::LoaderConfig> config(
        new mysql_harness::LoaderConfig(params, std::vector<std::string>(),
                                        mysql_harness::Config::allow_keys));

    // throws std::invalid_argument, std::runtime_error, syntax_error, ...
    for (const auto &config_file : config_files.available_config_files())
      config->read(config_file);

    return config.release();
  } catch (const mysql_harness::syntax_error &err) {
    throw std::runtime_error(string_format(err_msg, err.what()));
  } catch (const std::runtime_error &err) {
    throw std::runtime_error(string_format(err_msg, err.what()));
  }
}

// throws std::runtime_error
void MySQLRouter::init_loader(mysql_harness::LoaderConfig &config) {
  std::string err_msg =
      "Configuration error: %s.";  // TODO: is this error message right?
  try {
    loader_ = std::unique_ptr<mysql_harness::Loader>(
        new mysql_harness::Loader(kProgramName, config));
  } catch (const std::runtime_error &err) {
    throw std::runtime_error(string_format(err_msg.c_str(), err.what()));
  }
}

void MySQLRouter::start() {
  if (showing_info_ || !bootstrap_uri_.empty()) {
    // when we are showing info like --help or --version, we do not throw
    return;
  }

#ifndef _WIN32
  // if the --user parameter was provided on the command line, switch
  // to the user asap before accessing the external files to check
  // that the user has rights to use them
  if (!user_cmd_line_.empty()) {
    set_user(user_cmd_line_, true, this->sys_user_operations_);
  }
#endif

  // default configuration for boostrap is not supported
  // extra configuration for bootstrap is not supported
  ConfigFiles config_files(default_config_files_, config_files_,
                           extra_config_files_);
  // read config, and also make this config globally-available via DIM
  DIM::instance().reset_Config();  // simplifies unit tests
  DIM::instance().set_Config(
      [this, &config_files]() {
        return make_config(get_default_paths(), config_files);
      },
      std::default_delete<mysql_harness::LoaderConfig>());
  mysql_harness::LoaderConfig &config = DIM::instance().get_Config();

#ifndef _WIN32
  // --user param given on the command line has a priority over
  // the user in the configuration
  if (user_cmd_line_.empty() && config.has_default("user")) {
    set_user(config.get_default("user"), true, this->sys_user_operations_);
  }
#endif

  if (!can_start_) {
    throw std::runtime_error("Can not start");
  }

  // Using environment variable ROUTER_PID is a temporary solution. We will
  // remove this functionality when Harness introduces the `pid_file` option.
  auto pid_file_env = std::getenv("ROUTER_PID");
  if (pid_file_env != nullptr) {
    pid_file_path_ = pid_file_env;
    mysql_harness::Path pid_file_path(pid_file_path_);
    if (pid_file_path.is_regular()) {
      throw std::runtime_error(string_format(
          "PID file %s found. Already running?", pid_file_path_.c_str()));
    }
  }

  init_loader(config);  // throws std::runtime_error

  if (!pid_file_path_.empty()) {
    auto pid = getpid();
    std::ofstream pidfile(pid_file_path_);
    if (pidfile.good()) {
      pidfile << pid << std::endl;
      pidfile.close();
      log_info("PID %d written to '%s'", pid, pid_file_path_.c_str());
    } else {
      throw std::runtime_error(
          string_format("Failed writing PID to %s: %s", pid_file_path_.c_str(),
                        mysqlrouter::get_last_error(errno).c_str()));
    }
  }

  // make sure there is at most one [logger] section in the config and that it
  // has no key
  if (config.has_any(mysql_harness::logging::kConfigSectionLogger)) {
    const auto logger_sections =
        config.get(mysql_harness::logging::kConfigSectionLogger);
    if (logger_sections.size() > 1) {
      throw std::runtime_error(
          "There can be at most one [logger] section in the configuration");
    } else if (logger_sections.size() == 1) {
      auto const section = logger_sections.begin();
      if (!((*section)->key).empty()) {
        throw std::runtime_error("Section 'logger' does not support keys");
      }
    }
  }

  // before running the loader we need to make sure there is a logger section in
  // the configuration as logger plugin init() does all the logging setup now.
  // If there is none in the config let's add an empty one to go with the
  // defaults. This is for the backward compatibility as in the previous Router
  // versions this section was optional.
  if (!config.has(mysql_harness::logging::kConfigSectionLogger, "")) {
    config.add(mysql_harness::logging::kConfigSectionLogger);
  }

  // before running the loader we need to register loggers in the current
  // temporary registry for all the plugins as loader will start them soon and
  // they may want to log something; meanwhile the true logging registry will be
  // created later when logging plugin starts
  create_plugin_loggers(config, DIM::instance().get_LoggingRegistry(),
                        mysql_harness::logging::get_default_log_level(config));

  // there can be at most one metadata_cache section because
  // currently the router supports only one metadata_cache instance
  if (config.has_any("metadata_cache") &&
      config.get("metadata_cache").size() > 1)
    throw std::runtime_error(
        "MySQL Router currently supports only one metadata_cache instance. "
        "There is more than one metadata_cache section in the router "
        "configuration. Exiting.");

  init_keyring(config);
  init_dynamic_state(config);

  loader_->start();
}

void MySQLRouter::set_default_config_files(const char *locations) noexcept {
  std::stringstream ss_line{locations};

  // We remove all previous entries
  default_config_files_.clear();
  std::vector<string>().swap(default_config_files_);

  for (string file; std::getline(ss_line, file, ';');) {
    bool ok = mysqlrouter::substitute_envvar(file);
    if (ok) {  // if there's no placeholder in file path, this is OK too
      default_config_files_.push_back(
          mysqlrouter::substitute_variable(file, "{origin}", origin_.str()));
    } else {
      // Any other problem with placeholders we ignore and don't use file
    }
  }
}

string MySQLRouter::get_version() noexcept {
  return string(MYSQL_ROUTER_VERSION);
}

string MySQLRouter::get_version_line() noexcept {
  std::string version_string;
  build_version(std::string(MYSQL_ROUTER_PACKAGE_NAME), &version_string);

  return version_string;
}

vector<string> MySQLRouter::check_config_files() {
  ConfigFiles config_files(default_config_files_, config_files_,
                           extra_config_files_);
  if (config_files.empty())
    throw std::runtime_error(
        "No valid configuration file available. See --help for more "
        "information (looked at paths '" +
        config_files.paths_attempted() + "').");
  return config_files.available_config_files();
}

void MySQLRouter::save_bootstrap_option_not_empty(
    const std::string &option_name, const std::string &save_name,
    const std::string &option_value) {
  if (option_value.empty())
    throw std::runtime_error("Value for option '" + option_name +
                             "' can't be empty.");

  bootstrap_options_[save_name] = option_value;
}

void MySQLRouter::assert_bootstrap_mode(const std::string &option_name) const {
  if (this->bootstrap_uri_.empty())
    throw std::runtime_error("Option " + option_name +
                             " can only be used together with -B/--bootstrap");
}

void MySQLRouter::prepare_command_options() noexcept {
  // General guidelines for naming command line options:
  //
  // Option names that start with --conf are meant to affect
  // configuration only and used during bootstrap.
  // If an option affects the bootstrap process itself, it should
  // omit the --conf prefix, even if it affects both the bootstrap
  // and the configuration.

  arg_handler_.clear_options();

  arg_handler_.add_option(
      OptionNames({"--account-host"}),
      "Host pattern to be used when creating Router's database user, "
      "default='%'. "
      "It can be used multiple times to provide multiple patterns. (bootstrap)",
      CmdOptionValueReq::required, "account-host",
      [this](const string &host_pattern) {
        std::vector<std::string> &hostnames =
            this->bootstrap_multivalue_options_["account-host"];
        hostnames.push_back(host_pattern);

        // sort and eliminate any non-unique hostnames; we do this to ensure
        // that CREATE USER does not get called twice for the same user@host
        // later on in the ConfigGenerator
        std::sort(hostnames.begin(), hostnames.end());
        auto it = std::unique(hostnames.begin(), hostnames.end());
        hostnames.resize(std::distance(hostnames.begin(), it));
      },
      [this] { this->assert_bootstrap_mode("--account-host"); });

  arg_handler_.add_option(
      OptionNames({"-B", "--bootstrap"}),
      "Bootstrap and configure Router for operation with a MySQL InnoDB "
      "cluster.",
      CmdOptionValueReq::required, "server_url",
      [this](const string &server_url) {
        if (server_url.empty()) {
          throw std::runtime_error("Invalid value for --bootstrap/-B option");
        }
        this->bootstrap_uri_ = server_url;
      });

  arg_handler_.add_option(
      OptionNames({"--bootstrap-socket"}),
      "Bootstrap and configure Router via a Unix socket",
      CmdOptionValueReq::required, "socket_name",
      [this](const string &socket_name) {
        if (socket_name.empty()) {
          throw std::runtime_error(
              "Invalid value for --bootstrap-socket option");
        }

        this->save_bootstrap_option_not_empty("--bootstrap-socket",
                                              "bootstrap_socket", socket_name);
      },
      [this] { this->assert_bootstrap_mode("--bootstrap-socket"); });

  arg_handler_.add_option(
      OptionNames({"--conf-base-port"}),
      "Base port to use for listening router ports. (bootstrap)",
      CmdOptionValueReq::required, "port",
      [this](const string &port) {
        this->bootstrap_options_["base-port"] = port;
      },
      [this] { this->assert_bootstrap_mode("--conf-base-port"); });

  arg_handler_.add_option(
      OptionNames({"--conf-bind-address"}),
      "IP address of the interface to which router's listening sockets should "
      "bind. (bootstrap)",
      CmdOptionValueReq::required, "address",
      [this](const string &address) {
        this->bootstrap_options_["bind-address"] = address;
      },
      [this] { this->assert_bootstrap_mode("--conf-bind-address"); });

#ifndef _WIN32
  arg_handler_.add_option(
      OptionNames({"--conf-skip-tcp"}),
      "Whether to disable binding of a TCP port for incoming connections. "
      "(bootstrap)",
      CmdOptionValueReq::none, "",
      [this](const string &) { this->bootstrap_options_["skip-tcp"] = "1"; },
      [this] { this->assert_bootstrap_mode("--conf-skip-tcp"); });
  arg_handler_.add_option(
      OptionNames({"--conf-use-sockets"}),
      "Whether to use Unix domain sockets. (bootstrap)",
      CmdOptionValueReq::none, "",
      [this](const string &) { this->bootstrap_options_["use-sockets"] = "1"; },
      [this] { this->assert_bootstrap_mode("--conf-use-sockets"); });
#endif

  arg_handler_.add_option(OptionNames({"-c", "--config"}),
                          "Only read configuration from given file.",
                          CmdOptionValueReq::required, "path",
                          [this](const string &value) {
                            if (!config_files_.empty()) {
                              throw std::runtime_error(
                                  "Option -c/--config can only be used once; "
                                  "use -a/--extra-config instead.");
                            }

                            // When --config is used, no defaults shall be read
                            default_config_files_.clear();
                            check_and_add_conf(config_files_, value);
                          });

  arg_handler_.add_option(
      OptionNames({"--connect-timeout"}),
      "The time in seconds after which trying to connect to metadata server "
      "should timeout. It applies to bootstrap mode and is written to "
      "configuration file. It is also used in normal mode.",
      CmdOptionValueReq::optional, "", [this](const string &connect_timeout) {
        this->bootstrap_options_["connect-timeout"] = connect_timeout;
      });

  arg_handler_.add_option(
      OptionNames({"-d", "--directory"}),
      "Creates a self-contained directory for a new instance of the Router. "
      "(bootstrap)",
      CmdOptionValueReq::required, "directory",
      [this](const string &path) {
        if (path.empty()) {
          throw std::runtime_error("Invalid value for --directory option");
        }
        this->bootstrap_directory_ = path;
      },
      [this] { this->assert_bootstrap_mode("-d/--directory"); });

  arg_handler_.add_option(
      CmdOption::OptionNames({"-a", "--extra-config"}),
      "Read this file after configuration files are read from either "
      "default locations or from files specified by the --config option.",
      CmdOptionValueReq::required, "path", [this](const string &value) {
        check_and_add_conf(extra_config_files_, value);
      });

  arg_handler_.add_option(
      OptionNames({"--force"}),
      "Force reconfiguration of a possibly existing instance of the router. "
      "(bootstrap)",
      CmdOptionValueReq::none, "",
      [this](const string &) { this->bootstrap_options_["force"] = "1"; },
      [this] { this->assert_bootstrap_mode("--force"); });

  arg_handler_.add_option(
      OptionNames({"--force-password-validation"}),
      "When autocreating database account do not use HASHED password. "
      "(bootstrap)",
      CmdOptionValueReq::none, "",
      [this](const string &) {
        this->bootstrap_options_["force-password-validation"] = "1";
      },
      [this] { this->assert_bootstrap_mode("--force-password-validation"); });

  arg_handler_.add_option(CmdOption::OptionNames({"-?", "--help"}),
                          "Display this help and exit.",
                          CmdOptionValueReq::none, "", [this](const string &) {
                            this->show_help();
                            this->showing_info_ = true;
                          });

  arg_handler_.add_option(
      OptionNames({"--master-key-reader"}),
      "The tool that can be used to read master key, it has to be used "
      "together with --master-key-writer. (bootstrap)",
      CmdOptionValueReq::required, "",
      [this](const string &master_key_reader) {
        this->keyring_info_.set_master_key_reader(master_key_reader);
      },
      [this] {
        this->assert_bootstrap_mode("--master-key-reader");
        if (this->keyring_info_.get_master_key_reader().empty() !=
            this->keyring_info_.get_master_key_writer().empty())
          throw std::runtime_error(
              "Option --master-key-reader can only be used together with "
              "--master-key-writer.");
      });

  arg_handler_.add_option(
      OptionNames({"--master-key-writer"}),
      "The tool that can be used to store master key, it has to be used "
      "together with --master-key-reader. (bootstrap)",
      CmdOptionValueReq::required, "",
      [this](const string &master_key_writer) {
        this->keyring_info_.set_master_key_writer(master_key_writer);
      },
      [this] {
        this->assert_bootstrap_mode("--master-key-writer");
        if (this->keyring_info_.get_master_key_reader().empty() !=
            this->keyring_info_.get_master_key_writer().empty())
          throw std::runtime_error(
              "Option --master-key-writer can only be used together with "
              "--master-key-reader.");
      });

  arg_handler_.add_option(
      OptionNames({"--name"}),
      "Gives a symbolic name for the router instance. (bootstrap)",
      CmdOptionValueReq::optional, "name",
      [this](const string &name) { this->bootstrap_options_["name"] = name; },
      [this] { this->assert_bootstrap_mode("--name"); });

  arg_handler_.add_option(
      OptionNames({"--password-retries"}),
      "Number of the retries for generating the router's user password. "
      "(bootstrap)",
      CmdOptionValueReq::optional, "password-retries",
      [this](const string &retries) {
        this->bootstrap_options_["password-retries"] = retries;
      },
      [this] { this->assert_bootstrap_mode("--password-retries"); });

  arg_handler_.add_option(
      OptionNames({"--read-timeout"}),
      "The time in seconds after which read from metadata server should "
      "timeout. It applies to bootstrap mode and is written to configuration "
      "file. It is also used in normal mode.",
      CmdOptionValueReq::optional, "", [this](const string &read_timeout) {
        this->bootstrap_options_["read-timeout"] = read_timeout;
      });
  arg_handler_.add_option(
      OptionNames({"--report-host"}),
      "Host name of this computer (it will be queried from OS if not "
      "provided). "
      "It is used as suffix (the part after '@') in Router's database user "
      "name; "
      "should match host name as seen by the cluster nodes (bootstrap)",
      CmdOptionValueReq::required, "report-host",
      [this](const string &hostname) {
        if (!mysql_harness::is_valid_hostname(hostname.c_str()))
          throw std::runtime_error(
              "Option --report-host has an invalid value.");

        auto pr = this->bootstrap_options_.insert({"report-host", hostname});
        if (pr.second == false)
          throw std::runtime_error(
              "Option --report-host can only be used once.");
      },
      [this] { this->assert_bootstrap_mode("--report-host"); });

  arg_handler_.add_option(
      OptionNames({"--ssl-ca"}),
      "Path to SSL CA file to verify server's certificate against.",
      CmdOptionValueReq::required, "path",
      [this](const string &path) {
        this->save_bootstrap_option_not_empty("--ssl-ca", "ssl_ca", path);
      },
      [this] { this->assert_bootstrap_mode("--ssl-ca"); });

  arg_handler_.add_option(
      OptionNames({"--ssl-capath"}),
      "Path to directory containing SSL CA files to verify server's "
      "certificate against.",
      CmdOptionValueReq::required, "directory",
      [this](const string &path) {
        this->save_bootstrap_option_not_empty("--ssl-capath", "ssl_capath",
                                              path);
      },
      [this] { this->assert_bootstrap_mode("--ssl-capath"); });

  arg_handler_.add_option(
      OptionNames({"--ssl-cert"}),
      "Path to client SSL certificate, to be used if client certificate "
      "verification is required. Used during bootstrap only.",
      CmdOptionValueReq::required, "path",
      [this](const string &path) {
        this->save_bootstrap_option_not_empty("--ssl-cert", "ssl_cert", path);
      },
      [this] { this->assert_bootstrap_mode("--ssl-cert"); });

  arg_handler_.add_option(
      OptionNames({"--ssl-cipher"}),
      ": separated list of SSL ciphers to allow, if SSL is enabeld.",
      CmdOptionValueReq::required, "ciphers",
      [this](const string &cipher) {
        this->save_bootstrap_option_not_empty("--ssl-cipher", "ssl_cipher",
                                              cipher);
      },
      [this] { this->assert_bootstrap_mode("--ssl-cipher"); });

  arg_handler_.add_option(
      OptionNames({"--ssl-crl"}),
      "Path to SSL CRL file to use when verifying server certificate.",
      CmdOptionValueReq::required, "path",
      [this](const string &path) {
        this->save_bootstrap_option_not_empty("--ssl-crl", "ssl_crl", path);
      },
      [this] { this->assert_bootstrap_mode("--ssl-crl"); });

  arg_handler_.add_option(
      OptionNames({"--ssl-crlpath"}),
      "Path to directory containing SSL CRL files to use when verifying server "
      "certificate.",
      CmdOptionValueReq::required, "directory",
      [this](const string &path) {
        this->save_bootstrap_option_not_empty("--ssl-crlpath", "ssl_crlpath",
                                              path);
      },
      [this] { this->assert_bootstrap_mode("--ssl-crlpath"); });

  arg_handler_.add_option(
      OptionNames({"--ssl-key"}),
      "Path to private key for client SSL certificate, to be used if client "
      "certificate verification is required. Used during bootstrap only.",
      CmdOptionValueReq::required, "path",
      [this](const string &path) {
        this->save_bootstrap_option_not_empty("--ssl-key", "ssl_key", path);
      },
      [this] { this->assert_bootstrap_mode("--ssl-key"); });

  char ssl_mode_vals[128];
  char ssl_mode_desc[384];
  snprintf(ssl_mode_vals, sizeof(ssl_mode_vals), "%s|%s|%s|%s|%s",
           mysqlrouter::MySQLSession::kSslModeDisabled,
           mysqlrouter::MySQLSession::kSslModePreferred,
           mysqlrouter::MySQLSession::kSslModeRequired,
           mysqlrouter::MySQLSession::kSslModeVerifyCa,
           mysqlrouter::MySQLSession::kSslModeVerifyIdentity);
  snprintf(ssl_mode_desc, sizeof(ssl_mode_desc),
           "SSL connection mode for use during bootstrap and normal operation, "
           "when connecting to the metadata server. Analogous to --ssl-mode in "
           "mysql client. One of %s. Default = %s. (bootstrap)",
           ssl_mode_vals, mysqlrouter::MySQLSession::kSslModePreferred);

  arg_handler_.add_option(
      OptionNames({"--ssl-mode"}), ssl_mode_desc, CmdOptionValueReq::required,
      "mode",
      [this](const string &ssl_mode) {
        try {
          mysqlrouter::MySQLSession::parse_ssl_mode(
              ssl_mode);  // we only care if this succeeds
          bootstrap_options_["ssl_mode"] = ssl_mode;
        } catch (const std::logic_error &e) {
          throw std::runtime_error("Invalid value for --ssl-mode option");
        }
      },
      [this] { this->assert_bootstrap_mode("--ssl-mode"); });

  arg_handler_.add_option(
      OptionNames({"--tls-version"}),
      ", separated list of TLS versions to request, if SSL is enabled.",
      CmdOptionValueReq::required, "versions",
      [this](const string &version) {
        this->save_bootstrap_option_not_empty("--tls-version", "tls_version",
                                              version);
      },
      [this] { this->assert_bootstrap_mode("--tls-version"); });
#ifndef _WIN32
  arg_handler_.add_option(
      OptionNames({"-u", "--user"}),
      "Run the mysqlrouter as the user having the name user_name.",
      CmdOptionValueReq::required, "username",
      [this](const string &username) { this->username_ = username; },
      [this] {
        if (this->bootstrap_uri_.empty()) {
          this->user_cmd_line_ = this->username_;
        } else {
          check_user(this->username_, true, this->sys_user_operations_);
          this->bootstrap_options_["user"] = this->username_;
        }
      });
#endif
  arg_handler_.add_option(CmdOption::OptionNames({"-V", "--version"}),
                          "Display version information and exit.",
                          CmdOptionValueReq::none, "", [this](const string &) {
                            out_stream_ << this->get_version_line()
                                        << std::endl;
                            this->showing_info_ = true;
                          });

// These are additional Windows-specific options, added (at the time of writing)
// in check_service_operations(). Grep after '--install-service' and you shall
// find.
#ifdef _WIN32
  arg_handler_.add_option(
      CmdOption::OptionNames({"--clear-all-credentials"}),
      "Clear the vault, removing all the credentials stored on it",
      CmdOptionValueReq::none, "", [](const string &) {
        PasswordVault pv;
        pv.clear_passwords();
        log_info("Removed successfully all passwords from the vault.");
        throw silent_exception();
      });
  arg_handler_.add_option(CmdOption::OptionNames({"--install-service"}),
                          "Install Router as Windows service which starts "
                          "automatically at system boot",
                          CmdOptionValueReq::none, "",
                          [](const string &) { /*implemented elsewhere*/ });

  arg_handler_.add_option(
      CmdOption::OptionNames({"--install-service-manual"}),
      "Install Router as Windows service which needs to be started manually",
      CmdOptionValueReq::none, "",
      [](const string &) { /*implemented elsewhere*/ });

  arg_handler_.add_option(CmdOption::OptionNames({"--remove-service"}),
                          "Remove Router from Windows services",
                          CmdOptionValueReq::none, "",
                          [](const string &) { /*implemented elsewhere*/ });

  arg_handler_.add_option(CmdOption::OptionNames({"--service"}),
                          "Start Router as Windows service",
                          CmdOptionValueReq::none, "",
                          [](const string &) { /*implemented elsewhere*/ });

  arg_handler_.add_option(
      CmdOption::OptionNames({"--remove-credentials-section"}),
      "Removes the credentials for the given section",
      CmdOptionValueReq::required, "section_name", [](const string &value) {
        PasswordVault pv;
        pv.remove_password(value);
        pv.store_passwords();
        log_info("The password was removed successfully.");
        throw silent_exception();
      });

  arg_handler_.add_option(
      CmdOption::OptionNames({"--update-credentials-section"}),
      "Updates the credentials for the given section",
      CmdOptionValueReq::required, "section_name", [](const string &value) {
        std::string prompt = mysqlrouter::string_format(
            "Enter password for config section '%s'", value.c_str());
        std::string pass = mysqlrouter::prompt_password(prompt);
        PasswordVault pv;
        pv.update_password(value, pass);
        pv.store_passwords();
        log_info("The password was stored in the vault successfully.");
        throw silent_exception();
      });
#endif
}

// throws MySQLSession::Error, std::runtime_error, std::out_of_range,
// std::logic_error, ... ?
void MySQLRouter::bootstrap(const std::string &server_url) {
  mysqlrouter::ConfigGenerator config_gen(out_stream_, err_stream_
#ifndef _WIN32
                                          ,
                                          sys_user_operations_
#endif
  );
  config_gen.init(
      server_url,
      bootstrap_options_);  // throws MySQLSession::Error, std::runtime_error,
                            // std::out_of_range, std::logic_error
  config_gen.warn_on_no_ssl(bootstrap_options_);  // throws std::runtime_error

#ifdef _WIN32
  // Cannot run boostrap mode as windows service since it requires console
  // interaction.
  if (mysqlrouter::is_running_as_service()) {
    std::string msg = "Cannot run router in boostrap mode as Windows service.";
    mysqlrouter::write_windows_event_log(msg);
    throw std::runtime_error(msg);
  }
#endif

  auto default_paths = get_default_paths();

  if (bootstrap_directory_.empty()) {
    std::string config_file_path = mysqlrouter::substitute_variable(
        MYSQL_ROUTER_CONFIG_FOLDER "/mysqlrouter.conf", "{origin}",
        origin_.str());
    std::string state_file_path = mysqlrouter::substitute_variable(
        MYSQL_ROUTER_DATA_FOLDER "/state.json", "{origin}", origin_.str());
    std::string master_key_path = mysqlrouter::substitute_variable(
        MYSQL_ROUTER_CONFIG_FOLDER "/mysqlrouter.key", "{origin}",
        origin_.str());
    std::string default_keyring_file;
    default_keyring_file = mysqlrouter::substitute_variable(
        MYSQL_ROUTER_DATA_FOLDER, "{origin}", origin_.str());
    mysql_harness::Path keyring_dir(default_keyring_file);
    if (!keyring_dir.exists()) {
      if (mysql_harness::mkdir(default_keyring_file,
                               mysqlrouter::kStrictDirectoryPerm, true) < 0) {
        log_error("Cannot create directory '%s': %s",
                  truncate_string(default_keyring_file).c_str(),
                  get_strerror(errno).c_str());
        throw std::runtime_error("Could not create keyring directory");
      } else {
        // sets the directory owner for the --user if provided
        config_gen.set_file_owner(bootstrap_options_, default_keyring_file);
        default_keyring_file = keyring_dir.real_path().str();
      }
    }
    default_keyring_file.append("/").append(kDefaultKeyringFileName);

    keyring_info_.set_keyring_file(default_keyring_file);
    keyring_info_.set_master_key_file(master_key_path);
    config_gen.set_keyring_info(keyring_info_);
    config_gen.bootstrap_system_deployment(
        config_file_path, state_file_path, bootstrap_options_,
        bootstrap_multivalue_options_, default_paths);
  } else {
    keyring_info_.set_keyring_file(kDefaultKeyringFileName);
    keyring_info_.set_master_key_file("mysqlrouter.key");
    config_gen.set_keyring_info(keyring_info_);
    config_gen.bootstrap_directory_deployment(
        bootstrap_directory_, bootstrap_options_, bootstrap_multivalue_options_,
        default_paths);
  }
}

void MySQLRouter::show_help() noexcept {
  FILE *fp;
  out_stream_ << get_version_line() << std::endl;
  out_stream_ << ORACLE_WELCOME_COPYRIGHT_NOTICE("2015") << std::endl;

  for (auto line : wrap_string(
           "Configuration read from the following files in the given order"
           " (enclosed in parentheses means not available for reading):",
           kHelpScreenWidth, 0)) {
    out_stream_ << line << std::endl;
  }

  for (auto file : default_config_files_) {
    if ((fp = std::fopen(file.c_str(), "r")) == nullptr) {
      out_stream_ << "  (" << file << ")" << std::endl;
    } else {
      std::fclose(fp);
      out_stream_ << "  " << file << std::endl;
    }
  }
  const std::map<std::string, std::string> paths = get_default_paths();
  out_stream_ << "Plugins Path:"
              << "\n"
              << "  " << paths.at("plugin_folder") << "\n\n";
  out_stream_ << "Default Log Directory:"
              << "\n"
              << "  " << paths.at("logging_folder") << "\n\n";
  out_stream_ << "Default Persistent Data Directory:"
              << "\n"
              << "  " << paths.at("data_folder") << "\n\n";
  out_stream_ << "Default Runtime State Directory:"
              << "\n"
              << "  " << paths.at("runtime_folder") << "\n\n";
  out_stream_ << std::endl;

  show_usage();
}

/**
 * filter CmdOption by section.
 *
 * makes options "required" if needed for the usage output
 */
static std::pair<bool, CmdOption> cmd_option_acceptor(
    const std::string &section, const std::set<std::string> &accepted_opts,
    const CmdOption &opt) {
  for (const auto &name : opt.names) {
    if (accepted_opts.find(name) != accepted_opts.end()) {
      if ((section == "help" && name == "--help") ||
          (section == "version" && name == "--version") ||
          (section == "bootstrap" && name == "--bootstrap")) {
        CmdOption req_opt(opt);
        req_opt.required = true;
        return {true, req_opt};
      } else {
        return {true, opt};
      }
    }
  }

  return {false, opt};
}

void MySQLRouter::show_usage(bool include_options) noexcept {
  out_stream_ << Vt100::render(Vt100::Render::Bold) << "# Usage"
              << Vt100::render(Vt100::Render::Normal) << "\n\n";

  std::vector<std::pair<std::string, std::set<string>>> usage_sections{
      {"help", {"--help"}},
      {"version", {"--version"}},
      {"bootstrap",
       {"--account-host",
        "--bootstrap",
        "--bootstrap-socket",
        "--conf-use-sockets",
        "--conf-skip-tcp",
        "--conf-base-port",
        "--connect-timeout",
        "--directory",
        "--force",
        "--force-password-validation",
        "--name",
        "--master-key-reader",
        "--master-key-writer",
        "--password-retries",
        "--read-timeout",
        "--report-host",
        "--ssl-ca",
        "--ssl-cert",
        "--ssl-cipher",
        "--ssl-crl",
        "--ssl-crlpath",
        "--ssl-key",
        "--ssl-mode",
        "--tls-version",
        "--user"}},
      {"run",
       {"--user", "--config", "--extra-config", "--clear-all-credentials",
        "--service", "--remove-service", "--install-service",
        "--install-service-manual", "--remove-credentials-section",
        "--update-credentials-section"}}};

  for (const auto &section : usage_sections) {
    for (auto line : arg_handler_.usage_lines_if(
             "mysqlrouter", "", kHelpScreenWidth,
             std::bind(cmd_option_acceptor, section.first, section.second,
                       std::placeholders::_1))) {
      out_stream_ << line << "\n";
    }
    out_stream_ << "\n";
  }

  if (!include_options) {
    return;
  }

  out_stream_ << Vt100::render(Vt100::Render::Bold) << "# Options"
              << Vt100::render(Vt100::Render::Normal) << "\n\n";
  for (auto line :
       arg_handler_.option_descriptions(kHelpScreenWidth, kHelpScreenIndent)) {
    out_stream_ << line << std::endl;
  }

  out_stream_ << "\n"
              << Vt100::render(Vt100::Render::Bold) << "# Examples"
              << Vt100::render(Vt100::Render::Normal) << "\n\n";

#ifdef _WIN32
  constexpr const char kStartWithSudo[]{""};
  constexpr const char kStartWithUser[]{""};
  constexpr const char kStartScript[]{"start.ps1"};
#else
  constexpr const char kStartWithSudo[]{"sudo "};
  constexpr const char kStartWithUser[]{" --user=mysqlrouter"};
  constexpr const char kStartScript[]{"start.sh"};
#endif

  out_stream_ << "Bootstrap for use with InnoDB cluster into system-wide "
                 "installation\n\n"
              << "    " << kStartWithSudo
              << "mysqlrouter --bootstrap root@clusterinstance01"
              << kStartWithUser << "\n\n"
              << "Start router\n\n"
              << "    " << kStartWithSudo << "mysqlrouter" << kStartWithUser
              << "\n\n"
              << "Bootstrap for use with InnoDb cluster in a self-contained "
                 "directory\n\n"
              << "    "
              << "mysqlrouter --bootstrap root@clusterinstance01 -d myrouter"
              << "\n\n"
              << "Start router\n\n"
              << "    myrouter" << dir_sep << kStartScript << "\n\n";
}

void MySQLRouter::show_usage() noexcept { show_usage(true); }
