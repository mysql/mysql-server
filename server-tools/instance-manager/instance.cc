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

#ifdef __GNUC__
#pragma implementation
#endif

#include "instance.h"

#include "mysql_manager_error.h"
#include "log.h"
#include "instance_map.h"
#include "priv.h"

#include <sys/wait.h>
#include <my_sys.h>
#include <signal.h>
#include <m_string.h>
#include <mysql.h>

C_MODE_START

/*
  Proxy thread is a simple way to avoid all pitfalls of the threads
  implementation in the OS (e.g. LinuxThreads). With such a thread we
  don't have to process SIGCHLD, which is a tricky business if we want
  to do it in a portable way.
*/

pthread_handler_decl(proxy, arg)
{
  Instance *instance= (Instance *) arg;
  instance->fork_and_monitor();
  return 0;
}

C_MODE_END


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
  pid_t pid;

  /* clear crash flag */
  pthread_mutex_lock(&LOCK_instance);
  crashed= 0;
  pthread_mutex_unlock(&LOCK_instance);


  if (!is_running())
  {
    if ((pid= options.get_pid()) != 0)          /* check the pidfile */
      if (options.unlink_pidfile())             /* remove stalled pidfile */
        log_error("cannot remove pidfile for instance %i, this might be \
                  since IM lacks permmissions or hasn't found the pidifle",
                  options.instance_name);

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

  /* the instance is started already */
  return ER_INSTANCE_ALREADY_STARTED;
}


void Instance::fork_and_monitor()
{
  pid_t pid;
  log_info("starting instance %s", options.instance_name);
  switch (pid= fork()) {
  case 0:
    execv(options.mysqld_path, options.argv);
    /* exec never returns */
    exit(1);
  case -1:
    log_info("cannot fork() to start instance %s", options.instance_name);
    return;
  default:
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
      waitpid(pid, NULL, 0);
    /* set instance state to crashed */
    pthread_mutex_lock(&LOCK_instance);
    crashed= 1;
    pthread_mutex_unlock(&LOCK_instance);

    /*
      Wake connection threads waiting for an instance to stop. This
      is needed if a user issued command to stop an instance via
      mysql connection. This is not the case if Guardian stop the thread.
    */
    pthread_cond_signal(&COND_instance_stopped);
    /* wake guardian */
    pthread_cond_signal(&instance_map->guardian->COND_guardian);
    /* thread exits */
    return;
  }
  /* we should never end up here */
  DBUG_ASSERT(0);
}


Instance::Instance(): crashed(0)
{
  pthread_mutex_init(&LOCK_instance, 0);
  pthread_cond_init(&COND_instance_stopped, 0);
}


Instance::~Instance()
{
  pthread_cond_destroy(&COND_instance_stopped);
  pthread_mutex_destroy(&LOCK_instance);
}


int Instance::is_crashed()
{
  int val;
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
  const char *password= "321rarepassword213";
  const char *username= "645rareusername945";
  const char *access_denied_message= "Access denied for user";
  bool return_val;

  if (options.mysqld_port)
    port= options.mysqld_port_val;

  if (options.mysqld_socket)
    socket= strchr(options.mysqld_socket, '=') + 1;

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
  {
    if (!strncmp(access_denied_message, mysql_error(&mysql),
                 sizeof(access_denied_message)-1))
    {
      return_val= TRUE;
    }
    else
      return_val= FALSE;
  }

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
  pid_t pid;
  struct timespec timeout;
  uint waitchild= (uint)  DEFAULT_SHUTDOWN_DELAY;

  if (options.shutdown_delay_val)
    waitchild= options.shutdown_delay_val;

  kill_instance(SIGTERM);
  /* sleep on condition to wait for SIGCHLD */

  timeout.tv_sec= time(NULL) + waitchild;
  timeout.tv_nsec= 0;
  if (pthread_mutex_lock(&LOCK_instance))
    goto err;

  while (options.get_pid() != 0)              /* while server isn't stopped */
  {
    int status;

    status= pthread_cond_timedwait(&COND_instance_stopped,
                                   &LOCK_instance,
                                   &timeout);
    if (status == ETIMEDOUT)
      break;
  }

  pthread_mutex_unlock(&LOCK_instance);

  kill_instance(SIGKILL);

  return 0;

  return ER_INSTANCE_IS_NOT_STARTED;
err:
  return ER_STOP_INSTANCE;
}


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
    else
      if (signum == SIGKILL)      /* really killed instance with SIGKILL */
        log_error("The instance %s is being stopped forsibly. Normally \
                  it should not happed. Probably the instance has been \
                  hanging. You should also check your IM setup",
                  options.instance_name);
  }
  return;
}

/*
  We execute this function to initialize instance parameters.
  Return value: 0 - ok. 1 - unable to init DYNAMIC_ARRAY.
*/

int Instance::init(const char *name_arg)
{
  return options.init(name_arg);
}


int Instance::complete_initialization(Instance_map *instance_map_arg,
                                      const char *mysqld_path,
                                      int only_instance)
{
  instance_map= instance_map_arg;
  return options.complete_initialization(mysqld_path, only_instance);
}
