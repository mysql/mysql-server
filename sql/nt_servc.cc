/* ------------------------------------------------------------------------
   Windows NT Service class library
   Copyright Abandoned 1998 Irena Pancirov - Irnet Snc
   This file is public domain and comes with NO WARRANTY of any kind
 -------------------------------------------------------------------------- */
#include <windows.h>
#include <process.h>
#include "nt_servc.h"


static NTService *pService;

/* ------------------------------------------------------------------------

 -------------------------------------------------------------------------- */
NTService::NTService()
{

    bOsNT	     = FALSE;
    //service variables
    ServiceName      = NULL;
    hExitEvent	     = 0;
    bPause	     = FALSE;
    bRunning	     = FALSE;
    hThreadHandle    = 0;
    fpServiceThread  = NULL;

    //time-out variables
    nStartTimeOut    = 15000;
    nStopTimeOut     = 15000;
    nPauseTimeOut    = 5000;
    nResumeTimeOut   = 5000;

    //install variables
    dwDesiredAccess  = SERVICE_ALL_ACCESS;
    dwServiceType    = SERVICE_WIN32_OWN_PROCESS;
    dwStartType      = SERVICE_AUTO_START;
    dwErrorControl   = SERVICE_ERROR_NORMAL;
    szLoadOrderGroup = NULL;
    lpdwTagID	     = NULL;
    szDependencies   = NULL;

    my_argc	     = 0;
    my_argv	     = NULL;
    hShutdownEvent   = 0;
    nError	     = 0;
    dwState	     = 0;
}

/* ------------------------------------------------------------------------

 -------------------------------------------------------------------------- */
NTService::~NTService()
{
  if(ServiceName != NULL) delete[] ServiceName;
}
/* ------------------------------------------------------------------------

 -------------------------------------------------------------------------- */
BOOL NTService::GetOS()
{
  bOsNT = FALSE;
  memset(&osVer, 0, sizeof(OSVERSIONINFO));
  osVer.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
  if (GetVersionEx(&osVer))
  {
    if (osVer.dwPlatformId == VER_PLATFORM_WIN32_NT)
      bOsNT = TRUE;
  }
  return bOsNT;
}

/* ------------------------------------------------------------------------
 Init()  Registers the main service thread with the service manager

    ServiceThread - pointer to the main programs entry function
		    when the service is started
 -------------------------------------------------------------------------- */
long NTService::Init(LPCSTR szInternName,void *ServiceThread)
{

  pService = this;

  fpServiceThread = (THREAD_FC)ServiceThread;
  ServiceName = new char[lstrlen(szInternName)+1];
  lstrcpy(ServiceName,szInternName);

  SERVICE_TABLE_ENTRY stb[] =
  {
    { (char *)szInternName,(LPSERVICE_MAIN_FUNCTION) ServiceMain} ,
    { NULL, NULL }
  };

  return StartServiceCtrlDispatcher(stb); //register with the Service Manager
}
/* ------------------------------------------------------------------------
  Install() - Installs the service with Service manager
  nError values:
	0  success
	1  Can't open the Service manager
	2  Failed to create service
 -------------------------------------------------------------------------- */
BOOL NTService::Install(LPCSTR szInternName,LPCSTR szDisplayName,
		       LPCSTR szFullPath, LPCSTR szAccountName,LPCSTR szPassword)
{
  SC_HANDLE newService, scm;

  nError=0;

  // open a connection to the SCM
  scm = OpenSCManager(0, 0,SC_MANAGER_CREATE_SERVICE);
  if(scm)  // Install the new service
  {  newService = CreateService(
	scm,
	szInternName,
	szDisplayName,
	dwDesiredAccess,	//default: SERVICE_ALL_ACCESS
	dwServiceType,		//default: SERVICE_WIN32_OWN_PROCESS
	dwStartType,		//default: SERVICE_AUTOSTART
	dwErrorControl,		//default: SERVICE_ERROR_NORMAL
	szFullPath,		//exec full path
	szLoadOrderGroup,	//default: NULL
	lpdwTagID,		//default: NULL
	szDependencies,		//default: NULL
	szAccountName,		//default: NULL
	szPassword);		//default: NULL

     if (newService) CloseServiceHandle(newService);  // clean up
     else	     nError=2;

     // clean up
     CloseServiceHandle(scm);
  }
  else nError=1;

  return (!nError);
}
/* ------------------------------------------------------------------------
  Remove() - Removes  the service
  nError values:
	0  success
	1  Can't open the Service manager
	2  Failed to locate service
	3  Failed to delete service
 -------------------------------------------------------------------------- */
BOOL NTService::Remove(LPCSTR szInternName)
{

  SC_HANDLE service, scm;

  nError=0;

  // open a connection to the SCM
  scm = OpenSCManager(0, 0,SC_MANAGER_CREATE_SERVICE);

  if (scm)
  {
    //open the service
    service = OpenService(scm,szInternName, DELETE );
    if(service)
    {
      if(!DeleteService(service)) nError=3;
      CloseServiceHandle(service);
    }
    else
    {
     //MessageBox(NULL,"Can't find the service","Remove Error",MB_OK|MB_ICONHAND);
       nError=2;
    }
    CloseServiceHandle(scm);
  }
  else nError=1;

  return (!nError);
}

/* ------------------------------------------------------------------------
   Stop() - this function should be called before the app. exits to stop
	    the service
 -------------------------------------------------------------------------- */
void NTService::Stop(void)
{
  SetStatus(SERVICE_STOP_PENDING,NO_ERROR, 0, 1, 60000);
  StopService();
  SetStatus(SERVICE_STOPPED, NO_ERROR, 0, 1, 1000);
}

/* ------------------------------------------------------------------------
  ServiceMain() - This is the function that is called from the
		  service manager to start the service
 -------------------------------------------------------------------------- */
void NTService::ServiceMain(DWORD argc, LPTSTR *argv)
{

  // registration function
  pService->hServiceStatusHandle =
	   RegisterServiceCtrlHandler(pService->ServiceName,
		       (LPHANDLER_FUNCTION )NTService::ServiceCtrlHandler);

  if(!pService->hServiceStatusHandle)
  {
      pService->Exit(GetLastError());
      return;
  }

  // notify SCM of progress
  if(!pService->SetStatus(SERVICE_START_PENDING,NO_ERROR, 0, 1, 8000))
  {
      pService->Exit(GetLastError());
      return;
  }

  // create the exit event
  pService->hExitEvent = CreateEvent (0, TRUE, FALSE,0);
  if(!pService->hExitEvent)
  {
      pService->Exit(GetLastError());
      return;
  }

  if(!pService->SetStatus(SERVICE_START_PENDING,NO_ERROR, 0, 3, pService->nStartTimeOut))
  {
      pService->Exit(GetLastError());
      return;
  }

  // save start arguments
  pService->my_argc=argc;
  pService->my_argv=argv;

  // start the service
  if(!pService->StartService())
  {
      pService->Exit(GetLastError());
      return;
  }

  // the service is now running.
  if(!pService->SetStatus(SERVICE_RUNNING,NO_ERROR, 0, 0, 0))
  {
      pService->Exit(GetLastError());
      return;
  }

  // wait for exit event
  WaitForSingleObject (pService->hExitEvent, INFINITE);

  // wait for thread to exit
  if (WaitForSingleObject (pService->hThreadHandle, 1000)==WAIT_TIMEOUT)
   CloseHandle(pService->hThreadHandle);

  pService->Exit(0);
}

/* ------------------------------------------------------------------------
   StartService() - starts the appliaction thread
 -------------------------------------------------------------------------- */
BOOL NTService::StartService()
{

  // Start the real service's thread (application)
  hThreadHandle = (HANDLE) _beginthread((THREAD_FC)fpServiceThread,0,(void *)this);

  if (hThreadHandle==0) return FALSE;

  bRunning = TRUE;
  return TRUE;
}
/* ------------------------------------------------------------------------

 -------------------------------------------------------------------------- */
void NTService::StopService()
{
  bRunning=FALSE;

  // Set the event for application
  if(hShutdownEvent)
     SetEvent(hShutdownEvent);

  // Set the event for ServiceMain
  SetEvent(hExitEvent);
}
/* ------------------------------------------------------------------------

 -------------------------------------------------------------------------- */
void NTService::PauseService()
{
    bPause = TRUE;
    SuspendThread(hThreadHandle);
}
/* ------------------------------------------------------------------------

 -------------------------------------------------------------------------- */
void NTService::ResumeService()
{
    bPause=FALSE;
    ResumeThread(hThreadHandle);
}
/* ------------------------------------------------------------------------

 -------------------------------------------------------------------------- */
BOOL NTService::SetStatus (DWORD dwCurrentState,DWORD dwWin32ExitCode,
	    DWORD dwServiceSpecificExitCode,DWORD dwCheckPoint,DWORD dwWaitHint)
{
  BOOL bRet;
  SERVICE_STATUS serviceStatus;

   dwState=dwCurrentState;

   serviceStatus.dwServiceType	= SERVICE_WIN32_OWN_PROCESS;
   serviceStatus.dwCurrentState = dwCurrentState;

   if (dwCurrentState == SERVICE_START_PENDING)
	serviceStatus.dwControlsAccepted = 0;	//don't accept conrol events
   else
	serviceStatus.dwControlsAccepted =    SERVICE_ACCEPT_STOP |
	    SERVICE_ACCEPT_PAUSE_CONTINUE | SERVICE_ACCEPT_SHUTDOWN;

   // if a specific exit code is defined,set up the win32 exit code properly
   if (dwServiceSpecificExitCode == 0)
       serviceStatus.dwWin32ExitCode = dwWin32ExitCode;
   else
       serviceStatus.dwWin32ExitCode = ERROR_SERVICE_SPECIFIC_ERROR;

   serviceStatus.dwServiceSpecificExitCode = dwServiceSpecificExitCode;

   serviceStatus.dwCheckPoint = dwCheckPoint;
   serviceStatus.dwWaitHint   = dwWaitHint;

   // Pass the status to the Service Manager
   bRet=SetServiceStatus (hServiceStatusHandle, &serviceStatus);

   if(!bRet) StopService();

   return bRet;
}
/* ------------------------------------------------------------------------

 -------------------------------------------------------------------------- */
void NTService::ServiceCtrlHandler(DWORD ctrlCode)
{

  DWORD  dwState = 0;

  if(!pService) return;

  dwState=pService->dwState;  // get current state

  switch(ctrlCode)
  {

   /*********** do we need this ? *******************************
    case SERVICE_CONTROL_PAUSE:
	 if (pService->bRunning && ! pService->bPause)
	 {
	     dwState = SERVICE_PAUSED;
	     pService->SetStatus(SERVICE_PAUSE_PENDING,NO_ERROR, 0, 1, pService->nPauseTimeOut);
	     pService->PauseService();
	 }
	 break;

   case SERVICE_CONTROL_CONTINUE:
	if (pService->bRunning && pService->bPause)
	{
	    dwState = SERVICE_RUNNING;
	    pService->SetStatus(SERVICE_CONTINUE_PENDING,NO_ERROR, 0, 1, pService->nResumeTimeOut);
	    pService->ResumeService();
	}
	break;
   ****************************************************************/

    case SERVICE_CONTROL_SHUTDOWN:
    case SERVICE_CONTROL_STOP:
	 dwState = SERVICE_STOP_PENDING;
	 pService->SetStatus(SERVICE_STOP_PENDING,NO_ERROR, 0, 1, pService->nStopTimeOut);
	 pService->StopService();
	 break;

   default:
	pService->SetStatus(dwState, NO_ERROR,0, 0, 0);
	break;
  }
  //pService->SetStatus(dwState, NO_ERROR,0, 0, 0);
}
/* ------------------------------------------------------------------------

 -------------------------------------------------------------------------- */
void NTService::Exit(DWORD error)
{
  if (hExitEvent) CloseHandle(hExitEvent);

  // Send a message to the scm to tell that we stop
  if (hServiceStatusHandle)
      SetStatus(SERVICE_STOPPED, error,0, 0, 0);

  // If the thread has started kill it ???
  // if (hThreadHandle) CloseHandle(hThreadHandle);

}

/* ------------------------- the end -------------------------------------- */
