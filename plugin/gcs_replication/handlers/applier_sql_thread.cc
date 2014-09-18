/* Copyright (c) 2014, Oracle and/or its affiliates. All rights reserved.

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

#include "applier_sql_thread.h"

Applier_sql_thread::Applier_sql_thread()
  :sql_thread_interface()
{}

int Applier_sql_thread::initialize()
{
  DBUG_ENTER("Applier_sql_thread::initialize");
  DBUG_RETURN(0);
}

int Applier_sql_thread::terminate()
{
  DBUG_ENTER("Applier_sql_thread::terminate");

  int error= 0;

  if ((error= sql_thread_interface.stop_threads(true)))
  {
    if(error == REPLICATION_THREAD_STOP_ERROR)
    {
      log_message(MY_ERROR_LEVEL,
                  "Failed to stop the applier module SQL thread.");
    }
    if(error == REPLICATION_THREAD_STOP_RL_FLUSH_ERROR)
    {
      log_message(MY_ERROR_LEVEL,
                  "Error when flushing the applier module's relay log");
    }
  }
  else
  {
    sql_thread_interface.clean_thread_repositories();
  }

  DBUG_RETURN(error);
}

int
Applier_sql_thread::initialize_repositories(char *relay_log_name,
                                            char *relay_log_info_name,
                                            bool reset_logs,
                                            ulong plugin_shutdown_timeout)
{
  DBUG_ENTER("Applier_sql_thread::initialize_repositories");

  int error=
    sql_thread_interface.initialize_repositories(relay_log_name,
                                                 relay_log_info_name);

  if(error)
  {
    if (error == REPLICATION_THREAD_REPOSITORY_CREATION_ERROR)
    {
      log_message(MY_ERROR_LEVEL,
                  "Failed to setup the applier module metadata containers.");

    }
    if (error == REPLICATION_THREAD_MI_INIT_ERROR)
    {
      log_message(MY_ERROR_LEVEL,
                  "Failed to setup the applier's (mi) metadata container.");
    }
    if (error == REPLICATION_THREAD_RLI_INIT_ERROR)
    {
      log_message(MY_ERROR_LEVEL,
                  "Failed to setup the applier's (rli) metadata container.");
    }
    DBUG_RETURN(error);
  }
  sql_thread_interface.initialize_connection_parameters(NULL, 0, NULL, NULL,
                                                        NULL, -1);

  sql_thread_interface.set_stop_wait_timeout(plugin_shutdown_timeout);

  if (reset_logs)
  {
    log_message(MY_INFORMATION_LEVEL,
                "Detected previous RESET MASTER invocation."
                "Performing log purge on node.");

    if ((error = sql_thread_interface.purge_relay_logs()))
    {
      log_message(MY_ERROR_LEVEL,
              "Unknown error occurred while reseting applier's module logs");
      DBUG_RETURN(error);
    }
  }

  DBUG_RETURN(error);
}

int Applier_sql_thread::start_sql_thread()
{
  DBUG_ENTER("Applier_sql_thread::start_sql_thread");

  int error= 0;
  //Initialize only the SQL thread
  int thread_mask= SLAVE_SQL;
  DBUG_EXECUTE_IF("gcs_applier_do_not_start_sql_thread", thread_mask=0;);
  error= sql_thread_interface.start_replication_threads(thread_mask,
                                                        false);
  if (error)
  {
    if (error == REPLICATION_THREAD_START_ERROR)
    {
      log_message(MY_ERROR_LEVEL,
                  "Error on the Applier module SQL thread initialization");
    }
    if (error == REPLICATION_THREAD_START_NO_INFO_ERROR)
    {
        log_message(MY_ERROR_LEVEL,
                    "No information available when starting the Applier module "
                    "SQL thread");
    }
  }
  DBUG_RETURN(error);
}

int Applier_sql_thread::handle_event(PipelineEvent *event,Continuation *cont)
{
  DBUG_ENTER("Applier_sql_thread::handle_event");

  Data_packet* p=  NULL;
  event->get_Packet(&p);

  int error= sql_thread_interface.queue_packet((const char*)p->payload, p->len);

  if (error)
    cont->signal(error);
  else
    next(event,cont);

  DBUG_RETURN(error);
}

int Applier_sql_thread::handle_action(PipelineAction *action)
{
  DBUG_ENTER("Applier_sql_thread::handle_action");
  int error= 0;

  Plugin_handler_action action_type=
    (Plugin_handler_action)action->get_action_type();

  if(action_type ==  HANDLER_START_ACTION)
  {
    error= start_sql_thread();
    DBUG_RETURN(error);
  }
  else if (action_type == HANDLER_APPLIER_CONF_ACTION)
  {
    Handler_applier_configuration_action* conf_action=
      (Handler_applier_configuration_action*) action;

    if(conf_action->is_initialization_conf())
    {
      error =
        initialize_repositories(conf_action->get_applier_relay_log_name(),
                                conf_action->get_applier_relay_log_info_name(),
                                conf_action->is_reset_logs_planned(),
                                conf_action->get_applier_shutdown_timeout());

      rpl_gno last_delivered_gno=
        sql_thread_interface.get_last_delivered_gno(conf_action->get_sidno());
      conf_action->set_last_queued_gno(last_delivered_gno);
    }
    else
    {
      ulong timeout= conf_action->get_applier_shutdown_timeout();
      sql_thread_interface.set_stop_wait_timeout(timeout);
    }
  }

  DBUG_RETURN(next(action));
}

bool Applier_sql_thread::is_unique(){
  return true;
}

int Applier_sql_thread::get_role()
{
  return APPLIER;
}

int Applier_sql_thread::wait_for_gtid_execution(longlong timeout)
{
  DBUG_ENTER("Applier_sql_thread::wait_for_gtid_execution");

  int error= sql_thread_interface.wait_for_gtid_execution(timeout);

  DBUG_RETURN(error);
}

bool Applier_sql_thread::is_own_event_channel(my_thread_id id)
{
  DBUG_ENTER("Applier_sql_thread::is_own_event_channel");
  DBUG_RETURN(sql_thread_interface.is_own_event_channel(id));
}
