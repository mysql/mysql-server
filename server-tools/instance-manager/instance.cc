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
  pid_t pid;

  if (!is_running())
  {
    log_info("trying to start instance %s", options.instance_name);
    switch (pid= fork()) {
    case 0:
       if (fork()) /* zombie protection */
         exit(0); /* parent goes bye-bye */
       else
       {
         execv(options.mysqld_path, options.argv);
         exit(1);
       }
    case -1:
      return ER_CANNOT_START_INSTANCE;
    default:
      waitpid(pid, NULL, 0);
      return 0;
    }
  }

  /* the instance is started already */
  return ER_INSTANCE_ALREADY_STARTED;
}

int Instance::cleanup()
{
  /*
    We cannot close connection in destructor, as mysql_close needs alarm
    services which are definitely unavailaible at the time of destructor
    call.
  */
  if (is_connected)
    mysql_close(&mysql);
  return 0;
}


Instance::~Instance()
{
  pthread_mutex_destroy(&LOCK_instance);
}


bool Instance::is_running()
{
  uint port;
  const char *socket;

  if (options.mysqld_port)
    port= atoi(strchr(options.mysqld_port, '=') + 1);

  if (options.mysqld_socket)
    socket= strchr(options.mysqld_socket, '=') + 1;

  pthread_mutex_lock(&LOCK_instance);
  if (!is_connected)
  {
    mysql_init(&mysql);
    if (mysql_real_connect(&mysql, LOCAL_HOST, options.mysqld_user,
                           options.mysqld_password,
                           NullS, port,
                           socket, 0))
    {
      is_connected= TRUE;
      pthread_mutex_unlock(&LOCK_instance);
      return TRUE;
    }
    mysql_close(&mysql);
    pthread_mutex_unlock(&LOCK_instance);
    return FALSE;
  }
  else if (!mysql_ping(&mysql))
  {
    pthread_mutex_unlock(&LOCK_instance);
    return TRUE;
  }
  pthread_mutex_unlock(&LOCK_instance);
  return FALSE;
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
  if (is_running())
  {
    if (mysql_shutdown(&mysql, SHUTDOWN_DEFAULT))
      goto err;

    mysql_close(&mysql);
    is_connected= FALSE;
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
