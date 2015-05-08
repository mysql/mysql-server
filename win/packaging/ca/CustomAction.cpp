/* Copyright 2010, Oracle and/or its affiliates. All rights reserved.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef UNICODE
#define UNICODE
#endif

#include <winsock2.h>
#include <windows.h>
#include <winreg.h>
#include <msi.h>
#include <msiquery.h>
#include <wcautil.h>
#include <strutil.h>
#include <string.h>
#include <strsafe.h>
#include <assert.h>
#include <shellapi.h>
#include <stdlib.h>
#include <winservice.h>

#define ONE_MB 1048576
UINT ExecRemoveDataDirectory(wchar_t *dir)
{
   /* Strip stray backslash */
  DWORD len = (DWORD)wcslen(dir);
  if(len > 0 && dir[len-1]==L'\\')
    dir[len-1] = 0;

  SHFILEOPSTRUCTW fileop;
  fileop.hwnd= NULL;    /* no status display */
  fileop.wFunc= FO_DELETE;  /* delete operation */
  fileop.pFrom= dir;  /* source file name as double null terminated string */
  fileop.pTo= NULL;    /* no destination needed */
  fileop.fFlags= FOF_NOCONFIRMATION|FOF_SILENT;  /* do not prompt the user */

  fileop.fAnyOperationsAborted= FALSE;
  fileop.lpszProgressTitle= NULL;
  fileop.hNameMappings= NULL;

  return SHFileOperationW(&fileop);
}


extern "C" UINT __stdcall RemoveDataDirectory(MSIHANDLE hInstall) 
{
  HRESULT hr = S_OK;
  UINT er = ERROR_SUCCESS;

  hr = WcaInitialize(hInstall, __FUNCTION__);
  ExitOnFailure(hr, "Failed to initialize");
  WcaLog(LOGMSG_STANDARD, "Initialized.");

  wchar_t dir[MAX_PATH];
  DWORD len = MAX_PATH;
  MsiGetPropertyW(hInstall, L"CustomActionData", dir, &len);

  er= ExecRemoveDataDirectory(dir);
  WcaLog(LOGMSG_STANDARD, "SHFileOperation returned %d", er);
LExit:
  return WcaFinalize(er); 
}

/*
  Escape command line parameter fpr pass to CreateProcess().

  We assume out has enough space to include encoded string 
  2*wcslen(in) is enough.

  It is assumed that called will add double quotation marks before and after
  the string.
*/
static void EscapeCommandLine(const wchar_t *in, wchar_t *out, size_t buflen)
{
  const wchar_t special_chars[]=L" \t\n\v\"";
  bool needs_escaping= false;
  size_t pos;

  for(int i=0; i< sizeof(special_chars) -1; i++)
  {
    if (wcschr(in, special_chars[i]))
    {
      needs_escaping = true;
      break;
    }
  }

  if(!needs_escaping)
  {
    wcscpy_s(out, buflen, in);
    return;
  }

  pos= 0;
  for(int i = 0 ; ; i++) 
  {
    size_t n_backslashes = 0;
    wchar_t c;
    while (in[i] == L'\\') 
    {
      i++;
      n_backslashes++;
    }

    c= in[i];
    if (c == 0) 
    {
      /*
        Escape all backslashes, but let the terminating double quotation mark 
        that caller adds be interpreted as a metacharacter.
      */
      for(size_t j= 0; j < 2*n_backslashes;j++)
      {
        out[pos++]=L'\\';
      }
      break;
    }
    else if (c == L'"') 
    {
      /*
        Escape all backslashes and the following double quotation mark.
      */
      for(size_t j= 0; j < 2*n_backslashes + 1; j++)
      {
        out[pos++]=L'\\';
      }
      out[pos++]= L'"';
    }
    else 
    {
      /* Backslashes aren't special here. */
      for (size_t j=0; j < n_backslashes; j++)
        out[pos++] = L'\\';

      out[pos++]= c;
    }
  }
  out[pos++]= 0;
}
/* 
  Check for if directory is empty during install, 
  sets "<PROPERTY>_NOT_EMPTY" otherise
*/
extern "C" UINT __stdcall CheckDirectoryEmpty(MSIHANDLE hInstall, 
  const wchar_t *PropertyName)
{
  HRESULT hr = S_OK;
  UINT er = ERROR_SUCCESS;
  hr = WcaInitialize(hInstall, __FUNCTION__);
  ExitOnFailure(hr, "Failed to initialize");
  WcaLog(LOGMSG_STANDARD, "Initialized.");

  
  wchar_t buf[MAX_PATH];
  DWORD len = MAX_PATH;
  MsiGetPropertyW(hInstall, PropertyName, buf, &len);
  wcscat_s(buf, MAX_PATH, L"*.*");
  
  WcaLog(LOGMSG_STANDARD, "Checking files in %S", buf);
  WIN32_FIND_DATAW data;
  HANDLE h;
  bool empty;
  h= FindFirstFile(buf, &data);
  if (h !=  INVALID_HANDLE_VALUE)
  {
     empty= true;
     for(;;)
     {
       if (wcscmp(data.cFileName, L".") || wcscmp(data.cFileName, L".."))
       {
         empty= false;
         break;
       }
       if (!FindNextFile(h, &data))
         break;
     }
     FindClose(h);
  }
  else
  {
    /* Non-existent directory, we handle it as empty */
    empty = true;
  }

  if(empty)
    WcaLog(LOGMSG_STANDARD, "Directory %S is empty or non-existent", 
    PropertyName);
  else
    WcaLog(LOGMSG_STANDARD, "Directory %S is NOT empty", PropertyName);

  wcscpy_s(buf, MAX_PATH, PropertyName);
  wcscat_s(buf, L"NOTEMPTY");
  WcaSetProperty(buf, empty? L"":L"1");

LExit:
  return WcaFinalize(er); 
}

extern "C" UINT __stdcall CheckDataDirectoryEmpty(MSIHANDLE hInstall)
{
  return CheckDirectoryEmpty(hInstall, L"DATADIR");
}

bool CheckServiceExists(const wchar_t *name)
{
   SC_HANDLE manager =0, service=0;
   manager = OpenSCManager( NULL, NULL, SC_MANAGER_CONNECT);
   if (!manager) 
   {
     return false;
   }

   service = OpenService(manager, name, SC_MANAGER_CONNECT); 
   if(service)
     CloseServiceHandle(service);
   CloseServiceHandle(manager);

   return service?true:false;
}

/* User in rollback of create database custom action */
bool ExecRemoveService(const wchar_t *name)
{
   SC_HANDLE manager =0, service=0;
   manager = OpenSCManager( NULL, NULL, SC_MANAGER_ALL_ACCESS);
   bool ret;
   if (!manager) 
   {
     return false;
   }
   service = OpenService(manager, name, DELETE); 
   if(service)
   {
     ret= DeleteService(service);
   }
   else
   {
     ret= false;
   }
   CloseServiceHandle(manager);
   return ret;
}

/*
  Check if port is free by trying to bind to the port
*/
bool IsPortFree(short port)
{
   WORD wVersionRequested;
   WSADATA wsaData;

   wVersionRequested = MAKEWORD(2, 2);

   WSAStartup(wVersionRequested, &wsaData);

  struct sockaddr_in sin;
  SOCKET sock;
  sock = socket(AF_INET, SOCK_STREAM, 0);
  if(sock == -1)
  {
    return false;
  }
  sin.sin_port = htons(port);
  sin.sin_addr.s_addr = 0;
  sin.sin_addr.s_addr = INADDR_ANY;
  sin.sin_family = AF_INET;
  if(bind(sock, (struct sockaddr *)&sin,sizeof(struct sockaddr_in) ) == -1)
  {
    return false;
  }
  closesocket(sock);
  WSACleanup();
  return true;
}


/* 
   Helper function used in filename normalization.
   Removes leading quote and terminates string at the position of the next one
   (if applicable, does not change string otherwise). Returns modified string
*/
wchar_t *strip_quotes(wchar_t *s)
{
   if (s && (*s == L'"'))
   {
     s++;
     wchar_t *p = wcschr(s, L'"');
     if(p)
       *p = 0;
   }
   return s;
}


/*
  Checks  for consistency of service configuration.

  It can happen that SERVICENAME or DATADIR 
  MSI properties are in inconsistent state after somebody upgraded database 
  We catch this case during uninstall. In particular, either service is not 
  removed even if SERVICENAME was set (but this name is reused by someone else)
  or data directory is not removed (if it is used by someone else). To find out
  whether service name and datadirectory are in use For every service, 
  configuration is read and checked as follows:

  - look if a service has to do something with mysql
  - If so, check its name against SERVICENAME. if match, check binary path 
    against INSTALLDIR\bin.  If binary path does not match, then service runs
    under different  installation and won't be removed.
  - Check options file for datadir and look if this is inside this
    installation's datadir don't remove datadir if this is the case.

  "Don't remove" in this context means that custom action is removing 
  SERVICENAME property or CLEANUPDATA property, which later on in course of 
  installation mean, that either datadir or service is kept.
*/

void CheckServiceConfig(
  wchar_t *my_servicename,    /* SERVICENAME property in this installation*/
  wchar_t *datadir,           /* DATADIR property in this installation*/
  wchar_t *bindir,            /* INSTALLDIR\bin */
  wchar_t *other_servicename, /* Service to check against */
  QUERY_SERVICE_CONFIGW * config /* Other service's config */
  )
{

  bool same_bindir = false;
  wchar_t * commandline= config->lpBinaryPathName;
  int numargs;
  wchar_t **argv= CommandLineToArgvW(commandline, &numargs);
  WcaLog(LOGMSG_VERBOSE, "CommandLine= %S", commandline);
  if(!argv  ||  !argv[0]  || ! wcsstr(argv[0], L"mysqld"))
  {
    goto end;
  }

  WcaLog(LOGMSG_STANDARD, "MySQL service %S found: CommandLine= %S", 
    other_servicename, commandline);
  if (wcsstr(argv[0], bindir))
  {
    WcaLog(LOGMSG_STANDARD, "executable under bin directory");
    same_bindir = true;
  }

  bool is_my_service = (_wcsicmp(my_servicename, other_servicename) == 0);
  if(!is_my_service)
  {
    WcaLog(LOGMSG_STANDARD, "service does not match current service");
    /* 
      TODO probably the best thing possible would be to add temporary
      row to MSI ServiceConfig table with remove on uninstall
    */
  }
  else if (!same_bindir)
  {
    WcaLog(LOGMSG_STANDARD, 
      "Service name matches, but not the executable path directory, mine is %S", 
      bindir);
    WcaSetProperty(L"SERVICENAME", L"");
  }

  /* Check if data directory is used */
  if(!datadir || numargs <= 1 ||  wcsncmp(argv[1],L"--defaults-file=",16) != 0)
  {
    goto end;
  }

  wchar_t current_datadir_buf[MAX_PATH]={0};
  wchar_t normalized_current_datadir[MAX_PATH+1];
  wchar_t *current_datadir= current_datadir_buf;
  wchar_t *defaults_file= argv[1]+16;
  defaults_file= strip_quotes(defaults_file);

  WcaLog(LOGMSG_STANDARD, "parsed defaults file is %S", defaults_file);

  if (GetPrivateProfileStringW(L"mysqld", L"datadir", NULL, current_datadir, 
    MAX_PATH, defaults_file) == 0)
  {
    WcaLog(LOGMSG_STANDARD, 
      "Cannot find datadir in ini file '%S'", defaults_file);
    goto end;
  }

  WcaLog(LOGMSG_STANDARD, "datadir from defaults-file is %S", current_datadir);
  strip_quotes(current_datadir);

  /* Convert to Windows path */
  if (GetFullPathNameW(current_datadir, MAX_PATH, normalized_current_datadir, 
    NULL))
  {
    /* Add backslash to be compatible with directory formats in MSI */
    wcsncat(normalized_current_datadir, L"\\", MAX_PATH+1);
    WcaLog(LOGMSG_STANDARD, "normalized current datadir is '%S'", 
      normalized_current_datadir);
  }

  if (_wcsicmp(datadir, normalized_current_datadir) == 0 && !same_bindir)
  {
    WcaLog(LOGMSG_STANDARD, 
      "database directory from current installation, but different mysqld.exe");
    WcaSetProperty(L"CLEANUPDATA", L"");
  }

end:
  LocalFree((HLOCAL)argv);
}

/*
  Checks if database directory or service are modified by user
  For example, service may point to different mysqld.exe that it was originally
  installed, or some different service might use this database directory. This 
  would normally mean user has done an upgrade of the database and in this case 
  uninstall should neither delete service nor database directory.

  If this function find that service is modified by user (mysqld.exe used by 
  service does not point to the installation bin directory), MSI public variable
  SERVICENAME is removed, if DATADIR is used by some other service, variables 
  DATADIR and CLEANUPDATA are removed.

  The effect of variable removal is that service does not get uninstalled and
  datadir is not touched by uninstallation.

  Note that this function is running without elevation and does not use anything
  that would require special privileges.

*/
extern "C" UINT CheckDBInUse(MSIHANDLE hInstall)
{
  static BYTE buf[256*1024]; /* largest possible buffer for EnumServices */
  static char config_buffer[8*1024]; /*largest buffer for QueryServiceConfig */
  HRESULT hr = S_OK;
  UINT er = ERROR_SUCCESS;
  wchar_t *servicename= NULL;
  wchar_t *datadir= NULL;
  wchar_t *bindir=NULL;

  SC_HANDLE scm    = NULL; 
  ULONG  bufsize   = sizeof(buf); 
  ULONG  bufneed   = 0x00; 
  ULONG  num_services = 0x00; 
  LPENUM_SERVICE_STATUS_PROCESS info = NULL;  

  hr = WcaInitialize(hInstall, __FUNCTION__);
  ExitOnFailure(hr, "Failed to initialize");
  WcaLog(LOGMSG_STANDARD, "Initialized.");

  WcaGetProperty(L"SERVICENAME", &servicename);
  WcaGetProperty(L"DATADIR", &datadir);
  WcaGetFormattedString(L"[INSTALLDIR]bin\\", &bindir);
  WcaLog(LOGMSG_STANDARD,"SERVICENAME=%S, DATADIR=%S, bindir=%S",
    servicename, datadir, bindir);

  scm = OpenSCManager(NULL, NULL, 
    SC_MANAGER_ENUMERATE_SERVICE | SC_MANAGER_CONNECT);  
  if (scm == NULL) 
  { 
    ExitOnFailure(E_FAIL, "OpenSCManager failed");
  }

  BOOL ok= EnumServicesStatusExW(  scm,
    SC_ENUM_PROCESS_INFO, 
    SERVICE_WIN32,
    SERVICE_STATE_ALL,  
    buf, 
    bufsize,  
    &bufneed,  
    &num_services,  
    NULL, 
    NULL);
  if(!ok) 
  {
    WcaLog(LOGMSG_STANDARD, "last error %d", GetLastError());
    ExitOnFailure(E_FAIL, "EnumServicesStatusExW failed");
  }
  info = (LPENUM_SERVICE_STATUS_PROCESS)buf; 
  for (ULONG i=0; i < num_services; i++)
  {
    SC_HANDLE service= OpenServiceW(scm, info[i].lpServiceName, 
      SERVICE_QUERY_CONFIG);
    if (!service)
      continue;
    WcaLog(LOGMSG_VERBOSE, "Checking Service %S", info[i].lpServiceName);
    QUERY_SERVICE_CONFIGW *config= 
      (QUERY_SERVICE_CONFIGW *)(void *)config_buffer;
    DWORD needed;
    BOOL ok= QueryServiceConfigW(service, config,sizeof(config_buffer), 
      &needed);
    CloseServiceHandle(service);
    if (ok)
    {
       CheckServiceConfig(servicename, datadir, bindir, info[i].lpServiceName, 
         config);
    }
  }

LExit:
  if(scm)
    CloseServiceHandle(scm);

  ReleaseStr(servicename);
  ReleaseStr(datadir);
  ReleaseStr(bindir);
  return WcaFinalize(er); 
}

/* 
  Get maximum size of the buffer process can allocate.
  this is calculated as min(RAM,virtualmemorylimit)
  For 32bit processes, virtual address memory is 2GB (x86 OS)
  or 4GB(x64 OS).
  
  Fragmentation due to loaded modules, heap  and stack 
  limit maximum size of continous memory block further,
  so that limit for 32 bit process is about 1200 on 32 bit OS 
  or 2000 MB on 64 bit OS(found experimentally).
*/
unsigned long long GetMaxBufferSize(unsigned long long totalPhys)
{
#ifdef _M_IX86
  BOOL wow64;
  if (IsWow64Process(GetCurrentProcess(), &wow64))
    return min(totalPhys, 2000ULL*ONE_MB);
  else
    return min(totalPhys, 1200ULL*ONE_MB);
#else
  return totalPhys;
#endif
}


/*
  Checks SERVICENAME, PORT and BUFFERSIZE parameters 
*/
extern "C" UINT  __stdcall CheckDatabaseProperties (MSIHANDLE hInstall) 
{
  wchar_t ServiceName[MAX_PATH]={0};
  wchar_t SkipNetworking[MAX_PATH]={0};
  wchar_t QuickConfig[MAX_PATH]={0};
  wchar_t Password[MAX_PATH]={0};
  wchar_t EscapedPassword[2*MAX_PATH+2];
  wchar_t Port[6];
  wchar_t BufferPoolSize[16];
  DWORD PortLen=6;
  bool haveInvalidPort=false;
  const wchar_t *ErrorMsg=0;
  HRESULT hr= S_OK;
  UINT er= ERROR_SUCCESS;


  hr = WcaInitialize(hInstall, __FUNCTION__);
  ExitOnFailure(hr, "Failed to initialize");
  WcaLog(LOGMSG_STANDARD, "Initialized.");

  DWORD ServiceNameLen = MAX_PATH;
  MsiGetPropertyW (hInstall, L"SERVICENAME", ServiceName, &ServiceNameLen);
  if(ServiceName[0])
  {
    if(ServiceNameLen > 256)
    {
      ErrorMsg= L"Invalid service name. The maximum length is 256 characters.";
      goto LExit;
    }
    for(DWORD i=0; i< ServiceNameLen;i++)
    {
      if(ServiceName[i] == L'\\' || ServiceName[i] == L'/' 
        || ServiceName[i]=='\'' || ServiceName[i] ==L'"')
      {
        ErrorMsg = 
          L"Invalid service name. Forward slash and back slash are forbidden."
          L"Single and double quotes are also not permitted.";
        goto LExit;
      }
    }
    if(CheckServiceExists(ServiceName))
    {
      ErrorMsg=
        L"A service with the same name already exists. "
        L"Please use a different name.";
      goto LExit;
    }
  }

  DWORD PasswordLen= MAX_PATH;
  MsiGetPropertyW (hInstall, L"PASSWORD", Password, &PasswordLen);
  EscapeCommandLine(Password, EscapedPassword,
    sizeof(EscapedPassword)/sizeof(EscapedPassword[0]));
  MsiSetPropertyW(hInstall,L"ESCAPEDPASSWORD",EscapedPassword);

  DWORD SkipNetworkingLen= MAX_PATH;
  MsiGetPropertyW(hInstall, L"SKIPNETWORKING", SkipNetworking, 
    &SkipNetworkingLen);
  MsiGetPropertyW(hInstall, L"PORT", Port, &PortLen);
  
  if(SkipNetworking[0]==0 && Port[0] != 0)
  {
    /* Strip spaces */
    for(DWORD i=PortLen-1; i > 0; i--)
    {
      if(Port[i]== ' ')
        Port[i] = 0;
    }

    if(PortLen > 5 || PortLen <= 3)
      haveInvalidPort = true;
    else
    {
      for (DWORD i=0; i< PortLen && Port[i] != 0;i++)
      {
        if(Port[i] < '0' || Port[i] >'9')
        {
          haveInvalidPort=true;
          break;
        }
      }
    }
    if (haveInvalidPort)
    {
      ErrorMsg =
        L"Invalid port number. Please use a number between 1025 and 65535.";
      goto LExit;
    }

    short port = (short)_wtoi(Port);
    if (!IsPortFree(port))
    {
      ErrorMsg = 
        L"The TCP Port you selected is already in use. "
        L"Please choose a different port.";
      goto LExit;
    }
  }
  
  
  DWORD QuickConfigLen = MAX_PATH;
  MsiGetPropertyW (hInstall, L"STDCONFIG", QuickConfig, &QuickConfigLen);
  if(QuickConfig[0] !=0)
  {
     MEMORYSTATUSEX memstatus;
     memstatus.dwLength =sizeof(memstatus);
     wchar_t invalidValueMsg[256];

     if (!GlobalMemoryStatusEx(&memstatus))
     {
        WcaLog(LOGMSG_STANDARD, "Error %u from GlobalMemoryStatusEx", 
          GetLastError());
        er= ERROR_INSTALL_FAILURE;
        goto LExit;
     }
     DWORD BufferPoolSizeLen= 16;
     MsiGetPropertyW(hInstall, L"BUFFERPOOLSIZE", BufferPoolSize, &BufferPoolSizeLen);
     /* Strip spaces */
     for(DWORD i=BufferPoolSizeLen-1; i > 0; i--)
     {
      if(BufferPoolSize[i]== ' ')
        BufferPoolSize[i] = 0;
     }
     unsigned long long availableMemory=
       GetMaxBufferSize(memstatus.ullTotalPhys)/ONE_MB;
     swprintf_s(invalidValueMsg,
        L"Invalid buffer pool size. Please use a number between 1 and %llu",
         availableMemory);
     if(BufferPoolSizeLen == 0 || BufferPoolSizeLen > 15)
     {
       ErrorMsg= invalidValueMsg;
       goto LExit;
     }
     for (DWORD i=0; i < BufferPoolSizeLen && BufferPoolSize[BufferPoolSizeLen];
       i++)
     {
       if(BufferPoolSize[i]< '0' || BufferPoolSize[i] > '9')
       {
         ErrorMsg= invalidValueMsg;
         goto LExit;
       }
     }
     BufferPoolSize[BufferPoolSizeLen]=0;
     MsiSetPropertyW(hInstall, L"BUFFERPOOLSIZE", BufferPoolSize);
     long long sz = _wtoi64(BufferPoolSize);
     if(sz <= 0 || sz > (long long)availableMemory)
     {
       if(sz > 0)
       {
         swprintf_s(invalidValueMsg,
           L"Value for buffer pool size is too large."
           L"Only approximately %llu MB is available for allocation."
           L"Please use a number between 1 and %llu.",
          availableMemory, availableMemory);
       }
       ErrorMsg= invalidValueMsg;
       goto LExit;
     }
  }
LExit:
  MsiSetPropertyW (hInstall, L"WarningText", ErrorMsg);
  return WcaFinalize(er);
}

/* 
  Sets Innodb buffer pool size (1/8 of RAM by default), if not already specified
  via command line.
  Calculates innodb log file size as min(50, innodb buffer pool size/8)
*/
extern "C" UINT __stdcall PresetDatabaseProperties(MSIHANDLE hInstall)
{
  unsigned long long InnodbBufferPoolSize= 256;
  unsigned long long InnodbLogFileSize= 50;
  wchar_t buff[MAX_PATH];
  UINT er = ERROR_SUCCESS;
  HRESULT hr= S_OK;
  MEMORYSTATUSEX memstatus;
  hr = WcaInitialize(hInstall, __FUNCTION__);
  ExitOnFailure(hr, "Failed to initialize");
  WcaLog(LOGMSG_STANDARD, "Initialized.");

  /* Check if bufferpoolsize parameter was given on the command line*/
  DWORD BufferPoolsizeParamLen = MAX_PATH;
  MsiGetPropertyW(hInstall, L"BUFFERPOOLSIZE", buff, &BufferPoolsizeParamLen);

  if (BufferPoolsizeParamLen && buff[0])
  {
    WcaLog(LOGMSG_STANDARD, "BUFFERPOOLSIZE=%s, len=%u",buff, BufferPoolsizeParamLen);
    InnodbBufferPoolSize= _wtoi64(buff);
  }
  else
  {
    memstatus.dwLength = sizeof(memstatus);
    if (!GlobalMemoryStatusEx(&memstatus))
    {
       WcaLog(LOGMSG_STANDARD, "Error %u from GlobalMemoryStatusEx", 
         GetLastError());
       er= ERROR_INSTALL_FAILURE;
       goto LExit;
    }
    unsigned long long totalPhys= memstatus.ullTotalPhys;
    /* Give innodb 12.5% of available physical memory. */
    InnodbBufferPoolSize= totalPhys/ONE_MB/8;
 #ifdef _M_IX86
    /* 
      For 32 bit processes, take virtual address space limitation into account.
      Do not try to use more than 3/4 of virtual address space, even if there
      is plenty of physical memory.
    */
    InnodbBufferPoolSize= min(GetMaxBufferSize(totalPhys)/ONE_MB*3/4,
      InnodbBufferPoolSize);
 #endif 
    swprintf_s(buff, L"%llu",InnodbBufferPoolSize);
    MsiSetPropertyW(hInstall, L"BUFFERPOOLSIZE", buff);
  }
  InnodbLogFileSize = min(50, InnodbBufferPoolSize);
  swprintf_s(buff, L"%llu",InnodbLogFileSize);
  MsiSetPropertyW(hInstall, L"LOGFILESIZE", buff);

LExit:
  return WcaFinalize(er);
}
/* Remove service and data directory created by CreateDatabase operation */
extern "C" UINT __stdcall CreateDatabaseRollback(MSIHANDLE hInstall) 
{
  HRESULT hr = S_OK;
  UINT er = ERROR_SUCCESS;
  wchar_t* service= 0;
  wchar_t* dir= 0;

  hr = WcaInitialize(hInstall, __FUNCTION__);
  ExitOnFailure(hr, "Failed to initialize");
  WcaLog(LOGMSG_STANDARD, "Initialized.");
  wchar_t data[2*MAX_PATH];
  DWORD len= MAX_PATH;
  MsiGetPropertyW(hInstall, L"CustomActionData", data, &len);

  /* Property is encoded as [SERVICENAME]\[DBLOCATION] */
  if(data[0] == L'\\')
  {
    dir= data+1;
  }
  else
  {
    service= data;
    dir= wcschr(data, '\\');
    if (dir)
    {
     *dir=0;
     dir++;
    }
  }

  if(service)
  {
    ExecRemoveService(service);
  }
  if(dir)
  {
    ExecRemoveDataDirectory(dir);
  }
LExit:
  return WcaFinalize(er); 
}


/*
  Enables/disables optional "Launch upgrade wizard" checkbox at the end of 
  installation
*/
#define MAX_VERSION_PROPERTY_SIZE 64

extern "C" UINT __stdcall CheckServiceUpgrades(MSIHANDLE hInstall) 
{
  HRESULT hr = S_OK;
  UINT er = ERROR_SUCCESS;
  wchar_t* service= 0;
  wchar_t* dir= 0;
  wchar_t installerVersion[MAX_VERSION_PROPERTY_SIZE];
  char installDir[MAX_PATH];
  DWORD size =MAX_VERSION_PROPERTY_SIZE;
  int installerMajorVersion, installerMinorVersion, installerPatchVersion;
  bool upgradableServiceFound=false;

  hr = WcaInitialize(hInstall, __FUNCTION__);
   WcaLog(LOGMSG_STANDARD, "Initialized.");
  if (MsiGetPropertyW(hInstall, L"ProductVersion", installerVersion, &size) 
    != ERROR_SUCCESS)
  {
    hr = HRESULT_FROM_WIN32(GetLastError());
    ExitOnFailure(hr, "MsiGetPropertyW failed");
  }
   if (swscanf(installerVersion,L"%d.%d.%d",
    &installerMajorVersion, &installerMinorVersion, &installerPatchVersion) !=3)
  {
    assert(FALSE);
  }

  size= MAX_PATH;
  if (MsiGetPropertyA(hInstall,"INSTALLDIR", installDir, &size)
    != ERROR_SUCCESS)
  {
    hr = HRESULT_FROM_WIN32(GetLastError());
    ExitOnFailure(hr, "MsiGetPropertyW failed");
  }
 

  SC_HANDLE scm = OpenSCManager(NULL, NULL, 
    SC_MANAGER_ENUMERATE_SERVICE | SC_MANAGER_CONNECT);  
  if (scm == NULL) 
  { 
    hr = HRESULT_FROM_WIN32(GetLastError());
    ExitOnFailure(hr,"OpenSCManager failed");
  }

  static BYTE buf[64*1024];
  static BYTE config_buffer[8*1024];

  DWORD bufsize= sizeof(buf);
  DWORD bufneed;
  DWORD num_services;
  BOOL ok= EnumServicesStatusExW(scm, SC_ENUM_PROCESS_INFO,  SERVICE_WIN32,
    SERVICE_STATE_ALL,  buf, bufsize,  &bufneed, &num_services, NULL, NULL);
  if(!ok) 
  {
    hr = HRESULT_FROM_WIN32(GetLastError());
    ExitOnFailure(hr,"EnumServicesStatusEx failed");
  }
  LPENUM_SERVICE_STATUS_PROCESSW info =
    (LPENUM_SERVICE_STATUS_PROCESSW)buf;
  int index=-1;
  for (ULONG i=0; i < num_services; i++)
  {
    SC_HANDLE service= OpenServiceW(scm, info[i].lpServiceName, 
      SERVICE_QUERY_CONFIG);
    if (!service)
      continue;
    QUERY_SERVICE_CONFIGW *config= 
      (QUERY_SERVICE_CONFIGW*)(void *)config_buffer;
    DWORD needed;
    BOOL ok= QueryServiceConfigW(service, config,sizeof(config_buffer),
      &needed);
    CloseServiceHandle(service);
    if (ok)
    {
        mysqld_service_properties props;
       if (get_mysql_service_properties(config->lpBinaryPathName, &props))
                  continue;
        /* 
          Only look for services that have mysqld.exe outside of the current
          installation directory.
        */
       if(installDir[0] == 0 || strstr(props.mysqld_exe,installDir) == 0)
        {
           WcaLog(LOGMSG_STANDARD, "found service %S, major=%d, minor=%d",
            info[i].lpServiceName, props.version_major, props.version_minor);
          if(props.version_major < installerMajorVersion
            || (props.version_major == installerMajorVersion && 
                props.version_minor <= installerMinorVersion))
          {
            upgradableServiceFound= true;
            break;
          }
       }
    }
  }

  if(!upgradableServiceFound)
  {
    /* Disable optional checkbox at the end of installation */
    MsiSetPropertyW(hInstall, L"WIXUI_EXITDIALOGOPTIONALCHECKBOXTEXT", L"");
    MsiSetPropertyW(hInstall, L"WIXUI_EXITDIALOGOPTIONALCHECKBOX",L"");
  }
  else
  {
    MsiSetPropertyW(hInstall, L"UpgradableServiceFound", L"1");
    MsiSetPropertyW(hInstall, L"WIXUI_EXITDIALOGOPTIONALCHECKBOX",L"1");
  }
LExit:
  if(scm)
    CloseServiceHandle(scm);
  return WcaFinalize(er); 
}


/* DllMain - Initialize and cleanup WiX custom action utils */
extern "C" BOOL WINAPI DllMain(
  __in HINSTANCE hInst,
  __in ULONG ulReason,
  __in LPVOID
  )
{
  switch(ulReason)
  {
  case DLL_PROCESS_ATTACH:
    WcaGlobalInitialize(hInst);
    break;

  case DLL_PROCESS_DETACH:
    WcaGlobalFinalize();
    break;
  }

  return TRUE;
}
