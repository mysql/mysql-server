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
#include <my_sys.h>
#include <signal.h>
#include <m_string.h>
#include <sys/wait.h>

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
  /* echk for the pidfile and remove it */
  if (!is_running())
  {
    stop();
    log_info("starting instance %s", options.instance_name);
    switch (pid= fork()) {
    case 0:
      execv(options.mysqld_path, options.argv);
      exit(1);
    case -1:
      return ER_CANNOT_START_INSTANCE;
    default:
      return 0;
    }
  }

  /* the instance is started already */
  return ER_INSTANCE_ALREADY_STARTED;
}


int Instance::cleanup()
{
  return 0;
}


Instance::~Instance()
{
  pthread_mutex_destroy(&LOCK_instance);
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
    port= atoi(strchr(options.mysqld_port, '=') + 1);

  if (options.mysqld_socket)
    socket= strchr(options.mysqld_socket, '=') + 1;

  pthread_mutex_lock(&LOCK_instance);

  mysql_init(&mysql);
  /* try to connect to a server with the fake username/password pair */
  if (mysql_real_connect(&mysql, LOCAL_HOST, username,
                         password,
                         NullS, port,
                         socket, 0))
  {
    /*
      Very strange. We have successfully connected to the server using
      bullshit as username/password. Write a warning to the logfile.
    */
    log_info("The Instance Manager was able to log into you server \
             with faked compiled-in password while checking server status. \
             Looks like something is wrong.");
    mysql_close(&mysql);
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
  time_t waitchild= 35;                         /*  */

  if ((pid= options.get_pid()) != 0)            /* get pid from pidfile */
  {
    /*
      If we cannot kill mysqld, then it has propably crashed.
      Let us try to remove staled pidfile and return succes as mysqld
      is stopped
    */
    if (kill(pid, SIGTERM))
    {
      if (options.unlink_pidfile())
        log_error("cannot remove pidfile for instance %i, this might be \
                  since IM lacks permmissions or hasn't found the pidifle",
                  options.instance_name);

      log_error("The instance %s has probably crashed or IM lacks permissions \
                to kill it. in either case something seems to be wrong. \
                Check your setup", options.instance_name);
      return 0;
    }

    /* sleep on condition to wait for SIGCHLD */

    timeout.tv_sec= time(NULL) + waitchild;
    timeout.tv_nsec= 0;
    if (pthread_mutex_lock(&instance_map->pid_cond.LOCK_pid))
      goto err; /* perhaps this should be procecced differently */

    while (options.get_pid() != 0)
    {
      int status;

      status= pthread_cond_timedwait(&instance_map->pid_cond.COND_pid,
                                     &instance_map->pid_cond.LOCK_pid,
                                     &timeout);
      if (status == ETIMEDOUT)
        break;
    }

    pthread_mutex_unlock(&instance_map->pid_cond.LOCK_pid);

    if (!kill(pid, SIGKILL))
    {
      log_error("The instance %s has been stopped forsibly. Normally \
                it should not happed. Probably the instance has been \
                hanging. You should also check your IM setup",
                options.instance_name);
    }

    return 0;
  }

  return ER_INSTANCE_IS_NOT_STARTED;
err:
  return ER_STOP_INSTANCE;
}


/*
  We execute this function to initialize instance parameters.
  Return value: 0 - ok. 1 - unable to init DYNAMIC_ARRAY.
*/

int Instance::init(const char *name_arg)
{
  pthread_mutex_init(&LOCK_instance, 0);

  return options.init(name_arg);
}


int Instance::complete_initialization(Instance_map *instance_map_arg)
{
  instance_map= instance_map_arg;
  return 0;
}
