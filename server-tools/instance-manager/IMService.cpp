#include <windows.h>
#include "log.h"
#include "options.h"
#include "IMService.h"

IMService::IMService(void)
{
  serviceName = "MySqlManager";
  displayName = "MySQL Manager";
}

IMService::~IMService(void)
{
}

void IMService::Stop()
{
  ReportStatus(SERVICE_STOP_PENDING);
  // stop the IM work
}

void IMService::Run()
{
  // report to the SCM that we're about to start
  ReportStatus((DWORD)SERVICE_START_PENDING);

  // init goes here

	ReportStatus((DWORD)SERVICE_RUNNING);

  // wait for main loop to terminate
}

void IMService::Log(const char *msg)
{
  log_info(msg);
}

int HandleServiceOptions(Options options) 
{
  int ret_val = 0;

  IMService winService;

  if (options.install_as_service) 
  {
    if (winService.IsInstalled())
      log_info("Service is already installed\n");
    else if (winService.Install())
		  log_info("Service installed successfully\n");
    else
    {
	    log_info("Service failed to install\n");
      ret_val = -1;
    }
  }
  else if (options.remove_service)
  {
    if (! winService.IsInstalled())
      log_info("Service is not installed\n");
	  else if (winService.Remove())
		  log_info("Service removed successfully\n");
    else 
    {
	    log_info("Service failed to remove\n");
      ret_val = -1;
    }
  }
  else
    return (int)winService.Init();
  return ret_val;
}

