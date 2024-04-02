/*
  Copyright (c) 2015, 2024, Oracle and/or its affiliates.

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

#include "dim.h"
#include "harness_assert.h"
#include "keyring/keyring_manager.h"
#include "keyring_handler.h"
#include "mysql/harness/arg_handler.h"
#include "mysql/harness/config_option.h"
#include "mysql/harness/config_parser.h"
#include "mysql/harness/dynamic_config.h"
#include "mysql/harness/dynamic_state.h"
#include "mysql/harness/filesystem.h"
#include "mysql/harness/log_reopen_component.h"
#include "mysql/harness/logging/logger_plugin.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/logging/registry.h"
#include "mysql/harness/process_state_component.h"
#include "mysql/harness/section_config_exposer.h"
#include "mysql/harness/signal_handler.h"
#include "mysql/harness/string_utils.h"
#include "mysql/harness/supported_config_options.h"
#include "mysql/harness/utility/string.h"  // string_format
#include "mysql/harness/vt100.h"
#include "mysql_router_thread.h"  // kDefaultStackSizeInKiloByte
#include "mysqlrouter/config_files.h"
#include "mysqlrouter/connection_pool.h"
#include "mysqlrouter/default_paths.h"
#include "mysqlrouter/mysql_session.h"
#include "mysqlrouter/routing.h"
#include "mysqlrouter/supported_router_options.h"
#include "mysqlrouter/utils.h"
#include "print_version.h"
#include "router_config.h"  // MYSQL_ROUTER_VERSION
#include "scope_guard.h"
#include "welcome_copyright_notice.h"

#ifndef _WIN32
#include <fcntl.h>
#include <unistd.h>
#include <csignal>
const char dir_sep = '/';
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
#endif

IMPORT_LOG_FUNCTIONS()
using namespace std::string_literals;

using mysql_harness::DIM;
using mysql_harness::utility::string_format;
using mysql_harness::utility::wrap_string;
using mysqlrouter::SysUserOperationsBase;

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
  } else if (cfg_file_path.type() ==
             mysql_harness::Path::FileType::FILE_NOT_FOUND) {
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
      if (name != "logger.level" && name != "DEFAULT.plugin_folder") {
        throw std::runtime_error(
            "Invalid argument '--" + name +
            "'. Only '--logger.level' configuration option can be "
            "set with a command line parameter when bootstrapping.");
      }
    }
  }
}

std::string get_plugin_folder_overwrite(
    const CmdArgHandler::ConfigOverwrites &overwrites) {
  const auto default_key = std::make_pair("DEFAULT"s, ""s);
  if (overwrites.count(default_key) != 0) {
    const auto &default_overwrites = overwrites.at(default_key);
    if (default_overwrites.count("plugin_folder") != 0) {
      return default_overwrites.at("plugin_folder");
    }
  }

  return "";
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
      ,
      bootstrapper_(keyring_.get_ki(), out_stream_, err_stream_) {
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

  const bool is_bootstrap = bootstrapper_.is_bootstrap();
  const auto config_overwrites = arg_handler_.get_config_overwrites();
  check_config_overwrites(config_overwrites,
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
    bool user_option = bootstrapper_.bootstrap_options_.count("user") != 0;
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

    bootstrapper_.bootstrap(program_name, origin_, false,
                            get_plugin_folder_overwrite(config_overwrites)
#ifndef _WIN32
                                ,
                            sys_user_operations_
#endif
    );  // throws MySQLSession::Error, std::runtime_error,
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

static bool option_has_value(mysql_harness::ConfigSection *section,
                             const std::string &key,
                             const std::vector<std::string> &values) {
  if (!section->has(key)) return false;

  auto section_value = section->get(key);

  return std::any_of(values.begin(), values.end(),
                     [&section_value](auto &v) { return v == section_value; });
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

  if (!needs_keyring && config.has_any("mysql_rest_service")) {
    auto mysql_rest_service = config.get("mysql_rest_service");
    for (auto &section : mysql_rest_service) {
      if (section->has("mysql_user") ||
          section->has("mysql_user_data_access") ||
          option_has_value(section, "jwt_token", {"1", "true"})) {
        needs_keyring = true;
        break;
      }
    }
  }

  if (needs_keyring) {
    keyring_.init(config, false);
  }
}

void MySQLRouter::init_dynamic_state(mysql_harness::Config &config) {
  if (config.has_default(router::options::kDynamicState)) {
    using mysql_harness::DynamicState;

    const std::string dynamic_state_file =
        config.get_default(router::options::kDynamicState);
    DIM::instance().set_DynamicState(
        [=]() { return new DynamicState(dynamic_state_file); },
        std::default_delete<mysql_harness::DynamicState>());
    // force object creation, the further code relies on it's existence
    DIM::instance().get_DynamicState();
  }
}

/*static*/
void MySQLRouter::init_main_logger(mysql_harness::LoaderConfig &config,
                                   bool raw_mode /*= false*/,
                                   bool use_os_log /*= false*/) {
// currently logging to OS log is only supported on Windows
#ifndef _WIN32
  harness_assert(use_os_log == false);
#endif

  if (!config.has_default(mysql_harness::loader::options::kLoggingFolder))
    config.set_default(mysql_harness::loader::options::kLoggingFolder, "");

  const std::string logging_folder =
      config.get_default(mysql_harness::loader::options::kLoggingFolder);

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
  loader_->register_expose_app_config_callback(expose_router_configuration);
}

void MySQLRouter::start() {
  if (showing_info_ || bootstrapper_.is_bootstrap()) {
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
        return make_config(mysqlrouter::get_default_paths(origin_),
                           config_files);
      },
      std::default_delete<mysql_harness::LoaderConfig>());
  mysql_harness::LoaderConfig &config = DIM::instance().get_Config();

#ifndef _WIN32
  // --user param given on the command line has a priority over
  // the user in the configuration
  if (user_cmd_line_.empty() && config.has_default(router::options::kUser)) {
    set_user(config.get_default(router::options::kUser), true,
             this->sys_user_operations_);
  }
#endif

  if (!can_start_) {
    throw std::runtime_error("Can not start");
  }

  // Setup pidfile path for the application.
  // Order of significance: commandline > config file > ROUTER_PID envvar
  if (pid_file_path_.empty()) {
    if (config.has_default(router::options::kPidFile)) {
      const std::string pidfile = config.get_default(router::options::kPidFile);
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

  register_on_switch_to_configured_loggers_callback([]() {
    // once we switched to the configured logger(s) log the Router version
    log_system("Starting '%s', version: %s (%s)", MYSQL_ROUTER_PACKAGE_NAME,
               MYSQL_ROUTER_VERSION, MYSQL_ROUTER_VERSION_EDITION);
  });

  mysql_harness::ProcessStateComponent::get_instance()
      .register_on_shutdown_request_callback(
          [](mysql_harness::ShutdownPending::Reason reason,
             const std::string &msg) {
            // once we received the shutdown request, we want to log that along
            // with the Router version
            log_system(
                "Stopping '%s', version: %s (%s), reason: %s%s",
                MYSQL_ROUTER_PACKAGE_NAME, MYSQL_ROUTER_VERSION,
                MYSQL_ROUTER_VERSION_EDITION, to_string(reason).c_str(),
                msg.empty() ? "" : std::string(" ("s + msg + ")").c_str());
          });

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

    signal_handler_.add_sig_handler(
        SIGHUP,
        [&log_reopener](int /* sig */, const std::string & /*signal_info*/) {
          // is run by the signal-thread.
          log_reopener->request_reopen();
        });

    mysql_harness::on_service_ready(kLogReopenServiceName);

    // signal-handler
    signal_handler_.add_sig_handler(
        SIGTERM, [](int /* sig */, const std::string &signal_info) {
          mysql_harness::ProcessStateComponent::get_instance()
              .request_application_shutdown(
                  mysql_harness::ShutdownPending::Reason::REQUESTED,
                  signal_info);
        });

    signal_handler_.add_sig_handler(SIGINT, [](int /* sig */,
                                               const std::string &signal_info) {
      mysql_harness::ProcessStateComponent::get_instance()
          .request_application_shutdown(
              mysql_harness::ShutdownPending::Reason::REQUESTED, signal_info);
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

void MySQLRouter::assert_not_bootstrap_mode(
    const std::string &option_name) const {
  if (!this->bootstrapper_.bootstrap_uri_.empty())
    throw std::runtime_error("Option " + option_name +
                             " cannot be used together with -B/--bootstrap");
}

void MySQLRouter::prepare_command_options() noexcept {
  using OptionNames = CmdOption::OptionNames;

  // General guidelines for naming command line options:
  //
  // Option names that start with --conf are meant to affect
  // configuration only and used during bootstrap.
  // If an option affects the bootstrap process itself, it should
  // omit the --conf prefix, even if it affects both the bootstrap
  // and the configuration.

  arg_handler_.clear_options();
  bootstrapper_.prepare_command_options(arg_handler_);

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

#ifndef _WIN32
  arg_handler_.add_option(
      OptionNames({"-u", "--user"}),
      "Run the mysqlrouter as the user having the name user_name.",
      CmdOptionValueReq::required, "username",
      [this](const std::string &username) { this->username_ = username; },
      [this](const std::string &) {
        if (!bootstrapper_.is_bootstrap()) {
          this->user_cmd_line_ = this->username_;
        } else {
          check_user(this->username_, true, this->sys_user_operations_);
          bootstrapper_.bootstrap_options_["user"] = this->username_;
        }
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
      OptionNames({"-a", "--extra-config"}),
      "Read this file after configuration files are read from either "
      "default locations or from files specified by the --config option.",
      CmdOptionValueReq::required, "path", [this](const std::string &value) {
        check_and_add_conf(extra_config_files_, value);
      });
  arg_handler_.add_option(
      OptionNames({"-V", "--version"}), "Display version information and exit.",
      CmdOptionValueReq::none, "", [this](const std::string &) {
        out_stream_ << this->get_version_line() << std::endl;
        this->showing_info_ = true;
      });
  arg_handler_.add_option(
      OptionNames({"-?", "--help"}), "Display this help and exit.",
      CmdOptionValueReq::none, "", [this](const std::string &) {
        this->show_help();
        this->showing_info_ = true;
      });
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
  const std::map<std::string, std::string> paths =
      mysqlrouter::get_default_paths(origin_);
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

namespace {

class RouterAppConfigExposer : public mysql_harness::SectionConfigExposer {
 public:
  using DC = mysql_harness::DynamicConfig;
  RouterAppConfigExposer(const bool initial,
                         const mysql_harness::ConfigSection &default_section)
      : mysql_harness::SectionConfigExposer(initial, default_section,
                                            DC::SectionId{"common", ""}) {}

  void expose() override {
    // set "global" router options from DEFAULT section
    auto expose_int_option = [&](std::string_view option,
                                 const int64_t default_val) {
      auto value = default_val;
      if (default_section_.has(option)) {
        try {
          value = mysql_harness::option_as_int<int64_t>(
              default_section_.get(option), "");
        } catch (...) {
          // if it failed fallback to setting default
        }
      }

      expose_option(option, value, default_val);
    };

    auto expose_str_option = [&](std::string_view option,
                                 const std::string &default_val,
                                 bool skip_if_empty = false) {
      auto value = default_val;
      if (default_section_.has(option)) {
        value = default_section_.get(option);
      }

      const OptionValue op_val = (skip_if_empty && value.empty())
                                     ? OptionValue(std::monostate{})
                                     : value;
      const OptionValue op_default_val = (skip_if_empty && default_val.empty())
                                             ? OptionValue(std::monostate{})
                                             : default_val;

      expose_option(option, op_val, op_default_val);
    };

    expose_int_option(
        router::options::kMaxTotalConnections,
        static_cast<int64_t>(routing::kDefaultMaxTotalConnections));
    expose_str_option(router::options::kName, kSystemRouterName);
    // only share a user if it is not empty
    expose_str_option(router::options::kUser, kDefaultSystemUserName, true);
    expose_str_option("unknown_config_option", "error");
  }
};

}  // namespace

void expose_router_configuration(const bool initial,
                                 const mysql_harness::ConfigSection &section) {
  RouterAppConfigExposer(initial, section).expose();
}
