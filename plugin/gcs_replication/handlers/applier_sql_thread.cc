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

  int error= sql_thread_interface.initialize_repositories(applier_relay_log_name,
                                                          applier_relay_log_info_name);

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

  //Initialize only the SQL thread
  int thread_mask= SLAVE_SQL;
  DBUG_EXECUTE_IF("gcs_applier_do_not_start_sql_thread", thread_mask=0;);
  error= sql_thread_interface.start_replication_threads(thread_mask, true,
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

int Applier_sql_thread::handle(PipelineEvent *event,Continuation* cont)
{
  DBUG_ENTER("Applier_sql_thread::handle");

  Data_packet* p=  NULL;
  event->get_Packet(&p);

  int error= sql_thread_interface.queue_packet((const char*)p->payload, p->len);

  if (error)
    cont->signal(error);
  else
    next(event,cont);

  DBUG_RETURN(error);
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
  DBUG_ENTER("Applier_sql_thread::wait_on_event_consuption");

  int error= sql_thread_interface.wait_for_gtid_execution(timeout);

  DBUG_RETURN(error);
}

bool Applier_sql_thread::is_own_event_channel(my_thread_id id)
{
  DBUG_ENTER("Applier_sql_thread::is_own_event_channel");
  DBUG_RETURN(sql_thread_interface.is_own_event_channel(id));
}
