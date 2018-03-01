/* Copyright (c) 2014, 2018, Oracle and/or its affiliates. All rights reserved.

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

/*
  This plugin serves as an example for all those who which to use the new
  Hooks installed by Replication in order to capture:
  - Transaction progress
  - Server state
 */
#define LOG_SUBSYSTEM_TAG "replication_observers_example"

#include <assert.h>
#include <mysql/components/my_service.h>
#include <mysql/components/services/log_builtins.h>
#include <mysql/group_replication_priv.h>
#include <mysql/plugin.h>
#include <mysql/service_rpl_transaction_ctx.h>
#include <mysqld_error.h>
#include <sys/types.h>

#include "my_dbug.h"
#include "my_inttypes.h"
#include "sql/current_thd.h"

static MYSQL_PLUGIN plugin_info_ptr;
static SERVICE_TYPE(registry) *reg_srv = nullptr;
SERVICE_TYPE(log_builtins) *log_bi = nullptr;
SERVICE_TYPE(log_builtins_string) *log_bs = nullptr;

int validate_plugin_server_requirements(Trans_param *param);
int test_channel_service_interface_initialization();
int test_channel_service_interface();
int test_channel_service_interface_io_thread();
bool test_channel_service_interface_is_io_stopping();
bool test_channel_service_interface_is_sql_stopping();
bool test_channel_service_interface_relay_log_renamed();

/*
  Will register the number of calls to each method of Server state
 */
static int before_handle_connection_call = 0;
static int before_recovery_call = 0;
static int after_engine_recovery_call = 0;
static int after_recovery_call = 0;
static int before_server_shutdown_call = 0;
static int after_server_shutdown_call = 0;
static bool thread_aborted = false;

static void dump_server_state_calls() {
  if (before_handle_connection_call) {
    LogPluginErr(
        INFORMATION_LEVEL, ER_LOG_PRINTF_MSG,
        "\nreplication_observers_example_plugin:before_handle_connection");
  }

  if (before_recovery_call) {
    LogPluginErr(INFORMATION_LEVEL, ER_LOG_PRINTF_MSG,
                 "\nreplication_observers_example_plugin:before_recovery");
  }

  if (after_engine_recovery_call) {
    LogPluginErr(
        INFORMATION_LEVEL, ER_LOG_PRINTF_MSG,
        "\nreplication_observers_example_plugin:after_engine_recovery");
  }

  if (after_recovery_call) {
    LogPluginErr(INFORMATION_LEVEL, ER_LOG_PRINTF_MSG,
                 "\nreplication_observers_example_plugin:after_recovery");
  }

  if (before_server_shutdown_call) {
    LogPluginErr(
        INFORMATION_LEVEL, ER_LOG_PRINTF_MSG,
        "\nreplication_observers_example_plugin:before_server_shutdown");
  }

  if (after_server_shutdown_call) {
    LogPluginErr(
        INFORMATION_LEVEL, ER_LOG_PRINTF_MSG,
        "\nreplication_observers_example_plugin:after_server_shutdown");
  }
}

/*
  DBMS lifecycle events observers.
*/
static int before_handle_connection(Server_state_param *) {
  before_handle_connection_call++;

  return 0;
}

static int before_recovery(Server_state_param *) {
  before_recovery_call++;

  return 0;
}

static int after_engine_recovery(Server_state_param *) {
  after_engine_recovery_call++;

  return 0;
}

static int after_recovery(Server_state_param *) {
  after_recovery_call++;

  return 0;
}

static int before_server_shutdown(Server_state_param *) {
  before_server_shutdown_call++;

  return 0;
}

static int after_server_shutdown(Server_state_param *) {
  after_server_shutdown_call++;

  return 0;
}

Server_state_observer server_state_observer = {
    sizeof(Server_state_observer),

    before_handle_connection,  // before the client connect the node
    before_recovery,           // before_recovery
    after_engine_recovery,     // after engine recovery
    after_recovery,            // after_recovery
    before_server_shutdown,    // before shutdown
    after_server_shutdown,     // after shutdown
};

static int trans_before_dml_call = 0;
static int trans_before_commit_call = 0;
static int trans_before_rollback_call = 0;
static int trans_after_commit_call = 0;
static int trans_after_rollback_call = 0;

static void dump_transaction_calls() {
  if (trans_before_dml_call) {
    LogPluginErr(INFORMATION_LEVEL, ER_LOG_PRINTF_MSG,
                 "\nreplication_observers_example_plugin:trans_before_dml");
  }

  if (trans_before_commit_call) {
    LogPluginErr(INFORMATION_LEVEL, ER_LOG_PRINTF_MSG,
                 "\nreplication_observers_example_plugin:trans_before_commit");
  }

  if (trans_before_rollback_call) {
    LogPluginErr(
        INFORMATION_LEVEL, ER_LOG_PRINTF_MSG,
        "\nreplication_observers_example_plugin:trans_before_rollback");
  }

  if (trans_after_commit_call) {
    LogPluginErr(INFORMATION_LEVEL, ER_LOG_PRINTF_MSG,
                 "\nreplication_observers_example_plugin:trans_after_commit");
  }

  if (trans_after_rollback_call) {
    LogPluginErr(INFORMATION_LEVEL, ER_LOG_PRINTF_MSG,
                 "\nreplication_observers_example_plugin:trans_after_rollback");
  }
}

/*
  Transaction lifecycle events observers.
*/
static int trans_before_dml(Trans_param *,
                            int &out_val MY_ATTRIBUTE((unused))) {
  trans_before_dml_call++;

  DBUG_EXECUTE_IF("cause_failure_in_before_dml_hook", out_val = 1;);
  DBUG_EXECUTE_IF("validate_replication_observers_plugin_server_channels",
                  test_channel_service_interface(););
  DBUG_EXECUTE_IF(
      "validate_replication_observers_plugin_server_channel_io_thread",
      test_channel_service_interface_io_thread(););
  DBUG_EXECUTE_IF("validate_replication_observers_plugin_server_channels_init",
                  test_channel_service_interface_initialization(););
  DBUG_EXECUTE_IF("validate_replication_observers_plugin_server_is_io_stopping",
                  test_channel_service_interface_is_io_stopping(););
  DBUG_EXECUTE_IF(
      "validate_replication_observers_plugin_server_is_sql_stopping",
      test_channel_service_interface_is_sql_stopping(););
  DBUG_EXECUTE_IF(
      "validate_replication_observers_plugin_server_relay_log_renamed",
      test_channel_service_interface_relay_log_renamed(););
  return 0;
}

typedef enum enum_before_commit_test_cases {
  NEGATIVE_CERTIFICATION,
  POSITIVE_CERTIFICATION_WITH_GTID,
  POSITIVE_CERTIFICATION_WITHOUT_GTID,
  INVALID_CERTIFICATION_OUTCOME
} before_commit_test_cases;

#ifndef DBUG_OFF
static int before_commit_tests(Trans_param *param,
                               before_commit_test_cases test_case) {
  rpl_sid fake_sid;
  rpl_sidno fake_sidno;
  rpl_gno fake_gno;

  Transaction_termination_ctx transaction_termination_ctx;
  memset(&transaction_termination_ctx, 0, sizeof(transaction_termination_ctx));
  transaction_termination_ctx.m_thread_id = param->thread_id;

  switch (test_case) {
    case NEGATIVE_CERTIFICATION:
      transaction_termination_ctx.m_rollback_transaction = true;
      transaction_termination_ctx.m_generated_gtid = false;
      transaction_termination_ctx.m_sidno = -1;
      transaction_termination_ctx.m_gno = -1;
      break;

    case POSITIVE_CERTIFICATION_WITH_GTID:
      fake_sid.parse("aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa",
                     binary_log::Uuid::TEXT_LENGTH);
      fake_sidno = get_sidno_from_global_sid_map(fake_sid);
      fake_gno = get_last_executed_gno(fake_sidno);
      fake_gno++;

      transaction_termination_ctx.m_rollback_transaction = false;
      transaction_termination_ctx.m_generated_gtid = true;
      transaction_termination_ctx.m_sidno = fake_sidno;
      transaction_termination_ctx.m_gno = fake_gno;
      break;

    case POSITIVE_CERTIFICATION_WITHOUT_GTID:
      transaction_termination_ctx.m_rollback_transaction = false;
      transaction_termination_ctx.m_generated_gtid = false;
      transaction_termination_ctx.m_sidno = 0;
      transaction_termination_ctx.m_gno = 0;
      break;

    case INVALID_CERTIFICATION_OUTCOME:
      transaction_termination_ctx.m_rollback_transaction = true;
      transaction_termination_ctx.m_generated_gtid = true;
      transaction_termination_ctx.m_sidno = -1;
      transaction_termination_ctx.m_gno = -1;

    default:
      break;
  }

  if (set_transaction_ctx(transaction_termination_ctx)) {
    LogPluginErrMsg(
        ERROR_LEVEL, ER_LOG_PRINTF_MSG,
        "Unable to update transaction context service on server, thread_id: %u",
        param->thread_id);
    return 1;
  }

  return 0;
}
#endif

static int trans_before_commit(Trans_param *param MY_ATTRIBUTE((unused))) {
  trans_before_commit_call++;

  DBUG_EXECUTE_IF("force_error_on_before_commit_listener", return 1;);

  DBUG_EXECUTE_IF("force_negative_certification_outcome",
                  return before_commit_tests(param, NEGATIVE_CERTIFICATION););

  DBUG_EXECUTE_IF(
      "force_positive_certification_outcome_without_gtid",
      return before_commit_tests(param, POSITIVE_CERTIFICATION_WITHOUT_GTID););

  DBUG_EXECUTE_IF(
      "force_positive_certification_outcome_with_gtid",
      return before_commit_tests(param, POSITIVE_CERTIFICATION_WITH_GTID););

  DBUG_EXECUTE_IF(
      "force_invalid_certification_outcome",
      return before_commit_tests(param, INVALID_CERTIFICATION_OUTCOME););

  return 0;
}

static int trans_before_rollback(Trans_param *) {
  trans_before_rollback_call++;

  return 0;
}

static int trans_after_commit(Trans_param *) {
  trans_after_commit_call++;

  return 0;
}

static int trans_after_rollback(Trans_param *param MY_ATTRIBUTE((unused))) {
  trans_after_rollback_call++;

  DBUG_EXECUTE_IF("validate_replication_observers_plugin_server_requirements",
                  return validate_plugin_server_requirements(param););

  return 0;
}

Trans_observer trans_observer = {
    sizeof(Trans_observer),

    trans_before_dml,       trans_before_commit,  trans_before_rollback,
    trans_after_commit,     trans_after_rollback,
};

/*
  Binlog relay IO events observers.
*/
static int binlog_relay_thread_start_call = 0;
static int binlog_relay_thread_stop_call = 0;
static int binlog_relay_applier_start_call = 0;
static int binlog_relay_applier_stop_call = 0;
static int binlog_relay_before_request_transmit_call = 0;
static int binlog_relay_after_read_event_call = 0;
static int binlog_relay_after_queue_event_call = 0;
static int binlog_relay_after_reset_slave_call = 0;

static void dump_binlog_relay_calls() {
  if (binlog_relay_thread_start_call) {
    LogPluginErr(
        INFORMATION_LEVEL, ER_LOG_PRINTF_MSG,
        "\nreplication_observers_example_plugin:binlog_relay_thread_start");
  }

  if (binlog_relay_thread_stop_call) {
    LogPluginErr(
        INFORMATION_LEVEL, ER_LOG_PRINTF_MSG,
        "\nreplication_observers_example_plugin:binlog_relay_thread_stop");
  }

  if (binlog_relay_applier_start_call) {
    LogPluginErr(
        INFORMATION_LEVEL, ER_LOG_PRINTF_MSG,
        "\nreplication_observers_example_plugin:binlog_relay_applier_start");
  }

  if (binlog_relay_applier_stop_call) {
    LogPluginErr(
        INFORMATION_LEVEL, ER_LOG_PRINTF_MSG,
        "\nreplication_observers_example_plugin:binlog_relay_applier_stop");
  }

  if (binlog_relay_before_request_transmit_call) {
    LogPluginErr(INFORMATION_LEVEL, ER_LOG_PRINTF_MSG,
                 "\nreplication_observers_example_plugin:binlog_relay_"
                 "before_request_transmit");
  }

  if (binlog_relay_after_read_event_call) {
    LogPluginErr(
        INFORMATION_LEVEL, ER_LOG_PRINTF_MSG,
        "\nreplication_observers_example_plugin:binlog_relay_after_read_event");
  }

  if (binlog_relay_after_queue_event_call) {
    LogPluginErr(INFORMATION_LEVEL, ER_LOG_PRINTF_MSG,
                 "\nreplication_observers_example_plugin:binlog_relay_"
                 "after_queue_event");
  }

  if (binlog_relay_after_reset_slave_call) {
    LogPluginErr(INFORMATION_LEVEL, ER_LOG_PRINTF_MSG,
                 "\nreplication_observers_example_plugin:binlog_relay_"
                 "after_reset_slave");
  }
}

static int binlog_relay_thread_start(Binlog_relay_IO_param *) {
  binlog_relay_thread_start_call++;

  return 0;
}

static int binlog_relay_thread_stop(Binlog_relay_IO_param *) {
  binlog_relay_thread_stop_call++;

  return 0;
}

int binlog_relay_applier_start(Binlog_relay_IO_param *) {
  binlog_relay_applier_start_call++;
  return 0;
}

static int binlog_relay_applier_stop(Binlog_relay_IO_param *, bool aborted) {
  binlog_relay_applier_stop_call++;
  thread_aborted = aborted;
  return 0;
}

static int binlog_relay_before_request_transmit(Binlog_relay_IO_param *,
                                                uint32) {
  binlog_relay_before_request_transmit_call++;

  return 0;
}

static int binlog_relay_after_read_event(Binlog_relay_IO_param *, const char *,
                                         unsigned long, const char **,
                                         unsigned long *) {
  binlog_relay_after_read_event_call++;

  return 0;
}

static int binlog_relay_after_queue_event(Binlog_relay_IO_param *, const char *,
                                          unsigned long, uint32) {
  binlog_relay_after_queue_event_call++;

  return 0;
}

static int binlog_relay_after_reset_slave(Binlog_relay_IO_param *) {
  binlog_relay_after_reset_slave_call++;

  return 0;
}

static int binlog_relay_applier_log_event(Binlog_relay_IO_param *,
                                          Trans_param *, int &) {
  return 0;
}

Binlog_relay_IO_observer relay_io_observer = {
    sizeof(Binlog_relay_IO_observer),

    binlog_relay_thread_start,
    binlog_relay_thread_stop,
    binlog_relay_applier_start,
    binlog_relay_applier_stop,
    binlog_relay_before_request_transmit,
    binlog_relay_after_read_event,
    binlog_relay_after_queue_event,
    binlog_relay_after_reset_slave,
    binlog_relay_applier_log_event};

/*
  Validate plugin requirements on server code.
  This function is mainly to ensure that any change on server code
  will not break Group Replication requirements.
*/
int validate_plugin_server_requirements(Trans_param *param) {
  int success = 0;

  /*
    Instantiate a Gtid_log_event without a THD parameter.
  */
  rpl_sid fake_sid;
  fake_sid.parse("aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa",
                 binary_log::Uuid::TEXT_LENGTH);
  rpl_sidno fake_sidno = get_sidno_from_global_sid_map(fake_sid);
  rpl_gno fake_gno = get_last_executed_gno(fake_sidno) + 1;

  Gtid gtid = {fake_sidno, fake_gno};
  Gtid_specification gtid_spec = {ASSIGNED_GTID, gtid};
  Gtid_log_event *gle =
      new Gtid_log_event(param->server_id, true, 0, 1, true, 0, 0, gtid_spec);

  if (gle->is_valid())
    success++;
  else
    LogPluginErr(INFORMATION_LEVEL, ER_LOG_PRINTF_MSG,
                 "replication_observers_example_plugin:validate_"
                 "plugin_server_requirements:"
                 " failed to instantiate a Gtid_log_event");
  delete gle;

  /*
    Instantiate a anonymous Gtid_log_event without a THD parameter.
  */
  Gtid_specification anonymous_gtid_spec = {ANONYMOUS_GTID, gtid};
  gle = new Gtid_log_event(param->server_id, true, 0, 1, true, 0, 0,
                           anonymous_gtid_spec);

  if (gle->is_valid())
    success++;
  else
    LogPluginErr(INFORMATION_LEVEL, ER_LOG_PRINTF_MSG,
                 "replication_observers_example_plugin:validate_"
                 "plugin_server_requirements:"
                 " failed to instantiate a anonymous Gtid_log_event");
  delete gle;

  /*
    Instantiate a Transaction_context_log_event.
  */
  Transaction_context_log_event *tcle = new Transaction_context_log_event(
      param->server_uuid, true, param->thread_id, false);

  if (tcle->is_valid()) {
    Gtid_set *snapshot_version = tcle->get_snapshot_version();
    size_t snapshot_version_len = snapshot_version->get_encoded_length();
    uchar *snapshot_version_buf =
        (uchar *)my_malloc(PSI_NOT_INSTRUMENTED, snapshot_version_len, MYF(0));
    snapshot_version->encode(snapshot_version_buf);
    LogPluginErr(INFORMATION_LEVEL, ER_LOG_PRINTF_MSG,
                 "snapshot version is '%s'", snapshot_version_buf);
    my_free(snapshot_version_buf);
    success++;
  } else
    LogPluginErr(INFORMATION_LEVEL, ER_LOG_PRINTF_MSG,
                 "replication_observers_example_plugin:validate_plugin_server_"
                 "requirements:"
                 " failed to instantiate a Transaction_context_log_event");
  delete tcle;

  /*
    Instantiate a View_Change_log_event.
  */
  View_change_log_event *vcle =
      new View_change_log_event(const_cast<char *>("1421867646:1"));

  if (vcle->is_valid()) {
    success++;
  } else
    LogPluginErr(INFORMATION_LEVEL, ER_LOG_PRINTF_MSG,
                 "replication_observers_example_plugin:validate_"
                 "plugin_server_requirements:"
                 " failed to instantiate a View_change_log_event");
  delete vcle;

  /*
    include/mysql/group_replication_priv.h exported functions.
  */
  my_thread_attr_t *thread_attr = get_connection_attrib();

  char *hostname, *uuid;
  uint port;
  unsigned int server_version;
  st_server_ssl_variables server_ssl_variables = {false, NULL, NULL, NULL, NULL,
                                                  NULL,  NULL, NULL, NULL, 0};

  get_server_parameters(&hostname, &port, &uuid, &server_version,
                        &server_ssl_variables);

  Trans_context_info startup_pre_reqs;
  get_server_startup_prerequirements(startup_pre_reqs, false);

  // check the server is initialized by checking if the default channel exists
  bool server_engine_ready = channel_is_active("", CHANNEL_NO_THD);

  uchar *encoded_gtid_executed = NULL;
  size_t length;
  get_server_encoded_gtid_executed(&encoded_gtid_executed, &length);

#if !defined(DBUG_OFF)
  char *encoded_gtid_executed_string =
      encoded_gtid_set_to_string(encoded_gtid_executed, length);
#endif

  if (thread_attr != NULL && hostname != NULL && uuid != NULL && port > 0 &&
      startup_pre_reqs.gtid_mode == 3 && server_engine_ready &&
      encoded_gtid_executed != NULL
#if !defined(DBUG_OFF)
      && encoded_gtid_executed_string != NULL
#endif
  )
    success++;
  else
    LogPluginErr(
        INFORMATION_LEVEL, ER_LOG_PRINTF_MSG,
        "replication_observers_example_plugin:validate_plugin_server_"
        "requirements:"
        " failed to invoke group_replication_priv.h exported functions");

#if !defined(DBUG_OFF)
  my_free(encoded_gtid_executed_string);
#endif
  my_free(encoded_gtid_executed);

  /*
    Log number of successful validations.
  */
  LogPluginErrMsg(INFORMATION_LEVEL, ER_LOG_PRINTF_MSG,
                  "\nreplication_observers_example_plugin:validate_"
                  "plugin_server_requirements=%d",
                  success);

  return 0;
}

int test_channel_service_interface_initialization() {
  int error = initialize_channel_service_interface();
  DBUG_ASSERT(error);
  return error;
}

int test_channel_service_interface() {
  // The initialization method should return OK
  int error = initialize_channel_service_interface();
  DBUG_ASSERT(!error);

  // Test channel creation
  char interface_channel[] = "example_channel";
  Channel_creation_info info;
  initialize_channel_creation_info(&info);
  error = channel_create(interface_channel, &info);
  DBUG_ASSERT(!error);

  // Assert the channel exists
  bool exists = channel_is_active(interface_channel, CHANNEL_NO_THD);
  DBUG_ASSERT(exists);

  // Check that a non existing channel is declared as such
  char dummy_channel[] = "dummy_channel";
  exists = channel_is_active(dummy_channel, CHANNEL_NO_THD);
  DBUG_ASSERT(!exists);

  // Test that we cannot create a empty named channel (the default channel)
  char empty_interface_channel[] = "";
  initialize_channel_creation_info(&info);
  error = channel_create(empty_interface_channel, &info);
  DBUG_ASSERT(error == RPL_CHANNEL_SERVICE_DEFAULT_CHANNEL_CREATION_ERROR);

  // Start the applier thread (since it does not need an external server)
  Channel_connection_info connection_info;
  initialize_channel_connection_info(&connection_info);
  error = channel_start(interface_channel, &connection_info,
                        CHANNEL_APPLIER_THREAD, true);
  DBUG_ASSERT(!error);

  // Assert that the applier thread is running
  bool running = channel_is_active(interface_channel, CHANNEL_APPLIER_THREAD);
  DBUG_ASSERT(running);

  // Wait for execution of events (none in this case so it should return OK)
  error = channel_wait_until_apply_queue_applied(interface_channel, 100000);
  DBUG_ASSERT(!error);

  // Get the last delivered gno (should be 0)
  rpl_sid fake_sid;
  fake_sid.parse("aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa",
                 binary_log::Uuid::TEXT_LENGTH);
  rpl_sidno fake_sidno = get_sidno_from_global_sid_map(fake_sid);
  rpl_gno gno = channel_get_last_delivered_gno(interface_channel, fake_sidno);
  DBUG_ASSERT(gno == 0);

  // Check that for non existing channels it returns the corresponding error
  gno = channel_get_last_delivered_gno(dummy_channel, fake_sidno);
  DBUG_ASSERT(gno == RPL_CHANNEL_SERVICE_CHANNEL_DOES_NOT_EXISTS_ERROR);

  // Extract the applier id
  long unsigned int *applier_id = NULL;
  channel_get_thread_id(interface_channel, CHANNEL_APPLIER_THREAD, &applier_id);
  DBUG_ASSERT(*applier_id > 0);
  my_free(applier_id);

  DBUG_ASSERT(binlog_relay_applier_stop_call == 0);

  // Stop the channel applier
  error = channel_stop(interface_channel, 3, 10000);
  DBUG_ASSERT(!error);
  // Repeat the stop to check it goes ok
  error = channel_stop(interface_channel, 3, 10000);
  DBUG_ASSERT(!error);

  DBUG_ASSERT(binlog_relay_applier_stop_call > 0);
  DBUG_ASSERT(!thread_aborted);

  // Assert that the applier thread is not running
  running = channel_is_active(interface_channel, CHANNEL_APPLIER_THREAD);
  DBUG_ASSERT(!running);

  // Purge the channel and assert all is OK
  error = channel_purge_queue(interface_channel, true);
  DBUG_ASSERT(!error);

  // Assert the channel is not there.
  exists = channel_is_active(interface_channel, CHANNEL_NO_THD);
  DBUG_ASSERT(!exists);

  // Check that a queue in an empty channel will fail.
  char empty_event[] = "";
  error = channel_queue_packet(dummy_channel, empty_event, 0);
  DBUG_ASSERT(error);

  // Test a multi thread channel
  info.channel_mts_parallel_type = CHANNEL_MTS_PARALLEL_TYPE_LOGICAL_CLOCK;
  info.channel_mts_parallel_workers = 3;

  error = channel_create(interface_channel, &info);
  DBUG_ASSERT(!error);

  // Assert the channel exists
  exists = channel_is_active(interface_channel, CHANNEL_NO_THD);
  DBUG_ASSERT(exists);

  error = channel_start(interface_channel, &connection_info,
                        CHANNEL_APPLIER_THREAD, true);
  DBUG_ASSERT(!error);

  // Extract the applier ids
  applier_id = NULL;
  int num_appliers = channel_get_thread_id(interface_channel,
                                           CHANNEL_APPLIER_THREAD, &applier_id);
  DBUG_ASSERT(num_appliers == 4);

  unsigned long thread_id = 0;
  for (int i = 0; i < num_appliers; i++) {
    thread_id = applier_id[i];
    DBUG_ASSERT(thread_id > 0);
  }
  my_free(applier_id);

  // Stop the channel applier
  error = channel_stop(interface_channel, 3, 10000);
  DBUG_ASSERT(!error);

  // Purge the channel and assert all is OK
  error = channel_purge_queue(interface_channel, true);
  DBUG_ASSERT(!error);

  // Assert the channel is not there.
  exists = channel_is_active(interface_channel, CHANNEL_NO_THD);
  DBUG_ASSERT(!exists);

  return (error && exists && running && gno && num_appliers && thread_id);
}

int test_channel_service_interface_io_thread() {
  // The initialization method should return OK
  int error = initialize_channel_service_interface();
  DBUG_ASSERT(!error);

  char interface_channel[] = "example_channel";

  // Assert the channel exists
  bool exists = channel_is_active(interface_channel, CHANNEL_NO_THD);
  DBUG_ASSERT(exists);

  // Assert that the receiver is running
  bool running = channel_is_active(interface_channel, CHANNEL_RECEIVER_THREAD);
  DBUG_ASSERT(running);

  // Extract the receiver id
  long unsigned int *thread_id = NULL;
  int num_threads = channel_get_thread_id(interface_channel,
                                          CHANNEL_RECEIVER_THREAD, &thread_id);
  DBUG_ASSERT(num_threads == 1);
  DBUG_ASSERT(*thread_id > 0);
  my_free(thread_id);

  // Get the I/O thread retrieved GTID set
  char *retrieved_gtid_set;
  error =
      channel_get_retrieved_gtid_set(interface_channel, &retrieved_gtid_set);
  DBUG_ASSERT(!error);
  DBUG_ASSERT(strlen(retrieved_gtid_set) > 0);
  my_free(retrieved_gtid_set);

  // Check that the applier thread is waiting for events to be queued.
  int is_waiting = channel_is_applier_waiting(interface_channel);
  DBUG_ASSERT(is_waiting == 1);

  // Stop the channel
  error = channel_stop(interface_channel, 3, 10000);
  DBUG_ASSERT(!error);

  // Assert that the receiver thread is not running
  running = channel_is_active(interface_channel, CHANNEL_RECEIVER_THREAD);
  DBUG_ASSERT(!running);

  return (error && exists && running && num_threads && is_waiting);
}

bool test_channel_service_interface_is_io_stopping() {
  // The initialization method should return OK
  int error = initialize_channel_service_interface();
  DBUG_ASSERT(!error);

  // Initialize the channel to be used with the channel service interface
  char interface_channel[] = "example_channel";
  Channel_creation_info info;
  initialize_channel_creation_info(&info);
  error = channel_create(interface_channel, &info);
  DBUG_ASSERT(!error);

  // Reset the I/O stop counter
  binlog_relay_thread_stop_call = 0;

  // Unregister the thread stop hook
  error = unregister_binlog_relay_io_observer(&relay_io_observer,
                                              (void *)plugin_info_ptr);
  DBUG_ASSERT(!error);

  // Start the I/O thread
  Channel_connection_info connection_info;
  initialize_channel_connection_info(&connection_info);
  error = channel_start(interface_channel, &connection_info,
                        CHANNEL_RECEIVER_THREAD, true);
  DBUG_ASSERT(!error);

  // Assert the channel exists
  bool exists = channel_is_active(interface_channel, CHANNEL_NO_THD);
  DBUG_ASSERT(exists);

  // Wait until I/O thread reached the error and is going to stop
  DBUG_EXECUTE_IF("pause_after_io_thread_stop_hook", {
    const char act[] =
        "now "
        "WAIT_FOR reached_stopping_io_thread";
    DBUG_ASSERT(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
  };);

  // Register the thread stop hook again
  error = register_binlog_relay_io_observer(&relay_io_observer,
                                            (void *)plugin_info_ptr);
  DBUG_ASSERT(!error);

  // Assert that the receiver is stopping
  bool io_stopping =
      channel_is_stopping(interface_channel, CHANNEL_RECEIVER_THREAD);
  DBUG_ASSERT(io_stopping);

  // Assert that the receiver is running
  bool io_running =
      channel_is_active(interface_channel, CHANNEL_RECEIVER_THREAD);
  DBUG_ASSERT(io_running);

  // Signal to make the MTR test case to start monitoring the I/O thread
  DBUG_EXECUTE_IF("pause_after_io_thread_stop_hook", {
    const char act[] =
        "now "
        "SIGNAL reached_io_thread_started";
    DBUG_ASSERT(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
  };);

  DBUG_EXECUTE_IF("pause_after_io_thread_stop_hook", {
    const char act[] = "now SIGNAL continue_to_stop_io_thread";
    DBUG_ASSERT(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
  };);

  // The plug-in has missed the stop
  DBUG_ASSERT(binlog_relay_thread_stop_call == 0);

  return (error | exists | io_stopping | io_running);
}

bool test_channel_service_interface_is_sql_stopping() {
  // The initialization method should return OK
  int error = initialize_channel_service_interface();
  DBUG_ASSERT(!error);

  // Initialize the channel to be used with the channel service interface
  char interface_channel[] = "example_channel";
  Channel_creation_info info;
  initialize_channel_creation_info(&info);
  error = channel_create(interface_channel, &info);
  DBUG_ASSERT(!error);

  // Assert the channel exists
  bool exists = channel_is_active(interface_channel, CHANNEL_NO_THD);
  DBUG_ASSERT(exists);

  // Unregister the thread stop hook
  error = unregister_binlog_relay_io_observer(&relay_io_observer,
                                              (void *)plugin_info_ptr);
  DBUG_ASSERT(!error);

  // Start the I/O thread
  Channel_connection_info connection_info;
  initialize_channel_connection_info(&connection_info);
  error = channel_start(interface_channel, &connection_info,
                        CHANNEL_RECEIVER_THREAD, true);
  DBUG_ASSERT(!error);

  // Start the SQL thread
  error = channel_start(interface_channel, &connection_info,
                        CHANNEL_APPLIER_THREAD, true);
  DBUG_ASSERT(!error);

  // Wait until SQL thread reached the error and is going to stop
  DBUG_EXECUTE_IF("pause_after_sql_thread_stop_hook", {
    const char act[] =
        "now "
        "WAIT_FOR reached_stopping_sql_thread";
    DBUG_ASSERT(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
  };);

  // Register the thread stop hook again
  error = register_binlog_relay_io_observer(&relay_io_observer,
                                            (void *)plugin_info_ptr);
  DBUG_ASSERT(!error);

  // Assert that the applier is stopping
  bool sql_stopping =
      channel_is_stopping(interface_channel, CHANNEL_APPLIER_THREAD);
  DBUG_ASSERT(sql_stopping);

  // Assert that the applier is running
  bool sql_running =
      channel_is_active(interface_channel, CHANNEL_APPLIER_THREAD);
  DBUG_ASSERT(sql_running);

  // Signal to make the MTR test case to start monitoring the SQL thread
  DBUG_EXECUTE_IF("pause_after_sql_thread_stop_hook", {
    const char act[] =
        "now "
        "SIGNAL reached_sql_thread_started";
    DBUG_ASSERT(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
  };);

  DBUG_EXECUTE_IF("pause_after_sql_thread_stop_hook", {
    const char act[] = "now SIGNAL continue_to_stop_sql_thread";
    DBUG_ASSERT(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
  };);

  // The plug-in has missed the stop
  DBUG_ASSERT(binlog_relay_applier_stop_call == 0);

  return (error | exists | sql_stopping | sql_running);
}

bool test_channel_service_interface_relay_log_renamed() {
  // The initialization method should return OK
  int error = initialize_channel_service_interface();
  DBUG_ASSERT(!error);

  // Initialize the channel to be used with the channel service interface
  char interface_channel[] = "example_channel";
  char channel_hostname[] = "127.0.0.1";
  char channel_user[] = "root";
  Channel_creation_info info;
  initialize_channel_creation_info(&info);
  info.preserve_relay_logs = true;
  info.hostname = channel_hostname;
  info.user = channel_user;
  error = channel_create(interface_channel, &info);
  DBUG_ASSERT(!error);

  // Assert the channel exists
  bool exists = channel_is_active(interface_channel, CHANNEL_NO_THD);
  DBUG_ASSERT(exists);

  // Start the SQL thread
  Channel_connection_info connection_info;
  initialize_channel_connection_info(&connection_info);
  error = channel_start(interface_channel, &connection_info,
                        CHANNEL_APPLIER_THREAD, true);

  if (error) {
    THD *thd = current_thd;
    thd->clear_error();
#if !defined(DBUG_OFF)
    const char act[] = "now SIGNAL reached_sql_thread_startup_failed";
    DBUG_ASSERT(!debug_sync_set_action(thd, STRING_WITH_LEN(act)));
#endif
  } else {
#if !defined(DBUG_OFF)
    const char act[] = "now SIGNAL reached_sql_thread_started";
    DBUG_ASSERT(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
#endif
  }

  return (error | exists);
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

static int replication_observers_example_plugin_init(MYSQL_PLUGIN plugin_info) {
  plugin_info_ptr = plugin_info;

  DBUG_ENTER("replication_observers_example_plugin_init");

  if (init_logging_service_for_plugin(&reg_srv, &log_bi, &log_bs))
    DBUG_RETURN(1);

  if (register_server_state_observer(&server_state_observer,
                                     (void *)plugin_info_ptr)) {
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG,
                 "Failure in registering the server state observers");
    deinit_logging_service_for_plugin(&reg_srv, &log_bi, &log_bs);
    DBUG_RETURN(1);
  }

  if (register_trans_observer(&trans_observer, (void *)plugin_info_ptr)) {
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG,
                 "Failure in registering the transactions state observers");
    deinit_logging_service_for_plugin(&reg_srv, &log_bi, &log_bs);
    DBUG_RETURN(1);
  }

  if (register_binlog_relay_io_observer(&relay_io_observer,
                                        (void *)plugin_info_ptr)) {
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG,
                 "Failure in registering the relay io observer");
    deinit_logging_service_for_plugin(&reg_srv, &log_bi, &log_bs);
    DBUG_RETURN(1);
  }

  LogPluginErr(INFORMATION_LEVEL, ER_LOG_PRINTF_MSG,
               "replication_observers_example_plugin: init finished");

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

static int replication_observers_example_plugin_deinit(void *p) {
  DBUG_ENTER("replication_observers_example_plugin_deinit");

  dump_server_state_calls();
  dump_transaction_calls();
  dump_binlog_relay_calls();

  if (unregister_server_state_observer(&server_state_observer, p)) {
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG,
                 "Failure in unregistering the server state observers");
    deinit_logging_service_for_plugin(&reg_srv, &log_bi, &log_bs);
    DBUG_RETURN(1);
  }

  if (unregister_trans_observer(&trans_observer, p)) {
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG,
                 "Failure in unregistering the transactions state observers");
    deinit_logging_service_for_plugin(&reg_srv, &log_bi, &log_bs);
    DBUG_RETURN(1);
  }

  if (unregister_binlog_relay_io_observer(&relay_io_observer, p)) {
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG,
                 "Failure in unregistering the relay io observer");
    deinit_logging_service_for_plugin(&reg_srv, &log_bi, &log_bs);
    DBUG_RETURN(1);
  }

  LogPluginErr(INFORMATION_LEVEL, ER_LOG_PRINTF_MSG,
               "replication_observers_example_plugin: deinit finished");
  deinit_logging_service_for_plugin(&reg_srv, &log_bi, &log_bs);

  DBUG_RETURN(0);
}

/*
  Plugin library descriptor
*/
struct Mysql_replication replication_observers_example_plugin = {
    MYSQL_REPLICATION_INTERFACE_VERSION};

mysql_declare_plugin(replication_observers_example){
    MYSQL_REPLICATION_PLUGIN,
    &replication_observers_example_plugin,
    "replication_observers_example",
    "ORACLE",
    "Replication observer infrastructure example.",
    PLUGIN_LICENSE_GPL,
    replication_observers_example_plugin_init,   /* Plugin Init */
    NULL,                                        /* Plugin Check uninstall */
    replication_observers_example_plugin_deinit, /* Plugin Deinit */
    0x0100 /* 1.0 */,
    NULL, /* status variables                */
    NULL, /* system variables                */
    NULL, /* config options                  */
    0,    /* flags                           */
} mysql_declare_plugin_end;
