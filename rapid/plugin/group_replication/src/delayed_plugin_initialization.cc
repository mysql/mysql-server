/* Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

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
#include "sql_service_gr_user.h"
#include <mysql/group_replication_priv.h>

using std::string;

static void *launch_handler_thread(void* arg)
{
  Delayed_initialization_thread *handler= (Delayed_initialization_thread*) arg;
  handler->initialization_thread_handler();
  return 0;
}

Delayed_initialization_thread::Delayed_initialization_thread()
  : thread_running(false), is_server_ready(false)
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

void Delayed_initialization_thread::wait_for_initialization()
{
  DBUG_ENTER("Delayed_initialization_thread::wait_for_initialization");

  mysql_mutex_lock(&run_lock);
  while (thread_running)
  {
    DBUG_PRINT("sleep",("Waiting for the Delayed initialization thread to end"));
    mysql_cond_wait(&run_cond, &run_lock);
  }
  mysql_mutex_unlock(&run_lock);

  //give extra time for the thread to terminate
  my_sleep(1);

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

  //delayed initialization code starts here

  int error= 0;
  Sql_service_command *sql_command_interface= NULL;

  //Just terminate it
  if (!(delay_gr_user_creation || wait_on_engine_initialization) ||
      get_plugin_pointer() == NULL)
  {
    goto end;
  }

  sql_command_interface= new Sql_service_command();
  if (sql_command_interface->
          establish_session_connection(true, get_plugin_pointer()))
  {
    /* purecov: begin inspected */
    log_message(MY_ERROR_LEVEL,
                "It was not possible to establish a connection to "
                "server SQL service");
    goto end;
    /* purecov: end */
  }

  if (delay_gr_user_creation)
  {
    if (create_group_replication_user(false,
                                      sql_command_interface->get_sql_service_interface()))
    {
      /* purecov: begin inspected */
      log_message(MY_ERROR_LEVEL,
                  "It was not possible to create the group replication user used"
                  "by the plugin for internal operations.");
      goto end;
      /* purecov: end */
    }

    delay_gr_user_creation= false;
  }

  /*
    The plugin was initialized on server start
    so only now we can start the applier
  */
  if (wait_on_engine_initialization)
  {
    DBUG_ASSERT(server_engine_initialized());
    wait_on_engine_initialization= false;

    if ((error= configure_group_communication(
                    sql_command_interface->get_sql_service_interface())))
      goto err; /* purecov: inspected */

    if ((error= configure_group_member_manager()))
      goto err; /* purecov: inspected */

    configure_compatibility_manager();

    if ((error= initialize_recovery_module()))
      goto err; /* purecov: inspected */

    if (configure_and_start_applier_module())
    {
      error= GROUP_REPLICATION_REPLICATION_APPLIER_INIT_ERROR;
      goto err;
    }

    /*
     At this point in the code, set the super_read_only mode here on the
     server to protect recovery and version module of the Group Replication.
    */

    if (read_mode_handler->set_super_read_only_mode(sql_command_interface))
    {
      error =1; /* purecov: inspected */
      log_message(MY_ERROR_LEVEL,
                  "Could not enable the server read only mode and guarantee a "
                  "safe recovery execution"); /* purecov: inspected */
      goto err; /* purecov: inspected */
    }

    if ((error= start_group_communication()))
    {
      //terminate the before created pipeline
      log_message(MY_ERROR_LEVEL,
                  "Error on group communication initialization methods, "
                  "killing the Group Replication applier"); /* purecov: inspected */
      applier_module->terminate_applier_thread(); /* purecov: inspected */
      goto err; /* purecov: inspected */
    }

    if (view_change_notifier->wait_for_view_modification())
    {
      /* purecov: begin inspected */
      if (!view_change_notifier->is_cancelled())
      {
        //Only log a error when a view modification was not canceled.
        log_message(MY_ERROR_LEVEL,
                    "Timeout on wait for view after joining group");
      }
      error= view_change_notifier->get_error();
      goto err;
      /* purecov: end */
    }
    declare_plugin_running(); //All is OK

  err:
    if (error)
    {
      leave_group();
      terminate_plugin_modules();
      if (certification_latch != NULL)
      {
        delete certification_latch; /* purecov: inspected */
        certification_latch= NULL;  /* purecov: inspected */
      }
    }
  }

end:

  delete sql_command_interface;

  mysql_mutex_lock(&run_lock);
  thread_running= false;
  mysql_cond_broadcast(&run_cond);
  mysql_mutex_unlock(&run_lock);

  DBUG_RETURN(error);
}
