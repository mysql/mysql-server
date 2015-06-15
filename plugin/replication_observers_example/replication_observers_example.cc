/* Copyright (c) 2014, 2015, Oracle and/or its affiliates. All rights reserved.

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

#include <mysql/group_replication_priv.h>
#include <mysql/plugin.h>
#include <mysql/service_my_plugin_log.h>
#include <mysql/service_rpl_transaction_ctx.h>

static MYSQL_PLUGIN plugin_info_ptr;

int validate_plugin_server_requirements(Trans_param *param);
int test_channel_service_interface_initialization();
int test_channel_service_interface();

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

static int trans_before_dml_call= 0;
static int trans_before_commit_call= 0;
static int trans_before_rollback_call= 0;
static int trans_after_commit_call= 0;
static int trans_after_rollback_call= 0;

static void dump_transaction_calls()
{

  if(trans_before_dml_call)
  {
    my_plugin_log_message(&plugin_info_ptr,
                          MY_INFORMATION_LEVEL,
                          "\nreplication_observers_example_plugin:trans_before_dml");
  }

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
int trans_before_dml(Trans_param *param, int& out_val)
{
  trans_before_dml_call++;

  DBUG_EXECUTE_IF("cause_failure_in_before_dml_hook",
                  out_val= 1;);
  DBUG_EXECUTE_IF("validate_replication_observers_plugin_server_channels",
                  test_channel_service_interface(););
  DBUG_EXECUTE_IF("validate_replication_observers_plugin_server_channels_init",
                  test_channel_service_interface_initialization(););
  return 0;
}

typedef enum enum_before_commit_test_cases {
  NEGATIVE_CERTIFICATION,
  POSITIVE_CERTIFICATION_WITH_GTID,
  POSITIVE_CERTIFICATION_WITHOUT_GTID,
  INVALID_CERTIFICATION_OUTCOME
} before_commit_test_cases;

int before_commit_tests(Trans_param *param,
                        before_commit_test_cases test_case)
{
  rpl_sid fake_sid;
  rpl_sidno fake_sidno;
  rpl_gno fake_gno;

  Transaction_termination_ctx transaction_termination_ctx;
  memset(&transaction_termination_ctx, 0, sizeof(transaction_termination_ctx));
  transaction_termination_ctx.m_thread_id= param->thread_id;

  switch(test_case)
  {
  case NEGATIVE_CERTIFICATION:
    transaction_termination_ctx.m_rollback_transaction= TRUE;
    transaction_termination_ctx.m_generated_gtid= FALSE;
    transaction_termination_ctx.m_sidno= -1;
    transaction_termination_ctx.m_gno= -1;
    break;

  case POSITIVE_CERTIFICATION_WITH_GTID:
    fake_sid.parse("aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa");
    fake_sidno= get_sidno_from_global_sid_map(fake_sid);
    fake_gno= get_last_executed_gno(fake_sidno);
    fake_gno++;

    transaction_termination_ctx.m_rollback_transaction= FALSE;
    transaction_termination_ctx.m_generated_gtid= TRUE;
    transaction_termination_ctx.m_sidno= fake_sidno;
    transaction_termination_ctx.m_gno= fake_gno;
    break;

  case POSITIVE_CERTIFICATION_WITHOUT_GTID:
    transaction_termination_ctx.m_rollback_transaction= FALSE;
    transaction_termination_ctx.m_generated_gtid= FALSE;
    transaction_termination_ctx.m_sidno= 0;
    transaction_termination_ctx.m_gno= 0;
    break;

  case INVALID_CERTIFICATION_OUTCOME:
    transaction_termination_ctx.m_rollback_transaction= TRUE;
    transaction_termination_ctx.m_generated_gtid= TRUE;
    transaction_termination_ctx.m_sidno= -1;
    transaction_termination_ctx.m_gno= -1;

  default:
    break;
  }

  if (set_transaction_ctx(transaction_termination_ctx))
  {
    my_plugin_log_message(&plugin_info_ptr,
                          MY_ERROR_LEVEL,
                          "Unable to update transaction context service on server, thread_id: %lu",
                          param->thread_id);
    return 1;
  }

  return 0;
}

int trans_before_commit(Trans_param *param)
{
  trans_before_commit_call++;

  DBUG_EXECUTE_IF("force_error_on_before_commit_listener",
                  return 1;);

  DBUG_EXECUTE_IF("force_negative_certification_outcome",
                  return before_commit_tests(param, NEGATIVE_CERTIFICATION););

  DBUG_EXECUTE_IF("force_positive_certification_outcome_without_gtid",
                  return before_commit_tests(param, POSITIVE_CERTIFICATION_WITHOUT_GTID););

  DBUG_EXECUTE_IF("force_positive_certification_outcome_with_gtid",
                  return before_commit_tests(param, POSITIVE_CERTIFICATION_WITH_GTID););

  DBUG_EXECUTE_IF("force_invalid_certification_outcome",
                  return before_commit_tests(param, INVALID_CERTIFICATION_OUTCOME););

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

  DBUG_EXECUTE_IF("validate_replication_observers_plugin_server_requirements",
                  return validate_plugin_server_requirements(param););

  return 0;
}

Trans_observer trans_observer = {
  sizeof(Trans_observer),

  trans_before_dml,
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
static int binlog_relay_applier_stop_call= 0;
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

  if (binlog_relay_applier_stop_call)
  {
    my_plugin_log_message(&plugin_info_ptr,
                          MY_INFORMATION_LEVEL,
                          "\nreplication_observers_example_plugin:binlog_relay_applier_stop");
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

int binlog_relay_applier_stop(Binlog_relay_IO_param *param,
                              bool aborted)
{
  binlog_relay_applier_stop_call++;

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
  binlog_relay_applier_stop,
  binlog_relay_before_request_transmit,
  binlog_relay_after_read_event,
  binlog_relay_after_queue_event,
  binlog_relay_after_reset_slave,
};


/*
  Validate plugin requirements on server code.
  This function is mainly to ensure that any change on server code
  will not break Group Replication requirements.
*/
int validate_plugin_server_requirements(Trans_param *param)
{
  int success= 0;

  /*
    Instantiate a Gtid_log_event without a THD parameter.
  */
  rpl_sid fake_sid;
  fake_sid.parse("aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa");
  rpl_sidno fake_sidno= get_sidno_from_global_sid_map(fake_sid);
  rpl_gno fake_gno= get_last_executed_gno(fake_sidno)+1;

  Gtid gtid= { fake_sidno, fake_gno };
  Gtid_specification gtid_spec= { GTID_GROUP, gtid };
  Gtid_log_event *gle= new Gtid_log_event(param->server_id, true, 0, 1, gtid_spec);

  if (gle->is_valid())
    success++;
  else
    my_plugin_log_message(&plugin_info_ptr,
                          MY_INFORMATION_LEVEL,
                          "replication_observers_example_plugin:validate_plugin_server_requirements:"
                          " failed to instantiate a Gtid_log_event");
  delete gle;


  /*
    Instantiate a anonymous Gtid_log_event without a THD parameter.
  */
  Gtid_specification anonymous_gtid_spec= { ANONYMOUS_GROUP, gtid };
  gle= new Gtid_log_event(param->server_id, true, 0, 1, anonymous_gtid_spec);

  if (gle->is_valid())
    success++;
  else
    my_plugin_log_message(&plugin_info_ptr,
                          MY_INFORMATION_LEVEL,
                          "replication_observers_example_plugin:validate_plugin_server_requirements:"
                          " failed to instantiate a anonymous Gtid_log_event");
  delete gle;


  /*
    Instantiate a Transaction_context_log_event.
  */
  Transaction_context_log_event *tcle= new Transaction_context_log_event(param->server_uuid,
                                                                         true,
                                                                         param->thread_id,
                                                                         false);

  if (tcle->is_valid())
  {
    Gtid_set *snapshot_version= tcle->get_snapshot_version();
    size_t snapshot_version_len= snapshot_version->get_encoded_length();
    uchar* snapshot_version_buf= (uchar *)my_malloc(PSI_NOT_INSTRUMENTED,
                                                    snapshot_version_len, MYF(0));
    snapshot_version->encode(snapshot_version_buf);
    my_plugin_log_message(&plugin_info_ptr,
                          MY_INFORMATION_LEVEL,
                          "snapshot version is '%s'",
                          snapshot_version_buf);
    my_free(snapshot_version_buf);
    success++;
  }
  else
    my_plugin_log_message(&plugin_info_ptr,
                          MY_INFORMATION_LEVEL,
                          "replication_observers_example_plugin:validate_plugin_server_requirements:"
                          " failed to instantiate a Transaction_context_log_event");
  delete tcle;


  /*
    Instantiate a View_Change_log_event.
  */
  View_change_log_event *vcle= new View_change_log_event(const_cast<char*>("1421867646:1"));

  if (vcle->is_valid())
  {
    success++;
  }
  else
    my_plugin_log_message(&plugin_info_ptr,
                          MY_INFORMATION_LEVEL,
                          "replication_observers_example_plugin:validate_plugin_server_requirements:"
                          " failed to instantiate a View_change_log_event");
  delete vcle;


  /*
    include/mysql/group_replication_priv.h exported functions.
  */
  my_thread_attr_t *thread_attr= get_connection_attrib();

  char *hostname, *uuid;
  uint port;
  get_server_host_port_uuid(&hostname, &port, &uuid);

  Trans_context_info startup_pre_reqs;
  get_server_startup_prerequirements(startup_pre_reqs, false);

  bool server_engine_ready= is_server_engine_ready();

  uchar *encoded_gtid_executed= NULL;
  uint length;
  get_server_encoded_gtid_executed(&encoded_gtid_executed, &length);

#if !defined(DBUG_OFF)
  char *encoded_gtid_executed_string=
      encoded_gtid_set_to_string(encoded_gtid_executed, length);
#endif

  if (thread_attr != NULL &&
      hostname != NULL &&
      uuid != NULL &&
      port > 0 &&
      startup_pre_reqs.gtid_mode == 3 &&
      server_engine_ready &&
      encoded_gtid_executed != NULL
#if !defined(DBUG_OFF)
      && encoded_gtid_executed_string != NULL
#endif
     )
    success++;
  else
    my_plugin_log_message(&plugin_info_ptr,
                          MY_INFORMATION_LEVEL,
                          "replication_observers_example_plugin:validate_plugin_server_requirements:"
                          " failed to invoke group_replication_priv.h exported functions");

#if !defined(DBUG_OFF)
  my_free(encoded_gtid_executed_string);
#endif
  my_free(encoded_gtid_executed);


  /*
    Log number of successful validations.
  */
  my_plugin_log_message(&plugin_info_ptr,
                        MY_INFORMATION_LEVEL,
                        "\nreplication_observers_example_plugin:validate_plugin_server_requirements=%d",
                        success);

  return 0;
}

int test_channel_service_interface_initialization()
{
    int error= initialize_channel_service_interface();
    DBUG_ASSERT(error);
    return error;
}

int test_channel_service_interface()
{
    //The initialization method should return OK
    int error= initialize_channel_service_interface();
    DBUG_ASSERT(!error);

    //Test channel creation
    char interface_channel[]= "example_channel";
    Channel_creation_info info;
    initialize_channel_creation_info(&info);
    error= channel_create(interface_channel, &info);
    DBUG_ASSERT(!error);

    //Assert the channel exists
    bool exists= channel_is_active(interface_channel, CHANNEL_NO_THD);
    DBUG_ASSERT(exists);

    //Check that a non existing channel is declared as such
    char dummy_channel[]= "dummy_channel";
    exists= channel_is_active(dummy_channel, CHANNEL_NO_THD);
    DBUG_ASSERT(!exists);

    //Test that we cannot create a empty named channel (the default channel)
    char empty_interface_channel[]= "";
    initialize_channel_creation_info(&info);
    error= channel_create(empty_interface_channel, &info);
    DBUG_ASSERT(error == RPL_CHANNEL_SERVICE_DEFAULT_CHANNEL_CREATION_ERROR);

    //Start the applier thread (since it does not need an external server)
    Channel_connection_info connection_info;
    initialize_channel_connection_info(&connection_info);
    error= channel_start(interface_channel,
                         &connection_info,
                         CHANNEL_APPLIER_THREAD,
                         true);
    DBUG_ASSERT(!error);

    //Assert that the applier thread is running
    bool running= channel_is_active(interface_channel, CHANNEL_APPLIER_THREAD);
    DBUG_ASSERT(running);

    //Wait for execution of events (none in this case so it should return OK)
    error= channel_wait_until_apply_queue_empty(interface_channel, 100000);
    DBUG_ASSERT(!error);

    //Get the last delivered gno (should be 0)
    rpl_sid fake_sid;
    fake_sid.parse("aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa");
    rpl_sidno fake_sidno= get_sidno_from_global_sid_map(fake_sid);
    rpl_gno gno= channel_get_last_delivered_gno(interface_channel, fake_sidno);
    DBUG_ASSERT(gno == 0);

    //Check that for non existing channels it returns the corresponding error
    gno= channel_get_last_delivered_gno(dummy_channel, fake_sidno);
    DBUG_ASSERT(gno == RPL_CHANNEL_SERVICE_CHANNEL_DOES_NOT_EXISTS_ERROR);

    //Extract the applier id
    long unsigned int * applier_id= NULL;
    channel_get_appliers_thread_id(interface_channel,
                                   &applier_id);
    DBUG_ASSERT(*applier_id > 0);
    my_free(applier_id);

    //Stop the channel applier
    error= channel_stop(interface_channel,
                        3,
                        10000);
    DBUG_ASSERT(!error);

    //Assert that the applier thread is not running
    running= channel_is_active(interface_channel, CHANNEL_APPLIER_THREAD);
    DBUG_ASSERT(!running);

    //Purge the channel and assert all is OK
    error= channel_purge_queue(interface_channel, true);
    DBUG_ASSERT(!error);

    //Assert the channel is not there.
    exists= channel_is_active(interface_channel, CHANNEL_NO_THD);
    DBUG_ASSERT(!exists);

    //Check that a queue in an empty channel will fail.
    char empty_event[]= "";
    error= channel_queue_packet(dummy_channel, empty_event, 0);
    DBUG_ASSERT(error);

    return (error && exists && running && gno);
}

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
    DBUG_RETURN(1);
  }

  if (register_trans_observer(&trans_observer, (void *)plugin_info_ptr))
  {
    my_plugin_log_message(&plugin_info_ptr, MY_ERROR_LEVEL,"Failure in registering the transactions state observers");
    DBUG_RETURN(1);
  }

  if (register_binlog_relay_io_observer(&relay_io_observer, (void *)plugin_info_ptr))
  {
    my_plugin_log_message(&plugin_info_ptr, MY_ERROR_LEVEL,"Failure in registering the relay io observer");
    DBUG_RETURN(1);
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
    DBUG_RETURN(1);
  }

  if (unregister_trans_observer(&trans_observer, p))
  {
    my_plugin_log_message(&p, MY_ERROR_LEVEL,"Failure in unregistering the transactions state observers");
    DBUG_RETURN(1);
  }

  if (unregister_binlog_relay_io_observer(&relay_io_observer, p))
  {
    my_plugin_log_message(&p, MY_ERROR_LEVEL,"Failure in unregistering the relay io observer");
    DBUG_RETURN(1);
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
