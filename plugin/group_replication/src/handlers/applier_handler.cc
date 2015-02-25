/* Copyright (c) 2014, 2015, Oracle and/or its affiliates. All rights reserved.

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

#include "handlers/applier_handler.h"
#include "plugin_log.h"

Applier_handler::Applier_handler()
{}

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
                "Detected previous RESET MASTER invocation."
                "Purging existing logs.");

    if ((error = channel_interface.purge_logs()))
    {
      log_message(MY_ERROR_LEVEL,
                 "Unknown error occurred while resetting applier's module logs");
      DBUG_RETURN(error);
    }
  }

  channel_interface.set_stop_wait_timeout(plugin_shutdown_timeout);

  error= channel_interface.initialize_channel(const_cast<char*>("<NULL>"),
                                              0, NULL, NULL,
                                              GCS_APPLIER_THREAD_PRIORITY,
                                              false, 0, true);

  if (error)
  {
    log_message(MY_ERROR_LEVEL,
                "Failed to setup the group replication applier thread.");
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
                  "Failed to stop the group replication applier thread.");
  }

  DBUG_RETURN(error);
}

int Applier_handler::handle_event(Pipeline_event *event,Continuation *cont)
{
  DBUG_ENTER("Applier_handler::handle_event");

  Data_packet* p=  NULL;
  event->get_Packet(&p);

  int error= channel_interface.queue_packet((const char*)p->payload, p->len);

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

        rpl_gno last_delivered_gno=
            channel_interface.get_last_delivered_gno(conf_action->get_sidno());
        conf_action->set_last_queued_gno(last_delivered_gno);
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

int Applier_handler::wait_for_gtid_execution(longlong timeout)
{
  DBUG_ENTER("Applier_handler::wait_for_gtid_execution");

  int error= channel_interface.wait_for_gtid_execution(timeout);

  DBUG_RETURN(error);
}

bool Applier_handler::is_own_event_applier(my_thread_id id)
{
  DBUG_ENTER("Applier_handler::is_own_event_applier");
  DBUG_RETURN(channel_interface.is_own_event_applier(id));
}
