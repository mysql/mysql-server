/* Copyright (C) 2010-2011 Monty Program Ab & Vladislav Vaintroub

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
#include <winservice.h>

#include <windows.h>

/* We're using version APIs */
#pragma comment(lib, "version")

#define USAGETEXT \
"mysql_upgrade_service.exe  Ver 1.00 for Windows\n" \
"Copyright (C) 2010-2011 Monty Program Ab & Vladislav Vaintroub" \
"This software comes with ABSOLUTELY NO WARRANTY. This is free software,\n" \
"and you are welcome to modify and redistribute it under the GPL v2 license\n" \
"Usage: mysql_upgrade_service.exe [OPTIONS]\n" \
"OPTIONS:"

static char mysqld_path[MAX_PATH];
static char mysqladmin_path[MAX_PATH];
static char mysqlupgrade_path[MAX_PATH];

static char defaults_file_param[MAX_PATH + 16]; /*--defaults-file=<path> */
static char logfile_path[MAX_PATH];
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
  {"service", 'S', "Name of the existing Windows service",
  &opt_service, &opt_service, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};



static my_bool
get_one_option(int optid, 
   const struct my_option *opt __attribute__ ((unused)),
   char *argument __attribute__ ((unused)))
{
  DBUG_ENTER("get_one_option");
  switch (optid) {
  case '?':
    printf("%s\n", USAGETEXT);
    my_print_help(my_long_options);
    exit(0);
    break;
  }
  DBUG_RETURN(0);
}



static void log(const char *fmt, ...)
{
  va_list args;
  /* Print the error message */
  va_start(args, fmt);
  vfprintf(stdout,fmt, args);
  va_end(args);
  fputc('\n', stdout);
  fflush(stdout);
}


static void die(const char *fmt, ...)
{
  va_list args;
  DBUG_ENTER("die");

  /* Print the error message */
  va_start(args, fmt);

  fprintf(stderr, "FATAL ERROR: ");
  vfprintf(stderr, fmt, args);
  if (logfile_path[0])
  {
    fprintf(stderr, "Additional information can be found in the log file %s",
      logfile_path);
  }
  va_end(args);
  fputc('\n', stderr);
  fflush(stdout);
  /* Cleanup */

  /*
    Stop service that we started, if it was not initally running at
    program start.
  */
  if (initial_service_state != -1 && initial_service_state != SERVICE_RUNNING)
  {
    SERVICE_STATUS service_status;
    ControlService(service, SERVICE_CONTROL_STOP, &service_status);
  }

  if (scm)
    CloseServiceHandle(scm);
  if (service)
    CloseServiceHandle(service);
  /* Stop mysqld.exe, if it was started for upgrade */
  if (mysqld_process)
    TerminateProcess(mysqld_process, 3);
  if (logfile_handle)
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
  char *end;
  va_list args;
  va_start(args, program);
  if (!program)
    die("Invalid call to run_tool");
  end= strxmov(cmdline, "\"", program, "\"", NullS);

  for(;;) 
  {
    char *param= va_arg(args,char *);
    if(!param)
      break;
    end= strxmov(end, " \"", param, "\"", NullS);
  }
  va_end(args);
  
  /* Create output file if not alredy done */
  if (!logfile_handle)
  {
    char tmpdir[FN_REFLEN];
    GetTempPath(FN_REFLEN, tmpdir);
    sprintf_s(logfile_path, "%s\\mysql_upgrade_service.%s.log", tmpdir, 
      opt_service);
    logfile_handle= CreateFile(logfile_path, GENERIC_WRITE,  FILE_SHARE_READ, 
      NULL, TRUNCATE_EXISTING, 0, NULL);
    if (!logfile_handle)
    {
      die("Cannot open log file %s, windows error %u", 
        logfile_path, GetLastError());
    }
  }

  /* Start child process */
  STARTUPINFO si= {0};
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

  if (wait_flag == P_NOWAIT)
  {
    /* Do not wait for process to complete, return handle. */
    return (intptr_t)pi.hProcess;
  }

  /* Wait for process to complete. */
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
      die("QueryServiceStatusEx failed (%u)\n", GetLastError()); 
    }

    /*
      Remeber initial state of the service, we will restore it on
      exit.
    */
    if(initial_service_state == -1)
      initial_service_state= ssp.dwCurrentState;

    switch(ssp.dwCurrentState)
    {
      case SERVICE_STOPPED:
        return;
      case SERVICE_RUNNING:
        if(!ControlService(service, SERVICE_CONTROL_STOP, 
             (SERVICE_STATUS *)&ssp))
            die("ControlService failed, error %u\n", GetLastError());
      case SERVICE_START_PENDING:
      case SERVICE_STOP_PENDING:
        if(timeout < 0)
          die("Service does not stop after %d seconds timeout",shutdown_timeout);
        Sleep(100);
        timeout -= 100;
        break;
      default:
        die("Unexpected service state %d",ssp.dwCurrentState);
    }
  }
}


/* 
  Shutdown mysql server. Not using mysqladmin, since 
  our --skip-grant-tables do not work anymore after mysql_upgrade
  that does "flush privileges". Instead, the shutdown event  is set.
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

  char defaults_file[MAX_PATH];
  char default_character_set[64];
  char buf[MAX_PATH];
  char commandline[3*MAX_PATH + 19];
  int i;

  scm= OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
  if(!scm)
    die("OpenSCManager failed with %u", GetLastError());
  service= OpenService(scm, opt_service, SERVICE_ALL_ACCESS);
  if (!service)
    die("OpenService failed with %u", GetLastError());

  BYTE config_buffer[8*1024];
  LPQUERY_SERVICE_CONFIGW config= (LPQUERY_SERVICE_CONFIGW)config_buffer;
  DWORD size= sizeof(config_buffer);
  DWORD needed;
  if (!QueryServiceConfigW(service, config, size, &needed))
    die("QueryServiceConfig failed with %u", GetLastError());

  mysqld_service_properties props;
  if (get_mysql_service_properties(config->lpBinaryPathName, &props))
  {
    die("Not a valid MySQL service");
  }

  int my_major= MYSQL_VERSION_ID/10000;
  int my_minor= (MYSQL_VERSION_ID %10000)/100;
  int my_patch= MYSQL_VERSION_ID%100;

  if(my_major < props.version_major || 
    (my_major == props.version_major && my_minor < props.version_minor))
  {
    die("Can not downgrade, the service is currently running as version %d.%d.%d"
      ", my version is %d.%d.%d", props.version_major, props.version_minor, 
      props.version_patch, my_major, my_minor, my_patch);
  }

  if(props.inifile[0] == 0)
  {
    /*
      Weird case, no --defaults-file in service definition, need to create one.
    */
    sprintf_s(props.inifile, MAX_PATH, "%s\\my.ini", props.datadir);
  }

  /*
    Write datadir to my.ini, after converting  backslashes to 
    unix style slashes.
  */
  strcpy_s(buf, MAX_PATH, props.datadir);
  for(i= 0; buf[i]; i++)
  {
    if (buf[i] == '\\')
      buf[i]= '/';
  }
  WritePrivateProfileString("mysqld", "datadir",buf, props.inifile);

  /*
    Remove basedir from defaults file, otherwise the service wont come up in 
    the new version, and will complain about mismatched message file.
  */
  WritePrivateProfileString("mysqld", "basedir",NULL, props.inifile);

  /* 
    Replace default-character-set  with character-set-server, to avoid 
    "default-character-set is deprecated and will be replaced ..."
    message.
  */
  default_character_set[0]= 0;
  GetPrivateProfileString("mysqld", "default-character-set", NULL,
    default_character_set, sizeof(default_character_set), defaults_file);
  if (default_character_set[0])
  {
    WritePrivateProfileString("mysqld", "default-character-set", NULL, 
      defaults_file);
    WritePrivateProfileString("mysqld", "character-set-server",
      default_character_set, defaults_file);
  }

  sprintf(defaults_file_param,"--defaults-file=%s", props.inifile);
  sprintf_s(commandline, "\"%s\" \"%s\" \"%s\"", mysqld_path, 
   defaults_file_param, opt_service);
  if (!ChangeServiceConfig(service, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, 
         SERVICE_NO_CHANGE, commandline, NULL, NULL, NULL, NULL, NULL, NULL))
  {
    die("ChangeServiceConfig failed with %u", GetLastError());
  }

}


int main(int argc, char **argv)
{
  int error;
  MY_INIT(argv[0]);
  char bindir[FN_REFLEN];
  char *p;

  /* Parse options */
  if ((error= handle_options(&argc, &argv, my_long_options, get_one_option)))
    die("");
  if (!opt_service)
    die("--service=# parameter is mandatory");
 
 /*
    Get full path to mysqld, we need it when changing service configuration.
    Assume installation layout, i.e mysqld.exe, mysqladmin.exe, mysqlupgrade.exe
    and mysql_upgrade_service.exe are in the same directory.
  */
  GetModuleFileName(NULL, bindir, FN_REFLEN);
  p= strrchr(bindir, FN_LIBCHAR);
  if(p)
  {
    *p= 0;
  }
  sprintf_s(mysqld_path, "%s\\mysqld.exe", bindir);
  sprintf_s(mysqladmin_path, "%s\\mysqladmin.exe", bindir);
  sprintf_s(mysqlupgrade_path, "%s\\mysql_upgrade.exe", bindir);

  char *paths[]= {mysqld_path, mysqladmin_path, mysqlupgrade_path};
  for(int i= 0; i< 3;i++)
  {
    if(GetFileAttributes(paths[i]) == INVALID_FILE_ATTRIBUTES)
      die("File %s does not exist", paths[i]);
  }

  /*
    Messages written on stdout should not be buffered,  GUI upgrade program 
    reads them from pipe and uses as progress indicator.
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
  sprintf_s(socket_param,"--socket=mysql_upgrade_service_%d", 
    GetCurrentProcessId());

  log("Phase 3/8: Starting mysqld for upgrade");
  mysqld_process= (HANDLE)run_tool(P_NOWAIT, mysqld_path,
    defaults_file_param, "--skip-networking",  "--skip-grant-tables", 
    "--enable-named-pipe",  socket_param, NULL);

  if (mysqld_process == INVALID_HANDLE_VALUE)
  {
    die("Cannot start mysqld.exe process, errno=%d", errno);
  }

  log("Phase 4/8: Waiting for startup to complete");
  DWORD start_duration_ms= 0;
  for(;;)
  {
    if (WaitForSingleObject(mysqld_process, 0) != WAIT_TIMEOUT)
      die("mysqld.exe did not start");

    if (run_tool(P_WAIT, mysqladmin_path, "--protocol=pipe",
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
  int upgrade_err= (int) run_tool(P_WAIT,  mysqlupgrade_path, 
    "--protocol=pipe", "--force",  socket_param,
    NULL);

  if (upgrade_err)
    die("mysql_upgrade failed with error code %d\n", upgrade_err);

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
  if (logfile_handle)
    CloseHandle(logfile_handle);
  my_end(0);
  exit(0);
}