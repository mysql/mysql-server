/* Copyright (c) 2014, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "plugin/group_replication/include/handlers/applier_handler.h"

#include <stddef.h>

#include "my_dbug.h"
#include "plugin/group_replication/include/plugin.h"
#include "plugin/group_replication/include/plugin_log.h"

Applier_handler::Applier_handler() {}

int Applier_handler::initialize()
{
  DBUG_ENTER("Applier_handler::initialize");
  DBUG_RETURN(0);
}

int Applier_handler::terminate()
{
  DBUG_ENTER("Applier_handler::terminate");
  DBUG_RETURN(0);
}

int
Applier_handler::initialize_repositories(bool reset_logs,
                                         ulong plugin_shutdown_timeout)
{
  DBUG_ENTER("Applier_handler::initialize_repositories");

  int error=0;

  if (reset_logs)
  {
    log_message(MY_INFORMATION_LEVEL,
                "Detected previous RESET MASTER invocation or an issue exists "
                "in the group replication applier relay log. "
                "Purging existing applier logs.");

    if ((error = channel_interface.purge_logs(true)))
    {
      /* purecov: begin inspected */
      log_message(MY_ERROR_LEVEL,
                 "Unknown error occurred while resetting applier's module logs");
      DBUG_RETURN(error);
      /* purecov: end */
    }
  }

  channel_interface.set_stop_wait_timeout(plugin_shutdown_timeout);

  error= channel_interface.initialize_channel(const_cast<char*>("<NULL>"),
                                              0, NULL, NULL,
                                              false,
                                              NULL,
                                              NULL,
                                              NULL,
                                              NULL,
                                              NULL,
                                              NULL,
                                              NULL,
                                              false,
                                              GROUP_REPLICATION_APPLIER_THREAD_PRIORITY,
                                              0, true, NULL, false);

  if (error)
  {
    log_message(MY_ERROR_LEVEL,
                "Failed to setup the group replication applier thread."); /* purecov: inspected */
  }

  DBUG_RETURN(error);
}

int Applier_handler::start_applier_thread()
{
  DBUG_ENTER("Applier_handler::start_applier_thread");

  int error= channel_interface.start_threads(false, true,
                                             NULL, false);
  if (error)
  {
      log_message(MY_ERROR_LEVEL,
                  "Error while starting the group replication applier thread");
  }

  DBUG_RETURN(error);
}

int Applier_handler::stop_applier_thread()
{
  DBUG_ENTER("Applier_handler::stop_applier_thread");

  int error= 0;

  if (!channel_interface.is_applier_thread_running())
    DBUG_RETURN(0);

  if ((error= channel_interface.stop_threads(false, true)))
  {
      log_message(MY_ERROR_LEVEL,
                  "Failed to stop the group replication applier thread."); /* purecov: inspected */
  }

  DBUG_RETURN(error);
}

int Applier_handler::handle_event(Pipeline_event *event,Continuation *cont)
{
  DBUG_ENTER("Applier_handler::handle_event");
  int error= 0;

  Data_packet* p=  NULL;
  error= event->get_Packet(&p);
  DBUG_EXECUTE_IF("applier_handler_force_error_on_pipeline", error= 1;);
  if (error || (p == NULL))
  {
    log_message(MY_ERROR_LEVEL,
                "Failed to fetch transaction data containing required"
                " transaction info for applier");
    error= 1;
    goto end;
  }

  /*
    There is no need to queue Transaction_context_log_event to
    server applier, this event is only need for certification,
    performed on the previous handler.
  */
  if (event->get_event_type() != binary_log::TRANSACTION_CONTEXT_EVENT)
  {
    error= channel_interface.queue_packet((const char*)p->payload, p->len);

    if (event->get_event_type() == binary_log::GTID_LOG_EVENT &&
        local_member_info->get_recovery_status() == Group_member_info::MEMBER_ONLINE)
    {
      applier_module->get_pipeline_stats_member_collector()
          ->increment_transactions_waiting_apply();
    }
  }

end:
  if (error)
    cont->signal(error);
  else
    next(event,cont);

  DBUG_RETURN(error);
}

int Applier_handler::handle_action(Pipeline_action *action)
{
  DBUG_ENTER("Applier_handler::handle_action");
  int error= 0;

  Plugin_handler_action action_type=
    (Plugin_handler_action)action->get_action_type();

  switch(action_type)
  {
    case HANDLER_START_ACTION:
      error= start_applier_thread();
      break;
    case HANDLER_STOP_ACTION:
      error= stop_applier_thread();
      break;
    case HANDLER_APPLIER_CONF_ACTION:
    {
      Handler_applier_configuration_action* conf_action=
              (Handler_applier_configuration_action*) action;

      if (conf_action->is_initialization_conf())
      {
        channel_interface.set_channel_name(conf_action->get_applier_name());
        error= initialize_repositories(conf_action->is_reset_logs_planned(),
                                       conf_action->get_applier_shutdown_timeout());
      }
      else
      {
        ulong timeout= conf_action->get_applier_shutdown_timeout();
        channel_interface.set_stop_wait_timeout(timeout);
      }
      break;
    }
    default:
      break;
  }

  if (error)
    DBUG_RETURN(error);

  DBUG_RETURN(next(action));
}

bool Applier_handler::is_unique(){
  return true;
}

int Applier_handler::get_role()
{
  return APPLIER;
}

bool Applier_handler::is_applier_thread_waiting()
{
  DBUG_ENTER("Applier_handler::is_applier_thread_waiting");

  bool result= channel_interface.is_applier_thread_waiting();

  DBUG_RETURN(result);
}

int Applier_handler::wait_for_gtid_execution(double timeout)
{
  DBUG_ENTER("Applier_handler::wait_for_gtid_execution");

  int error= channel_interface.wait_for_gtid_execution(timeout);

  DBUG_RETURN(error);
}

int Applier_handler::is_partial_transaction_on_relay_log()
{
  return channel_interface.is_partial_transaction_on_relay_log();
}
