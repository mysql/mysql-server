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


/**********************************************************************
  Implementation of checking the actual thread model.
***********************************************************************/

namespace { /* no-indent */

class ThreadModelChecker: public Thread
{
public:
  ThreadModelChecker()
    :main_pid(getpid())
  { }

public:
  inline bool is_linux_threads() const
  {
    return linux_threads;
  }

protected:
  virtual void run()
  {
    linux_threads= main_pid != getpid();
  }

private:
  pid_t main_pid;
  bool linux_threads;
};

bool check_if_linux_threads(bool *linux_threads)
{
  ThreadModelChecker checker;

  if (checker.start() || checker.join())
    return TRUE;

  *linux_threads= checker.is_linux_threads();

  return FALSE;
}

}


/**********************************************************************
  Manager implementation
***********************************************************************/

Guardian *Manager::p_guardian;
Instance_map *Manager::p_instance_map;
Thread_registry *Manager::p_thread_registry;
User_map *Manager::p_user_map;

#ifndef __WIN__
bool Manager::linux_threads;
#endif // __WIN__


void Manager::stop_all_threads()
{
  /*
    Let guardian thread know that it should break it's processing cycle,
    once it wakes up.
  */
  p_guardian->request_shutdown();
  /* wake guardian */
  pthread_cond_signal(&p_guardian->COND_guardian);
  /* stop all threads */
  p_thread_registry->deliver_shutdown();
}


/*
  manager - entry point to the main instance manager process: start
  listener thread, write pid file and enter into signal handling.
  See also comments in mysqlmanager.cc to picture general Instance Manager
  architecture.

  TODO: how about returning error status.
*/

int Manager::main()
{
  int err_code;
  int rc= 1;
  const char *err_msg;
  bool shutdown_complete= FALSE;
  pid_t manager_pid= getpid();

  if (check_if_linux_threads(&linux_threads))
  {
    log_error("Can not determine thread model.");
    return 1;
  }

  log_info("Detected threads model: %s.",
           (const char *) (linux_threads ? "LINUX threads" : "POSIX threads"));

  Thread_registry thread_registry;
  /*
    All objects created in the manager() function live as long as
    thread_registry lives, and thread_registry is alive until there are
    working threads.
  */

  User_map user_map;
  Instance_map instance_map;
  Guardian guardian(&thread_registry, &instance_map,
                    Options::Main::monitoring_interval);

  Listener listener(&thread_registry, &user_map);

  p_instance_map= &instance_map;
  p_guardian= instance_map.guardian= &guardian;
  p_thread_registry= &thread_registry;
  p_user_map= &user_map;

  /* Initialize instance map. */

  if (instance_map.init())
  {
    log_error("Can not initialize instance list: out of memory.");
    return 1;
  }

  /* Initialize user map and load password file. */

  if (user_map.init())
  {
    log_error("Can not initialize user list: out of memory.");
    return 1;
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

      log_info("Warning: password file does not exist, "
               "nobody will be able to connect to Instance Manager.");
    }
    else
    {
      log_error("%s.", (const char *) err_msg);
      return 1;
    }
  }

  /* write Instance Manager pid file */

  log_info("IM pid file: '%s'; PID: %d.",
           (const char *) Options::Main::pid_file_name,
           (int) manager_pid);

  if (create_pid_file(Options::Main::pid_file_name, manager_pid))
    return 1; /* necessary logging has been already done. */

  /*
    Initialize signals and alarm-infrastructure.

    NOTE: To work nicely with LinuxThreads, the signal thread is the first
    thread in the process.

    NOTE: After init_thr_alarm() call it's possible to call thr_alarm()
    (from different threads), that results in sending ALARM signal to the
    alarm thread (which can be the main thread). That signal can interrupt
    blocking calls. In other words, a blocking call can be interrupted in
    the main thread after init_thr_alarm().
  */

  sigset_t mask;
  set_signals(&mask);

  /*
    Create the guardian thread. The newly started thread will block until
    we actually load instances.

    NOTE: Guardian should be shutdown first. Only then all other threads
    can be stopped. This should be done in this order because the guardian
    is responsible for shutting down all the guarded instances, and this
    is a long operation.

    NOTE: Guardian uses thr_alarm() when detects the current state of an
    instance (is_running()), but this does not interfere with
    flush_instances() call later in the code, because until
    flush_instances() completes in the main thread, Guardian thread is not
    permitted to process instances. And before flush_instances() has
    completed, there are no instances to guard.
  */
  if (guardian.start(Thread::DETACHED))
  {
    log_error("Can not start Guardian thread.");
    goto err;
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
      log_error("Can not init instances repository.");
      stop_all_threads();
      goto err;
    }
  }

  /* Initialize the Listener. */

  if (listener.start(Thread::DETACHED))
  {
    log_error("Can not start Listener thread.");
    stop_all_threads();
    goto err;
  }

  /*
    After the list of guarded instances have been initialized,
    Guardian should start them.
  */
  pthread_cond_signal(&guardian.COND_guardian);

  /* Main loop. */

  log_info("Manager: started.");

  while (!shutdown_complete)
  {
    int signo;
    int status= 0;

    if ((status= my_sigwait(&mask, &signo)) != 0)
    {
      log_error("sigwait() failed");
      stop_all_threads();
      goto err;
    }

    /*
      The general idea in this loop is the following:
        - we are waiting for SIGINT, SIGTERM -- signals that mean we should
          shutdown;
        - as shutdown signal is caught, we stop Guardian thread (by calling
          Guardian::request_shutdown());
        - as Guardian is stopped, it sends SIGTERM to this thread
          (by calling Thread_registry::request_shutdown()), so that the
          my_sigwait() above returns;
        - as we catch the second SIGTERM, we send signals to all threads
          registered in Thread_registry (by calling
          Thread_registry::deliver_shutdown()) and waiting for threads to stop;
    */

#ifndef __WIN__
/*
  On some Darwin kernels SIGHUP is delivered along with most
  signals. This is why we skip it's processing on these
  platforms. For more details and test program see
  Bug #14164 IM tests fail on MacOS X (powermacg5)
*/
#ifdef IGNORE_SIGHUP_SIGQUIT
    if (SIGHUP == signo)
      continue;
#endif
    if (THR_SERVER_ALARM == signo)
      process_alarm(signo);
    else
#endif
    {
      log_info("Manager: got shutdown signal.");

      if (!guardian.is_stopped())
      {
        guardian.request_shutdown();
        pthread_cond_signal(&guardian.COND_guardian);
      }
      else
      {
        thread_registry.deliver_shutdown();
        shutdown_complete= TRUE;
      }
    }
  }

  log_info("Manager: finished.");

  rc= 0;

err:
  /* delete the pid file */
  my_delete(Options::Main::pid_file_name, MYF(0));

#ifndef __WIN__
  /* free alarm structures */
  end_thr_alarm(1);
  /* don't pthread_exit to kill all threads who did not shut down in time */
#endif
  return rc;
}
