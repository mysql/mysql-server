/*
  Copyright (c) 2015, 2022, Oracle and/or its affiliates.

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
#include <initializer_list>
#include <memory>  // unique_ptr
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

#include "common.h"  // truncate_string
#include "config_generator.h"
#include "dim.h"
#include "harness_assert.h"
#include "hostname_validator.h"
#include "keyring/keyring_manager.h"
#include "mysql/harness/arg_handler.h"
#include "mysql/harness/config_parser.h"
#include "mysql/harness/dynamic_state.h"
#include "mysql/harness/filesystem.h"
#include "mysql/harness/log_reopen_component.h"
#include "mysql/harness/logging/logger_plugin.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/logging/registry.h"
#include "mysql/harness/process_state_component.h"
#include "mysql/harness/signal_handler.h"
#include "mysql/harness/utility/string.h"  // string_format
#include "mysql/harness/vt100.h"
#include "mysqlrouter/config_files.h"
#include "mysqlrouter/default_paths.h"
#include "mysqlrouter/mysql_session.h"
#include "mysqlrouter/supported_router_options.h"
#include "mysqlrouter/utils.h"  // substitute_envvar
#include "print_version.h"
#include "router_config.h"  // MYSQL_ROUTER_VERSION
#include "scope_guard.h"
#include "welcome_copyright_notice.h"

#ifndef _WIN32
#include <fcntl.h>
#include <unistd.h>
#include <csignal>
const char dir_sep = '/';
const std::string path_sep = ":";
#else
#include <process.h>
#include <windows.h>
#define getpid _getpid
#include <io.h>
#include <string.h>
#include "mysqlrouter/windows/password_vault.h"
#include "mysqlrouter/windows/service_operations.h"
#define strtok_r strtok_s
const char dir_sep = '\\';
const std::string path_sep = ";";
#endif

IMPORT_LOG_FUNCTIONS()
using namespace std::string_literals;

using mysql_harness::DIM;
using mysql_harness::truncate_string;
using mysql_harness::utility::string_format;
using mysql_harness::utility::wrap_string;
using mysqlrouter::substitute_envvar;
using mysqlrouter::SysUserOperations;
using mysqlrouter::SysUserOperationsBase;

static const char *kDefaultKeyringFileName = "keyring";
static const char kProgramName[] = "mysqlrouter";

namespace {

// Check if the value is valid regular filename and if it is add to the vector,
// if it is not throw an exception
void check_and_add_conf(std::vector<std::string> &configs,
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
  } else if (!cfg_file_path.exists()) {
    throw std::runtime_error(string_format(
        "The configuration file '%s' does not exist.", value.c_str()));
  } else {
    throw std::runtime_error(string_format(
        "The configuration file '%s' is expected to be a readable file, but it "
        "is %s.",
        value.c_str(), mysqlrouter::to_string(cfg_file_path.type()).c_str()));
  }
}

void check_config_overwrites(const CmdArgHandler::ConfigOverwrites &overwrites,
                             bool is_bootstrap) {
  for (const auto &overwrite : overwrites) {
    const std::string &section = overwrite.first.first;
    const std::string &key = overwrite.first.second;
    if (section == "DEFAULT" && !key.empty()) {
      throw std::runtime_error("Invalid argument '--" + section + ":" + key +
                               "'. Key not allowed on DEFAULT section");
    }

    if (!is_bootstrap) continue;
    // only --logger.level config overwrite is allowed currently for bootstrap
    for (const auto &option : overwrite.second) {
      const std::string name = section + "." + option.first;
      if (name != "logger.level") {
        throw std::runtime_error(
            "Invalid argument '--" + name +
            "'. Only '--logger.level' configuration option can be "
            "set with a command line parameter when bootstrapping.");
      }
    }
  }
}

}  // namespace

// throws MySQLSession::Error, std::runtime_error, std::out_of_range,
// std::logic_error, ...?
MySQLRouter::MySQLRouter(const std::string &program_name,
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
      origin_(mysql_harness::Path(
                  mysqlrouter::find_full_executable_path(program_name))
                  .dirname()),
      out_stream_(out_stream),
      err_stream_(err_stream)
#ifndef _WIN32
      ,
      sys_user_operations_(sys_user_operations)
#endif
{
  signal_handler_.register_ignored_signals_handler();  // SIGPIPE

  init(program_name,
       arguments);  // throws MySQLSession::Error, std::runtime_error,
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
    : MySQLRouter(std::string(argv[0]),
                  std::vector<std::string>({argv + 1, argv + argc}), out_stream,
                  err_stream
#ifndef _WIN32
                  ,
                  sys_user_operations
#endif
      ) {
}

// throws std::runtime_error
void MySQLRouter::parse_command_options(
    const std::vector<std::string> &arguments) {
  prepare_command_options();
  try {
    arg_handler_.process(arguments);
  } catch (const std::invalid_argument &exc) {
    throw std::runtime_error(exc.what());
  }
}

// throws MySQLSession::Error, std::runtime_error, std::out_of_range,
// std::logic_error, ...?
void MySQLRouter::init(const std::string &program_name,
                       const std::vector<std::string> &arguments) {
  set_default_config_files(CONFIG_FILES);

  parse_command_options(arguments);  // throws std::runtime_error

  if (showing_info_) {
    return;
  }

  // block non-fatal signal handling for all threads
  //
  // - no other thread than the signal-handler thread should receive signals
  // - syscalls should not get interrupted by signals either
  //
  // on windows, this is a no-op
  signal_handler_.block_all_nonfatal_signals();

  // for the fatal signals we want to have a handler that prints the stack-trace
  // if possible
  signal_handler_.register_fatal_signal_handler(core_file_);
  signal_handler_.spawn_signal_handler_thread();

#ifdef _WIN32
  signal_handler_.register_ctrl_c_handler();
#endif

  const bool is_bootstrap = !bootstrap_uri_.empty();
  check_config_overwrites(arg_handler_.get_config_overwrites(),
                          is_bootstrap);  // throws std::runtime_error

  if (is_bootstrap) {
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
          "You are bootstrapping as a superuser.\n"
          "This will make all the result files (config etc.) privately owned "
          "by the superuser.\n"
          "Please use --user=username option to specify the user that will be "
          "running the router.\n"
          "Use --user=root if this really should be the superuser.");

      throw std::runtime_error(msg);
    }
#endif

    // default configuration for bootstrap is not supported
    // extra configuration for bootstrap is not supported
    auto config_files_res =
        ConfigFilePathValidator({}, config_files_, {}).validate();
    std::vector<std::string> config_files;
    if (config_files_res && !config_files_res.value().empty()) {
      config_files = std::move(config_files_res.value());
    }

    DIM::instance().reset_Config();  // simplifies unit tests
    DIM::instance().set_Config(
        [this, &config_files]() { return make_config({}, config_files); },
        std::default_delete<mysql_harness::LoaderConfig>());
    mysql_harness::LoaderConfig &config = DIM::instance().get_Config();

    // reinit logger (right now the logger is configured to log to STDERR,
    // here we re-configure it with settings from config file)
    init_main_logger(config, true);  // true = raw logging mode

    bootstrap(
        program_name,
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
    keyring_info_.init(config);

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

#if 0
/*static*/
std::map<std::string, std::string> MySQLRouter::get_default_paths(
    const mysql_harness::Path &origin) {
  return mysqlrouter::get_default_paths(origin);
}
#endif

std::map<std::string, std::string> MySQLRouter::get_default_paths() const {
  return mysqlrouter::get_default_paths(
      origin_);  // throws std::invalid_argument
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
    const std::map<std::string, std::string> params,
    const std::vector<std::string> &config_files) {
  constexpr const char *err_msg = "Configuration error: %s.";

  try {
    // LoaderConfig ctor throws bad_option (std::runtime_error)
    std::unique_ptr<mysql_harness::LoaderConfig> config(
        new mysql_harness::LoaderConfig(params, std::vector<std::string>(),
                                        mysql_harness::Config::allow_keys,
                                        arg_handler_.get_config_overwrites()));

    // throws std::invalid_argument, std::runtime_error, syntax_error, ...
    for (const auto &config_file : config_files) {
      config->read(config_file);
    }

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
    loader_ = std::make_unique<mysql_harness::Loader>(kProgramName, config);
  } catch (const std::runtime_error &err) {
    throw std::runtime_error(string_format(err_msg.c_str(), err.what()));
  }

  loader_->register_supported_app_options(router_supported_options);
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

  // throws system_error() in case of failure
  const auto config_files = check_config_files();

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

  // Setup pidfile path for the application.
  // Order of significance: commandline > config file > ROUTER_PID envvar
  if (pid_file_path_.empty()) {
    if (config.has_default("pid_file")) {
      const std::string pidfile = config.get_default("pid_file");
      if (!pidfile.empty()) {
        pid_file_path_ = pidfile;
      } else {
        throw std::runtime_error(string_format("PID filename '%s' is illegal.",
                                               pid_file_path_.c_str()));
      }
    }
    // ... if still empty, check ENV
    if (pid_file_path_.empty()) {
      const auto pid_file_env = std::getenv("ROUTER_PID");
      if (pid_file_env != nullptr) {
        const std::string pidfile = std::string(pid_file_env);
        if (!pidfile.empty()) {
          pid_file_path_ = pidfile;
        } else {
          throw std::runtime_error(
              string_format("PID filename '%s' is illegal.", pid_file_env));
        }
      }
    }
  }

  // Check existing if set
  if (!pid_file_path_.empty()) {
    mysql_harness::Path pid_file_path(pid_file_path_);
    // append runtime path to relative paths
    if (!pid_file_path.is_absolute()) {
      mysql_harness::Path runtime_path =
          mysql_harness::Path(config.get_default("runtime_folder"));
      // mkdir if runtime_folder doesn't exist
      if (!runtime_path.exists() &&
          (mysql_harness::mkdir(runtime_path.str(),
                                mysql_harness::kStrictDirectoryPerm,
                                true) != 0)) {
        auto last_error =
#ifdef _WIN32
            GetLastError()
#else
            errno
#endif
            ;
        throw std::system_error(last_error, std::system_category(),
                                "Error when creating dir '" +
                                    runtime_path.str() +
                                    "': " + std::to_string(last_error));
      }
      mysql_harness::Path tmp = mysql_harness::Path(pid_file_path);
      pid_file_path = runtime_path.join(tmp);
      pid_file_path_ = std::string(pid_file_path.c_str());
    }
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
      pid_file_created_ = true;
      pidfile << pid << std::endl;
      pidfile.close();
      log_info("PID %d written to '%s'", pid, pid_file_path_.c_str());
    } else {
#ifdef _WIN32
      const std::error_code ec{static_cast<int>(GetLastError()),
                               std::system_category()};
#else
      const std::error_code ec{errno, std::generic_category()};
#endif

      throw std::system_error(ec,
                              "Failed writing PID to '" + pid_file_path_ + "'");
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

  // before running the loader we need to make sure there is a logger section
  // in the configuration as logger plugin init() does all the logging setup
  // now. If there is none in the config let's add an empty one to go with the
  // defaults. This is for the backward compatibility as in the previous
  // Router versions this section was optional.
  if (!config.has(mysql_harness::logging::kConfigSectionLogger, "")) {
    config.add(mysql_harness::logging::kConfigSectionLogger);
  }

  // before running the loader we need to register loggers in the current
  // temporary registry for all the plugins as loader will start them soon and
  // they may want to log something; meanwhile the true logging registry will
  // be created later when logging plugin starts
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

#if !defined(_WIN32)
  //
  // reopen the logfile on SIGHUP.
  // shutdown at SIGTERM|SIGINT
  //

  auto &log_reopener = mysql_harness::LogReopenComponent::get_instance();

  static const char kLogReopenServiceName[]{"log_reopen"};
  static const char kSignalHandlerServiceName[]{"signal_handler"};

  // report readiness of all services only after the log-reopen handlers is
  // installed ... after all plugins are started.
  loader_->waitable_services().emplace_back(kLogReopenServiceName);

  loader_->waitable_services().emplace_back(kSignalHandlerServiceName);

  loader_->after_all_started([&]() {
    // as the LogReopener depends on the loggers being started, it must be
    // initialized after Loader::start_all() has been called.
    log_reopener.init();

    log_reopener->set_complete_callback([](const auto &errmsg) {
      if (errmsg.empty()) return;

      mysql_harness::ProcessStateComponent::get_instance()
          .request_application_shutdown(
              mysql_harness::ShutdownPending::Reason::FATAL_ERROR, errmsg);
    });

    signal_handler_.add_sig_handler(SIGHUP, [&log_reopener](int /* sig */) {
      // is run by the signal-thread.
      log_reopener->request_reopen();
    });

    mysql_harness::on_service_ready(kLogReopenServiceName);

    // signal-handler
    signal_handler_.add_sig_handler(SIGTERM, [](int /* sig */) {
      mysql_harness::ProcessStateComponent::get_instance()
          .request_application_shutdown(
              mysql_harness::ShutdownPending::Reason::REQUESTED);
    });

    signal_handler_.add_sig_handler(SIGINT, [](int /* sig */) {
      mysql_harness::ProcessStateComponent::get_instance()
          .request_application_shutdown(
              mysql_harness::ShutdownPending::Reason::REQUESTED);
    });

    mysql_harness::on_service_ready(kSignalHandlerServiceName);
  });

  // after the first plugin finished, stop the log-reopener and signal-handler
  loader_->after_first_finished([&]() {
    signal_handler_.remove_sig_handler(SIGTERM);
    signal_handler_.remove_sig_handler(SIGINT);

    signal_handler_.remove_sig_handler(SIGHUP);
    log_reopener.reset();
  });
#endif

  loader_->start();
}

void MySQLRouter::stop() {
  // Remove the pidfile if present and was created by us.
  if (!pid_file_path_.empty() && pid_file_created_) {
    mysql_harness::Path pid_file_path(pid_file_path_);
    if (pid_file_path.is_regular()) {
      log_debug("Removing pidfile %s", pid_file_path.c_str());
      std::remove(pid_file_path.c_str());
    }
  }
}

void MySQLRouter::set_default_config_files(const char *locations) noexcept {
  std::stringstream ss_line{locations};

  // We remove all previous entries
  default_config_files_.clear();
  std::vector<std::string>().swap(default_config_files_);

  for (std::string file; std::getline(ss_line, file, ';');) {
    bool ok = mysqlrouter::substitute_envvar(file);
    if (ok) {  // if there's no placeholder in file path, this is OK too
      default_config_files_.push_back(
          mysqlrouter::substitute_variable(file, "{origin}", origin_.str()));
    } else {
      // Any other problem with placeholders we ignore and don't use file
    }
  }
}

std::string MySQLRouter::get_version() noexcept { return MYSQL_ROUTER_VERSION; }

std::string MySQLRouter::get_version_line() noexcept {
  std::string version_string;
  build_version(std::string(MYSQL_ROUTER_PACKAGE_NAME), &version_string);

  return version_string;
}

std::vector<std::string> MySQLRouter::check_config_files() {
  const auto res = ConfigFilePathValidator(default_config_files_, config_files_,
                                           extra_config_files_)
                       .validate();

  if (!res) {
    const auto err = res.error();
    if (err.ec == make_error_code(ConfigFilePathValidatorErrc::kDuplicate)) {
      throw std::runtime_error(string_format(
          "The configuration file '%s' is provided multiple "
          "times.\nAlready known "
          "configuration files:\n\n%s",
          err.current_filename.c_str(),
          mysql_harness::join(err.paths_attempted, "\n").c_str()));
    } else if (err.ec ==
               make_error_code(ConfigFilePathValidatorErrc::kNotReadable)) {
      throw std::runtime_error(
          string_format("The configuration file '%s' is not readable.",
                        err.current_filename.c_str()));
    } else if (err.ec ==
               make_error_code(
                   ConfigFilePathValidatorErrc::kExtraWithoutMainConfig)) {
      throw std::runtime_error(
          "Extra configuration files " +
          mysql_harness::join(extra_config_files_, ", ") +
          " provided, but neither default configuration files "
          "nor --config=<file> are readable files.\nChecked:\n\n" +
          mysql_harness::join(err.paths_attempted, "\n"));
    } else if (err.ec ==
               make_error_code(ConfigFilePathValidatorErrc::kNoConfigfile)) {
      throw std::runtime_error(
          "None of the default configuration files is readable and "
          "--config=<file> was not specified.\n"
          "Checked default configuration files:\n\n" +
          mysql_harness::join(err.paths_attempted, "\n"));
    } else {
      throw std::system_error(err.ec);
    }
  }

  return res.value();
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

void MySQLRouter::assert_not_bootstrap_mode(
    const std::string &option_name) const {
  if (!this->bootstrap_uri_.empty())
    throw std::runtime_error("Option " + option_name +
                             " cannot be used together with -B/--bootstrap");
}

void MySQLRouter::assert_option_value_in_range(const std::string &value,
                                               const int min,
                                               const int max) const {
  try {
    std::size_t last_char = 0;
    auto val = std::stoi(value, &last_char);
    if (last_char != value.size())
      throw std::invalid_argument{"invalid value: " + value};

    if (val < min || val > max) {
      throw std::out_of_range{std::string{"not in allowed range ["} +
                              std::to_string(min) + ", " + std::to_string(max) +
                              "]"};
    }
  } catch (const std::invalid_argument &) {
    throw std::invalid_argument{"invalid value: " + value};
  }
}

/**
 * upper-case a string.
 */
static std::string make_upper(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), ::toupper);

  return s;
}

/**
 * assert 'value' is one of the allowed values.
 *
 * value is compared case-insensitive
 *
 * @param key key name to report in case of failure
 * @param value value to check
 * @param allowed_values allowed values.
 *
 * @throws std::invalid_argument if value is not part of allowed_values.
 */
static void assert_one_of_ci(
    const std::string &key, const std::string &value,
    std::initializer_list<const char *> allowed_values) {
  const auto value_upper = make_upper(value);

  const auto it = std::find_if(allowed_values.begin(), allowed_values.end(),
                               [&value_upper](const auto &allowed_value) {
                                 return value_upper == allowed_value;
                               });

  if (it == allowed_values.end()) {
    throw std::invalid_argument("value '" + value + "' provided to " + key +
                                " is not one of " +
                                mysql_harness::join(allowed_values, ","));
  }
}

void MySQLRouter::prepare_command_options() noexcept {
  // General guidelines for naming command line options:
  //
  // Option names that start with --conf are meant to affect
  // configuration only and used during bootstrap.
  // If an option affects the bootstrap process itself, it should
  // omit the --conf prefix, even if it affects both the bootstrap
  // and the configuration.

  using OptionNames = CmdOption::OptionNames;

  arg_handler_.clear_options();

  arg_handler_.add_option(
      OptionNames({"--account"}),
      "Account (username) to be used by Router when talking to cluster."
      " (bootstrap)",
      CmdOptionValueReq::required, "account",
      [this](const std::string &username) {
        if (username.empty())
          throw std::runtime_error(
              "Value for --account option cannot be empty");
        if (this->bootstrap_options_.count("account"))
          throw std::runtime_error("Option --account can only be given once");
        this->bootstrap_options_["account"] = username;
      },
      [this](const std::string &) {
        this->assert_bootstrap_mode("--account");
      });

  arg_handler_.add_option(
      OptionNames({"--account-create"}),
      "Specifies account creation policy (useful for guarding against "
      "accidentally bootstrapping using a wrong account). <mode> is one of:\n"
      "  'always'        - bootstrap only if account doesn't exist\n"
      "  'never'         - bootstrap only if account exists\n"
      "  'if-not-exists' - bootstrap either way (default)\n"
      "This option can only be used if option '--account' is also used.\n"
      "Argument 'never' cannot be used together with option "
      "'--account-host'\n"
      "(bootstrap)",
      CmdOptionValueReq::required, "mode",
      [this](const std::string &create) {
        if (create != "always" && create != "if-not-exists" &&
            create != "never")
          throw std::runtime_error(
              "Invalid value for --account-create option.  Valid values: "
              "always, if-not-exists, never");
        if (this->bootstrap_options_.count("account-create"))
          throw std::runtime_error(
              "Option --account-create can only be given once");
        this->bootstrap_options_["account-create"] = create;
      },
      [this](const std::string &) {
        this->assert_bootstrap_mode("--account-create");
        if (!this->bootstrap_options_.count("account"))
          throw std::runtime_error(
              "Option --account-create can only be used together with "
              "--account.");
      });

  arg_handler_.add_option(
      OptionNames({"--account-host"}),
      "Host pattern to be used when creating Router's database user, "
      "default='%'. "
      "It can be used multiple times to provide multiple patterns. "
      "(bootstrap)",
      CmdOptionValueReq::required, "account-host",
      [this](const std::string &host_pattern) {
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
      [this](const std::string &) {
        this->assert_bootstrap_mode("--account-host");
        const auto it = this->bootstrap_options_.find("account-create");
        if (it != this->bootstrap_options_.end() && it->second == "never")
          throw std::runtime_error(
              "Option '--account-create never' cannot be used together with "
              "'--account-host <host>'");
      });

  arg_handler_.add_option(
      OptionNames({"-B", "--bootstrap"}),
      "Bootstrap and configure Router for operation with a MySQL InnoDB "
      "cluster.",
      CmdOptionValueReq::required, "server_url",
      [this](const std::string &server_url) {
        if (server_url.empty()) {
          throw std::runtime_error("Invalid value for --bootstrap/-B option");
        }
        this->bootstrap_uri_ = server_url;
      });

  arg_handler_.add_option(
      OptionNames({"--bootstrap-socket"}),
      "Bootstrap and configure Router via a Unix socket",
      CmdOptionValueReq::required, "socket_name",
      [this](const std::string &socket_name) {
        if (socket_name.empty()) {
          throw std::runtime_error(
              "Invalid value for --bootstrap-socket option");
        }

        this->save_bootstrap_option_not_empty("--bootstrap-socket",
                                              "bootstrap_socket", socket_name);
      },
      [this](const std::string &) {
        this->assert_bootstrap_mode("--bootstrap-socket");
      });

  arg_handler_.add_option(
      OptionNames({"--client-ssl-cert"}),
      "name of a PEM file containing a SSL certificate used "
      "for accepting TLS connections between client and router",
      CmdOptionValueReq::required, "path",
      [this](const auto &value) {
        this->save_bootstrap_option_not_empty("--client-ssl-cert",
                                              "client_ssl_cert", value);
      },
      [this](const auto &) {
        this->assert_bootstrap_mode("--client-ssl-cert");

        if (!bootstrap_options_["client_ssl_cert"].empty() &&
            bootstrap_options_["client_ssl_key"].empty()) {
          throw std::runtime_error(
              "If --client-ssl-cert is set, --client-ssl-key can't be empty.");
        }
      });

  arg_handler_.add_option(
      OptionNames({"--client-ssl-cipher"}),
      "list of one or more colon separated cipher names used for accepting "
      "TLS connections between client and router",
      CmdOptionValueReq::required, "",
      [this](const auto &value) {
        this->save_bootstrap_option_not_empty("--client-ssl-cipher",
                                              "client_ssl_cipher", value);
      },
      [this](const auto &) {
        this->assert_bootstrap_mode("--client-ssl-cipher");
      });

  arg_handler_.add_option(
      OptionNames({"--client-ssl-curves"}),
      "list of one or more colon separated elliptic curve names used for "
      "accepting TLS connections between client and router",
      CmdOptionValueReq::required, "",
      [this](const auto &value) {
        this->save_bootstrap_option_not_empty("--client-ssl-curves",
                                              "client_ssl_curves", value);
      },
      [this](const auto &) {
        this->assert_bootstrap_mode("--client-ssl-curves");
      });

  arg_handler_.add_option(
      OptionNames({"--client-ssl-key"}),
      "name of a PEM file containing a SSL private key used "
      "for accepting TLS connections between client and router",
      CmdOptionValueReq::required, "path",
      [this](const auto &value) {
        this->save_bootstrap_option_not_empty("--client-ssl-key",
                                              "client_ssl_key", value);
      },
      [this](const auto &) {
        this->assert_bootstrap_mode("--client-ssl-key");

        if (!bootstrap_options_["client_ssl_key"].empty() &&
            bootstrap_options_["client_ssl_cert"].empty()) {
          throw std::runtime_error(
              "If --client-ssl-key is set, --client-ssl-cert can't be empty.");
        }
      });

  arg_handler_.add_option(
      OptionNames({"--client-ssl-mode"}),
      "SSL mode for connections from client to router. One "
      "of DISABLED, PREFERRED, REQUIRED or PASSTHROUGH.",
      CmdOptionValueReq::required, "mode",
      [this](const auto &value) {
        assert_one_of_ci("--client-ssl-mode", value,
                         {"DISABLED", "PREFERRED", "REQUIRED", "PASSTHROUGH"});

        this->save_bootstrap_option_not_empty(
            "--client-ssl-mode", "client_ssl_mode", make_upper(value));
      },
      [this](const auto &) {
        this->assert_bootstrap_mode("--client-ssl-mode");

        if (bootstrap_options_["client_ssl_mode"] == "PASSTHROUGH") {
          auto server_ssl_mode_it = bootstrap_options_.find("server_ssl_mode");
          if (server_ssl_mode_it != bootstrap_options_.end()) {
            if (server_ssl_mode_it->second != "AS_CLIENT") {
              throw std::runtime_error(
                  "--server-ssl-mode must be AS_CLIENT or not specified, if "
                  "--client-ssl-mode is PASSTHROUGH.");
            }
          }
        }
      });
  arg_handler_.add_option(
      OptionNames({"--client-ssl-dh-params"}),
      "name of a PEM file containing DH paramaters",
      CmdOptionValueReq::required, "",
      [this](const auto &value) {
        this->save_bootstrap_option_not_empty("--client-ssl-dh-params",
                                              "client_ssl_dh_params", value);
      },
      [this](const auto &) {
        this->assert_bootstrap_mode("--client-ssl-dh-params");
      });

  arg_handler_.add_option(
      OptionNames({"--conf-base-port"}),
      "Base port to use for listening router ports. (bootstrap)",
      CmdOptionValueReq::required, "port",
      [this](const std::string &port) {
        this->bootstrap_options_["base-port"] = port;
      },
      [this](const std::string &) {
        this->assert_bootstrap_mode("--conf-base-port");
      });

  arg_handler_.add_option(
      OptionNames({"--conf-bind-address"}),
      "IP address of the interface to which router's listening sockets "
      "should bind. (bootstrap)",
      CmdOptionValueReq::required, "address",
      [this](const std::string &address) {
        this->bootstrap_options_["bind-address"] = address;
      },
      [this](const std::string &) {
        this->assert_bootstrap_mode("--conf-bind-address");
      });

#ifndef _WIN32
  arg_handler_.add_option(
      OptionNames({"--conf-skip-tcp"}),
      "Whether to disable binding of a TCP port for incoming connections. "
      "(bootstrap)",
      CmdOptionValueReq::none, "",
      [this](const std::string &) {
        this->bootstrap_options_["skip-tcp"] = "1";
      },
      [this](const std::string &) {
        this->assert_bootstrap_mode("--conf-skip-tcp");
      });
  arg_handler_.add_option(
      OptionNames({"--conf-use-sockets"}),
      "Whether to use Unix domain sockets. (bootstrap)",
      CmdOptionValueReq::none, "",
      [this](const std::string &) {
        this->bootstrap_options_["use-sockets"] = "1";
      },
      [this](const std::string &) {
        this->assert_bootstrap_mode("--conf-use-sockets");
      });
#endif

  arg_handler_.add_option(OptionNames({"-c", "--config"}),
                          "Only read configuration from given file.",
                          CmdOptionValueReq::required, "path",
                          [this](const std::string &value) {
                            if (!config_files_.empty()) {
                              throw std::runtime_error(
                                  "Option -c/--config can only be used once; "
                                  "use -a/--extra-config instead.");
                            }

                            check_and_add_conf(config_files_, value);
                          });

  arg_handler_.add_option(
      OptionNames({"--core-file"}), "Write a core file if mysqlrouter dies.",
      CmdOptionValueReq::optional, "", [this](const std::string &value) {
        if (value.empty() || value == "1") {
          this->core_file_ = true;
        } else if (value == "0") {
          this->core_file_ = false;
        } else {
          throw std::runtime_error(
              "Value for parameter '--core-file' needs to be "
              "one of: ['0', '1']");
        }
      });

  arg_handler_.add_option(
      OptionNames({"--connect-timeout"}),
      "The time in seconds after which trying to connect to metadata server "
      "should timeout. It is used when bootstrapping and also written to the "
      "configuration file (bootstrap)",
      CmdOptionValueReq::optional, "",
      [this](const std::string &connect_timeout) {
        this->bootstrap_options_["connect-timeout"] = connect_timeout;
      },
      [this](const std::string &) {
        this->assert_bootstrap_mode("--connect-timeout");
      });

  arg_handler_.add_option(
      OptionNames({"--conf-use-gr-notifications"}),
      "Whether to enable handling of cluster state change GR notifications.",
      CmdOptionValueReq::optional, "",
      [this](const std::string &value) {
        if (value == "0" || value == "1") {
          this->bootstrap_options_["use-gr-notifications"] = value;
        } else if (value.empty()) {
          this->bootstrap_options_["use-gr-notifications"] = "1";
        } else {
          throw std::runtime_error(
              "Value for parameter '--conf-use-gr-notifications' needs to be "
              "one of: ['0', '1']");
        }
      },
      [this](const std::string &) {
        this->assert_bootstrap_mode("--conf-use-gr-notifications");
      });

  arg_handler_.add_option(
      OptionNames({"--conf-target-cluster"}),
      "Router's target Cluster from the ClusterSet('current' or 'primary').",
      CmdOptionValueReq::required, "",
      [this](const std::string &value) {
        if (this->bootstrap_options_.count("target-cluster-by-name") > 0) {
          throw std::runtime_error(
              "Parameters '--conf-target-cluster' and "
              "'--conf-target-cluster-by-name' are mutually exclusive and "
              "can't be used together");
        }

        std::string value_lowercase{value};
        std::transform(value_lowercase.begin(), value_lowercase.end(),
                       value_lowercase.begin(), ::tolower);

        if (value_lowercase != "primary" && value_lowercase != "current") {
          throw std::runtime_error(
              "Value for parameter '--conf-target-cluster' needs to be one of: "
              "['primary', 'current']");
        }

        this->bootstrap_options_["target-cluster"] = value_lowercase;
      },
      [this](const std::string &) {
        this->assert_bootstrap_mode("--conf-target-cluster");
      });

  arg_handler_.add_option(
      OptionNames({"--conf-target-cluster-by-name"}),
      "Name of the target Cluster for the Router when bootstrapping against "
      "the ClusterSet",
      CmdOptionValueReq::required, "",
      [this](const std::string &value) {
        if (this->bootstrap_options_.count("target-cluster") > 0) {
          throw std::runtime_error(
              "Parameters '--conf-target-cluster' and "
              "'--conf-target-cluster-by-name' are mutually exclusive and "
              "can't be used together");
        }
        if (value.empty()) {
          throw std::runtime_error(
              "Value for parameter '--conf-target-cluster-by-name' can't be "
              "empty");
        }
        this->bootstrap_options_["target-cluster-by-name"] = value;
      },
      [this](const std::string &) {
        this->assert_bootstrap_mode("--conf-target-cluster-by-name");
      });

  arg_handler_.add_option(
      OptionNames({"-d", "--directory"}),
      "Creates a self-contained directory for a new instance of the Router. "
      "(bootstrap)",
      CmdOptionValueReq::required, "directory",
      [this](const std::string &path) {
        if (path.empty()) {
          throw std::runtime_error("Invalid value for --directory option");
        }
        this->bootstrap_directory_ = path;
      },
      [this](const std::string &) {
        this->assert_bootstrap_mode("-d/--directory");
      });

  arg_handler_.add_option(
      CmdOption::OptionNames({"-a", "--extra-config"}),
      "Read this file after configuration files are read from either "
      "default locations or from files specified by the --config option.",
      CmdOptionValueReq::required, "path", [this](const std::string &value) {
        check_and_add_conf(extra_config_files_, value);
      });

  arg_handler_.add_option(
      OptionNames({"--force"}),
      "Force reconfiguration of a possibly existing instance of the router. "
      "(bootstrap)",
      CmdOptionValueReq::none, "",
      [this](const std::string &) { this->bootstrap_options_["force"] = "1"; },
      [this](const std::string &) { this->assert_bootstrap_mode("--force"); });

  arg_handler_.add_option(
      OptionNames({"--force-password-validation"}),
      "When autocreating database account do not use HASHED password. "
      "(bootstrap)",
      CmdOptionValueReq::none, "",
      [this](const std::string &) {
        this->bootstrap_options_["force-password-validation"] = "1";
      },
      [this](const std::string &) {
        this->assert_bootstrap_mode("--force-password-validation");
      });

  arg_handler_.add_option(
      CmdOption::OptionNames({"-?", "--help"}), "Display this help and exit.",
      CmdOptionValueReq::none, "", [this](const std::string &) {
        this->show_help();
        this->showing_info_ = true;
      });

  arg_handler_.add_option(
      OptionNames({"--master-key-reader"}),
      "The tool that can be used to read master key, it has to be used "
      "together with --master-key-writer. (bootstrap)",
      CmdOptionValueReq::required, "",
      [this](const std::string &master_key_reader) {
        this->keyring_info_.set_master_key_reader(master_key_reader);
      },
      [this](const std::string &) {
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
      [this](const std::string &master_key_writer) {
        this->keyring_info_.set_master_key_writer(master_key_writer);
      },
      [this](const std::string &) {
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
      [this](const std::string &name) {
        this->bootstrap_options_["name"] = name;
      },
      [this](const std::string &) { this->assert_bootstrap_mode("--name"); });

  arg_handler_.add_option(
      OptionNames({"--password-retries"}),
      "Number of the retries for generating the router's user password. "
      "(bootstrap)",
      CmdOptionValueReq::optional, "password-retries",
      [this](const std::string &retries) {
        this->bootstrap_options_["password-retries"] = retries;
      },
      [this](const std::string &) {
        this->assert_bootstrap_mode("--password-retries");
      });

  arg_handler_.add_option(
      OptionNames({"--pid-file"}), "Path and filename of pid file",
      CmdOptionValueReq::required, "pidfile",
      [this](const std::string &pidfile_url) {
        if (!this->pid_file_path_.empty())
          throw std::runtime_error("Option --pid-file can only be given once");
        if (pidfile_url.empty()) {
          throw std::runtime_error("Invalid empty value for --pid-file option");
        }
        this->pid_file_path_ = pidfile_url;
      },
      [this](const std::string &) {
        this->assert_not_bootstrap_mode("--pid-file");
      });

  arg_handler_.add_option(
      OptionNames({"--read-timeout"}),
      "The time in seconds after which reads from metadata server should "
      "timeout. It is used when bootstrapping and is also written to "
      "configuration file. (bootstrap)",
      CmdOptionValueReq::optional, "",
      [this](const std::string &read_timeout) {
        this->bootstrap_options_["read-timeout"] = read_timeout;
      },
      [this](const std::string &) {
        this->assert_bootstrap_mode("--read-timeout");
      });
  arg_handler_.add_option(
      OptionNames({"--report-host"}),
      "Host name of this computer (it will be queried from OS if not "
      "provided). "
      "It is used as suffix (the part after '@') in Router's database user "
      "name; "
      "should match host name as seen by the cluster nodes (bootstrap)",
      CmdOptionValueReq::required, "report-host",
      [this](const std::string &hostname) {
        if (!mysql_harness::is_valid_hostname(hostname.c_str()))
          throw std::runtime_error(
              "Option --report-host has an invalid value.");

        auto pr = this->bootstrap_options_.insert({"report-host", hostname});
        if (pr.second == false)
          throw std::runtime_error(
              "Option --report-host can only be used once.");
      },
      [this](const std::string &) {
        this->assert_bootstrap_mode("--report-host");
      });

  arg_handler_.add_option(
      OptionNames({"--server-ssl-ca"}),
      "path name of the Certificate Authority (CA) certificate file in PEM "
      "format. Used when forwarding a client connection from router to a "
      "server.",
      CmdOptionValueReq::required, "path",
      [this](const auto &value) {
        this->save_bootstrap_option_not_empty("--server-ssl-ca",
                                              "server_ssl_ca", value);
      },
      [this](const auto &) { this->assert_bootstrap_mode("--server-ssl-ca"); });

  arg_handler_.add_option(
      OptionNames({"--server-ssl-capath"}),
      "path name of the directory that contains trusted SSL Certificate "
      "Authority (CA) certificate files in PEM format. Used when forwarding "
      "a client connection from router to a server.",
      CmdOptionValueReq::required, "directory",
      [this](const auto &value) {
        this->save_bootstrap_option_not_empty("--server-ssl-capath",
                                              "server_ssl_capath", value);
      },
      [this](const auto &) {
        this->assert_bootstrap_mode("--server-ssl-capath");
      });

  arg_handler_.add_option(
      OptionNames({"--server-ssl-cipher"}),
      "list of one or more colon separated cipher names. Used when "
      "forwarding "
      "client connection from router to a server.",
      CmdOptionValueReq::required, "",
      [this](const auto &value) {
        this->save_bootstrap_option_not_empty("--server-ssl-cipher",
                                              "server_ssl_cipher", value);
      },
      [this](const auto &) {
        this->assert_bootstrap_mode("--server-ssl-cipher");
      });

  arg_handler_.add_option(
      OptionNames({"--server-ssl-crl"}),
      "path name of the file containing certificate revocation lists in PEM "
      "format. Used when forwarding a client connection from router to a "
      "server.",
      CmdOptionValueReq::required, "path",
      [this](const auto &value) {
        this->save_bootstrap_option_not_empty("--server-ssl-crl",
                                              "server_ssl_crl", value);
      },
      [this](const auto &) {
        this->assert_bootstrap_mode("--server-ssl-crl");
      });

  arg_handler_.add_option(
      OptionNames({"--server-ssl-crlpath"}),
      "path name of the directory that contains certificate revocation-list "
      "files in PEM format. Used when forwarding a client connection from "
      "router to a server.",
      CmdOptionValueReq::required, "directory",
      [this](const auto &value) {
        this->save_bootstrap_option_not_empty("--server-ssl-crlpath",
                                              "server_ssl_crlpath", value);
      },
      [this](const auto &) {
        this->assert_bootstrap_mode("--server-ssl-crlpath");
      });

  arg_handler_.add_option(
      OptionNames({"--server-ssl-curves"}),
      "list of one or more colon separated elliptic curve names. Used when "
      "forwarding a client connection from router to a server.",
      CmdOptionValueReq::required, "",
      [this](const auto &value) {
        this->save_bootstrap_option_not_empty("--server-ssl-curves",
                                              "server_ssl_curves", value);
      },
      [this](const auto &) {
        this->assert_bootstrap_mode("--server-ssl-curves");
      });

  arg_handler_.add_option(
      OptionNames({"--server-ssl-mode"}),
      "SSL mode to use when forwarding a client connection from router to a "
      "server. One of DISABLED, PREFERRED, REQUIRED or AS_CLIENT.",
      CmdOptionValueReq::required, "ssl-mode",
      [this](const auto &value) {
        assert_one_of_ci("--server-ssl-mode", value,
                         {"DISABLED", "PREFERRED", "REQUIRED", "AS_CLIENT"});

        this->save_bootstrap_option_not_empty(
            "--server-ssl-mode", "server_ssl_mode", make_upper(value));
      },
      [this](const auto &) {
        this->assert_bootstrap_mode("--server-ssl-mode");
      });

  arg_handler_.add_option(
      OptionNames({"--server-ssl-verify"}),
      "verification mode when forwarding a client connection from router to "
      "server. One of DISABLED, VERIFY_CA or VERIFY_IDENTITY.",
      CmdOptionValueReq::required, "verify-mode",
      [this](const auto &value) {
        assert_one_of_ci("--server-ssl-verify", value,
                         {"DISABLED", "VERIFY_CA", "VERIFY_IDENTITY"});

        this->save_bootstrap_option_not_empty(
            "--server-ssl-verify", "server_ssl_verify", make_upper(value));
      },
      [this](const auto &) {
        this->assert_bootstrap_mode("--server-ssl-verify");
      });

  arg_handler_.add_option(
      OptionNames({"--ssl-ca"}),
      "Path to SSL CA file to verify server's certificate against when "
      "connecting to the metadata servers",
      CmdOptionValueReq::required, "path",
      [this](const std::string &path) {
        this->save_bootstrap_option_not_empty("--ssl-ca", "ssl_ca", path);
      },
      [this](const std::string &) { this->assert_bootstrap_mode("--ssl-ca"); });

  arg_handler_.add_option(
      OptionNames({"--ssl-capath"}),
      "Path to directory containing SSL CA files to verify server's "
      "certificate against when connecting to the metadata servers.",
      CmdOptionValueReq::required, "directory",
      [this](const std::string &path) {
        this->save_bootstrap_option_not_empty("--ssl-capath", "ssl_capath",
                                              path);
      },
      [this](const std::string &) {
        this->assert_bootstrap_mode("--ssl-capath");
      });

  arg_handler_.add_option(
      OptionNames({"--ssl-cert"}),
      "Path to a SSL certificate, to be used if client certificate "
      "verification is required when connecting to the metadata servers.",
      CmdOptionValueReq::required, "path",
      [this](const std::string &path) {
        this->save_bootstrap_option_not_empty("--ssl-cert", "ssl_cert", path);
      },
      [this](const std::string &) {
        this->assert_bootstrap_mode("--ssl-cert");
      });

  arg_handler_.add_option(
      OptionNames({"--ssl-cipher"}),
      ": separated list of SSL ciphers to allow when connecting to the "
      "metadata servers, if SSL is enabled.",
      CmdOptionValueReq::required, "ciphers",
      [this](const std::string &cipher) {
        this->save_bootstrap_option_not_empty("--ssl-cipher", "ssl_cipher",
                                              cipher);
      },
      [this](const std::string &) {
        this->assert_bootstrap_mode("--ssl-cipher");
      });

  arg_handler_.add_option(
      OptionNames({"--ssl-crl"}),
      "Path to SSL CRL file to use when connecting to metadata-servers and "
      "verifying their SSL certificate",
      CmdOptionValueReq::required, "path",
      [this](const std::string &path) {
        this->save_bootstrap_option_not_empty("--ssl-crl", "ssl_crl", path);
      },
      [this](const std::string &) {
        this->assert_bootstrap_mode("--ssl-crl");
      });

  arg_handler_.add_option(
      OptionNames({"--ssl-crlpath"}),
      "Path to directory containing SSL CRL files to use when connecting to "
      "metadata-servers and verifying their SSL certificate.",
      CmdOptionValueReq::required, "directory",
      [this](const std::string &path) {
        this->save_bootstrap_option_not_empty("--ssl-crlpath", "ssl_crlpath",
                                              path);
      },
      [this](const std::string &) {
        this->assert_bootstrap_mode("--ssl-crlpath");
      });

  arg_handler_.add_option(
      OptionNames({"--ssl-key"}),
      "Path to private key for client SSL certificate, to be used if client "
      "certificate verification is required when connecting to "
      "metadata-servers.",
      CmdOptionValueReq::required, "path",
      [this](const std::string &path) {
        this->save_bootstrap_option_not_empty("--ssl-key", "ssl_key", path);
      },
      [this](const std::string &) {
        this->assert_bootstrap_mode("--ssl-key");
      });

  arg_handler_.add_option(
      OptionNames({"--disable-rest"}),
      "Disable REST web service for Router monitoring", CmdOptionValueReq::none,
      "",
      [this](const std::string &) {
        this->bootstrap_options_["disable-rest"] = "1";
      },
      [this](const std::string &) {
        this->assert_bootstrap_mode("--disable-rest");
      });

  arg_handler_.add_option(
      OptionNames({"--https-port"}),
      "HTTPS port for Router monitoring REST web service",
      CmdOptionValueReq::required, "https-port",
      [this](const std::string &https_port) {
        this->bootstrap_options_["https-port"] = https_port;
      },
      [this](const std::string &https_port) {
        this->assert_bootstrap_mode("--https-port");
        if (this->bootstrap_options_.count("disable-rest") != 0) {
          throw std::runtime_error(
              "Option --disable-rest is not allowed when using --https-port "
              "option");
        }
        try {
          assert_option_value_in_range(https_port, 1, 65535);
        } catch (const std::exception &e) {
          throw std::runtime_error{
              std::string{"processing --https-port option failed, "} +
              e.what()};
        }
      });

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
      [this](const std::string &ssl_mode) {
        try {
          mysqlrouter::MySQLSession::parse_ssl_mode(
              ssl_mode);  // we only care if this succeeds
          bootstrap_options_["ssl_mode"] = ssl_mode;
        } catch (const std::logic_error &) {
          throw std::runtime_error("Invalid value for --ssl-mode option");
        }
      },
      [this](const std::string &) {
        this->assert_bootstrap_mode("--ssl-mode");
      });

  arg_handler_.add_option(
      OptionNames({"--strict"}),
      "Upgrades account verification failure warning into a fatal error. "
      "(bootstrap)",
      CmdOptionValueReq::none, "",
      [this](const std::string &) { this->bootstrap_options_["strict"] = "1"; },
      [this](const std::string &) { this->assert_bootstrap_mode("--strict"); });

  arg_handler_.add_option(
      OptionNames({"--tls-version"}),
      ", separated list of TLS versions to request, if SSL is enabled.",
      CmdOptionValueReq::required, "versions",
      [this](const std::string &version) {
        this->save_bootstrap_option_not_empty("--tls-version", "tls_version",
                                              version);
      },
      [this](const std::string &) {
        this->assert_bootstrap_mode("--tls-version");
      });
#ifndef _WIN32
  arg_handler_.add_option(
      OptionNames({"-u", "--user"}),
      "Run the mysqlrouter as the user having the name user_name.",
      CmdOptionValueReq::required, "username",
      [this](const std::string &username) { this->username_ = username; },
      [this](const std::string &) {
        if (this->bootstrap_uri_.empty()) {
          this->user_cmd_line_ = this->username_;
        } else {
          check_user(this->username_, true, this->sys_user_operations_);
          this->bootstrap_options_["user"] = this->username_;
        }
      });
#endif
  arg_handler_.add_option(
      CmdOption::OptionNames({"-V", "--version"}),
      "Display version information and exit.", CmdOptionValueReq::none, "",
      [this](const std::string &) {
        out_stream_ << this->get_version_line() << std::endl;
        this->showing_info_ = true;
      });

  arg_handler_.add_option(
      OptionNames({"--conf-set-option"}),
      "Allows forcing selected option in the configuration file when "
      "bootstrapping (--conf-set-option=section_name.option_name=value)",
      CmdOptionValueReq::required, "conf-set-option",
      [this](const std::string &conf_option) {
        std::vector<std::string> &conf_options =
            this->bootstrap_multivalue_options_["conf-set-option"];
        conf_options.push_back(conf_option);
      },
      [this](const std::string &) {
        this->assert_bootstrap_mode("--conf-set-option");
      });

// These are additional Windows-specific options, added (at the time of writing)
// in check_service_operations(). Grep after '--install-service' and you shall
// find.
#ifdef _WIN32
  arg_handler_.add_option(
      CmdOption::OptionNames({"--clear-all-credentials"}),
      "Clear the vault, removing all the credentials stored on it",
      CmdOptionValueReq::none, "", [](const std::string &) {
        PasswordVault pv;
        pv.clear_passwords();
        log_info("Removed successfully all passwords from the vault.");
        throw silent_exception();
      });

  // in this context we only want the service-related options to be known and
  // displayed with --help; they are handled elsewhere (main-windows.cc)
  ServiceConfOptions unused;
  add_service_options(arg_handler_, unused);

  arg_handler_.add_option(
      CmdOption::OptionNames({"--remove-credentials-section"}),
      "Removes the credentials for the given section",
      CmdOptionValueReq::required, "section_name",
      [](const std::string &value) {
        PasswordVault pv;
        pv.remove_password(value);
        pv.store_passwords();
        log_info("The password was removed successfully.");
        throw silent_exception();
      });

  arg_handler_.add_option(
      CmdOption::OptionNames({"--update-credentials-section"}),
      "Updates the credentials for the given section",
      CmdOptionValueReq::required, "section_name",
      [](const std::string &value) {
        std::string prompt = string_format(
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
void MySQLRouter::bootstrap(const std::string &program_name,
                            const std::string &server_url) {
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
  // Cannot run bootstrap mode as windows service since it requires console
  // interaction.
  if (mysqlrouter::is_running_as_service()) {
    std::string msg = "Cannot run router in boostrap mode as Windows service.";
    mysqlrouter::write_windows_event_log(msg);
    throw std::runtime_error(msg);
  }
#endif

  auto default_paths = get_default_paths();

  if (bootstrap_directory_.empty()) {
    std::string config_file_path =
        mysql_harness::Path(default_paths.at("config_folder"s))
            .join("mysqlrouter.conf"s)
            .str();
    std::string state_file_path =
        mysql_harness::Path(default_paths.at("data_folder"s))
            .join("state.json"s)
            .str();
    std::string master_key_path =
        mysql_harness::Path(default_paths.at("config_folder"s))
            .join("mysqlrouter.key"s)
            .str();
    std::string default_keyring_file = default_paths.at("data_folder"s);
    mysql_harness::Path keyring_dir(default_keyring_file);
    if (!keyring_dir.exists()) {
      if (mysql_harness::mkdir(default_keyring_file,
                               mysqlrouter::kStrictDirectoryPerm, true) < 0) {
        log_error(
            "Cannot create directory '%s': %s",
            truncate_string(default_keyring_file).c_str(),
            std::error_code{errno, std::generic_category()}.message().c_str());
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
        program_name, config_file_path, state_file_path, bootstrap_options_,
        bootstrap_multivalue_options_, default_paths);
  } else {
    keyring_info_.set_keyring_file(kDefaultKeyringFileName);
    keyring_info_.set_master_key_file("mysqlrouter.key");
    config_gen.set_keyring_info(keyring_info_);
    config_gen.bootstrap_directory_deployment(
        program_name, bootstrap_directory_, bootstrap_options_,
        bootstrap_multivalue_options_, default_paths);
  }
}

// format filename with indent
//
// if file isn't readable, wrap it in (...)
static void markup_configfile(std::ostream &os, const std::string &filename) {
  const bool file_is_readable = mysql_harness::Path(filename).is_readable();

  os << "  "
     //
     << (file_is_readable ? "" : "(") << filename
     << (file_is_readable ? "" : ")") << std::endl;
}

void MySQLRouter::show_help() {
  out_stream_ << get_version_line() << std::endl;
  out_stream_ << ORACLE_WELCOME_COPYRIGHT_NOTICE("2015") << std::endl;

  for (auto line : wrap_string(
           "Configuration read from the following files in the given order"
           " (enclosed in parentheses means not available for reading):",
           kHelpScreenWidth, 0)) {
    out_stream_ << line << std::endl;
  }

  for (const auto &file : default_config_files_) {
    markup_configfile(out_stream_, file);

    // fallback to .ini for each .conf file
    const std::string conf_ext(".conf");
    if (mysql_harness::utility::ends_with(file, conf_ext)) {
      // replace .conf by .ini
      std::string ini_filename =
          file.substr(0, file.size() - conf_ext.size()) + ".ini";

      markup_configfile(out_stream_, ini_filename);
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

  std::vector<std::pair<std::string, std::set<std::string>>> usage_sections{
      {"help", {"--help"}},
      {"version", {"--version"}},
      {"bootstrap",
       {"--account-host",
        "--bootstrap",
        "--bootstrap-socket",
        "--conf-use-sockets",
        "--conf-set-option",
        "--conf-skip-tcp",
        "--conf-base-port",
        "--conf-use-gr-notifications",
        "--connect-timeout",
        "--client-ssl-cert",
        "--client-ssl-cipher",
        "--client-ssl-curves",
        "--client-ssl-key",
        "--client-ssl-mode",
        "--core-file",
        "--directory",
        "--force",
        "--force-password-validation",
        "--name",
        "--master-key-reader",
        "--master-key-writer",
        "--password-retries",
        "--read-timeout",
        "--report-host",
        "--server-ssl-ca",
        "--server-ssl-capath",
        "--server-ssl-cipher",
        "--server-ssl-crl",
        "--server-ssl-crlpath",
        "--server-ssl-curves",
        "--server-ssl-mode",
        "--server-ssl-verify",
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
       {
           "--user",
           "--config",
           "--extra-config",
           "--clear-all-credentials",
           "--service",
           "--remove-service",
           "--install-service",
           "--install-service-manual",
           "--pid-file",
           "--remove-credentials-section",
           "--update-credentials-section",
           "--core-file",
       }}};

  for (const auto &section : usage_sections) {
    for (auto line : arg_handler_.usage_lines_if(
             "mysqlrouter", "", kHelpScreenWidth,
             [&section](const CmdOption &opt) {
               return cmd_option_acceptor(section.first, section.second, opt);
             })) {
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
