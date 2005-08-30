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

#include <my_global.h>
#include "manager.h"

#include "priv.h"
#include "thread_registry.h"
#include "listener.h"
#include "instance_map.h"
#include "options.h"
#include "user_map.h"
#include "log.h"
#include "guardian.h"

#include <my_sys.h>
#include <m_string.h>
#include <signal.h>
#include <thr_alarm.h>
#ifndef __WIN__
#include <sys/wait.h>
#endif


static int create_pid_file(const char *pid_file_name)
{
  if (FILE *pid_file= my_fopen(pid_file_name,
                               O_WRONLY | O_CREAT | O_BINARY, MYF(0)))
  {
    fprintf(pid_file, "%d\n", (int) getpid());
    my_fclose(pid_file, MYF(0));
    return 0;
  }
  log_error("can't create pid file %s: errno=%d, %s",
            pid_file_name, errno, strerror(errno));
  return 1;
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
  have_signal= true;
}

void set_signals(sigset_t *set)
{
  signal(SIGINT, onsignal);
  signal(SIGTERM, onsignal);
  have_signal= false;
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
*/

void manager(const Options &options)
{
  Thread_registry thread_registry;
  /*
    All objects created in the manager() function live as long as
    thread_registry lives, and thread_registry is alive until there are
    working threads.
  */

  User_map user_map;
  Instance_map instance_map(options.default_mysqld_path);
  Guardian_thread guardian_thread(thread_registry,
                                  &instance_map,
                                  options.monitoring_interval);

  Listener_thread_args listener_args(thread_registry, options, user_map,
                                     instance_map);

  manager_pid= getpid();
  instance_map.guardian= &guardian_thread;

  if (instance_map.init() || user_map.init())
    return;


  if (instance_map.load())
  {
    log_error("Cannot init instances repository. This might be caused by "
               "the wrong config file options. For instance, missing mysqld "
               "binary. Aborting.");
    return;
  }

  if (user_map.load(options.password_file_name))
    return;

  /* write pid file */
  if (create_pid_file(options.pid_file_name))
    return;

  sigset_t mask;
  set_signals(&mask);

  /* create the listener */
  {
    pthread_t listener_thd_id;
    pthread_attr_t listener_thd_attr;
    int rc;

    pthread_attr_init(&listener_thd_attr);
    pthread_attr_setdetachstate(&listener_thd_attr, PTHREAD_CREATE_DETACHED);
    rc= pthread_create(&listener_thd_id, &listener_thd_attr, listener,
                       &listener_args);
    pthread_attr_destroy(&listener_thd_attr);
    if (rc)
    {
      log_error("manager(): pthread_create(listener) failed");
      goto err;
    }

  }

  /* create guardian thread */
  {
    pthread_t guardian_thd_id;
    pthread_attr_t guardian_thd_attr;
    int rc;

    /*
       NOTE: Guardian should be shutdown first. Only then all other threads
       need to be stopped. This should be done, as guardian is responsible for
       shutting down the instances, and this is a long operation.
    */

    pthread_attr_init(&guardian_thd_attr);
    pthread_attr_setdetachstate(&guardian_thd_attr, PTHREAD_CREATE_DETACHED);
    rc= pthread_create(&guardian_thd_id, &guardian_thd_attr, guardian,
                       &guardian_thread);
    pthread_attr_destroy(&guardian_thd_attr);
    if (rc)
    {
      log_error("manager(): pthread_create(guardian) failed");
      goto err;
    }

  }

  /*
    To work nicely with LinuxThreads, the signal thread is the first thread
    in the process.
  */
  int signo;
  bool shutdown_complete;

  shutdown_complete= FALSE;

  /* init list of guarded instances */
  guardian_thread.lock();

  guardian_thread.init();

  guardian_thread.unlock();

  /*
    After the list of guarded instances have been initialized,
    Guardian should start them.
  */
  pthread_cond_signal(&guardian_thread.COND_guardian);

  while (!shutdown_complete)
  {
    int status= 0;

    if ((status= my_sigwait(&mask, &signo)) != 0)
    {
      log_error("sigwait() failed");
      goto err;
    }

#ifndef __WIN__
    if (THR_SERVER_ALARM == signo)
      process_alarm(signo);
    else
#endif
    {
      if (!guardian_thread.is_stopped())
      {
        bool stop_instances= true;
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
  my_delete(options.pid_file_name, MYF(0));

#ifndef __WIN__
  /* free alarm structures */
  end_thr_alarm(1);
  /* don't pthread_exit to kill all threads who did not shut down in time */
#endif
}

