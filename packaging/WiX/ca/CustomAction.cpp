/* Copyright (c) 2010, 2018, Oracle and/or its affiliates. All rights reserved.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>  // Needs to become before several Windows system headers.

#include <direct.h>
#include <msi.h>
#include <msiquery.h>
#include <string.h>
#include <strsafe.h>
#include <wcautil.h>
#include <winreg.h>

/*
 * Search the registry for a service whose ImagePath starts
 * with our install directory. Stop and remove it if requested.
 */
static TCHAR last_service_name[128];
int remove_service(TCHAR *installdir, int check_only) {
  HKEY hKey;
  int done = 0;

  if (wcslen(installdir) < 3) {
    WcaLog(LOGMSG_STANDARD,
           "INSTALLDIR is suspiciously short, better not do anything.");
    return 0;
  }

  if (check_only == 0) {
    WcaLog(LOGMSG_STANDARD, "Determining number of matching services...");
    int servicecount = remove_service(installdir, 1);
    if (servicecount <= 0) {
      WcaLog(LOGMSG_STANDARD, "No services found, not removing anything.");
      return 0;
    } else if (servicecount == 1) {
      TCHAR buf[256];
      swprintf_s(
          buf, sizeof(buf),
          TEXT("There is a service called '%ls' set up to run from this "
               "installation. Do you wish me to stop and remove that service?"),
          last_service_name);
      int rc = MessageBox(NULL, buf, TEXT("Removing MySQL Server"),
                          MB_ICONQUESTION | MB_YESNOCANCEL | MB_SYSTEMMODAL);
      if (rc == IDCANCEL) return -1;
      if (rc != IDYES) return 0;
    } else if (servicecount > 0) {
      TCHAR buf[256];
      swprintf_s(buf, sizeof(buf),
                 TEXT("There appear to be %d services set up to run from this "
                      "installation. Do you wish me to stop and remove those "
                      "services?"),
                 servicecount);
      int rc = MessageBox(NULL, buf, TEXT("Removing MySQL Server"),
                          MB_ICONQUESTION | MB_YESNOCANCEL | MB_SYSTEMMODAL);
      if (rc == IDCANCEL) return -1;
      if (rc != IDYES) return 0;
    }
  }

  if (check_only == -1) check_only = 0;

  WcaLog(LOGMSG_STANDARD, "Looking for service...");
  WcaLog(LOGMSG_STANDARD, "INSTALLDIR = %ls", installdir);
  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                    TEXT("SYSTEM\\CurrentControlSet\\services"), 0, KEY_READ,
                    &hKey) == ERROR_SUCCESS) {
    DWORD index = 0;
    TCHAR keyname[1024];
    DWORD keylen = sizeof(keyname);
    FILETIME t;
    /* Go through all services in the registry */
    while (RegEnumKeyExW(hKey, index, keyname, &keylen, NULL, NULL, NULL, &t) ==
           ERROR_SUCCESS) {
      HKEY hServiceKey = 0;
      TCHAR path[1024];
      DWORD pathlen = sizeof(path) - 1;
      if (RegOpenKeyExW(hKey, keyname, NULL, KEY_READ, &hServiceKey) ==
          ERROR_SUCCESS) {
        /* Look at the ImagePath value of each service */
        if (RegQueryValueExW(hServiceKey, TEXT("ImagePath"), NULL, NULL,
                             (LPBYTE)path, &pathlen) == ERROR_SUCCESS) {
          path[pathlen] = 0;
          TCHAR *p = path;
          if (p[0] == '"') p += 1;
          /* See if it is similar to our install directory */
          if (wcsncmp(p, installdir, wcslen(installdir)) == 0) {
            WcaLog(LOGMSG_STANDARD, "Found service '%ls' with ImagePath '%ls'.",
                   keyname, path);
            swprintf_s(last_service_name, sizeof(last_service_name),
                       TEXT("%ls"), keyname);
            /* If we are supposed to stop and remove the service... */
            if (!check_only) {
              WcaLog(LOGMSG_STANDARD, "Trying to stop the service.");
              SC_HANDLE hSCM = NULL;
              hSCM = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
              if (hSCM != NULL) {
                SC_HANDLE hService = NULL;
                hService =
                    OpenService(hSCM, keyname,
                                SERVICE_STOP | SERVICE_QUERY_STATUS | DELETE);
                if (hService != NULL) {
                  WcaLog(LOGMSG_STANDARD, "Waiting for the service to stop...");
                  SERVICE_STATUS status;
                  /* Attempt to stop the service */
                  if (ControlService(hService, SERVICE_CONTROL_STOP, &status)) {
                    /* Now wait until it's stopped */
                    while ("it's one big, mean and cruel world out there") {
                      if (!QueryServiceStatus(hService, &status)) break;
                      if (status.dwCurrentState == SERVICE_STOPPED) break;
                      Sleep(1000);
                    }
                    WcaLog(LOGMSG_STANDARD, "Stopped the service.");
                  }
                  /* Mark the service for deletion */
                  DeleteService(hService);
                  CloseServiceHandle(hService);
                }
                CloseServiceHandle(hSCM);
              }
            }
            done++;
          }
        }
        RegCloseKey(hServiceKey);
      }
      index++;
      keylen = sizeof(keyname) - 1;
    }
    RegCloseKey(hKey);
  } else {
    WcaLog(LOGMSG_STANDARD,
           "Can't seem to go through the list of installed services in the "
           "registry.");
  }
  return done;
}

#ifdef CLUSTER_EXTRA_CUSTOM_ACTIONS
UINT RunProcess(TCHAR *AppName, TCHAR *CmdLine, TCHAR *WorkDir) {
  PROCESS_INFORMATION processInformation;
  STARTUPINFO startupInfo;
  memset(&processInformation, 0, sizeof(processInformation));
  memset(&startupInfo, 0, sizeof(startupInfo));
  startupInfo.cb = sizeof(startupInfo);

  BOOL result;
  UINT er = ERROR_SUCCESS;

  TCHAR tempCmdLine[MAX_PATH * 2];  // Needed since CreateProcessW may change
                                    // the contents of CmdLine

  wcscpy_s(tempCmdLine, MAX_PATH * 2, TEXT("\""));
  wcscat_s(tempCmdLine, MAX_PATH * 2, AppName);
  wcscat_s(tempCmdLine, MAX_PATH * 2, TEXT("\" \""));
  wcscat_s(tempCmdLine, MAX_PATH * 2, CmdLine);
  wcscat_s(tempCmdLine, MAX_PATH * 2, TEXT("\""));

  result = ::CreateProcess(AppName, tempCmdLine, NULL, NULL, false,
                           NORMAL_PRIORITY_CLASS, NULL, WorkDir, &startupInfo,
                           &processInformation);

  if (result == 0) {
    WcaLog(LOGMSG_STANDARD, "CreateProcess = %ls %ls failed", AppName, CmdLine);
    er = ERROR_CANT_ACCESS_FILE;
  } else {
    WcaLog(LOGMSG_STANDARD, "CreateProcess = %ls %ls waiting to finish",
           AppName, CmdLine);
    WaitForSingleObject(processInformation.hProcess, INFINITE);
    CloseHandle(processInformation.hProcess);
    CloseHandle(processInformation.hThread);
    WcaLog(LOGMSG_STANDARD, "CreateProcess = %ls %ls finished", AppName,
           CmdLine);
  }

  return er;
}

UINT mccPostInstall(MSIHANDLE hInstall) {
  HRESULT hr = S_OK;
  UINT er = ERROR_SUCCESS;

  hr = WcaInitialize(hInstall, "MccPostInstall");
  ExitOnFailure(hr, "Failed to initialize");

  WcaLog(LOGMSG_STANDARD, "Initialized.");

  TCHAR INSTALLDIR[1024];
  DWORD INSTALLDIR_size = sizeof(INSTALLDIR);
  TCHAR path[MAX_PATH * 2];
  TCHAR param[MAX_PATH * 2];

  if (MsiGetPropertyW(hInstall, TEXT("CustomActionData"), INSTALLDIR,
                      &INSTALLDIR_size) == ERROR_SUCCESS) {
    WcaLog(LOGMSG_STANDARD, "INSTALLDIR = %ls", INSTALLDIR);

    // C:\Program Files (x86)\MySQL\MySQL Cluster 7.2\share\mcc
    wcscpy_s(path, MAX_PATH * 2, INSTALLDIR);
    wcscat_s(path, MAX_PATH * 2, TEXT("\\share\\mcc\\Python\\python.exe"));

    wcscpy_s(param, MAX_PATH * 2, INSTALLDIR);
    wcscat_s(param, MAX_PATH * 2, TEXT("\\share\\mcc\\post-install.py"));

    er = RunProcess(path, param, NULL);

  } else {
    er = ERROR_CANT_ACCESS_FILE;
  }

LExit:
  return WcaFinalize(er);
}
#endif

UINT wrap(MSIHANDLE hInstall, char *name, int check_only) {
  HRESULT hr = S_OK;
  UINT er = ERROR_SUCCESS;

  hr = WcaInitialize(hInstall, name);
  ExitOnFailure(hr, "Failed to initialize");

  WcaLog(LOGMSG_STANDARD, "Initialized.");

  TCHAR INSTALLDIR[1024];
  DWORD INSTALLDIR_size = sizeof(INSTALLDIR);
  if (MsiGetPropertyW(hInstall, TEXT("CustomActionData"), INSTALLDIR,
                      &INSTALLDIR_size) == ERROR_SUCCESS) {
    int rc = remove_service(INSTALLDIR, check_only);
    if (rc < 0) {
      er = ERROR_CANCELLED;
    }
  } else {
    er = ERROR_CANT_ACCESS_FILE;
  }

LExit:
  return WcaFinalize(er);
}

UINT __stdcall RemoveServiceNoninteractive(MSIHANDLE hInstall) {
  return wrap(hInstall, "RemoveServiceNoninteractive", -1);
}

UINT __stdcall RemoveService(MSIHANDLE hInstall) {
  return wrap(hInstall, "RemoveService", 0);
}

UINT __stdcall TestService(MSIHANDLE hInstall) {
  return wrap(hInstall, "TestService", 1);
}

UINT __stdcall RunPostInstall(MSIHANDLE hInstall) {
#ifdef CLUSTER_EXTRA_CUSTOM_ACTIONS
  return mccPostInstall(hInstall);
#else
  return ERROR_SUCCESS;
#endif
}

/* DllMain - Initialize and cleanup WiX custom action utils */
extern "C" BOOL WINAPI DllMain(__in HINSTANCE hInst, __in ULONG ulReason,
                               __in LPVOID) {
  switch (ulReason) {
    case DLL_PROCESS_ATTACH:
      WcaGlobalInitialize(hInst);
      break;

    case DLL_PROCESS_DETACH:
      WcaGlobalFinalize();
      break;
  }

  return true;
}
