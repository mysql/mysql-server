/* Copyright (C) 2005 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#include <winsock2.h>
#include <signal.h>

#include "IMService.h"

#include "log.h"
#include "manager.h"
#include "options.h"

static const char * const IM_SVC_USERNAME= NULL;
static const char * const IM_SVC_PASSWORD= NULL;

IMService::IMService(void)
  :WindowsService("MySqlManager", "MySQL Manager")
{
}

IMService::~IMService(void)
{
}

void IMService::Stop()
{
  ReportStatus(SERVICE_STOP_PENDING);

  /* stop the IM work */
  raise(SIGTERM);
}

void IMService::Run(DWORD argc, LPTSTR *argv)
{
  /* report to the SCM that we're about to start */
  ReportStatus((DWORD)SERVICE_START_PENDING);

  Options::load(argc, argv);

  /* init goes here */
  ReportStatus((DWORD)SERVICE_RUNNING);

  /* wait for main loop to terminate */
  (void) Manager::main();
  Options::cleanup();
}

void IMService::Log(const char *msg)
{
  log_info(msg);
}

int IMService::main()
{
  IMService winService;

  if (Options::Service::install_as_service)
  {
    if (winService.IsInstalled())
    {
      log_info("Service is already installed.");
      return 1;
    }

    if (winService.Install(IM_SVC_USERNAME, IM_SVC_PASSWORD))
    {
      log_info("Service installed successfully.");
      return 0;
    }
    else
    {
      log_error("Service failed to install.");
      return 1;
    }
  }

  if (Options::Service::remove_service)
  {
    if (!winService.IsInstalled())
    {
      log_info("Service is not installed.");
      return 1;
    }

    if (winService.Remove())
    {
      log_info("Service removed successfully.");
      return 0;
    }
    else
    {
      log_error("Service failed to remove.");
      return 1;
    }
  }

  log_info("Initializing Instance Manager service...");

  if (!winService.Init())
  {
    log_error("Service failed to initialize.");

    fprintf(stderr,
      "The service should be started by Windows Service Manager.\n"
      "The MySQL Manager should be started with '--standalone'\n"
      "to run from command line.");

    return 1;
  }

  return 0;
}
