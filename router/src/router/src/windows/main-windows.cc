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

#include "mysql/harness/loader.h"
#include "nt_servc.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <fstream>
#include <iostream>

namespace {

const char *kRouterServiceName = "MySQLRouter";
const char *kRouterServiceDisplayName = "MySQL Router";
const char *kAccount = "NT AUTHORITY\\LocalService";

NTService g_service;
extern "C" bool g_windows_service = false;
int (*g_real_main)(int, char **);

std::string &add_quoted_string(std::string &to, const char *from) {
  if (!strchr(from, ' ')) return to.append(from);

  to.append("\"").append(from).append("\"");
  return to;
}

int router_service(void *p) {
  g_real_main(g_service.my_argc, g_service.my_argv);
  g_service.Stop();  // signal NTService to exit its thread, so we can exit the
                     // process
  return 0;
}

enum class ServiceStatus { StartNormal, StartAsService, Done, Error };

bool file_exists(const char *path) {
  std::ifstream f(path);
  return (!f) ? false : true;
}

ServiceStatus check_service_operations(int argc, char **argv) {
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
          std::cerr << "Service install option requires an existing "
                       "configuration file to be specified (-c <file>)\n";
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
ServiceStatus do_windows_init(int argc, char **argv) {
  // WinSock init
  WSADATA wsaData;
  int result;
  result = WSAStartup(MAKEWORD(2, 2), &wsaData);
  if (result != 0) {
    std::cerr << "WSAStartup failed with error: " << result << std::endl;
    return ServiceStatus::Error;
  }
  // check Windows service specific command line options
  ServiceStatus status = check_service_operations(argc, argv);
  // Windows service init
  g_service.my_argc = argc;
  g_service.my_argv = argv;
  return status;
}

void do_windows_cleanup() {
  // WinSock cleanup
  WSACleanup();

  // Windows service deinit
  if (g_service.IsNT() && g_windows_service) {
    g_service.Stop();
  } else {
    g_service.SetShutdownEvent(0);
  }
}

BOOL CtrlC_handler(DWORD ctrl_type) {
  if (ctrl_type == CTRL_C_EVENT) {
    // user presed Ctrl+C
    request_application_shutdown();
    return TRUE;  // don't pass this event to further handlers
  } else {
    // some other event
    return FALSE;  // let the default Windows handler deal with it
  }
}

void register_CtrlC_handler() {
  if (!SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlC_handler, TRUE)) {
    std::cerr << "Could not install Ctrl+C handler, exiting.\n";
    exit(1);
  }
}

}  // unnamed namespace

int proxy_main(int (*real_main)(int, char **), int argc, char **argv) {
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
        g_service.Init(kRouterServiceName, (void *)router_service,
                       request_application_shutdown);
        break;
      } else {
        std::cerr << "Could not find service 'MySQLRouter'!\n"
                  << "Use --install-service or --install-service-manual option "
                     "to install the service first.\n";
        exit(1);
      }
    case ServiceStatus::StartNormal:  // case when Router runs from "DOS"
                                      // console
      register_CtrlC_handler();
      g_service.SetRunning();
      result = real_main(argc, argv);
      break;
    case ServiceStatus::Done:
      return 0;
    case ServiceStatus::Error:
      return 1;
  }
  do_windows_cleanup();
  return result;
}
