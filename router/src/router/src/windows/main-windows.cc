/*
  Copyright (c) 2016, 2024, Oracle and/or its affiliates.

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

#ifdef _WIN32

#include "../router_app.h"

#include <cstring>
#include <fstream>
#include <iostream>

#include <windows.h>
#include <winsock2.h>

#include "harness_assert.h"
#include "main-windows.h"
#include "mysql/harness/loader.h"
#include "mysql/harness/logging/eventlog_plugin.h"
#include "mysql/harness/process_state_component.h"
#include "mysql/harness/signal_handler.h"
#include "mysqlrouter/default_paths.h"
#include "mysqlrouter/utils.h"  // write_windows_event_log
#include "mysqlrouter/windows/service_operations.h"
#include "nt_servc.h"

// forward declarations
void allow_windows_service_to_write_logs(const std::string &conf_file);

namespace {
const char *kAccount = "NT AUTHORITY\\LocalService";

NTService g_service;
int (*g_real_main)(int, char **, bool);

/** @brief log error message to console and Eventlog (default option)
 *
 * This function may be called at times when we don't yet know if we are
 * running as a service or not (therefore we don't know if the user expects
 * logs to land on the console or in the Eventlog). Therefore it has been
 * decided to always log into both, unless we know FOR SURE that we are NOT
 * running as a service, in which case we should log to console only.
 *
 * Rationale: When running as a service, user can't see the console, that's why
 *            we need to log to Eventlog. OTOH when running as a normal
 *            process, user can see console, so logging to Eventlog is not
 *            necessary, furthermore, probably something user doesn't expect,
 *            therefore we should not do it. However, there are times when we
 *            don't know if we're running as a service or not, in which case we
 *            must choose the safe approach and log to Eventlog, just in case
 *            the user might not have the console available.
 *
 * @param msg Error message to log
 *
 * @param certain_that_not_running_as_service Set this to true ONLY IF you are
 *        sure you're NOT running as a service. It will disable (needless)
 *        logging to Eventlog.
 */
void log_error(const std::string &msg,
               bool certain_that_not_running_as_service = false) noexcept {
  // We don't have to write to console when running as a service, but we do it
  // anyway because it doesn't hurt. Always better to err on the safe side.
  std::cerr << "Error: " << msg << std::endl;

  if (certain_that_not_running_as_service == false) {
    try {
      mysqlrouter::write_windows_event_log(msg);
    } catch (const std::runtime_error &) {
      // there's not much we can do other than to silently ignore logging
      // failure
    }
  }
}

std::string &add_quoted_string(std::string &to, const char *from) noexcept {
  if (!strchr(from, ' ')) return to.append(from);

  to.append("\"").append(from).append("\"");
  return to;
}

int router_service(void * /* p */) {
  g_real_main(g_service.my_argc, g_service.my_argv,
              true);  // true = log initially to Windows Eventlog
  g_service.Stop();   // signal NTService to exit its thread, so we can exit the
                      // process
  return 0;
}

enum class ServiceStatus { StartNormal, StartAsService, Done, Error };

bool file_exists(const std::string &path) noexcept {
  std::ifstream f(path);
  return (!f) ? false : true;
}

ServiceStatus check_service_operations(int argc, char **argv,
                                       std::string &out_service_name) {
  if (g_service.GetOS()) { /* true NT family */
    std::string full_service_path;
    ServiceConfOptions conf_opts;

    CmdArgHandler arg_handler{false, true};
    arg_handler.add_option(CmdOption::OptionNames({"-c", "--config"}),
                           "Only read configuration from given file.",
                           CmdOptionValueReq::required, "path",
                           [&conf_opts](const std::string &value) {
                             conf_opts.config_file = value;
                           });
    add_service_options(arg_handler, &conf_opts);

    try {
      arg_handler.process(std::vector<std::string>({argv + 1, argv + argc}));
    } catch (const std::invalid_argument &exc) {
      log_error(exc.what());
      return ServiceStatus::Error;
    }

    switch (conf_opts.operation) {
      case ServiceOperation::Install:
      case ServiceOperation::InstallManual:
        if (!file_exists(conf_opts.config_file)) {
          log_error(
              "Service install option requires an existing "
              "configuration file to be specified (-c <file>)",
              true);
          return ServiceStatus::Error;
        }

        try {
          // this will parse the config file, thus partially validate it as a
          // side-effect
          allow_windows_service_to_write_logs(conf_opts.config_file);
        } catch (const std::runtime_error &e) {
          log_error(
              std::string(
                  "Setting up file permissons for user LocalService failed: ") +
              e.what());
          return ServiceStatus::Error;
        }

        {
          char abs_path[1024];
          GetFullPathName(argv[0], sizeof(abs_path), abs_path, nullptr);
          add_quoted_string(full_service_path, abs_path);
          full_service_path.append(" -c ");
          GetFullPathName(conf_opts.config_file.c_str(), sizeof(abs_path),
                          abs_path, nullptr);
          add_quoted_string(full_service_path, abs_path);
        }
        full_service_path.append(" --service ");
        add_quoted_string(full_service_path, conf_opts.service_name.c_str());
        g_service.Install(
            conf_opts.operation == ServiceOperation::Install ? 1 : 0,
            conf_opts.service_name.c_str(),
            conf_opts.service_display_name.c_str(), full_service_path.c_str(),
            kAccount);
        return ServiceStatus::Done;
      case ServiceOperation::Remove:
        g_service.Remove(conf_opts.service_name.c_str());
        return ServiceStatus::Done;
      case ServiceOperation::Start:
        out_service_name = conf_opts.service_name;
        return ServiceStatus::StartAsService;
      case ServiceOperation::None:
        // normal start
        break;
    }
  }
  return ServiceStatus::StartNormal;
}

/* Windows specific initialization code.
 *
 * Performs socket library initialization and service related things, including
 * command line param handling for installation/removal of service.
 */
ServiceStatus do_windows_init(int argc, char **argv,
                              std::string &out_service_name) {
  // WinSock init
  WSADATA wsaData;
  int result;
  result = WSAStartup(MAKEWORD(2, 2), &wsaData);
  if (result != 0) {
    log_error(std::string("WSAStartup failed with error: ") +
              std::to_string(result));
    return ServiceStatus::Error;
  }
  // check Windows service specific command line options
  ServiceStatus status = check_service_operations(argc, argv, out_service_name);
  // Windows service init
  g_service.my_argc = argc;
  g_service.my_argv = argv;
  return status;
}

void do_windows_cleanup() noexcept {
  // WinSock cleanup
  WSACleanup();

  // Windows service deinit
  if (g_service.IsNT() && g_windows_service) {
    g_service.Stop();
  } else {
    g_service.SetShutdownEvent(nullptr);
  }
}

}  // unnamed namespace

/** @brief Returns path to directory containing Router's logfile
 *
 * This function first searches the config file for `logging_folder` and returns
 * that if found. If not, it returns default value (computed based on `argv0`).
 *
 * @param conf_file Path to Router configuration file
 *
 * @throws std::runtime_error if opening/parsing config file fails.
 *
 * @note this function is private to this compilation unit, but outside of
 *       unnamed namespace so it can be unit-tested.
 */
std::string get_logging_folder(const std::string &conf_file) {
  constexpr char kLoggingFolder[] = "logging_folder";
  std::string logging_folder;

  // try to obtain the logging_folder from config; if logging_folder is not
  // specified in the config file, config.read() will return an empty string
  mysql_harness::LoaderConfig config(mysql_harness::Config::allow_keys);
  {
    try {
      config.read(conf_file);  // throws (derivatives of) std::runtime_error,
                               // std::logic_error, ...?
    } catch (const std::exception &e) {
      std::string msg = std::string("Reading configuration file '") +
                        conf_file + "' failed: " + e.what();
      throw std::runtime_error(msg);
    }

    try {
      if (config.has_default(kLoggingFolder))
        logging_folder = config.get_default(kLoggingFolder);
    } catch (const std::runtime_error &) {
      // it could throw only if kLoggingFolder contained illegal characters
      harness_assert_this_should_not_execute();
    }
  }

  // if not provided, we have to compute the the logging_folder based on exec
  // path and predefined standard locations
  if (logging_folder.empty()) {
    const std::string router_exec_path = mysqlrouter::find_full_executable_path(
        std::string() /*ignored on Win*/);
    const mysql_harness::Path router_parent_dir =
        mysql_harness::Path(router_exec_path).dirname();
    const auto default_paths =
        mysqlrouter::get_default_paths(router_parent_dir);

    harness_assert(
        default_paths.count(kLoggingFolder));  // ensure .at() below won't throw
    logging_folder = default_paths.at(kLoggingFolder);
  }

  return logging_folder;
}

/** @brief Sets appropriate permissions on log dir so that Router can run
 *         as a Windows service
 *
 * This function first obtains logging_folder (first it checks Router config
 * file, if not found there, it uses the predefined default) and then sets RW
 * access for that folder, such that Router can run as a Windows service
 *
 *
 * @param conf_file Path to Router configuration file
 *
 * @throws std::runtime_error on any error (i.e. opening/parsing config file
 *         fails, log dir is bogus, setting permissions on log dir fails)
 *
 *
 * @note this function is private to this compilation unit, but outside of
 *       unnamed namespace so it can be unit-tested.
 *
 * @note we do not care about the logfile access rights. The assumption is that
 *       if the Service creates the file it will have a proper rights to write
 *       to it. If the file already exists and is missing proper rights there
 *       will be an error - we are letting the user to fix that.
 */
void allow_windows_service_to_write_logs(const std::string &conf_file) {
  // obtain logging_folder; throws std::runtime_error on failure
  std::string logging_folder = get_logging_folder(conf_file);
  harness_assert(!logging_folder.empty());

  using mysql_harness::Path;
  const Path path_to_logging_folder{logging_folder};

  if (!path_to_logging_folder.is_directory())
    throw std::runtime_error(
        std::string("logging_folder '") + logging_folder +
        "' specified (or implied) by configuration file '" + conf_file +
        "' does not point to a valid directory");

  // set RW permission for user LocalService on log directory
  try {
    mysql_harness::make_file_private(
        logging_folder, false /* false means: RW access for LocalService */);
  } catch (const std::exception &e) {
    std::string msg = "Setting RW access for LocalService on log directory '" +
                      logging_folder + "' failed: " + e.what();
    throw std::runtime_error(msg);
  }
}

int proxy_main(int (*real_main)(int, char **, bool), int argc, char **argv) {
  int result = 0;
  std::string service_name;
  switch (do_windows_init(argc, argv, service_name)) {
    case ServiceStatus::StartAsService:
      if (g_service.IsService(service_name.c_str())) {
        /* start the default service */
        g_windows_service = true;
        g_real_main = real_main;

        // blocks until one of following 2 functions are called:
        // - g_service.Stop()        (called by us after main() finishes)
        // - g_service.StopService() (triggered by OS due to outside event, such
        // as termination request)
        BOOL ok =
            g_service.Init(service_name.c_str(), (void *)router_service, []() {
              mysql_harness::ProcessStateComponent::get_instance()
                  .request_application_shutdown(
                      mysql_harness::ShutdownPending::Reason::REQUESTED);
            });
        if (!ok) {
          const std::error_code ec{static_cast<int>(GetLastError()),
                                   std::system_category()};
          if (ec.value() == ERROR_FAILED_SERVICE_CONTROLLER_CONNECT) {
            // typical reason for this failure, give hint
            log_error(
                "Starting service failed (are you trying to run a service from "
                "command-line?): " +
                ec.message());
          } else {
            log_error("Starting service failed: " + ec.message());
          }
        }
        result = 1;
      } else {
        log_error("Could not find service '" + service_name +
                  "'!\n"
                  "Use --install-service or --install-service-manual option "
                  "to install the service first.");
        exit(1);
      }
      break;
    case ServiceStatus::StartNormal:  // case when Router runs from "DOS"
                                      // console
      g_service.SetRunning();
      result = real_main(argc, argv, false);  // false = log initially to STDERR
      break;
    case ServiceStatus::Done:
      return 0;
    case ServiceStatus::Error:
      return 1;
  }
  do_windows_cleanup();
  return result;
}
#endif
