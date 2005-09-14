#include <windows.h>
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
  
  // stop the IM work
  raise(SIGTERM);
}

void IMService::Run(DWORD argc, LPTSTR *argv)
{
  // report to the SCM that we're about to start
  ReportStatus((DWORD)SERVICE_START_PENDING);

  Options o;
  o.load(argc, argv);
  
  // init goes here
  ReportStatus((DWORD)SERVICE_RUNNING);

  // wait for main loop to terminate
  manager(o);
  o.cleanup();
}

void IMService::Log(const char *msg)
{
  log_info(msg);
}

int HandleServiceOptions(Options options)
{
  int ret_val= 0;

  IMService winService;

  if (options.install_as_service)
  {
    if (winService.IsInstalled())
      log_info("Service is already installed");
    else if (winService.Install())
      log_info("Service installed successfully");
    else
    {
      log_info("Service failed to install");
      ret_val= 1;
    }
  }
  else if (options.remove_service)
  {
    if (! winService.IsInstalled())
      log_info("Service is not installed");
    else if (winService.Remove())
      log_info("Service removed successfully");
    else
    {
      log_info("Service failed to remove");
      ret_val= 1;
    }
  }
  else
    ret_val= !winService.Init();
  return ret_val;
}
