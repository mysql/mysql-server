/**
  @file

  @brief
  Windows NT Service class library.

  Copyright Abandoned 1998 Irena Pancirov - Irnet Snc
  This file is public domain and comes with NO WARRANTY of any kind

  Modifications Copyright (c) 2000, 2024, Oracle and/or its affiliates.
  All rights reserved.
*/
#ifdef _WIN32

#include "nt_servc.h"

#include <VersionHelpers.h>  // IsWindowsXPOrGreater
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

static NTService *pService;

/* ------------------------------------------------------------------------

 -------------------------------------------------------------------------- */
NTService::NTService() {
  bOsNT = FALSE;

  // service variables
  ServiceName = nullptr;
  hExitEvent = nullptr;
  bPause = FALSE;
  bRunning = FALSE;
  hThreadHandle = nullptr;
  fpServiceThread = nullptr;

  // time-out variables
  nStartTimeOut = 15000;
  nStopTimeOut = 86400000;
  nPauseTimeOut = 5000;
  nResumeTimeOut = 5000;

  // install variables
  dwDesiredAccess = SERVICE_ALL_ACCESS;
  dwServiceType = SERVICE_WIN32_OWN_PROCESS;
  dwStartType = SERVICE_AUTO_START;
  dwErrorControl = SERVICE_ERROR_NORMAL;
  szLoadOrderGroup = nullptr;
  lpdwTagID = nullptr;
  szDependencies = nullptr;

  my_argc = 0;
  my_argv = nullptr;
  hShutdownEvent = nullptr;
  nError = 0;
  dwState = 0;
}

/* ------------------------------------------------------------------------

 -------------------------------------------------------------------------- */
NTService::~NTService() {
  if (ServiceName != nullptr) delete[] ServiceName;
}
/* ------------------------------------------------------------------------

 -------------------------------------------------------------------------- */

BOOL NTService::GetOS() noexcept { return IsWindowsXPOrGreater(); }

/**
  Registers the main service thread with the service manager.

  @param szInternName   Name of service to run in this process.
  @param ServiceThread  pointer to the main programs entry function
                        when the service is started
  @param fpReqAppShutdownCb  pointer to callback function to call after
  receiving shutdown signal from Service Controller
*/

long NTService::Init(LPCSTR szInternName, void *ServiceThread,
                     void (*fpReqAppShutdownCb)()) noexcept {
  pService = this;

  fpRequestApplicationShutdownCallback = fpReqAppShutdownCb;

  fpServiceThread = (THREAD_FC)ServiceThread;
  ServiceName = new char[lstrlen(szInternName) + 1];
  lstrcpy(ServiceName, szInternName);

  SERVICE_TABLE_ENTRY stb[] = {
      {const_cast<char *>(szInternName),
       reinterpret_cast<LPSERVICE_MAIN_FUNCTION>(ServiceMain)},
      {nullptr, nullptr}};

  return StartServiceCtrlDispatcher(stb);  // register with the Service Manager
}

/**
  Installs the service with Service manager.

  nError values:
  - 0  success
  - 1  Can't open the Service manager
  - 2  Failed to create service.
*/

BOOL NTService::Install(int startType, LPCSTR szInternName,
                        LPCSTR szDisplayName, LPCSTR szFullPath,
                        LPCSTR szAccountName, LPCSTR szPassword) noexcept {
  BOOL ret_val = FALSE;
  SC_HANDLE newService, scm;

  if (!SeekStatus(szInternName, 1)) return FALSE;

  char szFilePath[_MAX_PATH];
  GetModuleFileName(nullptr, szFilePath, sizeof(szFilePath));

  // open a connection to the SCM
  if (!(scm = OpenSCManager(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE)))
    printf("Failed to install the service (Couldn't open the SCM)\n");
  else  // Install the new service
  {
    if (!(newService = CreateService(
              scm, szInternName, szDisplayName,
              dwDesiredAccess,  // default: SERVICE_ALL_ACCESS
              dwServiceType,    // default: SERVICE_WIN32_OWN_PROCESS
                                // default: SERVICE_AUTOSTART
              (startType == 1 ? SERVICE_AUTO_START : SERVICE_DEMAND_START),
              dwErrorControl,    // default: SERVICE_ERROR_NORMAL
              szFullPath,        // exec full path
              szLoadOrderGroup,  // default: NULL
              lpdwTagID,         // default: NULL
              szDependencies,    // default: NULL
              szAccountName,     // default: NULL
              szPassword)))      // default: NULL
      printf("Failed to install the service (Couldn't create service)\n");
    else {
      printf("Service successfully installed.\n");
      CloseServiceHandle(newService);
      ret_val = TRUE;  // Everything went ok
    }
    CloseServiceHandle(scm);
  }
  return ret_val;
}

/**
  Removes  the service.

  nError values:
  - 0  success
  - 1  Can't open the Service manager
  - 2  Failed to locate service
  - 3  Failed to delete service.
*/

BOOL NTService::Remove(LPCSTR szInternName) {
  BOOL ret_value = FALSE;
  SC_HANDLE service, scm;

  if (!SeekStatus(szInternName, 0)) return FALSE;

  nError = 0;

  // open a connection to the SCM
  if (!(scm = OpenSCManager(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE))) {
    printf("Failed to remove the service (Couldn't open the SCM)\n");
  } else {
    if ((service = OpenService(scm, szInternName, DELETE))) {
      if (!DeleteService(service))
        printf("Failed to remove the service (code %lu)\n", GetLastError());
      else {
        printf("Service successfully removed.\n");
        ret_value = TRUE;  // everything went ok
      }
      CloseServiceHandle(service);
    } else
      printf("Failed to remove the service (Couldn't open the service)\n");
    CloseServiceHandle(scm);
  }
  return ret_value;
}

/**
  this function should be called before the app. exits to stop
  the service
*/
void NTService::Stop(void) {
  SetStatus(SERVICE_STOP_PENDING, NO_ERROR, 0, 1, 60000);
  StopService();
  SetStatus(SERVICE_STOPPED, NO_ERROR, 0, 1, 1000);
}

/**
  This is the function that is called from the
  service manager to start the service.
*/

void NTService::ServiceMain(DWORD /* argc */, LPTSTR * /* argv */) {
  // registration function
  if (!(pService->hServiceStatusHandle = RegisterServiceCtrlHandler(
            pService->ServiceName,
            (LPHANDLER_FUNCTION)NTService::ServiceCtrlHandler)))
    goto error;

  // notify SCM of progress
  if (!pService->SetStatus(SERVICE_START_PENDING, NO_ERROR, 0, 1, 8000))
    goto error;

  // create the exit event
  if (!(pService->hExitEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr)))
    goto error;

  if (!pService->SetStatus(SERVICE_START_PENDING, NO_ERROR, 0, 3,
                           pService->nStartTimeOut))
    goto error;

  // save start arguments
  // pService->my_argc=argc;
  // pService->my_argv=argv;

  pService->SetSlowStarting(120000);

  // start the service
  if (!pService->StartService()) goto error;

  pService->SetRunning();

  // wait for exit event
  WaitForSingleObject(pService->hExitEvent, INFINITE);

  // wait for Router main() thread to exit
  if (WaitForSingleObject(pService->hThreadHandle, INFINITE) == WAIT_TIMEOUT)
    CloseHandle(pService->hThreadHandle);

  pService->Exit(0);
  return;

error:
  pService->Exit(GetLastError());
  return;
}

void NTService::SetRunning() {
  if (pService) pService->SetStatus(SERVICE_RUNNING, NO_ERROR, 0, 0, 0);
}

void NTService::SetSlowStarting(unsigned long timeout) {
  if (pService)
    pService->SetStatus(SERVICE_START_PENDING, NO_ERROR, 0, 0, timeout);
}

/* ------------------------------------------------------------------------
   StartService() - starts the application thread
 -------------------------------------------------------------------------- */

BOOL NTService::StartService() {
  // Start the real service's thread (application)
  if (!(hThreadHandle =
            (HANDLE)_beginthread((THREAD_FC)fpServiceThread, 0, (void *)this)))
    return FALSE;
  bRunning = TRUE;
  return TRUE;
}
/* ------------------------------------------------------------------------

 -------------------------------------------------------------------------- */
void NTService::StopService() {
  // notify Loader to initiate shutdown
  fpRequestApplicationShutdownCallback();

  bRunning = FALSE;

  // Set the event for application
  if (hShutdownEvent) SetEvent(hShutdownEvent);

  // Set the event for ServiceMain
  SetEvent(hExitEvent);
}
/* ------------------------------------------------------------------------

 -------------------------------------------------------------------------- */
void NTService::PauseService() {
  bPause = TRUE;
  SuspendThread(hThreadHandle);
}
/* ------------------------------------------------------------------------

 -------------------------------------------------------------------------- */
void NTService::ResumeService() {
  bPause = FALSE;
  ResumeThread(hThreadHandle);
}
/* ------------------------------------------------------------------------

 -------------------------------------------------------------------------- */
BOOL NTService::SetStatus(DWORD dwCurrentState, DWORD dwWin32ExitCode,
                          DWORD dwServiceSpecificExitCode, DWORD dwCheckPoint,
                          DWORD dwWaitHint) {
  BOOL bRet;
  SERVICE_STATUS serviceStatus;

  dwState = dwCurrentState;

  serviceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
  serviceStatus.dwCurrentState = dwCurrentState;

  if (dwCurrentState == SERVICE_START_PENDING)
    serviceStatus.dwControlsAccepted = 0;  // don't accept control events
  else
    serviceStatus.dwControlsAccepted =
        (SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_PAUSE_CONTINUE |
         SERVICE_ACCEPT_SHUTDOWN);

  // if a specific exit code is defined,set up the win32 exit code properly
  if (dwServiceSpecificExitCode == 0)
    serviceStatus.dwWin32ExitCode = dwWin32ExitCode;
  else
    serviceStatus.dwWin32ExitCode = ERROR_SERVICE_SPECIFIC_ERROR;

  serviceStatus.dwServiceSpecificExitCode = dwServiceSpecificExitCode;

  serviceStatus.dwCheckPoint = dwCheckPoint;
  serviceStatus.dwWaitHint = dwWaitHint;

  // Pass the status to the Service Manager
  if (!(bRet = SetServiceStatus(hServiceStatusHandle, &serviceStatus)))
    StopService();

  return bRet;
}
/* ------------------------------------------------------------------------

 -------------------------------------------------------------------------- */
void NTService::ServiceCtrlHandler(DWORD ctrlCode) {
  DWORD dwState;

  if (!pService) return;

  dwState = pService->dwState;  // get current state

  switch (ctrlCode) {
    case SERVICE_CONTROL_SHUTDOWN:
    case SERVICE_CONTROL_STOP:
      dwState = SERVICE_STOP_PENDING;
      pService->SetStatus(SERVICE_STOP_PENDING, NO_ERROR, 0, 1,
                          pService->nStopTimeOut);
      pService->StopService();
      break;

    default:
      pService->SetStatus(dwState, NO_ERROR, 0, 0, 0);
      break;
  }
  // pService->SetStatus(dwState, NO_ERROR,0, 0, 0);
}

/* ------------------------------------------------------------------------

 -------------------------------------------------------------------------- */

void NTService::Exit(DWORD error) {
  if (hExitEvent) CloseHandle(hExitEvent);

  // Send a message to the scm to tell that we stop
  if (hServiceStatusHandle) SetStatus(SERVICE_STOPPED, error, 0, 0, 0);

  // If the thread has started kill it ???
  // if (hThreadHandle) CloseHandle(hThreadHandle);
}

/* ------------------------------------------------------------------------

 -------------------------------------------------------------------------- */

BOOL NTService::SeekStatus(LPCSTR szInternName, int OperationType) {
  BOOL ret_value = FALSE;
  SC_HANDLE service, scm;

  // open a connection to the SCM
  if (!(scm = OpenSCManager(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE))) {
    DWORD ret_error = GetLastError();
    if (ret_error == ERROR_ACCESS_DENIED) {
      printf("Install/Remove of the Service Denied!\n");
      if (!is_super_user())
        printf(
            "That operation should be made by a user with Administrator "
            "privileges!\n");
    } else
      printf("There is a problem when opening the Service Control Manager!\n");
  } else {
    if (OperationType == 1) {
      /* an install operation */
      if ((service = OpenService(scm, szInternName, SERVICE_ALL_ACCESS))) {
        LPQUERY_SERVICE_CONFIG ConfigBuf;
        DWORD dwSize;

        ConfigBuf = (LPQUERY_SERVICE_CONFIG)LocalAlloc(LPTR, 4096);
        printf("The service already exists!\n");
        if (QueryServiceConfig(service, ConfigBuf, 4096, &dwSize))
          printf("The current server installed: %s\n",
                 ConfigBuf->lpBinaryPathName);
        LocalFree(ConfigBuf);
        CloseServiceHandle(service);
      } else
        ret_value = TRUE;
    } else {
      /* a remove operation */
      if (!(service = OpenService(scm, szInternName, SERVICE_ALL_ACCESS)))
        printf("The service doesn't exist!\n");
      else {
        SERVICE_STATUS ss;

        memset(&ss, 0, sizeof(ss));
        if (QueryServiceStatus(service, &ss)) {
          DWORD dwState = ss.dwCurrentState;
          if (dwState == SERVICE_RUNNING)
            printf(
                "Failed to remove the service because the service is "
                "running\nStop the service and try again\n");
          else if (dwState == SERVICE_STOP_PENDING)
            printf(
                "Failed to remove the service because the service is in stop "
                "pending state!\n"
                "Wait 30 seconds and try again.\n"
                "If this condition persist, reboot the machine and try "
                "again\n");
          else
            ret_value = TRUE;
        }
        CloseServiceHandle(service);
      }
    }
    CloseServiceHandle(scm);
  }

  return ret_value;
}

/**
 * @brief Checks if service named `service_name` exists
 *
 * @param service_name Name of the service
 * @return true if service exists, false otherwise
 *
 * @note It could also return false if opening Service Control Manager failed
 *       for some reason.
 */
/*static*/ BOOL NTService::IsService(LPCSTR service_name) noexcept {
  BOOL ret_value = FALSE;
  SC_HANDLE service, scm;

  if ((scm = OpenSCManager(nullptr, nullptr, SC_MANAGER_ENUMERATE_SERVICE))) {
    if ((service = OpenService(scm, service_name, SERVICE_QUERY_STATUS))) {
      ret_value = TRUE;
      CloseServiceHandle(service);
    }
    CloseServiceHandle(scm);
  }
  return ret_value;
}
/* ------------------------------------------------------------------------
 -------------------------------------------------------------------------- */
BOOL NTService::got_service_option(char **argv, char *service_option) {
  char *option;
  for (option = argv[1]; *option; option++)
    if (!strcmp(option, service_option)) return TRUE;
  return FALSE;
}
/* ------------------------------------------------------------------------
 -------------------------------------------------------------------------- */
BOOL NTService::is_super_user() {
  HANDLE hAccessToken;
  UCHAR InfoBuffer[1024];
  PTOKEN_GROUPS ptgGroups = (PTOKEN_GROUPS)InfoBuffer;
  DWORD dwInfoBufferSize;
  PSID psidAdministrators;
  SID_IDENTIFIER_AUTHORITY siaNtAuthority = SECURITY_NT_AUTHORITY;
  UINT x;
  BOOL ret_value = FALSE;

  if (!OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, &hAccessToken)) {
    if (GetLastError() != ERROR_NO_TOKEN) return FALSE;

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hAccessToken))
      return FALSE;
  }

  ret_value = GetTokenInformation(hAccessToken, TokenGroups, InfoBuffer, 1024,
                                  &dwInfoBufferSize);

  CloseHandle(hAccessToken);

  if (!ret_value) return FALSE;

  if (!AllocateAndInitializeSid(&siaNtAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
                                DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0,
                                &psidAdministrators))
    return FALSE;

  ret_value = FALSE;

  for (x = 0; x < ptgGroups->GroupCount; x++) {
    if (EqualSid(psidAdministrators, ptgGroups->Groups[x].Sid)) {
      ret_value = TRUE;
      break;
    }
  }
  FreeSid(psidAdministrators);
  return ret_value;
}
#endif
