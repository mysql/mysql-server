/* Copyright (c) 2009, 2017, Oracle and/or its affiliates. All rights reserved.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifdef _WIN32
#include <process.h>
#endif
#include <BaseString.hpp>
#include <ndb_daemon.h>
#include <portlib/NdbHost.h>
#include <stdio.h>

#include "m_string.h"
#include "my_sys.h"

static FILE *dlog_file;

static int ERR1(const char* fmt, ...)
  ATTRIBUTE_FORMAT(printf, 1, 2);

char ndb_daemon_error[1024];
static int ERR1(const char* fmt, ...)
{
  va_list argptr;
  va_start(argptr, fmt);
  vsnprintf(ndb_daemon_error,sizeof(ndb_daemon_error),fmt,argptr);
  va_end(argptr);
  return 1;
}


#ifdef _WIN32

#include "sql/nt_servc.h"

static NTService g_ntsvc;

static int g_argc;
static char** g_argv;

static HANDLE g_shutdown_event;
static ndb_daemon_stop_t g_stop_func;
static ndb_daemon_run_t g_run_func;

static void stopper_thread(void*)
{
  // Wait forever until the shutdown event is signaled
  WaitForSingleObject(g_shutdown_event, INFINITE);

  // Call the installed stop callback function
  g_stop_func();
}


/*
  This function is called like:
    <service dipatcher thread>
       - NTService::ServiceMain
        - NTService::StartService
           <new service thread>
             -service_main
 and runs the "application" through
 the installed callback function "g_run_func"
*/

static int service_main(NTService* service)
{
  /* Inform SCM that service is running and can be stopped */
  service->SetRunning();

  /* Run the application with with argc/argv */
  return g_run_func(g_argc, g_argv);
}


/*
  Check if "arg" contains "option", return the
  options argument in opt_arg(i.e everything after =)
*/

static bool
is_option(const char* arg, const char* option, const char** opt_arg)
{
  size_t option_len = strlen(option);
  if (strncmp(arg, option, option_len))
    return false; // No hit

  // Step forward to the end of --<option>
  arg+= option_len;
  if (*arg == '=')
  {
    /* Assign opt_arg pointer to first char after = */
    *opt_arg= arg + 1;
  }
  return true;
}


static int
install_or_remove_service(int argc, char** argv,
                          const char* name, const char* display_name)
{
  if (argc < 2)
    return 0; // Nothing to do

  /* --remove as first argument on command line */
  const char* remove_name = NULL;
  if (is_option(argv[1], "--remove", &remove_name))
  {
    if (remove_name)
    {
       /* Use the part after = as service name _and_ display name */
      name = display_name = remove_name;
    }
    printf("Removing service '%s'\n", display_name);
    /* Remove service. Ignore return value, error is printed to stdout */
    (void)g_ntsvc.Remove(name);
    return 1;
  }

  const char* install_name = NULL;
  if (is_option(argv[1], "--install", &install_name))
  {
    if (install_name)
    {
       /* Use the part after = as service name _and_ display name */
      name = display_name = install_name;
    }

    BaseString cmd;

    /* Full path to this binary */
    char exe[MAX_PATH];
    GetModuleFileName(NULL, exe, sizeof(exe));
    cmd.assfmt("\"%s\"", exe);

    /* The option that tells which service is starting  */
    cmd.appfmt(" \"--service=%s\"", name);

    /* All the args after --install(which must be first) */
    for (int i = 2; i < argc; i++)
      cmd.appfmt(" \"%s\"", argv[i]);

    printf("Installing service '%s' as '%s'\n", display_name, cmd.c_str());

     /* Install service. Ignore return value, error is printed to stdout */
    (void)g_ntsvc.Install(1, name, display_name, cmd.c_str());
    return 1;
  }
  return 0;
}
#endif


int ndb_daemon_init(int argc, char** argv,
                   ndb_daemon_run_t run, ndb_daemon_stop_t stop,
                   const char* name, const char* display_name)
{
#ifdef _WIN32
  // Check for --install or --remove options
  if (install_or_remove_service(argc, argv, name, display_name))
    return 1;

  // Check if first arg is --service -> run as service
  const char* service_name = NULL;
  if (argc > 1 &&
      is_option(argv[1], "--service", &service_name) &&
     service_name)
  {
    // Create the shutdown event that will be signaled
    // by g_ntsvc if the service is to be stopped
    g_shutdown_event = CreateEvent(0, 0, 0, 0);

    // Install the shutdown event in g_ntsvc
    g_ntsvc.SetShutdownEvent(g_shutdown_event);

    // Save the stop function so it can be called
    // by 'stopper_thread'
    g_stop_func = stop;

    // Create a thread whose only purpose is to wait for
    // the shutdown event to be signaled and then call the 'stop'
    // function
    uintptr_t stop_thread = _beginthread(stopper_thread,0,0);
    if(!stop_thread)
      return ERR1("couldn't start stopper thread");

    // Save the run function so it can be called
    // by 'service_main'
    g_run_func = run;

    // Build argv without --service
    BaseString cmd;
    for (int i = 2; i < argc; i++)
      cmd.appfmt(" %s", argv[i]);
    g_argv = BaseString::argify(argv[0], cmd.c_str());
    g_argc = argc - 1;

    // Start the service thread and let it run 'service_main'
    // This call will not return until the service thread returns
    return g_ntsvc.Init(service_name, service_main);
  }
#endif

  // Default behaviour, run the "run" function which
  // should be the "applications" real main
  return run(argc, argv);

}


#ifdef _WIN32

#include <sys/locking.h>

#define F_TLOCK _LK_NBLCK
#define F_ULOCK _LK_UNLCK
#define F_LOCK  _LK_LOCK

static inline int lockf(int fd, int cmd, off_t len)
{
  return _locking(fd, cmd, len);
}

static inline int ftruncate(int fd, off_t length)
{
  return _chsize(fd, length);
}

static inline int unlink(const char *filename)
{
  return _unlink(filename);
}
#endif

static const char *g_pidfile_name = 0;
static int g_pidfd = -1, g_logfd = -1;

static int
check_files(const char *pidfile_name,
            const char *logfile_name,
            int *pidfd_ret, int *logfd_ret)
{
  /* Open log file if any */
  if (logfile_name)
  {
    int logfd = open(logfile_name, O_CREAT | O_WRONLY | O_APPEND, 0644);
    if(logfd == -1)
      return ERR1("Failed to open logfile '%s' for write, errno: %d",
                logfile_name, errno);
    g_logfd = logfd;
    dlog_file = fdopen(logfd, "a");
    *logfd_ret = logfd;
  }

  /* Check that we have write access to lock file */
  if (!pidfile_name)
    return ERR1("Missing pid file name");
  int pidfd= open(pidfile_name, O_CREAT | O_RDWR, 0644);
  if (pidfd == -1)
    return ERR1("Failed to open pidfile '%s' for write, errno: %d",
                pidfile_name, errno);
  g_pidfd = pidfd;

  /* Read any old pid from lock file */
  char buf[32];
  int bytes_read = read(pidfd, buf, sizeof(buf));
  if(bytes_read < 0)
    return ERR1("Failed to read from pidfile '%s', errno: %d",
                pidfile_name, errno);
  buf[bytes_read]= 0;
  long currpid= atol(buf);
  if(lseek(pidfd, 0, SEEK_SET) == -1)
    return ERR1("Failed to lseek pidfile '%s', errno: %d",
                pidfile_name, errno);

  /* Check that file can be locked */
  if(lockf(pidfd, F_TLOCK, 0) == -1)
  {
    if(errno == EACCES || errno == EAGAIN)
      return ERR1("Failed to lock pidfile '%s', already locked by "
                  "pid=%ld, errno: %d", pidfile_name, currpid, errno);
  }
  if(lockf(pidfd, F_ULOCK, 0) == -1)
    return ERR1("Failed to unlock pidfile '%s', errno: %d",
                pidfile_name, errno);

  *pidfd_ret = pidfd;
  return 0;
}


static int
do_files(const char *pidfile_name, const char* logfile_name, int pidfd, int logfd)
{
  /* Lock the lock file */
  if (lockf(pidfd, F_LOCK, 0) == -1)
    return ERR1("Failed to lock pidfile '%s', errno: %d",
                pidfile_name, errno);

  /* Write pid to lock file */
  if (ftruncate(pidfd, 0) == -1)
    return ERR1("Failed to truncate file '%s', errno: %d",
                pidfile_name, errno);

  char buf[32];
  int length = (int)snprintf(buf, sizeof(buf), "%ld",
                           (long)NdbHost_GetProcessId());
  if (write(pidfd, buf, length) != length)
    return ERR1("Failed to write pid to pidfile '%s', errno: %d",
                pidfile_name, errno);

#ifdef _WIN32
  // Redirect stdout and stderr to the daemon log file
  freopen(logfile_name, "a+", stdout);
  freopen(logfile_name, "a+", stderr);
  setbuf(stderr, NULL);
#else
  /* Do input/output redirections (assume fd 0,1,2 not in use) */
  close(0);
  const char* fname = "/dev/null";
  if (open(fname, O_RDONLY) == -1)
    return ERR1("Failed to open '%s', errno: %d", fname, errno);

  if (logfd != -1)
  {
    dup2(logfd, 1);
    dup2(logfd, 2);
    close(logfd);
    dlog_file= stdout;
  }
#endif

  return 0;
}


int ndb_daemonize(const char* pidfile_name, const char *logfile_name)
{
  int pidfd = -1, logfd = -1;

  if (check_files(pidfile_name, logfile_name, &pidfd, &logfd))
    return 1;

#ifndef _WIN32
  pid_t child = fork();
  if (child == -1)
    return ERR1("fork failed, errno: %d, error: %s", errno, strerror(errno));

  /* Exit if we are the parent */
  if (child != 0)
    exit(0);

  /* Become process group leader */
  if(setsid() == -1)
    return ERR1("Failed to setsid, errno: %d", errno);

#endif

  if (do_files(pidfile_name, logfile_name, pidfd, logfd))
    return 1;

  g_pidfile_name = pidfile_name;

  return 0;
}

void ndb_daemon_exit(int status)
{
  if (g_pidfd != -1)
    close(g_pidfd);

  if (g_logfd != -1)
    close(g_logfd);

  if (g_pidfile_name)
    unlink(g_pidfile_name);

#ifdef _WIN32
  /*
    Stop by calling NtService::Stop if running
     as a service(i.e g_shutdown_event created)
  */
  if (g_shutdown_event)
    g_ntsvc.Stop();
#endif

#ifdef HAVE_GCOV
   exit(status);
#else
  _exit(status);
#endif

}

void ndb_service_print_options(const char* name)
{
#ifdef _WIN32
  puts("");
  puts("The following Windows specific options may be given as "
       "the first argument:");
  printf("  --install[=name]\tInstall %s as service with given "
         "name(default: %s), \n"
         "\t\t\tusing the arguments currently given on command line.\n",
         name, name);
  printf("  --remove[=name]\tRemove service with name(default: %s)\n",
         name);
  puts("");
#endif
}


void ndb_service_wait_for_debugger(int timeout_sec)
{
#ifdef _WIN32
   if(!IsDebuggerPresent())
   {
     int i;
     printf("Waiting for debugger to attach, pid=%u\n",GetCurrentProcessId());
     fflush(stdout);
     for(i= 0; i < timeout_sec; i++)
     {
       Sleep(1000);
       if(IsDebuggerPresent())
       {
         /* Break into debugger */
         __debugbreak();
         return;
       }
     }
     printf("pid=%u, debugger not attached after %d seconds, resuming\n",GetCurrentProcessId(),
       timeout_sec);
     fflush(stdout);
   }
#endif
}
