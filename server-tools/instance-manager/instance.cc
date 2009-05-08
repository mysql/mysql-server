/* Copyright (C) 2004 MySQL AB

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

#if defined(__GNUC__) && defined(USE_PRAGMA_IMPLEMENTATION)
#pragma implementation
#endif

#include <my_global.h>
#include <mysql.h>

#include <signal.h>
#ifndef __WIN__
#include <sys/wait.h>
#endif

#include "manager.h"
#include "guardian.h"
#include "instance.h"
#include "log.h"
#include "mysql_manager_error.h"
#include "portability.h"
#include "priv.h"
#include "thread_registry.h"
#include "instance_map.h"


/*************************************************************************
  {{{ Platform-specific functions.
*************************************************************************/

#ifndef __WIN__
typedef pid_t My_process_info;
#else
typedef PROCESS_INFORMATION My_process_info;
#endif

/*
  Wait for an instance

  SYNOPSIS
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

  if (Manager::is_linux_threads())
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

  SYNOPSIS
    start_process()
    instance_options     Pointer to the options of the instance to be
                         launched.
    pi                   Pointer to the process information structure
                         (platform-dependent).

  RETURN
   FALSE - Success
   TRUE  - Cannot create an instance
*/

#ifndef __WIN__
static bool start_process(Instance_options *instance_options,
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
    log_error("Instance '%s': can not start mysqld: fork() failed.",
              (const char *) instance_options->instance_name.str);
    return TRUE;
  }

  return FALSE;
}
#else
static bool start_process(Instance_options *instance_options,
                          My_process_info *pi)
{
  STARTUPINFO si;

  ZeroMemory(&si, sizeof(STARTUPINFO));
  si.cb= sizeof(STARTUPINFO);
  ZeroMemory(pi, sizeof(PROCESS_INFORMATION));

  int cmdlen= 0;
  for (int i= 0; instance_options->argv[i] != 0; i++)
    cmdlen+= (uint) strlen(instance_options->argv[i]) + 3;
  cmdlen++;   /* make room for the null */

  char *cmdline= new char[cmdlen];
  if (cmdline == NULL)
    return TRUE;

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

  return !result;
}
#endif

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

/*************************************************************************
  }}}
*************************************************************************/


/*************************************************************************
  {{{ Static constants.
*************************************************************************/

const LEX_STRING
Instance::DFLT_INSTANCE_NAME= { C_STRING_WITH_LEN("mysqld") };

/*************************************************************************
  }}}
*************************************************************************/


/*************************************************************************
  {{{ Instance Monitor thread.
*************************************************************************/

/**
  Proxy thread is a simple way to avoid all pitfalls of the threads
  implementation in the OS (e.g. LinuxThreads). With such a thread we
  don't have to process SIGCHLD, which is a tricky business if we want
  to do it in a portable way.

  Instance Monitor Thread forks a child process, execs mysqld and waits for
  the child to die.

  Instance Monitor assumes that the monitoring instance will not be dropped.
  This is guaranteed by having flag monitoring_thread_active and
  Instance::is_active() operation.
*/

class Instance_monitor: public Thread
{
public:
  Instance_monitor(Instance *instance_arg) :instance(instance_arg) {}
protected:
  virtual void run();
  void start_and_monitor_instance();
private:
  Instance *instance;
};


void Instance_monitor::run()
{
  start_and_monitor_instance();
  delete this;
}


void Instance_monitor::start_and_monitor_instance()
{
  Thread_registry *thread_registry= Manager::get_thread_registry();
  Guardian *guardian= Manager::get_guardian();

  My_process_info mysqld_process_info;
  Thread_info monitor_thread_info;

  log_info("Instance '%s': Monitor: started.",
           (const char *) instance->get_name()->str);

  /*
    For guarded instance register the thread in Thread_registry to wait for
    the thread to stop on shutdown (nonguarded instances are not stopped on
    shutdown, so the thread will no finish).
  */

  if (instance->is_guarded())
  {
    thread_registry->register_thread(&monitor_thread_info, FALSE);
  }

  /* Starting mysqld. */

  log_info("Instance '%s': Monitor: starting mysqld...",
           (const char *) instance->get_name()->str);

  if (start_process(&instance->options, &mysqld_process_info))
  {
    instance->lock();
    instance->monitoring_thread_active= FALSE;
    instance->unlock();

    return;
  }

  /* Waiting for mysqld to die. */

  log_info("Instance '%s': Monitor: waiting for mysqld to stop...",
           (const char *) instance->get_name()->str);

  wait_process(&mysqld_process_info); /* Don't check for return value. */

  log_info("Instance '%s': Monitor: mysqld stopped.",
           (const char *) instance->get_name()->str);

  /* Update instance status. */

  instance->lock();

  if (instance->is_guarded())
    thread_registry->unregister_thread(&monitor_thread_info);

  instance->crashed= TRUE;
  instance->monitoring_thread_active= FALSE;

  log_info("Instance '%s': Monitor: finished.",
           (const char *) instance->get_name()->str);

  instance->unlock();

  /* Wake up guardian. */

  guardian->ping();
}

/**************************************************************************
  }}}
**************************************************************************/


/**************************************************************************
  {{{ Static operations.
**************************************************************************/

/**
  The operation is intended to check whether string is a well-formed
  instance name or not.

  SYNOPSIS
    is_name_valid()
    name  string to check

  RETURN
    TRUE    string is a valid instance name
    FALSE   string is not a valid instance name

  TODO: Move to Instance_name class: Instance_name::is_valid().
*/

bool Instance::is_name_valid(const LEX_STRING *name)
{
  const char *name_suffix= name->str + DFLT_INSTANCE_NAME.length;

  if (strncmp(name->str, Instance::DFLT_INSTANCE_NAME.str,
              Instance::DFLT_INSTANCE_NAME.length) != 0)
    return FALSE;

  return *name_suffix == 0 || my_isdigit(default_charset_info, *name_suffix);
}


/**
  The operation is intended to check if the given instance name is
  mysqld-compatible or not.

  SYNOPSIS
    is_mysqld_compatible_name()
    name  name to check

  RETURN
    TRUE    name is mysqld-compatible
    FALSE   otherwise

  TODO: Move to Instance_name class: Instance_name::is_mysqld_compatible().
*/

bool Instance::is_mysqld_compatible_name(const LEX_STRING *name)
{
  return strcmp(name->str, DFLT_INSTANCE_NAME.str) == 0;
}


/**
  Return client state name. Must not be used outside the class.
  Use Instance::get_state_name() instead.
*/

const char * Instance::get_instance_state_name(enum_instance_state state)
{
  switch (state) {
  case STOPPED:
    return "offline";

  case NOT_STARTED:
    return "not started";

  case STARTING:
    return "starting";

  case STARTED:
    return "online";

  case JUST_CRASHED:
    return "failed";

  case CRASHED:
    return "crashed";

  case CRASHED_AND_ABANDONED:
    return "abandoned";

  case STOPPING:
    return "stopping";
  }

  return NULL; /* just to ignore compiler warning. */
}

/**************************************************************************
  }}}
**************************************************************************/


/**************************************************************************
  {{{ Initialization & deinitialization.
**************************************************************************/

Instance::Instance()
  :monitoring_thread_active(FALSE),
  crashed(FALSE),
  configured(FALSE),
  /* mysqld_compatible is initialized in init() */
  state(NOT_STARTED),
  restart_counter(0),
  crash_moment(0),
  last_checked(0)
{
  pthread_mutex_init(&LOCK_instance, 0);
}


Instance::~Instance()
{
  log_info("Instance '%s': destroying...", (const char *) get_name()->str);

  pthread_mutex_destroy(&LOCK_instance);
}


/**
  Initialize instance options.

  SYNOPSIS
    init()
    name_arg      name of the instance

  RETURN:
    FALSE - ok
    TRUE  - error
*/

bool Instance::init(const LEX_STRING *name_arg)
{
  mysqld_compatible= is_mysqld_compatible_name(name_arg);

  return options.init(name_arg);
}


/**
  @brief Complete instance options initialization.

  @return Error status.
    @retval FALSE ok
    @retval TRUE error
*/

bool Instance::complete_initialization()
{
  configured= ! options.complete_initialization();
  return !configured;
}

/**************************************************************************
  }}}
**************************************************************************/


/**************************************************************************
  {{{ Instance: public interface implementation.
**************************************************************************/

/**
  Determine if there is some activity with the instance.

  SYNOPSIS
    is_active()

  DESCRIPTION
    An instance is active if one of the following conditions is true:
      - Instance-monitoring thread is running;
      - Instance is guarded and its state is other than STOPPED;
      - Corresponding mysqld-server accepts connections.

    MT-NOTE: instance must be locked before calling the operation.

  RETURN
    TRUE  - instance is active
    FALSE - otherwise.
*/

bool Instance::is_active()
{
  if (monitoring_thread_active)
    return TRUE;

  if (is_guarded() && get_state() != STOPPED)
    return TRUE;

  return is_mysqld_running();
}


/**
  Determine if mysqld is accepting connections.

  SYNOPSIS
    is_mysqld_running()

  DESCRIPTION
    Try to connect to mysqld with fake login/password to check whether it is
    accepting connections or not.

    MT-NOTE: instance must be locked before calling the operation.

  RETURN
    TRUE  - mysqld is alive and accept connections
    FALSE - otherwise.
*/

bool Instance::is_mysqld_running()
{
  MYSQL mysql;
  uint port= options.get_mysqld_port(); /* 0 if not specified. */
  const char *socket= NULL;
  static const char *password= "check_connection";
  static const char *username= "MySQL_Instance_Manager";
  static const char *access_denied_message= "Access denied for user";
  bool return_val;

  if (options.mysqld_socket)
    socket= options.mysqld_socket;

  /* no port was specified => instance falled back to default value */
  if (!port && !options.mysqld_socket)
    port= SERVER_DEFAULT_PORT;

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
    log_error("Instance '%s': was able to log into mysqld.",
              (const char *) get_name()->str);
    return_val= TRUE;                           /* server is alive */
  }
  else
    return_val= test(!strncmp(access_denied_message, mysql_error(&mysql),
                              sizeof(access_denied_message) - 1));

  mysql_close(&mysql);

  return return_val;
}


/**
  @brief Start mysqld.

  Reset flags and start Instance Monitor thread, which will start mysqld.

  @note Instance must be locked before calling the operation.

  @return Error status code
    @retval FALSE Ok
    @retval TRUE Could not start instance
*/

bool Instance::start_mysqld()
{
  Instance_monitor *instance_monitor;

  if (!configured)
    return TRUE;

  /*
    Prepare instance to start Instance Monitor thread.

    NOTE: It's important to set these actions here in order to avoid
    race conditions -- these actions must be done under acquired lock on
    Instance.
  */

  crashed= FALSE;
  monitoring_thread_active= TRUE;

  remove_pid();

  /* Create and start the Instance Monitor thread. */

  instance_monitor= new Instance_monitor(this);

  if (instance_monitor == NULL || instance_monitor->start(Thread::DETACHED))
  {
    delete instance_monitor;
    monitoring_thread_active= FALSE;

    log_error("Instance '%s': can not create instance monitor thread.",
              (const char *) get_name()->str);

    return TRUE;
  }

  ++restart_counter;

  /* The Instance Monitor thread will delete itself when it's finished. */

  return FALSE;
}


/**
  Stop mysqld.

  SYNOPSIS
    stop_mysqld()

  DESCRIPTION
    Try to stop mysqld gracefully. Otherwise kill it with SIGKILL.

    MT-NOTE: instance must be locked before calling the operation.

  RETURN
    FALSE - ok
    TRUE  - could not stop the instance
*/

bool Instance::stop_mysqld()
{
  log_info("Instance '%s': stopping mysqld...",
           (const char *) get_name()->str);

  kill_mysqld(SIGTERM);

  if (!wait_for_stop())
  {
    log_info("Instance '%s': mysqld stopped gracefully.",
             (const char *) get_name()->str);
    return FALSE;
  }

  log_info("Instance '%s': mysqld failed to stop gracefully within %d seconds.",
           (const char *) get_name()->str,
           (int) options.get_shutdown_delay());

  log_info("Instance'%s': killing mysqld...",
           (const char *) get_name()->str);

  kill_mysqld(SIGKILL);

  if (!wait_for_stop())
  {
    log_info("Instance '%s': mysqld has been killed.",
             (const char *) get_name()->str);
    return FALSE;
  }

  log_info("Instance '%s': can not kill mysqld within %d seconds.",
           (const char *) get_name()->str,
           (int) options.get_shutdown_delay());

  return TRUE;
}


/**
  Send signal to mysqld.

  SYNOPSIS
    kill_mysqld()

  DESCRIPTION
    Load pid from the pid file and send the given signal to that process.
    If the signal is SIGKILL, remove the pid file after sending the signal.

    MT-NOTE: instance must be locked before calling the operation.

  TODO
    This too low-level and OS-specific operation for public interface.
    Also, it has some implicit behaviour for SIGKILL signal. Probably, we
    should have the following public operations instead:
      - start_mysqld() -- as is;
      - stop_mysqld -- request mysqld to shutdown gracefully (send SIGTERM);
        don't wait for complete shutdown;
      - wait_for_stop() (or join_mysqld()) -- wait for mysqld to stop within
        time interval;
      - kill_mysqld() -- request to terminate mysqld; don't wait for
        completion.
    These operations should also be used in Guardian to manage instances.
*/

bool Instance::kill_mysqld(int signum)
{
  pid_t mysqld_pid= options.load_pid();

  if (mysqld_pid == 0)
  {
    log_info("Instance '%s': no pid file to send a signal (%d).",
             (const char *) get_name()->str,
             (int) signum);
    return TRUE;
  }

  log_info("Instance '%s': sending %d to %d...",
           (const char *) get_name()->str,
           (int) signum,
           (int) mysqld_pid);

  if (kill(mysqld_pid, signum))
  {
    log_info("Instance '%s': kill() failed.",
             (const char *) get_name()->str);
    return TRUE;
  }

  /* Kill suceeded */
  if (signum == SIGKILL)      /* really killed instance with SIGKILL */
  {
    log_error("Instance '%s': killed.",
              (const char *) options.instance_name.str);

    /* After sucessful hard kill the pidfile need to be removed */
    options.unlink_pidfile();
  }

  return FALSE;
}


/**
  Lock instance.
*/

void Instance::lock()
{
  pthread_mutex_lock(&LOCK_instance);
}


/**
  Unlock instance.
*/

void Instance::unlock()
{
  pthread_mutex_unlock(&LOCK_instance);
}


/**
  Return instance state name.

  SYNOPSIS
    get_state_name()

  DESCRIPTION
    The operation returns user-friendly state name. The operation can be
    used both for guarded and non-guarded instances.

    MT-NOTE: instance must be locked before calling the operation.

  TODO: Replace with the static get_state_name(state_code) function.
*/

const char *Instance::get_state_name()
{
  if (!is_configured())
    return "misconfigured";

  if (is_guarded())
  {
    /* The instance is managed by Guardian: we can report precise state. */

    return get_instance_state_name(get_state());
  }

  /* The instance is not managed by Guardian: we can report status only.  */

  return is_active() ? "online" : "offline";
}


/**
  Reset statistics.

  SYNOPSIS
    reset_stat()

  DESCRIPTION
    The operation resets statistics used for guarding the instance.

    MT-NOTE: instance must be locked before calling the operation.

  TODO: Make private.
*/

void Instance::reset_stat()
{
  restart_counter= 0;
  crash_moment= 0;
  last_checked= 0;
}

/**************************************************************************
  }}}
**************************************************************************/


/**************************************************************************
  {{{ Instance: implementation of private operations.
**************************************************************************/

/**
  Remove pid file.
*/

void Instance::remove_pid()
{
  int mysqld_pid= options.load_pid();

  if (mysqld_pid == 0)
    return;

  if (options.unlink_pidfile())
  {
    log_error("Instance '%s': can not unlink pid file.",
              (const char *) options.instance_name.str);
  }
}


/**
  Wait for mysqld to stop within shutdown interval.
*/

bool Instance::wait_for_stop()
{
  int start_time= (int) time(NULL);
  int finish_time= start_time + options.get_shutdown_delay();

  log_info("Instance '%s': waiting for mysqld to stop "
           "(timeout: %d seconds)...",
           (const char *) get_name()->str,
           (int) options.get_shutdown_delay());

  while (true)
  {
    if (options.load_pid() == 0 && !is_mysqld_running())
      return FALSE;

    if (time(NULL) >= finish_time)
      return TRUE;

    /* Sleep for 0.3 sec and check again. */

    my_sleep(300000);
  }
}

/**************************************************************************
  }}}
**************************************************************************/
