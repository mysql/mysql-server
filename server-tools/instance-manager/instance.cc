/* Copyright (C) 2004 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#if defined(__GNUC__) && defined(USE_PRAGMA_IMPLEMENTATION)
#pragma implementation
#endif

#include "instance.h"

#include <my_global.h>
#include <mysql.h>

#include <signal.h>
#ifndef __WIN__
#include <sys/wait.h>
#endif

#include "guardian.h"
#include "instance_map.h"
#include "log.h"
#include "mysql_manager_error.h"
#include "portability.h"
#include "priv.h"


const LEX_STRING
Instance::DFLT_INSTANCE_NAME= { C_STRING_WITH_LEN("mysqld") };

static const char * const INSTANCE_NAME_PREFIX= Instance::DFLT_INSTANCE_NAME.str;
static const int INSTANCE_NAME_PREFIX_LEN= Instance::DFLT_INSTANCE_NAME.length;


static void start_and_monitor_instance(Instance_options *old_instance_options,
                                       Instance_map *instance_map);

#ifndef __WIN__
typedef pid_t My_process_info;
#else
typedef PROCESS_INFORMATION My_process_info;
#endif

/*
  Proxy thread is a simple way to avoid all pitfalls of the threads
  implementation in the OS (e.g. LinuxThreads). With such a thread we
  don't have to process SIGCHLD, which is a tricky business if we want
  to do it in a portable way.
*/

pthread_handler_t proxy(void *arg)
{
  Instance *instance= (Instance *) arg;
  start_and_monitor_instance(&instance->options,
                             instance->get_map());
  return 0;
}

/*
  Wait for an instance

  SYNOPSYS
    wait_process()
    pi                   Pointer to the process information structure
                         (platform-dependent).

  RETURN
   0  -  Success
   1  -  Error
*/

#ifndef __WIN__
static int wait_process(My_process_info *pi)
{
  /*
    Here we wait for the child created. This process differs for systems
    running LinuxThreads and POSIX Threads compliant systems. This is because
    according to POSIX we could wait() for a child in any thread of the
    process. While LinuxThreads require that wait() is called by the thread,
    which created the child.
    On the other hand we could not expect mysqld to return the pid, we
    got in from fork(), to wait4() fucntion when running on LinuxThreads.
    This is because MySQL shutdown thread is not the one, which was created
    by our fork() call.
    So basically we have two options: whether the wait() call returns only in
    the creator thread, but we cannot use waitpid() since we have no idea
    which pid we should wait for (in fact it should be the pid of shutdown
    thread, but we don't know this one). Or we could use waitpid(), but
    couldn't use wait(), because it could return in any wait() in the program.
  */
  if (linuxthreads)
    wait(NULL);                               /* LinuxThreads were detected */
  else
    waitpid(*pi, NULL, 0);

  return 0;
}
#else
static int wait_process(My_process_info *pi)
{
  /* Wait until child process exits. */
  WaitForSingleObject(pi->hProcess, INFINITE);

  DWORD exitcode;
  ::GetExitCodeProcess(pi->hProcess, &exitcode);

  /* Close process and thread handles. */
  CloseHandle(pi->hProcess);
  CloseHandle(pi->hThread);

  /*
    GetExitCodeProces returns zero on failure. We should revert this value
    to report an error.
  */
  return (!exitcode);
}
#endif


/*
  Launch an instance

  SYNOPSYS
    start_process()
    instance_options     Pointer to the options of the instance to be
                         launched.
    pi                   Pointer to the process information structure
                         (platform-dependent).

  RETURN
   0  -  Success
   1  -  Cannot create an instance
*/

#ifndef __WIN__
static int start_process(Instance_options *instance_options,
                         My_process_info *pi)
{
#ifndef __QNX__
  *pi= fork();
#else
  /*
     On QNX one cannot use fork() in multithreaded environment and we
     should use spawn() or one of it's siblings instead.
     Here we use spawnv(), which  is a combination of fork() and execv()
     in one call. It returns the pid of newly created process (>0) or -1
  */
  *pi= spawnv(P_NOWAIT, instance_options->mysqld_path, instance_options->argv);
#endif

  switch (*pi) {
  case 0:                                       /* never happens on QNX */
    execv(instance_options->mysqld_path.str, instance_options->argv);
    /* exec never returns */
    exit(1);
  case -1:
    log_info("cannot create a new process to start instance %s",
             instance_options->instance_name);
    return 1;
  }
  return 0;
}
#else
static int start_process(Instance_options *instance_options,
                         My_process_info *pi)
{
  STARTUPINFO si;

  ZeroMemory(&si, sizeof(STARTUPINFO));
  si.cb= sizeof(STARTUPINFO);
  ZeroMemory(pi, sizeof(PROCESS_INFORMATION));

  int cmdlen= 0;
  for (int i= 0; instance_options->argv[i] != 0; i++)
    cmdlen+= strlen(instance_options->argv[i]) + 3;
  cmdlen++;   /* make room for the null */

  char *cmdline= new char[cmdlen];
  if (cmdline == NULL)
    return 1;

  cmdline[0]= 0;
  for (int i= 0; instance_options->argv[i] != 0; i++)
  {
    strcat(cmdline, "\"");
    strcat(cmdline, instance_options->argv[i]);
    strcat(cmdline, "\" ");
  }

  /* Start the child process */
  BOOL result=
    CreateProcess(NULL,          /* Put it all in cmdline */
                  cmdline,       /* Command line */
                  NULL,          /* Process handle not inheritable */
                  NULL,          /* Thread handle not inheritable */
                  FALSE,         /* Set handle inheritance to FALSE */
                  0,             /* No creation flags */
                  NULL,          /* Use parent's environment block */
                  NULL,          /* Use parent's starting directory */
                  &si,           /* Pointer to STARTUPINFO structure */
                  pi);           /* Pointer to PROCESS_INFORMATION structure */
  delete cmdline;

  return (!result);
}
#endif

/*
  Fork child, exec an instance and monitor it.

  SYNOPSYS
    start_and_monitor_instance()
    old_instance_options   Pointer to the options of the instance to be
                           launched. This info is likely to become obsolete
                           when function returns from wait_process()
    instance_map           Pointer to the instance_map. We use it to protect
                           the instance from deletion, while we are working
                           with it.

  DESCRIPTION
    Fork a child, then exec and monitor it. When the child is dead,
    find appropriate instance (for this purpose we save its name),
    set appropriate flags and wake all threads waiting for instance
    to stop.

  RETURN
    Function returns no value
*/

static void start_and_monitor_instance(Instance_options *old_instance_options,
                                       Instance_map *instance_map)
{
  Instance_name instance_name(&old_instance_options->instance_name);
  Instance *current_instance;
  My_process_info process_info;

  /*
    Lock instance map to guarantee that no instances are deleted during
    strmake() and execv() calls.
  */
  instance_map->lock();

  /*
    Save the instance name in the case if Instance object we
    are using is destroyed. (E.g. by "FLUSH INSTANCES")
  */

  log_info("starting instance %s", (const char *) instance_name.get_c_str());

  if (start_process(old_instance_options, &process_info))
  {
    instance_map->unlock();
    return;                                     /* error is logged */
  }

  /* allow users to delete instances */
  instance_map->unlock();

  /* don't check for return value */
  wait_process(&process_info);

  instance_map->lock();

  current_instance= instance_map->find(instance_name.get_str());

  if (current_instance)
    current_instance->set_crash_flag_n_wake_all();

  instance_map->unlock();

  return;
}


bool Instance::is_name_valid(const LEX_STRING *name)
{
  const char *name_suffix= name->str + INSTANCE_NAME_PREFIX_LEN;

  if (strncmp(name->str, INSTANCE_NAME_PREFIX, INSTANCE_NAME_PREFIX_LEN) != 0)
    return FALSE;

  return *name_suffix == 0 || my_isdigit(default_charset_info, *name_suffix);
}


bool Instance::is_mysqld_compatible_name(const LEX_STRING *name)
{
  return strcmp(name->str, INSTANCE_NAME_PREFIX) == 0;
}


Instance_map *Instance::get_map()
{
  return instance_map;
}


void Instance::remove_pid()
{
  int pid;
  if ((pid= options.get_pid()) != 0)          /* check the pidfile */
    if (options.unlink_pidfile())             /* remove stalled pidfile */
      log_error("cannot remove pidfile for instance %i, this might be \
                since IM lacks permmissions or hasn't found the pidifle",
                options.instance_name);
}


/*
  The method starts an instance.

  SYNOPSYS
    start()

  RETURN
    0                             ok
    ER_CANNOT_START_INSTANCE      Cannot start instance
    ER_INSTANCE_ALREADY_STARTED   The instance on the specified port/socket
                                  is already started
*/

int Instance::start()
{
  /* clear crash flag */
  pthread_mutex_lock(&LOCK_instance);
  crashed= FALSE;
  pthread_mutex_unlock(&LOCK_instance);


  if (configured && !is_running())
  {
    remove_pid();

    /*
      No need to monitor this thread in the Thread_registry, as all
      instances are to be stopped during shutdown.
    */
    pthread_t proxy_thd_id;
    pthread_attr_t proxy_thd_attr;
    int rc;

    pthread_attr_init(&proxy_thd_attr);
    pthread_attr_setdetachstate(&proxy_thd_attr, PTHREAD_CREATE_DETACHED);
    rc= pthread_create(&proxy_thd_id, &proxy_thd_attr, proxy,
                       this);
    pthread_attr_destroy(&proxy_thd_attr);
    if (rc)
    {
      log_error("Instance::start(): pthread_create(proxy) failed");
      return ER_CANNOT_START_INSTANCE;
    }

    return 0;
  }

  /* The instance is started already or misconfigured. */
  return configured ? ER_INSTANCE_ALREADY_STARTED : ER_INSTANCE_MISCONFIGURED;
}

/*
  The method sets the crash flag and wakes all waiters on
  COND_instance_stopped and COND_guardian

  SYNOPSYS
    set_crash_flag_n_wake_all()

  DESCRIPTION
    The method is called when an instance is crashed or terminated.
    In the former case it might indicate that guardian probably should
    restart it.

  RETURN
    Function returns no value
*/

void Instance::set_crash_flag_n_wake_all()
{
  /* set instance state to crashed */
  pthread_mutex_lock(&LOCK_instance);
  crashed= TRUE;
  pthread_mutex_unlock(&LOCK_instance);

  /*
    Wake connection threads waiting for an instance to stop. This
    is needed if a user issued command to stop an instance via
    mysql connection. This is not the case if Guardian stop the thread.
  */
  pthread_cond_signal(&COND_instance_stopped);
  /* wake guardian */
  pthread_cond_signal(&instance_map->guardian->COND_guardian);
}



Instance::Instance(): crashed(FALSE), configured(FALSE)
{
  pthread_mutex_init(&LOCK_instance, 0);
  pthread_cond_init(&COND_instance_stopped, 0);
}


Instance::~Instance()
{
  pthread_cond_destroy(&COND_instance_stopped);
  pthread_mutex_destroy(&LOCK_instance);
}


bool Instance::is_crashed()
{
  bool val;
  pthread_mutex_lock(&LOCK_instance);
  val= crashed;
  pthread_mutex_unlock(&LOCK_instance);
  return val;
}


bool Instance::is_running()
{
  MYSQL mysql;
  uint port= 0;
  const char *socket= NULL;
  static const char *password= "check_connection";
  static const char *username= "MySQL_Instance_Manager";
  static const char *access_denied_message= "Access denied for user";
  bool return_val;

  if (options.mysqld_port)
  {
    /*
      NOTE: it is important to check mysqld_port here, but use
      mysqld_port_val. The idea is that if the option is unset, mysqld_port
      will be NULL, but mysqld_port_val will not be reset.
    */
    port= options.mysqld_port_val;
  }

  if (options.mysqld_socket)
    socket= options.mysqld_socket;

  /* no port was specified => instance falled back to default value */
  if (!options.mysqld_port && !options.mysqld_socket)
    port= SERVER_DEFAULT_PORT;

  pthread_mutex_lock(&LOCK_instance);

  mysql_init(&mysql);
  /* try to connect to a server with a fake username/password pair */
  if (mysql_real_connect(&mysql, LOCAL_HOST, username,
                         password,
                         NullS, port,
                         socket, 0))
  {
    /*
      We have successfully connected to the server using fake
      username/password. Write a warning to the logfile.
    */
    log_info("The Instance Manager was able to log into you server \
             with faked compiled-in password while checking server status. \
             Looks like something is wrong.");
    pthread_mutex_unlock(&LOCK_instance);
    return_val= TRUE;                           /* server is alive */
  }
  else
    return_val= test(!strncmp(access_denied_message, mysql_error(&mysql),
                              sizeof(access_denied_message) - 1));

  mysql_close(&mysql);
  pthread_mutex_unlock(&LOCK_instance);

  return return_val;
}


/*
  Stop an instance.

  SYNOPSYS
    stop()

  RETURN:
    0                            ok
    ER_INSTANCE_IS_NOT_STARTED   Looks like the instance it is not started
    ER_STOP_INSTANCE             mysql_shutdown reported an error
*/

int Instance::stop()
{
  struct timespec timeout;
  uint waitchild= (uint)  DEFAULT_SHUTDOWN_DELAY;

  if (is_running())
  {
    if (options.shutdown_delay)
    {
      /*
        NOTE: it is important to check shutdown_delay here, but use
        shutdown_delay_val. The idea is that if the option is unset,
        shutdown_delay will be NULL, but shutdown_delay_val will not be reset.
      */
      waitchild= options.shutdown_delay_val;
    }
  }

    kill_instance(SIGTERM);
    /* sleep on condition to wait for SIGCHLD */

    timeout.tv_sec= time(NULL) + waitchild;
    timeout.tv_nsec= 0;
    if (pthread_mutex_lock(&LOCK_instance))
      return ER_STOP_INSTANCE;

    while (options.get_pid() != 0)              /* while server isn't stopped */
    {
      int status;

      status= pthread_cond_timedwait(&COND_instance_stopped,
                                     &LOCK_instance,
                                     &timeout);
      if (status == ETIMEDOUT || status == ETIME)
        break;
    }

    pthread_mutex_unlock(&LOCK_instance);

    kill_instance(SIGKILL);

    return 0;
  }

  return ER_INSTANCE_IS_NOT_STARTED;
}

#ifdef __WIN__

BOOL SafeTerminateProcess(HANDLE hProcess, UINT uExitCode)
{
  DWORD dwTID, dwCode, dwErr= 0;
  HANDLE hProcessDup= INVALID_HANDLE_VALUE;
  HANDLE hRT= NULL;
  HINSTANCE hKernel= GetModuleHandle("Kernel32");
  BOOL bSuccess= FALSE;

  BOOL bDup= DuplicateHandle(GetCurrentProcess(),
                             hProcess, GetCurrentProcess(), &hProcessDup,
                             PROCESS_ALL_ACCESS, FALSE, 0);

  // Detect the special case where the process is
  // already dead...
  if (GetExitCodeProcess((bDup) ? hProcessDup : hProcess, &dwCode) &&
      (dwCode == STILL_ACTIVE))
  {
    FARPROC pfnExitProc;

    pfnExitProc= GetProcAddress(hKernel, "ExitProcess");

    hRT= CreateRemoteThread((bDup) ? hProcessDup : hProcess, NULL, 0,
                            (LPTHREAD_START_ROUTINE)pfnExitProc,
                            (PVOID)uExitCode, 0, &dwTID);

    if (hRT == NULL)
      dwErr= GetLastError();
  }
  else
    dwErr= ERROR_PROCESS_ABORTED;

  if (hRT)
  {
    // Must wait process to terminate to
    // guarantee that it has exited...
    WaitForSingleObject((bDup) ? hProcessDup : hProcess, INFINITE);

    CloseHandle(hRT);
    bSuccess= TRUE;
  }

  if (bDup)
    CloseHandle(hProcessDup);

  if (!bSuccess)
    SetLastError(dwErr);

  return bSuccess;
}

int kill(pid_t pid, int signum)
{
  HANDLE processhandle= ::OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
  if (signum == SIGTERM)
    ::SafeTerminateProcess(processhandle, 0);
  else
    ::TerminateProcess(processhandle, -1);
  return 0;
}
#endif

void Instance::kill_instance(int signum)
{
  pid_t pid;
  /* if there are no pid, everything seems to be fine */
  if ((pid= options.get_pid()) != 0)            /* get pid from pidfile */
  {
    /*
      If we cannot kill mysqld, then it has propably crashed.
      Let us try to remove staled pidfile and return successfully
      as mysqld is probably stopped.
    */
    if (!kill(pid, signum))
      options.unlink_pidfile();
    else if (signum == SIGKILL)      /* really killed instance with SIGKILL */
      log_error("The instance %s is being stopped forsibly. Normally \
                it should not happed. Probably the instance has been \
                hanging. You should also check your IM setup",
                options.instance_name);
  }
  return;
}

/*
  Initialize instance parameters.

  SYNOPSYS
    Instance::init()
    name_arg      name of the instance

  RETURN:
    0             ok
    !0            error
*/

int Instance::init(const LEX_STRING *name_arg)
{
  mysqld_compatible= is_mysqld_compatible_name(name_arg);

  return options.init(name_arg);
}


int Instance::complete_initialization(Instance_map *instance_map_arg,
                                      const char *mysqld_path)
{
  instance_map= instance_map_arg;
  configured= !options.complete_initialization(mysqld_path);
  return 0;
  /*
    TODO: return actual status (from
    Instance_options::complete_initialization()) here.
  */
}
