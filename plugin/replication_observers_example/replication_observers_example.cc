/* Copyright (c) 2014, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

/*
  This plugin serves as an example for all those who which to use the new
  Hooks installed by Replication in order to capture:
  - Transaction progress
  - Server state
 */

#ifndef MYSQL_SERVER
#define MYSQL_SERVER
#endif
#ifndef HAVE_REPLICATION
#define HAVE_REPLICATION
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <my_global.h>
#include <my_sys.h>
#include <stdlib.h>
#include <mysql_version.h>
#include <mysql/plugin.h>
#include "sql_plugin.h"                         // st_plugin_int

#include <mysql/service_my_plugin_log.h>

#include "log_event.h"
#include "replication.h"

static MYSQL_PLUGIN plugin_info_ptr;

/*
  Will register the number of calls to each method of Server state
 */
static int before_handle_connection_call= 0;
static int before_recovery_call= 0;
static int after_engine_recovery_call= 0;
static int after_recovery_call= 0;
static int before_server_shutdown_call= 0;
static int after_server_shutdown_call= 0;

static void dump_server_state_calls()
{
  if(before_handle_connection_call)
  {
    my_plugin_log_message(&plugin_info_ptr,
                          MY_INFORMATION_LEVEL,
                          "\nreplication_observers_example_plugin:before_handle_connection");
  }

  if(before_recovery_call)
  {
    my_plugin_log_message(&plugin_info_ptr,
                          MY_INFORMATION_LEVEL,
                          "\nreplication_observers_example_plugin:before_recovery");
  }

  if(after_engine_recovery_call)
  {
    my_plugin_log_message(&plugin_info_ptr,
                          MY_INFORMATION_LEVEL,
                          "\nreplication_observers_example_plugin:after_engine_recovery");
  }

  if(after_recovery_call)
  {
    my_plugin_log_message(&plugin_info_ptr,
                          MY_INFORMATION_LEVEL,
                          "\nreplication_observers_example_plugin:after_recovery");
  }

  if(before_server_shutdown_call)
  {
    my_plugin_log_message(&plugin_info_ptr,
                          MY_INFORMATION_LEVEL,
                          "\nreplication_observers_example_plugin:before_server_shutdown");
  }

  if(after_server_shutdown_call)
  {
    my_plugin_log_message(&plugin_info_ptr,
                          MY_INFORMATION_LEVEL,
                          "\nreplication_observers_example_plugin:after_server_shutdown");
  }
}

/*
  DBMS lifecycle events observers.
*/
int before_handle_connection(Server_state_param *param)
{
  before_handle_connection_call++;

  return 0;
}

int before_recovery(Server_state_param *param)
{
  before_recovery_call++;

  return 0;
}

int after_engine_recovery(Server_state_param *param)
{
  after_engine_recovery_call++;

  return 0;
}

int after_recovery(Server_state_param *param)
{
  after_recovery_call++;

  return 0;
}

int before_server_shutdown(Server_state_param *param)
{
  before_server_shutdown_call++;

  return 0;
}

int after_server_shutdown(Server_state_param *param)
{
  after_server_shutdown_call++;

  return 0;
}

Server_state_observer server_state_observer = {
  sizeof(Server_state_observer),

  before_handle_connection, //before the client connect the node
  before_recovery,           //before_recovery
  after_engine_recovery,     //after engine recovery
  after_recovery,            //after_recovery
  before_server_shutdown,    //before shutdown
  after_server_shutdown,     //after shutdown
};

static int trans_before_commit_call= 0;
static int trans_before_rollback_call= 0;
static int trans_after_commit_call= 0;
static int trans_after_rollback_call= 0;

static void dump_transaction_calls()
{

  if(trans_before_commit_call)
  {
    my_plugin_log_message(&plugin_info_ptr,
                          MY_INFORMATION_LEVEL,
                          "\nreplication_observers_example_plugin:trans_before_commit");
  }

  if(trans_before_rollback_call)
  {
    my_plugin_log_message(&plugin_info_ptr,
                          MY_INFORMATION_LEVEL,
                          "\nreplication_observers_example_plugin:trans_before_rollback");
  }

  if(trans_after_commit_call)
  {
    my_plugin_log_message(&plugin_info_ptr,
                          MY_INFORMATION_LEVEL,
                          "\nreplication_observers_example_plugin:trans_after_commit");
  }

  if(trans_after_rollback_call)
  {
    my_plugin_log_message(&plugin_info_ptr,
                          MY_INFORMATION_LEVEL,
                          "\nreplication_observers_example_plugin:trans_after_rollback");
  }
}
/*
  Transaction lifecycle events observers.
*/
int trans_before_commit(Trans_param *param)
{
  trans_before_commit_call++;

  return 0;
}

int trans_before_rollback(Trans_param *param)
{
  trans_before_rollback_call++;

  return 0;
}

int trans_after_commit(Trans_param *param)
{
  trans_after_commit_call++;

  return 0;
}

int trans_after_rollback(Trans_param *param)
{
  trans_after_rollback_call++;

  return 0;
}

Trans_observer trans_observer = {
  sizeof(Trans_observer),

  trans_before_commit,
  trans_before_rollback,
  trans_after_commit,
  trans_after_rollback,
};

/*
  Binlog relay IO events observers.
*/
static int binlog_relay_thread_start_call= 0;
static int binlog_relay_thread_stop_call= 0;
static int binlog_relay_consumer_thread_stop_call= 0;
static int binlog_relay_before_request_transmit_call= 0;
static int binlog_relay_after_read_event_call= 0;
static int binlog_relay_after_queue_event_call= 0;
static int binlog_relay_after_reset_slave_call= 0;

static void dump_binlog_relay_calls()
{
  if (binlog_relay_thread_start_call)
  {
    my_plugin_log_message(&plugin_info_ptr,
                          MY_INFORMATION_LEVEL,
                          "\nreplication_observers_example_plugin:binlog_relay_thread_start");
  }

  if (binlog_relay_thread_stop_call)
  {
    my_plugin_log_message(&plugin_info_ptr,
                          MY_INFORMATION_LEVEL,
                          "\nreplication_observers_example_plugin:binlog_relay_thread_stop");
  }

  if (binlog_relay_consumer_thread_stop_call)
  {
    my_plugin_log_message(&plugin_info_ptr,
                          MY_INFORMATION_LEVEL,
                          "\nreplication_observers_example_plugin:binlog_relay_consumer_thread_stop");
  }

  if (binlog_relay_before_request_transmit_call)
  {
    my_plugin_log_message(&plugin_info_ptr,
                          MY_INFORMATION_LEVEL,
                          "\nreplication_observers_example_plugin:binlog_relay_before_request_transmit");
  }

  if (binlog_relay_after_read_event_call)
  {
    my_plugin_log_message(&plugin_info_ptr,
                          MY_INFORMATION_LEVEL,
                          "\nreplication_observers_example_plugin:binlog_relay_after_read_event");
  }

  if (binlog_relay_after_queue_event_call)
  {
    my_plugin_log_message(&plugin_info_ptr,
                          MY_INFORMATION_LEVEL,
                          "\nreplication_observers_example_plugin:binlog_relay_after_queue_event");
  }

  if (binlog_relay_after_reset_slave_call)
  {
    my_plugin_log_message(&plugin_info_ptr,
                          MY_INFORMATION_LEVEL,
                          "\nreplication_observers_example_plugin:binlog_relay_after_reset_slave");
  }
}

int binlog_relay_thread_start(Binlog_relay_IO_param *param)
{
  binlog_relay_thread_start_call++;

  return 0;
}

int binlog_relay_thread_stop(Binlog_relay_IO_param *param)
{
  binlog_relay_thread_stop_call++;

  return 0;
}

int binlog_relay_consumer_thread_stop(Binlog_relay_IO_param *param,
                                      bool aborted)
{
  binlog_relay_consumer_thread_stop_call++;

  return 0;
}

int binlog_relay_before_request_transmit(Binlog_relay_IO_param *param,
                                         uint32 flags)
{
  binlog_relay_before_request_transmit_call++;

  return 0;
}

int binlog_relay_after_read_event(Binlog_relay_IO_param *param,
                                  const char *packet, unsigned long len,
                                  const char **event_buf, unsigned long *event_len)
{
  binlog_relay_after_read_event_call++;

  return 0;
}

int binlog_relay_after_queue_event(Binlog_relay_IO_param *param,
                                   const char *event_buf,
                                   unsigned long event_len,
                                   uint32 flags)
{
  binlog_relay_after_queue_event_call++;

  return 0;
}

int binlog_relay_after_reset_slave(Binlog_relay_IO_param *param)
{
  binlog_relay_after_reset_slave_call++;

  return 0;
}

Binlog_relay_IO_observer relay_io_observer = {
  sizeof(Binlog_relay_IO_observer),

  binlog_relay_thread_start,
  binlog_relay_thread_stop,
  binlog_relay_consumer_thread_stop,
  binlog_relay_before_request_transmit,
  binlog_relay_after_read_event,
  binlog_relay_after_queue_event,
  binlog_relay_after_reset_slave,
};

/*
  Initialize the Replication Observer example at server start or plugin
  installation.

  SYNOPSIS
    replication_observers_example_plugin_init()

  DESCRIPTION
    Registers Server state observer and Transaction Observer

  RETURN VALUE
    0                    success
    1                    failure (cannot happen)
*/

static int replication_observers_example_plugin_init(MYSQL_PLUGIN plugin_info)
{
  plugin_info_ptr= plugin_info;

  DBUG_ENTER("replication_observers_example_plugin_init");

  if(register_server_state_observer(&server_state_observer, (void *)plugin_info_ptr))
  {
    my_plugin_log_message(&plugin_info_ptr, MY_ERROR_LEVEL, "Failure in registering the server state observers");
    return 1;
  }

  if (register_trans_observer(&trans_observer, (void *)plugin_info_ptr))
  {
    my_plugin_log_message(&plugin_info_ptr, MY_ERROR_LEVEL,"Failure in registering the transactions state observers");
    return 1;
  }

  if (register_binlog_relay_io_observer(&relay_io_observer, (void *)plugin_info_ptr))
  {
    my_plugin_log_message(&plugin_info_ptr, MY_ERROR_LEVEL,"Failure in registering the relay io observer");
    return 1;
  }

  my_plugin_log_message(&plugin_info_ptr, MY_INFORMATION_LEVEL,"replication_observers_example_plugin: init finished");

  DBUG_RETURN(0);
}


/*
  Terminate the Replication Observer example at server shutdown or
  plugin deinstallation.

  SYNOPSIS
    replication_observers_example_plugin_deinit()

  DESCRIPTION
    Unregisters Server state observer and Transaction Observer

  RETURN VALUE
    0                    success
    1                    failure (cannot happen)

*/

static int replication_observers_example_plugin_deinit(void *p)
{
  DBUG_ENTER("replication_observers_example_plugin_deinit");

  dump_server_state_calls();
  dump_transaction_calls();
  dump_binlog_relay_calls();

  if (unregister_server_state_observer(&server_state_observer, p))
  {
    my_plugin_log_message(&p, MY_ERROR_LEVEL,"Failure in unregistering the server state observers");
    return 1;
  }

  if (unregister_trans_observer(&trans_observer, p))
  {
    my_plugin_log_message(&p, MY_ERROR_LEVEL,"Failure in unregistering the transactions state observers");
    return 1;
  }

  if (unregister_binlog_relay_io_observer(&relay_io_observer, p))
  {
    my_plugin_log_message(&p, MY_ERROR_LEVEL,"Failure in unregistering the relay io observer");
    return 1;
  }

  my_plugin_log_message(&p, MY_INFORMATION_LEVEL,"replication_observers_example_plugin: deinit finished");

  DBUG_RETURN(0);
}

/*
  Plugin library descriptor
*/
struct Mysql_replication replication_observers_example_plugin=
{ MYSQL_REPLICATION_INTERFACE_VERSION };

mysql_declare_plugin(replication_observers_example)
{
  MYSQL_REPLICATION_PLUGIN,
  &replication_observers_example_plugin,
  "replication_observers_example",
  "ORACLE",
  "Replication observer infrastructure example.",
  PLUGIN_LICENSE_GPL,
  replication_observers_example_plugin_init, /* Plugin Init */
  replication_observers_example_plugin_deinit, /* Plugin Deinit */
  0x0100 /* 1.0 */,
  NULL,                       /* status variables                */
  NULL,                       /* system variables                */
  NULL,                       /* config options                  */
  0,                          /* flags                           */
}
mysql_declare_plugin_end;
