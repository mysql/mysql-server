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

#include "my_global.h"
#include <windows.h>
#include "WindowsService.h"

static WindowsService *gService;

WindowsService::WindowsService(const char *p_serviceName,
                               const char *p_displayName) :
  statusCheckpoint(0),
  serviceName(p_serviceName),
  displayName(p_displayName),
  inited(FALSE),
  dwAcceptedControls(SERVICE_ACCEPT_STOP),
  debugging(FALSE)
{
  DBUG_ASSERT(serviceName != NULL);

  /* TODO: shouldn't we check displayName too (can it really be NULL)? */

  /* WindowsService is assumed to be singleton. Let's assure this. */
  DBUG_ASSERT(gService == NULL);

  gService= this;

  status.dwServiceType= SERVICE_WIN32_OWN_PROCESS;
  status.dwServiceSpecificExitCode= 0;
}

WindowsService::~WindowsService(void)
{
}

BOOL WindowsService::Install(const char *username, const char *password)
{
  bool ret_val= FALSE;
  SC_HANDLE newService;
  SC_HANDLE scm;

  if (IsInstalled())
    return TRUE;

  // determine the name of the currently executing file
  char szFilePath[_MAX_PATH];
  GetModuleFileName(NULL, szFilePath, sizeof(szFilePath));

  // open a connection to the SCM
  if (!(scm= OpenSCManager(0, 0,SC_MANAGER_CREATE_SERVICE)))
    return FALSE;

  newService= CreateService(scm, serviceName, displayName,
                            SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
                            SERVICE_AUTO_START, SERVICE_ERROR_NORMAL,
                            szFilePath, NULL, NULL, NULL, username,
                            password);

  if (newService)
  {
    CloseServiceHandle(newService);
    ret_val= TRUE;
  }

  CloseServiceHandle(scm);
  return ret_val;
}

BOOL WindowsService::Init()
{
  DBUG_ASSERT(serviceName != NULL);

  if (inited)
    return TRUE;

  SERVICE_TABLE_ENTRY stb[] =
  {
    { (LPSTR)serviceName, (LPSERVICE_MAIN_FUNCTION) ServiceMain},
    { NULL, NULL }
  };
  inited= TRUE;
  return StartServiceCtrlDispatcher(stb); //register with the Service Manager
}

BOOL WindowsService::Remove()
{
  bool ret_val= FALSE;

  if (!IsInstalled())
    return TRUE;

  // open a connection to the SCM
  SC_HANDLE scm= OpenSCManager(0, 0,SC_MANAGER_CREATE_SERVICE);
  if (!scm)
    return FALSE;

  SC_HANDLE service= OpenService(scm, serviceName, DELETE);
  if (service)
  {
    if (DeleteService(service))
      ret_val= TRUE;
    DWORD dw= ::GetLastError();
    CloseServiceHandle(service);
  }

  CloseServiceHandle(scm);
  return ret_val;
}

BOOL WindowsService::IsInstalled()
{
  BOOL ret_val= FALSE;

  SC_HANDLE scm= ::OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
  SC_HANDLE serv_handle= ::OpenService(scm, serviceName, SERVICE_QUERY_STATUS);

  ret_val= serv_handle != NULL;

  ::CloseServiceHandle(serv_handle);
  ::CloseServiceHandle(scm);

  return ret_val;
}

void WindowsService::SetAcceptedControls(DWORD acceptedControls)
{
  dwAcceptedControls= acceptedControls;
}


BOOL WindowsService::ReportStatus(DWORD currentState, DWORD waitHint,
                                  DWORD dwError)
{
  if (debugging)
    return TRUE;

  if(currentState == SERVICE_START_PENDING)
    status.dwControlsAccepted= 0;
  else
    status.dwControlsAccepted= dwAcceptedControls;

  status.dwCurrentState= currentState;
  status.dwWin32ExitCode= dwError != 0 ?
    ERROR_SERVICE_SPECIFIC_ERROR : NO_ERROR;
  status.dwWaitHint= waitHint;
  status.dwServiceSpecificExitCode= dwError;

  if(currentState == SERVICE_RUNNING || currentState == SERVICE_STOPPED)
  {
    status.dwCheckPoint= 0;
    statusCheckpoint= 0;
  }
  else
    status.dwCheckPoint= ++statusCheckpoint;

  // Report the status of the service to the service control manager.
  BOOL result= SetServiceStatus(statusHandle, &status);
  if (!result)
    Log("ReportStatus failed");

  return result;
}

void WindowsService::RegisterAndRun(DWORD argc, LPTSTR *argv)
{
  statusHandle= ::RegisterServiceCtrlHandler(serviceName, ControlHandler);
  if (statusHandle && ReportStatus(SERVICE_START_PENDING))
    Run(argc, argv);
  ReportStatus(SERVICE_STOPPED);
}

void WindowsService::HandleControlCode(DWORD opcode)
{
  // Handle the requested control code.
  switch(opcode) {
  case SERVICE_CONTROL_STOP:
    // Stop the service.
    status.dwCurrentState= SERVICE_STOP_PENDING;
    Stop();
    break;

  case SERVICE_CONTROL_PAUSE:
    status.dwCurrentState= SERVICE_PAUSE_PENDING;
    Pause();
    break;

  case SERVICE_CONTROL_CONTINUE:
    status.dwCurrentState= SERVICE_CONTINUE_PENDING;
    Continue();
    break;

  case SERVICE_CONTROL_SHUTDOWN:
    Shutdown();
    break;

  case SERVICE_CONTROL_INTERROGATE:
    ReportStatus(status.dwCurrentState);
    break;

  default:
    // invalid control code
    break;
  }
}

void WINAPI WindowsService::ServiceMain(DWORD argc, LPTSTR *argv)
{
  DBUG_ASSERT(gService != NULL);

  // register our service control handler:
  gService->RegisterAndRun(argc, argv);
}

void WINAPI WindowsService::ControlHandler(DWORD opcode)
{
  DBUG_ASSERT(gService != NULL);

  return gService->HandleControlCode(opcode);
}
