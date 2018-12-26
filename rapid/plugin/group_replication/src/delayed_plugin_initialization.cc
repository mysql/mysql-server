/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#include "delayed_plugin_initialization.h"
#include "plugin.h"
#include "plugin_psi.h"

#include <mysql/group_replication_priv.h>

using std::string;

static void *launch_handler_thread(void* arg)
{
  Delayed_initialization_thread *handler= (Delayed_initialization_thread*) arg;
  handler->initialization_thread_handler();
  return 0;
}

Delayed_initialization_thread::Delayed_initialization_thread()
  : thread_running(false), is_server_ready(false), is_super_read_only_set(false)
{
  mysql_mutex_init(key_GR_LOCK_delayed_init_run, &run_lock, MY_MUTEX_INIT_FAST);
  mysql_mutex_init(key_GR_LOCK_delayed_init_server_ready,
                   &server_ready_lock,
                   MY_MUTEX_INIT_FAST);

  mysql_cond_init(key_GR_COND_delayed_init_run, &run_cond);
  mysql_cond_init(key_GR_COND_delayed_init_server_ready, &server_ready_cond);

}

Delayed_initialization_thread::~Delayed_initialization_thread()
{
  mysql_mutex_destroy(&run_lock);
  mysql_cond_destroy(&run_cond);
  mysql_mutex_destroy(&server_ready_lock);
  mysql_cond_destroy(&server_ready_cond);
}

void Delayed_initialization_thread::signal_thread_ready()
{
  DBUG_ENTER("Delayed_initialization_thread::signal_thread_ready");

  mysql_mutex_lock(&server_ready_lock);
  is_server_ready= true;
  mysql_cond_broadcast(&server_ready_cond);
  mysql_mutex_unlock(&server_ready_lock);

  DBUG_VOID_RETURN;
}

void Delayed_initialization_thread::wait_for_thread_end()
{
  DBUG_ENTER("Delayed_initialization_thread::wait_for_thread_end");

  mysql_mutex_lock(&run_lock);
  while (thread_running)
  {
    DBUG_PRINT("sleep",("Waiting for the Delayed initialization thread to finish"));
    mysql_cond_wait(&run_cond, &run_lock);
  }
  mysql_mutex_unlock(&run_lock);

  //give extra time for the thread to terminate
  my_sleep(1);

  DBUG_VOID_RETURN;
}

void Delayed_initialization_thread::signal_read_mode_ready()
{
  DBUG_ENTER("Delayed_initialization_thread::signal_read_mode_ready");

  mysql_mutex_lock(&run_lock);
  is_super_read_only_set= true;
  mysql_cond_broadcast(&run_cond);
  mysql_mutex_unlock(&run_lock);

  DBUG_VOID_RETURN;
}

void Delayed_initialization_thread::wait_for_read_mode()
{
  DBUG_ENTER("Delayed_initialization_thread::wait_for_read_mode");

  mysql_mutex_lock(&run_lock);
  while (!is_super_read_only_set)
  {
    DBUG_PRINT("sleep",("Waiting for the Delayed initialization thread to set super_read_only"));
    mysql_cond_wait(&run_cond, &run_lock);
  }
  mysql_mutex_unlock(&run_lock);

  DBUG_VOID_RETURN;
}

int Delayed_initialization_thread::launch_initialization_thread()
{
  DBUG_ENTER("Delayed_initialization_thread::launch_initialization_thread");

  mysql_mutex_lock(&run_lock);

  if(thread_running)
  {
    mysql_mutex_unlock(&run_lock); /* purecov: inspected */
    DBUG_RETURN(0);                /* purecov: inspected */
  }

  if (mysql_thread_create(key_GR_THD_delayed_init,
                          &delayed_init_pthd,
                          get_connection_attrib(),
                          launch_handler_thread,
                          (void*)this))
  {
    mysql_mutex_unlock(&run_lock); /* purecov: inspected */
    DBUG_RETURN(1); /* purecov: inspected */
  }

  while (!thread_running)
  {
    DBUG_PRINT("sleep",("Waiting for the Delayed initialization thread to start"));
    mysql_cond_wait(&run_cond, &run_lock);
  }
  mysql_mutex_unlock(&run_lock);

  DBUG_RETURN(0);
}

int Delayed_initialization_thread::initialization_thread_handler()
{
  DBUG_ENTER("initialize_thread_handler");
  int error= 0;

  mysql_mutex_lock(&run_lock);
  thread_running= true;
  mysql_cond_broadcast(&run_cond);
  mysql_mutex_unlock(&run_lock);

  mysql_mutex_lock(&server_ready_lock);
  while(!is_server_ready)
  {
    DBUG_PRINT("sleep",("Waiting for server start signal"));
    mysql_cond_wait(&server_ready_cond, &server_ready_lock);
  }
  mysql_mutex_unlock(&server_ready_lock);

  if (server_engine_initialized())
  {
    //Protect this delayed start against other start/stop requests
    Mutex_autolock auto_lock_mutex(get_plugin_running_lock());

    error= initialize_plugin_and_join(PSESSION_INIT_THREAD, this);
  }
  else
  {
    error= 1;
    log_message(MY_ERROR_LEVEL,
                "Unable to start Group Replication. Replication applier "
                "infrastructure is not initialized since the server was "
                "started with --initialize or --initialize-insecure.");
  }

  mysql_mutex_lock(&run_lock);
  thread_running= false;
  mysql_cond_broadcast(&run_cond);
  mysql_mutex_unlock(&run_lock);

  DBUG_RETURN(error);
}
