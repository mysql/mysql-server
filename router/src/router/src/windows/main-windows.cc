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

#include "../router_app.h"
#include "harness_assert.h"
#include "mysql/harness/loader.h"
#include "mysql/harness/logging/eventlog_plugin.h"
#include "nt_servc.h"
#include "utils.h"

#include <windows.h>
#include <winsock2.h>
#include <fstream>
#include <iostream>

// forward declarations
std::string get_logging_folder(const std::string &conf_file);
void allow_windows_service_to_write_logs(const std::string &conf_file);

namespace {

const char *kRouterServiceName = "MySQLRouter";
const char *kRouterServiceDisplayName = "MySQL Router";
const char *kAccount = "NT AUTHORITY\\LocalService";

NTService g_service;
extern "C" bool g_windows_service = false;
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
 *            neccessary, furthermore, probably something user doesn't expect,
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
static void log_error(
    const std::string &msg,
    bool certain_that_not_running_as_service = false) noexcept {
  // We don't have to write to console when running as a service, but we do it
  // anyway because it doesn't hurt. Always better to err on the safe side.
  std::cerr << "ERROR: " << msg << std::endl;

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

int router_service(void *p) {
  g_real_main(g_service.my_argc, g_service.my_argv,
              true);  // true = log initially to Windows Eventlog
  g_service.Stop();   // signal NTService to exit its thread, so we can exit the
                      // process
  return 0;
}

enum class ServiceStatus { StartNormal, StartAsService, Done, Error };

bool file_exists(const char *path) noexcept {
  std::ifstream f(path);
  return (!f) ? false : true;
}

ServiceStatus check_service_operations(int argc, char **argv) noexcept {
  if (g_service.GetOS()) { /* true NT family */
    // check if a service installation option was passed
    const char *config_path = NULL;
    std::string full_service_path;
    enum class ServiceOperation {
      None,
      Install,
      InstallManual,
      Remove,
      Start
    } operation = ServiceOperation::None;
    for (int i = 1; i < argc; i++) {
      if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--config") == 0) {
        if (i < argc - 1) {
          config_path = argv[++i];
        } else {
          config_path = NULL;
        }
      } else if (strcmp(argv[i], "--install-service") == 0) {
        operation = ServiceOperation::Install;
      } else if (strcmp(argv[i], "--install-service-manual") == 0) {
        operation = ServiceOperation::InstallManual;
      } else if (strcmp(argv[i], "--remove-service") == 0) {
        operation = ServiceOperation::Remove;
      } else if (strcmp(argv[i], "--service") == 0) {
        operation = ServiceOperation::Start;
      }
    }
    switch (operation) {
      case ServiceOperation::Install:
      case ServiceOperation::InstallManual:
        if (config_path == NULL || !file_exists(config_path)) {
          log_error(
              "Service install option requires an existing "
              "configuration file to be specified (-c <file>)",
              true);
          return ServiceStatus::Error;
        }

        try {
          // this will parse the config file, thus partially validate it as a
          // side-effect
          allow_windows_service_to_write_logs(config_path);
        } catch (const std::runtime_error &e) {
          log_error(
              std::string(
                  "Setting up file permissons for user LocalService failed: ") +
              e.what());
          return ServiceStatus::Error;
        }

        {
          char abs_path[1024];
          GetFullPathName(argv[0], sizeof(abs_path), abs_path, NULL);
          add_quoted_string(full_service_path, abs_path);
          full_service_path.append(" -c ");
          GetFullPathName(config_path, sizeof(abs_path), abs_path, NULL);
          add_quoted_string(full_service_path, abs_path);
        }
        full_service_path.append(" --service");
        g_service.Install(operation == ServiceOperation::Install ? 1 : 0,
                          kRouterServiceName, kRouterServiceDisplayName,
                          full_service_path.c_str(), kAccount);
        return ServiceStatus::Done;
      case ServiceOperation::Remove:
        g_service.Remove(kRouterServiceName);
        return ServiceStatus::Done;
      case ServiceOperation::Start:
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
ServiceStatus do_windows_init(int argc, char **argv) noexcept {
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
  ServiceStatus status = check_service_operations(argc, argv);
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
    g_service.SetShutdownEvent(0);
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
    const std::string router_exec_path =
        MySQLRouter::find_full_path(std::string() /*ignored on Win*/);
    const mysql_harness::Path router_parent_dir =
        mysql_harness::Path(router_exec_path).dirname();
    const auto default_paths =
        MySQLRouter::get_default_paths(router_parent_dir);

    harness_assert(
        default_paths.count(kLoggingFolder));  // ensure .at() below won't throw
    logging_folder = default_paths.at(kLoggingFolder);
  }

  return logging_folder;
}

/** @brief Sets appropriate permissions on log dir/file so that Router can run
 *         as a Windows service
 *
 * This function first obtains logging_folder (first it checks Router config
 * file, if not found there, it uses the predefined default) and then sets RW
 * access for that folder, and log file inside of it (if present), such that
 * Router can run as a Windows service (at the time of writing, it runs as user
 * `LocalService`).
 *
 * @param conf_file Path to Router configuration file
 *
 * @throws std::runtime_error on any error (i.e. opening/parsing config file
 *         fails, log dir or file is bogus, setting permissions on log dir or
 *         file fails)
 *
 * @note At the moment we don't give delete rights, but these might be needed
 *       when we implement log file rotation on Windows.
 *
 * @note this function is private to this compilation unit, but outside of
 *       unnamed namespace so it can be unit-tested.
 */
void allow_windows_service_to_write_logs(const std::string &conf_file) {
  // obtain logging_folder; throws std::runtime_error on failure
  std::string logging_folder = get_logging_folder(conf_file);
  harness_assert(!logging_folder.empty());

  using mysql_harness::Path;
  const Path path_to_logging_folder{logging_folder};
  Path path_to_logging_file{path_to_logging_folder.join("mysqlrouter.log")};

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

  // set RW permission for user LocalService on log file
  if (path_to_logging_file.is_regular()) {
    try {
      mysql_harness::make_file_private(
          path_to_logging_file.str(),
          false /* false means: RW access for LocalService */);
    } catch (const std::exception &e) {
      std::string msg = "Setting RW access for LocalService on log file '" +
                        path_to_logging_file.str() + "' failed: " + e.what();
      throw std::runtime_error(msg);
    }
  } else if (path_to_logging_file.exists()) {
    throw std::runtime_error(std::string("Path '") +
                             path_to_logging_file.str() +
                             "' does not point to a regular file");
  }
}

int proxy_main(int (*real_main)(int, char **, bool), int argc, char **argv) {
  int result = 0;
  switch (do_windows_init(argc, argv)) {
    case ServiceStatus::StartAsService:
      if (g_service.IsService(kRouterServiceName)) {
        /* start the default service */
        g_windows_service = true;
        g_real_main = real_main;

        // blocks until one of following 2 functions are called:
        // - g_service.Stop()        (called by us after main() finishes)
        // - g_service.StopService() (triggered by OS due to outside event, such
        // as termination request)
        BOOL ok = g_service.Init(kRouterServiceName, (void *)router_service,
                                 request_application_shutdown);
        if (!ok) {
          DWORD WINAPI err = GetLastError();

          char err_msg[512];
          FormatMessage(
              FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_MAX_WIDTH_MASK,
              nullptr, err, LANG_NEUTRAL, err_msg, sizeof(err_msg), nullptr);
          if (err == ERROR_FAILED_SERVICE_CONTROLLER_CONNECT) {
            // typical reason for this failure, give hint
            log_error(
                std::string("Starting service failed (are you trying to "
                            "run Router as a service from command-line?): ") +
                err_msg);
          } else {
            log_error(std::string("Starting service failed: ") + err_msg);
          }
        }
        result = 1;
      } else {
        log_error(
            "Could not find service 'MySQLRouter'!\n"
            "Use --install-service or --install-service-manual option "
            "to install the service first.");
        exit(1);
      }
      break;
    case ServiceStatus::StartNormal:  // case when Router runs from "DOS"
                                      // console
      register_ctrl_c_handler();
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
