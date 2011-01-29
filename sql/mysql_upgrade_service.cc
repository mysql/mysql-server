/* Copyright (C) 2010 Monty Program Ab

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

/*
  mysql_upgrade_service upgrades mysql service on Windows.
  It changes service definition to point to the new mysqld.exe, restarts the 
  server and runs mysql_upgrade
*/

#define DONT_DEFINE_VOID
#include <process.h>
#include <my_global.h>
#include <my_getopt.h>
#include <my_sys.h>
#include <m_string.h>
#include <mysql_version.h>

#include <stdlib.h>
#include <stdio.h>
#include <windows.h>

/* We're using version APIs */
#pragma comment(lib, "version")

static char mysqld_path[MAX_PATH];
static char mysqladmin_path[MAX_PATH];
static char mysqlupgrade_path[MAX_PATH];

static char defaults_file_param[FN_REFLEN];
static char logfile_path[FN_REFLEN];
static char *opt_service;
static SC_HANDLE service;
static SC_HANDLE scm;
HANDLE mysqld_process; // mysqld.exe started for upgrade
DWORD initial_service_state= -1; // initial state of the service
HANDLE logfile_handle;

/*
  Startup and shutdown timeouts, in seconds. 
  Maybe,they can be made parameters
*/
static unsigned int startup_timeout= 60;
static unsigned int shutdown_timeout= 60;

static struct my_option my_long_options[]=
{
  {"help", '?', "Display this help message and exit.", 0, 0, 0, GET_NO_ARG,
   NO_ARG, 0, 0, 0, 0, 0, 0},
  {"service", 's', "Name of the existing Windows service",
  &opt_service, &opt_service, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
};



static my_bool
get_one_option(int optid, 
   const struct my_option *opt __attribute__ ((unused)),
   char *argument __attribute__ ((unused)))
{
  DBUG_ENTER("get_one_option");
  switch (optid) {
  case '?':
    my_print_help(my_long_options);
    exit(0);
    break;
  }
  DBUG_RETURN(0);
}



static void log(const char *fmt, ...)
{
  va_list args;
  char buf[4096];

  /* Print the error message */
  va_start(args, fmt);
  if (fmt)
  {
    vsprintf_s(buf,  fmt, args);
    fprintf(stdout, "%s\n", buf);
  }
  va_end(args);
  my_end(0);
}


static void die(const char *fmt, ...)
{
  va_list args;
  DBUG_ENTER("die");
  char buf[4096];

  /* Print the error message */
  va_start(args, fmt);
  if (fmt)
  {
    fprintf(stderr, "FATAL ERROR: ");
    int count= vsprintf_s(buf,  fmt, args);
    fprintf(stderr, "%s.", buf);
    if(logfile_path[0])
    {
      fprintf(stderr, "Additional information can be found in the log file %s",
        logfile_path);
    }
  }
  va_end(args);

  /* Cleanup */
  if(service && initial_service_state != -1)
  {
    /* Stop service if it was not running */
    if(initial_service_state != SERVICE_RUNNING)
    {
      SERVICE_STATUS service_status;
      ControlService(service, SERVICE_CONTROL_STOP, &service_status);
    }
    CloseServiceHandle(service);
  }
  if(scm)
    CloseServiceHandle(scm);

  /* Stop mysqld.exe if it was started for upgrade */
  if(mysqld_process)
    TerminateProcess(mysqld_process, 3);
  if(logfile_handle)
    CloseHandle(logfile_handle);
  my_end(0);
  exit(1);
}

/*
  spawn-like function to run subprocesses. 
  We also redirect the full output to the log file.

  Typical usage could be something like
  run_tool(P_NOWAIT, "cmd.exe", "/c" , "echo", "foo", NULL)
  
  @param    wait_flag (P_WAIT or P_NOWAIT)
  @program  program to run

  Rest of the parameters is NULL terminated strings building command line.

  @return intptr containing either process handle, if P_NOWAIT is used
  or return code of the process (if P_WAIT is used)
*/
static intptr_t run_tool(int wait_flag, const char *program,...)
{
  static char cmdline[32*1024];
  va_list args;
  va_start(args, program);
  if(!program)
    die("Invalid call to run_tool");

  strcpy_s(cmdline, "\"");
  strcat_s(cmdline, program);
  strcat_s(cmdline, "\"");
  for(;;) 
  {
    char *param= va_arg(args,char *);
    if(!param)
      break;
    strcat_s(cmdline, " \"");
    strcat_s(cmdline, param);
    strcat_s(cmdline, "\"");
  }
  va_end(args);
  
  /* Create output file if not alredy done */
  if(!logfile_handle)
  {
    char tmpdir[FN_REFLEN];
    GetTempPath(FN_REFLEN, tmpdir);
    sprintf_s(logfile_path, "%s\\mysql_upgrade_service.%s.log", tmpdir, 
      opt_service);
    logfile_handle = CreateFile(logfile_path, GENERIC_WRITE,  FILE_SHARE_READ, 
      NULL, TRUNCATE_EXISTING, 0, NULL);
    if(!logfile_handle)
      die("Cannot open log file %s", logfile_path);
  }

  /* Start child process */
  STARTUPINFO si={0};
  si.cb= sizeof(si);
  si.hStdInput= GetStdHandle(STD_INPUT_HANDLE);
  si.hStdError= logfile_handle;
  si.hStdOutput= logfile_handle;
  si.dwFlags= STARTF_USESTDHANDLES;
  PROCESS_INFORMATION pi;
  if (!CreateProcess(NULL, cmdline, NULL, 
       NULL, TRUE, NULL, NULL, NULL, &si, &pi))
  {
    die("CreateProcess failed (commandline %s)", cmdline);
  }
  CloseHandle(pi.hThread);

  if(wait_flag == P_NOWAIT)
  {
    /* Do not wait for process to complete, return handle */
    return (intptr_t)pi.hProcess;
  }

  /* Eait for process to complete */
  if (WaitForSingleObject(pi.hProcess, INFINITE) != WAIT_OBJECT_0)
  {
    die("WaitForSingleObject() failed");
  }
  DWORD exit_code;
  if (!GetExitCodeProcess(pi.hProcess, &exit_code))
  {
    die("GetExitCodeProcess() failed");
  }
  return (intptr_t)exit_code;
}



void stop_mysqld_service()
{
  DWORD needed;
  SERVICE_STATUS_PROCESS ssp;
  int timeout= shutdown_timeout*1000; 
  for(;;)
  {
    if (!QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO,
          (LPBYTE)&ssp, 
          sizeof(SERVICE_STATUS_PROCESS),
          &needed))
    {
      die("QueryServiceStatusEx failed (%d)\n", GetLastError()); 
    }

    /*
      Remeber initial state of the service, we will restore it on
      exit.
    */
    if(initial_service_state == -1)
      initial_service_state =ssp.dwCurrentState;

    switch(ssp.dwCurrentState)
    {
      case SERVICE_STOPPED:
        return;
      case SERVICE_RUNNING:
        if(!ControlService(service, SERVICE_CONTROL_STOP, 
             (SERVICE_STATUS *)&ssp))
            die("ControlService failed, error %d\n", GetLastError());
      case SERVICE_START_PENDING:
      case SERVICE_STOP_PENDING:
        if(timeout < 0)
          die("Service does not stop after 1 minute timeout");
        Sleep(100);
        break;
      default:
        die("Unexpected service state %d",ssp.dwCurrentState);
    }
  }
}


/* Helper routine. Used to prevent downgrades by mysql_upgrade_service */
void get_file_version(const wchar_t *path, int *major, int *minor)
{
  *major= *minor=0;
  DWORD version_handle;
  char *ver= 0;
  VS_FIXEDFILEINFO info;
  UINT len;
  void *p;

  DWORD size = GetFileVersionInfoSizeW(path, &version_handle);
  if (size == 0) 
    return;
  ver = new char[size];
  if(!GetFileVersionInfoW(path, version_handle, size, ver))
    goto end;

  if(!VerQueryValue(ver,"\\",&p,&len))
    goto end;
  memcpy(&info,p ,sizeof(VS_FIXEDFILEINFO));

  *major = (info.dwFileVersionMS & 0xFFFF0000) >> 16;
  *minor = (info.dwFileVersionMS & 0x0000FFFF);
end:
  delete []ver;
}

/* 
  Shutdown mysql server. Not using mysqladmin, since 
  our --skip-grant-tables do not work anymore after mysql_upgrade
  that does "flush privileges". Instead, the shutdown handle is set.
*/
void initiate_mysqld_shutdown()
{
  char event_name[32];
  DWORD pid= GetProcessId(mysqld_process);
  sprintf_s(event_name, "MySQLShutdown%d", pid);
  HANDLE shutdown_handle= OpenEvent(EVENT_MODIFY_STATE, FALSE, event_name);
  if(!shutdown_handle)
  {
    die("OpenEvent() failed for shutdown event");
  }

  if(!SetEvent(shutdown_handle))
  {
    die("SetEvent() failed");
  }
}


/*
  Change service configuration (binPath) to point to mysqld from 
  this installation.
*/
static void change_service_config()
{
  wchar_t old_mysqld_path[MAX_PATH];
  wchar_t *file_part;
  char defaults_file[MAX_PATH];
  char default_character_set[64];

  scm = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
  if(!scm)
    die("OpenSCManager failed with %d", GetLastError());
  service= OpenService(scm, opt_service, SERVICE_ALL_ACCESS);
  if (!service)
    die("OpenService failed with %d", GetLastError());

  BYTE config_buffer[8*1024];
  LPQUERY_SERVICE_CONFIGW config= (LPQUERY_SERVICE_CONFIGW)config_buffer;
  DWORD size=sizeof(config_buffer);
  DWORD needed;
  if (!QueryServiceConfigW(service, config, size, &needed))
    die("QueryServiceConfig failed with %d\n", GetLastError());

  int numargs;
  LPWSTR *args= CommandLineToArgvW(config->lpBinaryPathName, &numargs);

  char commandline[3*FN_REFLEN +32];

  /* Run some checks to ensure we're really upgrading mysql service */

  if(numargs != 3)
  {
    die("Expected 3 parameters in service configuration binPath,"
      "got %d parameters instead\n. binPath: %S", numargs, 
      config->lpBinaryPathName);
  }
  if(wcsncmp(args[1], L"--defaults-file=", 16) != 0)
  {
    die("Unexpected service configuration, second parameter must start with "
      "--defaults-file. binPath= %S", config->lpBinaryPathName);
  }
  GetFullPathNameW(args[0], MAX_PATH, old_mysqld_path, &file_part);

  if(wcsicmp(file_part, L"mysqld.exe") != 0 && 
    wcsicmp(file_part, L"mysqld") != 0)
  {
    die("The service executable is not mysqld. binPath: %S", 
         config->lpBinaryPathName);
  }

  if(wcsicmp(file_part, L"mysqld") == 0)
    wcscat_s(old_mysqld_path, L".exe");

  int old_mysqld_major, old_mysqld_minor;
  get_file_version(old_mysqld_path, &old_mysqld_major, &old_mysqld_minor);
  int my_major= MYSQL_VERSION_ID/10000;
  int my_minor= (MYSQL_VERSION_ID - 10000*my_major)/100;

  if(my_major < old_mysqld_major || 
    (my_major == old_mysqld_major && my_minor < old_mysqld_minor))
  {
    die("Can not downgrade, the service is currently running as version %d.%d"
      ", my version is %d.%d", old_mysqld_major, old_mysqld_minor, my_major, 
      my_minor);
  }

  wcstombs(defaults_file, args[1] + 16, MAX_PATH);
  /*
    Remove basedir from defaults file, otherwise the service wont come up in 
    the new  version, and will complain about mismatched message file.
  */
  WritePrivateProfileString("mysqld", "basedir",NULL, defaults_file);

#ifdef _WIN64
  /* Currently, pbxt is non-functional on x64 */
  WritePrivateProfileString("mysqld", "loose-skip-pbxt","1", defaults_file);
#endif
  /* 
    Replace default-character-set  with character-set-server, to avoid 
    "default-character-set is deprecated and will be replaced ..."
    message.
  */
  default_character_set[0]=0;
  GetPrivateProfileStringA("mysqld", "default-character-set", NULL,
    default_character_set, sizeof(default_character_set), defaults_file);
  if(default_character_set[0])
  {
    WritePrivateProfileString("mysqld", "default-character-set", NULL, 
      defaults_file);
    WritePrivateProfileString("mysqld", "character-set-server",
      default_character_set, defaults_file);
  }

  sprintf_s(commandline, "\"%s\" \"%S\" \"%S\"", mysqld_path, args[1], args[2]);
  if (!ChangeServiceConfig(service, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, 
         SERVICE_NO_CHANGE, commandline, NULL, NULL, NULL, NULL, NULL, NULL))
  {
    die("ChangeServiceConfigW failed with %d", GetLastError());
  }

  sprintf_s(defaults_file_param, "%S", args[1]);
  LocalFree(args);
}



int main(int argc, char **argv)
{
  int error;
  MY_INIT(argv[0]);
  char bindir[FN_REFLEN];
  char *p;

  /*
    Get full path to mysqld, we need it when changing service configuration.
    Assume installation layout, i.e mysqld.exe, mysqladmin.exe, mysqlupgrade.exe
    and mysql_upgrade_service.exe are in the same directory.
  */
  GetModuleFileName(NULL, bindir, FN_REFLEN);
  p = strrchr(bindir, FN_LIBCHAR);
  if(p)
  {
    *p=0;
  }
  sprintf_s(mysqld_path, "%s\\mysqld.exe", bindir);
  sprintf_s(mysqladmin_path, "%s\\mysqladmin.exe", bindir);
  sprintf_s(mysqlupgrade_path, "%s\\mysql_upgrade.exe", bindir);

  char *paths[]= {mysqld_path, mysqladmin_path, mysqlupgrade_path};
  for(int i=0; i< 3;i++)
  {
    if(GetFileAttributes(paths[i]) == INVALID_FILE_ATTRIBUTES)
      die("File %s does not exist", paths[i]);
  }


  /* Parse options */
  if ((error= handle_options(&argc, &argv, my_long_options, get_one_option)))
    die("");
  if(!opt_service)
    die("service parameter is mandatory");
 
  /*
    Messages written on stdout should not be buffered,  GUI upgrade program 
    read them from pipe and uses as progress indicator.
  */
  setvbuf(stdout, NULL, _IONBF, 0);

  log("Phase 1/8: Changing service configuration");
  change_service_config();

  log("Phase 2/8: Stopping service");
  stop_mysqld_service();

  /* 
    Start mysqld.exe as non-service skipping privileges (so we do not 
    care about the password). But disable networking and enable pipe 
    for communication, for security reasons.
  */
  char socket_param[FN_REFLEN];
  sprintf_s(socket_param,"--shared_memory_base_name=mysql_upgrade_service_%d", 
    GetCurrentProcessId());

  log("Phase 3/8: Starting mysqld for upgrade");
  mysqld_process= (HANDLE)run_tool(P_NOWAIT, mysqld_path,
    defaults_file_param, "--skip-networking",  "--skip-grant-tables", 
    "--enable-shared-memory",  socket_param, NULL);

  if(mysqld_process == INVALID_HANDLE_VALUE)
  {
    die("Cannot start mysqld.exe process, errno=%d", errno);
  }

  log("Phase 4/8: Waiting for startup to complete");
  DWORD start_duration_ms= 0;
  for (int i=0; ; i++)
  {
    if (WaitForSingleObject(mysqld_process, 0) != WAIT_TIMEOUT)
      die("mysqld.exe did not start");

    if (run_tool(P_WAIT, mysqladmin_path, "--protocol=memory",
      socket_param, "ping",  NULL) == 0)
    {
      break;
    }
    if (start_duration_ms > startup_timeout*1000)
      die("Server did not come up in %d seconds",startup_timeout);
    Sleep(500);
    start_duration_ms+= 500;
  }

  log("Phase 5/8: Running mysql_upgrade");
  int upgrade_err = (int) run_tool(P_WAIT,  mysqlupgrade_path, 
    "--protocol=memory", "--force",  socket_param,
    NULL);

  log("Phase 6/8: Initiating server shutdown");
  initiate_mysqld_shutdown();

  log("Phase 7/8: Waiting for shutdown to complete");
  if (WaitForSingleObject(mysqld_process, shutdown_timeout*1000)
      != WAIT_OBJECT_0)
  {
    /* Shutdown takes too long */
    die("mysqld does not shutdown.");
  }
  CloseHandle(mysqld_process);
  mysqld_process= NULL;

  log("Phase 8/8: Starting service%s",
    (initial_service_state == SERVICE_RUNNING)?"":" (skipped)");
  if (initial_service_state == SERVICE_RUNNING)
  {
    StartService(service, NULL, NULL);
  }

  log("Service '%s' successfully upgraded.\nLog file is written to %s",
    opt_service, logfile_path);
  CloseServiceHandle(service);
  CloseServiceHandle(scm);
  if(logfile_handle)
    CloseHandle(logfile_handle);
  my_end(0);
  exit(0);
}