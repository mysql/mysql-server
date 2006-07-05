/* Copyright (C) 2003 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

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

#include "manager.h"

#include <my_global.h>
#include <m_string.h>
#include <my_sys.h>
#include <thr_alarm.h>

#include <signal.h>
#ifndef __WIN__
#include <sys/wait.h>
#endif

#include "exit_codes.h"
#include "guardian.h"
#include "instance_map.h"
#include "listener.h"
#include "log.h"
#include "options.h"
#include "priv.h"
#include "thread_registry.h"
#include "user_map.h"


int create_pid_file(const char *pid_file_name, int pid)
{
  FILE *pid_file;

  if (!(pid_file= my_fopen(pid_file_name, O_WRONLY | O_CREAT | O_BINARY,
                           MYF(0))))
  {
    log_error("Error: can not create pid file '%s': %s (errno: %d)",
              (const char *) pid_file_name,
              (const char *) strerror(errno),
              (int) errno);
    return 1;
  }

  if (fprintf(pid_file, "%d\n", (int) pid) <= 0)
  {
    log_error("Error: can not write to pid file '%s': %s (errno: %d)",
              (const char *) pid_file_name,
              (const char *) strerror(errno),
              (int) errno);
    return 1;
  }

  my_fclose(pid_file, MYF(0));

  return 0;
}

#ifndef __WIN__
void set_signals(sigset_t *mask)
{
  /* block signals */
  sigemptyset(mask);
  sigaddset(mask, SIGINT);
  sigaddset(mask, SIGTERM);
  sigaddset(mask, SIGPIPE);
  sigaddset(mask, SIGHUP);
  signal(SIGPIPE, SIG_IGN);

  /*
    We want this signal to be blocked in all theads but the signal
    one. It is needed for the thr_alarm subsystem to work.
  */
  sigaddset(mask,THR_SERVER_ALARM);

  /* all new threads will inherite this signal mask */
  pthread_sigmask(SIG_BLOCK, mask, NULL);

  /*
     In our case the signal thread also implements functions of alarm thread.
     Here we init alarm thread functionality. We suppose that we won't have
     more then 10 alarms at the same time.
  */
  init_thr_alarm(10);
}
#else

bool have_signal;

void onsignal(int signo)
{
  have_signal= TRUE;
}

void set_signals(sigset_t *set)
{
  signal(SIGINT, onsignal);
  signal(SIGTERM, onsignal);
  have_signal= FALSE;
}

int my_sigwait(const sigset_t *set, int *sig)
{
  while (!have_signal)
  {
    Sleep(100);
  }
  return 0;
}

#endif


/*
  manager - entry point to the main instance manager process: start
  listener thread, write pid file and enter into signal handling.
  See also comments in mysqlmanager.cc to picture general Instance Manager
  architecture.

  TODO: how about returning error status.
*/

void manager()
{
  int err_code;
  const char *err_msg;
  bool shutdown_complete= FALSE;

  Thread_registry thread_registry;
  /*
    All objects created in the manager() function live as long as
    thread_registry lives, and thread_registry is alive until there are
    working threads.
  */

  User_map user_map;
  Instance_map instance_map(Options::Main::default_mysqld_path);
  Guardian_thread guardian_thread(thread_registry,
                                  &instance_map,
                                  Options::Main::monitoring_interval);

  Listener_thread_args listener_args(thread_registry, user_map, instance_map);

  manager_pid= getpid();
  instance_map.guardian= &guardian_thread;

  /* Initialize instance map. */

  if (instance_map.init())
  {
    log_error("Error: can not initialize instance list: out of memory.");
    return;
  }

  /* Initialize user map and load password file. */

  if (user_map.init())
  {
    log_error("Error: can not initialize user list: out of memory.");
    return;
  }

  if ((err_code= user_map.load(Options::Main::password_file_name, &err_msg)))
  {
    if (err_code == ERR_PASSWORD_FILE_DOES_NOT_EXIST &&
        Options::Main::mysqld_safe_compatible)
    {
      /*
        The password file does not exist, but we are running in
        mysqld_safe-compatible mode. Continue, but complain in log.
      */

      log_error("Warning: password file does not exist, "
                "nobody will be able to connect to Instance Manager.");
    }
    else
    {
      log_error("Error: %s.", (const char *) err_msg);
      return;
    }
  }

  /* write Instance Manager pid file */

  log_info("IM pid file: '%s'; PID: %d.",
           (const char *) Options::Main::pid_file_name,
           (int) manager_pid);

  if (create_pid_file(Options::Main::pid_file_name, manager_pid))
    return; /* necessary logging has been already done. */

  /*
    Initialize signals and alarm-infrastructure.

    NOTE: To work nicely with LinuxThreads, the signal thread is the first
    thread in the process.

    NOTE:
      After init_thr_alarm() call it's possible to call thr_alarm() (from
      different threads), that results in sending ALARM signal to the alarm
      thread (which can be the main thread). That signal can interrupt
      blocking calls.

      In other words, a blocking call can be interrupted in the main thread
      after init_thr_alarm().
  */

  sigset_t mask;
  set_signals(&mask);

  /* create guardian thread */
  {
    pthread_t guardian_thd_id;
    pthread_attr_t guardian_thd_attr;
    int rc;

    /*
      NOTE: Guardian should be shutdown first. Only then all other threads
      need to be stopped. This should be done, as guardian is responsible
      for shutting down the instances, and this is a long operation.

      NOTE: Guardian uses thr_alarm() when detects current state of
      instances (is_running()), but it is not interfere with
      flush_instances() later in the code, because until flush_instances()
      complete in the main thread, Guardian thread is not permitted to
      process instances. And before flush_instances() there is no instances
      to proceed.
    */

    pthread_attr_init(&guardian_thd_attr);
    pthread_attr_setdetachstate(&guardian_thd_attr, PTHREAD_CREATE_DETACHED);
    rc= set_stacksize_n_create_thread(&guardian_thd_id, &guardian_thd_attr,
                                      guardian, &guardian_thread);
    pthread_attr_destroy(&guardian_thd_attr);
    if (rc)
    {
      log_error("manager(): set_stacksize_n_create_thread(guardian) failed");
      goto err;
    }

  }

  /* Load instances. */


  {
    instance_map.guardian->lock();
    instance_map.lock();

    int flush_instances_status= instance_map.flush_instances();

    instance_map.unlock();
    instance_map.guardian->unlock();

    if (flush_instances_status)
    {
      log_error("Cannot init instances repository. This might be caused by "
        "the wrong config file options. For instance, missing mysqld "
        "binary. Aborting.");
      return;
    }
  }

  /* create the listener */
  {
    pthread_t listener_thd_id;
    pthread_attr_t listener_thd_attr;
    int rc;

    pthread_attr_init(&listener_thd_attr);
    pthread_attr_setdetachstate(&listener_thd_attr, PTHREAD_CREATE_DETACHED);
    rc= set_stacksize_n_create_thread(&listener_thd_id, &listener_thd_attr,
                                      listener, &listener_args);
    pthread_attr_destroy(&listener_thd_attr);
    if (rc)
    {
      log_error("manager(): set_stacksize_n_create_thread(listener) failed");
      goto err;
    }
  }

  /*
    After the list of guarded instances have been initialized,
    Guardian should start them.
  */
  pthread_cond_signal(&guardian_thread.COND_guardian);

  while (!shutdown_complete)
  {
    int signo;
    int status= 0;

    if ((status= my_sigwait(&mask, &signo)) != 0)
    {
      log_error("sigwait() failed");
      goto err;
    }

#ifndef __WIN__
/*
  On some Darwin kernels SIGHUP is delivered along with most
  signals. This is why we skip it's processing on these
  platforms. For more details and test program see
  Bug #14164 IM tests fail on MacOS X (powermacg5)
*/
#ifdef IGNORE_SIGHUP_SIGQUIT
    if ( SIGHUP == signo )
      continue;
#endif
    if (THR_SERVER_ALARM == signo)
      process_alarm(signo);
    else
#endif
    {
      if (!guardian_thread.is_stopped())
      {
        bool stop_instances= TRUE;
        guardian_thread.request_shutdown(stop_instances);
        pthread_cond_signal(&guardian_thread.COND_guardian);
      }
      else
      {
        thread_registry.deliver_shutdown();
        shutdown_complete= TRUE;
      }
    }
  }

err:
  /* delete the pid file */
  my_delete(Options::Main::pid_file_name, MYF(0));

#ifndef __WIN__
  /* free alarm structures */
  end_thr_alarm(1);
  /* don't pthread_exit to kill all threads who did not shut down in time */
#endif
}
