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
#include "log.h"
#include "options.h"
#include "IMService.h"
#include "manager.h"

IMService::IMService(void)
{
  serviceName= "MySqlManager";
  displayName= "MySQL Manager";
  username= NULL;
  password= NULL;
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

int HandleServiceOptions()
{
  int ret_val= 0;

  IMService winService;

  if (Options::Service::install_as_service)
  {
    if (winService.IsInstalled())
      log_info("Service is already installed.");
    else if (winService.Install())
      log_info("Service installed successfully.");
    else
    {
      log_error("Service failed to install.");
      ret_val= 1;
    }
  }
  else if (Options::Service::remove_service)
  {
    if (! winService.IsInstalled())
      log_info("Service is not installed.");
    else if (winService.Remove())
      log_info("Service removed successfully.");
    else
    {
      log_error("Service failed to remove.");
      ret_val= 1;
    }
  }
  else
  {
    log_info("Initializing Instance Manager service...");

    if (!winService.Init())
    {
      log_error("Service failed to initialize.");
      fprintf(stderr,
              "The service should be started by Windows Service Manager.\n"
              "The MySQL Manager should be started with '--standalone'\n"
              "to run from command line.");
      ret_val= 1;
    }
  }

  return ret_val;
}
