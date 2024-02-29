/* Copyright (c) 2014, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <time.h>

#include <mysql/components/services/log_builtins.h>
#include "my_dbug.h"
#include "my_systime.h"
#include "plugin/group_replication/include/leave_group_on_failure.h"
#include "plugin/group_replication/include/member_info.h"
#include "plugin/group_replication/include/plugin.h"
#include "plugin/group_replication/include/plugin_handlers/recovery_metadata.h"
#include "plugin/group_replication/include/plugin_messages/recovery_message.h"
#include "plugin/group_replication/include/recovery.h"
#include "plugin/group_replication/include/recovery_channel_state_observer.h"
#include "plugin/group_replication/include/services/notification/notification.h"
#include "string_with_len.h"

using std::list;
using std::string;
using std::vector;

/*
  The maximum time in seconds till which recovery thread will wait for recovery
  metadata from sender.
*/
#define MAX_RECOVERY_METADATA_WAIT_TIME 300

/** The relay log name*/
static char recovery_channel_name[] = "group_replication_recovery";

static void *launch_handler_thread(void *arg) {
  Recovery_module *handler = (Recovery_module *)arg;
  handler->recovery_thread_handle();
  return nullptr;
}

Recovery_module::Recovery_module(Applier_module_interface *applier,
                                 Channel_observation_manager *channel_obsr_mngr)
    : applier_module(applier),
      recovery_state_transfer(recovery_channel_name,
                              local_member_info->get_uuid(), channel_obsr_mngr),
      recovery_thd_state(),
      m_until_condition(CHANNEL_UNTIL_VIEW_ID),
      m_max_metadata_wait_time(MAX_RECOVERY_METADATA_WAIT_TIME),
      m_state_transfer_return(STATE_TRANSFER_OK) {
  mysql_mutex_init(key_GR_LOCK_recovery_module_run, &run_lock,
                   MY_MUTEX_INIT_FAST);
  mysql_cond_init(key_GR_COND_recovery_module_run, &run_cond);
  mysql_mutex_init(key_GR_LOCK_recovery_metadata_receive,
                   &m_recovery_metadata_receive_lock, MY_MUTEX_INIT_FAST);
  mysql_cond_init(key_GR_COND_recovery_metadata_receive,
                  &m_recovery_metadata_receive_waiting_condition);
}

Recovery_module::~Recovery_module() {
  delete_recovery_metadata_message();
  mysql_mutex_destroy(&run_lock);
  mysql_cond_destroy(&run_cond);
  mysql_mutex_destroy(&m_recovery_metadata_receive_lock);
  mysql_cond_destroy(&m_recovery_metadata_receive_waiting_condition);
}

int Recovery_module::start_recovery(const string &group_name,
                                    const string &view_id) {
  DBUG_TRACE;

  mysql_mutex_lock(&run_lock);

  this->group_name = group_name;
  if (is_vcle_enable()) {
    m_until_condition = CHANNEL_UNTIL_VIEW_ID;
  } else {
    m_until_condition = CHANNEL_UNTIL_APPLIER_AFTER_GTIDS;
  }

  recovery_state_transfer.initialize(view_id, is_vcle_enable());

  // reset the recovery aborted status here to avoid concurrency
  recovery_aborted = false;

  // reset the value of state transfer return
  m_state_transfer_return = STATE_TRANSFER_OK;

  if (mysql_thread_create(key_GR_THD_recovery, &recovery_pthd,
                          get_connection_attrib(), launch_handler_thread,
                          (void *)this)) {
    /* purecov: begin inspected */
    mysql_mutex_unlock(&run_lock);
    return 1;
    /* purecov: end */
  }
  recovery_thd_state.set_created();

  while (recovery_thd_state.is_alive_not_running() && !recovery_aborted) {
    DBUG_PRINT("sleep", ("Waiting for recovery thread to start"));
    mysql_cond_wait(&run_cond, &run_lock);
  }
  mysql_mutex_unlock(&run_lock);

  return 0;
}

int Recovery_module::stop_recovery(bool wait_for_termination) {
  DBUG_TRACE;

  mysql_mutex_lock(&run_lock);

  if (recovery_thd_state.is_thread_dead()) {
    mysql_mutex_unlock(&run_lock);
    return 0;
  }

  recovery_aborted = true;

  while (recovery_thd_state.is_thread_alive() && wait_for_termination) {
    DBUG_PRINT("loop", ("killing group replication recovery thread"));

    if (recovery_thd_state.is_initialized()) {
      mysql_mutex_lock(&recovery_thd->LOCK_thd_data);

      recovery_thd->awake(THD::NOT_KILLED);
      mysql_mutex_unlock(&recovery_thd->LOCK_thd_data);

      // Break the wait for recovery metadata.
      awake_recovery_metadata_suspension(false);

      // Break the wait for the applier suspension
      applier_module->interrupt_applier_suspension_wait();
      // Break the state transfer process
      recovery_state_transfer.abort_state_transfer();
    }

    /*
      There is a small chance that thread might miss the first
      alarm. To protect against it, resend the signal until it reacts
    */
    struct timespec abstime;
    set_timespec(&abstime, 2);
#ifndef NDEBUG
    int error =
#endif
        mysql_cond_timedwait(&run_cond, &run_lock, &abstime);

    assert(error == ETIMEDOUT || error == 0);
  }

  assert((wait_for_termination && !recovery_thd_state.is_running()) ||
         !wait_for_termination);

  mysql_mutex_unlock(&run_lock);

  return (m_state_transfer_return == STATE_TRANSFER_STOP);
}

/*
 If recovery failed, it's no use to continue in the group as the member cannot
 take an active part in it, so it must leave.
*/
void Recovery_module::leave_group_on_recovery_failure() {
  // tell the update process that we are already stopping
  recovery_aborted = true;

  const char *exit_state_action_abort_log_message =
      "Fatal error in the recovery module of Group Replication.";
  leave_group_on_failure::mask leave_actions;
  leave_actions.set(leave_group_on_failure::HANDLE_EXIT_STATE_ACTION, true);
  leave_group_on_failure::leave(leave_actions, ER_GRP_RPL_FATAL_REC_PROCESS,
                                nullptr, exit_state_action_abort_log_message);
}

/*
  Recovery core method:

  * Step 0: Declare recovery running after extracting group information

  * Step 1: Wait for the applier to execute pending transactions and suspend.
    Even if the joiner is alone, it goes through this phase so it is declared
    ONLINE only when it executed all pending local transactions.

  * Step 2: Declare the node ONLINE if alone.
    This is done solely based on the number of member the group had when
    recovery started. No further group changes affect this decision.

  * Step 3: Wait for the recovery metadata and process it after receiving.
    Wait for the recovery metadata and after it is received decompress
    unserialize after_gtids and Certification info from received Recovery
    Metadata Message from the sender. The after_gtids received is used for
    asynchronous replication's Until Condition. The Certification info received
    is added to member's Certification info for detecting conflicts. This is
    only used when VCLE is not enabled and start recovery until condition is
    using after_gtids option instead of view_id.

    Step 3.1: Wait for the recovery metadata from the sender.
    Step 3.2: Set replication's Until Condition after_gtids from received
              Recovery Metadata.

    Step 3.3: Set Certification info from received Recovery Metadata Message.

  * Step 4:  State transfer.
    This can be summarized as:
      1) Connect to a donor
      2) Wait until the data comes.
    It can be interrupted/terminated by:
      > recovery_aborted is set to true. This means recovery was aborted.
        The wait is awaken and the loop is broken. The thread shutdowns.
      > on_failover is set to true. This means the current donor left.
        The loop cycles and another donor is selected.
        The threads are stopped but the logs are not purged.
        A connections is established to another donor.
      > donor_channel_applier_error is set to true. This means an error was
        detected in the recovery applier.
        When the loop cycles, kill the threads and purge the logs
        A connections is established to another donor.
      > donor_transfer_finished. This means we received all the data.
        The loop exits

  * Step 5: Set certifier gtid_executed after recovery has applied
    transactions.
    This is only used when VCLE is not enabled and start recovery until
    condition is using after_gtids option instead of view_id.

  * Step 6: Awake the applier and wait for the execution of cached transactions.

  * Step 7: Notify the group that we are now online if no error occurred.
    This is done even if the member is alone in the group.

  * Step 8: If an error occurred and recovery is impossible leave the group.
    We leave the group but the plugin is left running.

  * Step 9: Terminate the recovery thread.
*/
int Recovery_module::recovery_thread_handle() {
  DBUG_TRACE;

  /* Step 0 */

  int error = 0;
  Plugin_stage_monitor_handler stage_handler;

  if (stage_handler.initialize_stage_monitor())
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_NO_STAGE_SERVICE);

  set_recovery_thread_context();
  mysql_mutex_lock(&run_lock);
  recovery_thd_state.set_initialized();
  mysql_mutex_unlock(&run_lock);

  // take this before the start method returns
  size_t number_of_members = group_member_mgr->get_number_of_members();
  recovery_state_transfer.initialize_group_info();

  mysql_mutex_lock(&run_lock);
  recovery_thd_state.set_running();
  stage_handler.set_stage(info_GR_STAGE_module_executing.m_key, __FILE__,
                          __LINE__, 0, 0);
  mysql_cond_broadcast(&run_cond);
  mysql_mutex_unlock(&run_lock);

  /* Step 1 */

  // wait for the appliers suspension

  std::string gtid_set_to_apply;
  channel_get_gtid_set_to_apply("group_replication_applier", gtid_set_to_apply);

  if (!gtid_set_to_apply.empty()) {
    const size_t max_length = 7000;
    if (gtid_set_to_apply.size() > max_length) {
      gtid_set_to_apply.resize(max_length);
      gtid_set_to_apply.replace(max_length - 3, 3, "...");
    }

    LogPluginErr(SYSTEM_LEVEL, ER_GRP_RPL_RECOVERY_WAIT_APPLIER_BACKLOG_START,
                 gtid_set_to_apply.c_str());
  }

  error =
      applier_module->wait_for_applier_complete_suspension(&recovery_aborted);

  // If the applier is already stopped then something went wrong and we are
  // already leaving the group
  if (error == APPLIER_THREAD_ABORTED) {
    /* purecov: begin inspected */
    error = 0;
    recovery_aborted = true;
    goto cleanup;
    /* purecov: end */
  }

  if (!recovery_aborted && error) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_UNABLE_TO_EVALUATE_APPLIER_STATUS);
    goto cleanup;
    /* purecov: end */
  }

  if (!gtid_set_to_apply.empty()) {
    LogPluginErr(SYSTEM_LEVEL, ER_GRP_RPL_RECOVERY_WAIT_APPLIER_BACKLOG_FINISH,
                 gtid_set_to_apply.c_str());
  }

#ifndef NDEBUG
  DBUG_EXECUTE_IF("recovery_thread_start_wait_num_of_members", {
    assert(number_of_members != 1);
    DBUG_SET("d,recovery_thread_start_wait");
  });
  DBUG_EXECUTE_IF("recovery_thread_start_wait", {
    const char act[] =
        "now signal signal.recovery_waiting wait_for signal.recovery_continue";
    assert(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
  });
#endif  // NDEBUG

  /* Step 2 */

  if (number_of_members == 1) {
    if (!recovery_aborted) {
      LogPluginErr(INFORMATION_LEVEL, ER_GRP_RPL_ONLY_ONE_SERVER_ALIVE);
    }
    goto single_member_online;
  }

  /* Step 3 */

  // Wait for the recovery metadata and process it after receiving.
  if (m_until_condition == CHANNEL_UNTIL_APPLIER_AFTER_GTIDS) {
    // Step 3.1: Wait for the recovery metadata from sender.
    enum_recovery_metadata_error recovery_metadata_error_status =
        wait_for_recovery_metadata_gtid_executed();

    // If sender is unable to send the recovery metadata then set error.
    if (recovery_metadata_error_status ==
        enum_recovery_metadata_error::RECOVERY_METADATA_RECEIVED_ERROR) {
      error = 1;
      LogPluginErr(ERROR_LEVEL,
                   ER_GROUP_REPLICATION_ERROR_RECEIVED_WAITING_METADATA);
      goto cleanup;
    } else if (recovery_metadata_error_status ==
               enum_recovery_metadata_error::
                   RECOVERY_METADATA_RECOVERY_ABORTED_ERROR) {
      goto cleanup;
    } else if (recovery_metadata_error_status ==
               enum_recovery_metadata_error::
                   RECOVERY_METADATA_RECEIVED_TIMEOUT_ERROR) {
      error = 1;
      LogPluginErr(ERROR_LEVEL,
                   ER_GROUP_REPLICATION_TIMEOUT_ERROR_FETCHING_METADATA,
                   m_max_metadata_wait_time);
      goto cleanup;
    } else if (m_recovery_metadata_message != nullptr) {
      /*
        Step 3.2: Set replication's Until Condition after_gtids from received
                 Recovery Metadata.
      */
      std::pair<Recovery_metadata_message::enum_recovery_metadata_message_error,
                std::reference_wrapper<std::string>>
          payload_after_gtids_error =
              m_recovery_metadata_message->get_decoded_group_gtid_executed();

      /*
        3.2.1: Check for error.
               PIT_UNTIL_CONDITION_AFTER_GTIDS can be empty when starting a
               new group.
      */
      if (payload_after_gtids_error.first !=
              Recovery_metadata_message::enum_recovery_metadata_message_error::
                  RECOVERY_METADATA_MESSAGE_OK &&
          payload_after_gtids_error.first !=
              Recovery_metadata_message::enum_recovery_metadata_message_error::
                  ERR_CERT_INFO_EMPTY) {
        error = 1;
        LogPluginErr(ERROR_LEVEL,
                     ER_GROUP_REPLICATION_METADATA_PAYLOAD_DECODING);
        goto cleanup;
      }

      // 3.2.2: Save after_gtids if not error.
      if (payload_after_gtids_error.first ==
          Recovery_metadata_message::enum_recovery_metadata_message_error::
              RECOVERY_METADATA_MESSAGE_OK) {
        recovery_state_transfer.set_until_condition_after_gtids(
            payload_after_gtids_error.second.get());
      }

      /*
        Step 3.3: Set Certification info from received Recovery Metadata
                  Message.
      */
      Certifier_interface *certifier_module =
          (applier_module && applier_module->get_certification_handler())
              ? applier_module->get_certification_handler()->get_certifier()
              : nullptr;
      if (certifier_module != nullptr) {
        bool status = false;
        try {
          /*
            3.2.1: Decode recovery metadata message and get Certification info.
          */
          status = certifier_module->set_certification_info_recovery_metadata(
              m_recovery_metadata_message);
        } catch (const std::bad_alloc &) {
          LogPluginErr(ERROR_LEVEL, ER_GROUP_REPLICATION_METADATA_MEMORY_ALLOC,
                       "decoding and saving certification information");
          error = 1;
          goto cleanup;
        }

        // 3.2.2: Check for error.
        if (status) {
          LogPluginErr(
              ERROR_LEVEL,
              ER_GROUP_REPLICATION_METADATA_CERT_INFO_ERROR_PROCESSING);
          error = 1;
          goto cleanup;
        }
      } else {
        LogPluginErr(ERROR_LEVEL,
                     ER_GROUP_REPLICATION_CERTIFICATION_MODULE_FAILURE);
        error = 1;
        goto cleanup;
      }
    } else {
      LogPluginErr(ERROR_LEVEL,
                   ER_GROUP_REPLICATION_METADATA_INITIALIZATION_FAILURE);
      error = 1;
      goto cleanup;
    }
  }

  /* Step 4 */

  m_state_transfer_return =
      recovery_state_transfer.state_transfer(stage_handler);
  error = m_state_transfer_return;

  stage_handler.set_stage(info_GR_STAGE_module_executing.m_key, __FILE__,
                          __LINE__, 0, 0);

#ifndef NDEBUG
  DBUG_EXECUTE_IF("recovery_thread_wait_before_finish", {
    const char act[] =
        "now signal signal.recovery_thread_wait_before_finish_reached "
        "wait_for signal.recovery_end";
    assert(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
  });
#endif  // NDEBUG

  if (error) {
    goto cleanup;
  }

  /* Step 5 */

  /*
    Recovery metadata message is set only on joiner,
    after data is received by joining member.
  */
  if (!recovery_aborted && !error &&
      m_until_condition == CHANNEL_UNTIL_APPLIER_AFTER_GTIDS) {
    Certifier_interface *certifier_module =
        (applier_module && applier_module->get_certification_handler())
            ? applier_module->get_certification_handler()->get_certifier()
            : nullptr;
    if (certifier_module != nullptr) {
      // update gtid_set after recovery
      certifier_module->initialize_server_gtid_set_after_distributed_recovery();
    } else {
      LogPluginErr(ERROR_LEVEL,
                   ER_GROUP_REPLICATION_CERTIFICATION_MODULE_FAILURE);
      error = 1;
      goto cleanup;
    }
  }

single_member_online:

  /* Step 6 */
  if (!recovery_aborted && !error) {
    /*
      Recovery through `group_replication_recovery` is complete,
      enable the guarantee that the binlog commit order will
      follow the order instructed by GR.
    */
    Commit_stage_manager::enable_manual_session_tickets();
  }

  /**
    If recovery fails or is aborted, it never makes sense to awake the applier,
    as that would lead to the certification and execution of transactions on
    the wrong context.
  */
  if (!recovery_aborted) applier_module->awake_applier_module();

#ifndef NDEBUG
  DBUG_EXECUTE_IF(
      "recovery_thread_wait_before_wait_for_applier_module_recovery", {
        const char act[] =
            "now signal "
            "signal.recovery_thread_wait_before_wait_for_applier_module_"
            "recovery "
            "wait_for "
            "signal.recovery_thread_resume_before_wait_for_applier_module_"
            "recovery";
        assert(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
      });
#endif  // NDEBUG

  error = wait_for_applier_module_recovery();

#ifndef NDEBUG
  DBUG_EXECUTE_IF(
      "recovery_thread_wait_after_wait_for_applier_module_recovery", {
        const char act[] =
            "now signal "
            "signal.recovery_thread_wait_after_wait_for_applier_module_"
            "recovery "
            "wait_for "
            "signal.recovery_thread_resume_after_wait_for_applier_module_"
            "recovery";
        assert(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
      });
#endif  // NDEBUG

cleanup:

  /* Step 7 */

  // if finished, declare the member online
  if (!recovery_aborted && !error) {
    notify_group_recovery_end();
  }

  /* Step 8 */

  /*
   If recovery failed, it's no use to continue in the group as the member cannot
   take an active part in it, so it must leave.
  */
  if (!recovery_aborted && error) {
    leave_group_on_recovery_failure();
  }

  stage_handler.end_stage();
  stage_handler.terminate_stage_monitor();
#ifndef NDEBUG
  DBUG_EXECUTE_IF("recovery_thread_wait_before_cleanup", {
    const char act[] = "now wait_for signal.recovery_end_end";
    debug_sync_set_action(current_thd, STRING_WITH_LEN(act));
  });
#endif  // NDEBUG

  /* Step 9 */

  delete_recovery_metadata_message();
  clean_recovery_thread_context();

  mysql_mutex_lock(&run_lock);

  recovery_aborted = true;  // to avoid the start missing signals
  delete recovery_thd;

  Gcs_interface_factory::cleanup_thread_communication_resources(
      Gcs_operations::get_gcs_engine());

  my_thread_end();
  recovery_thd_state.set_terminated();
  mysql_cond_broadcast(&run_cond);
  mysql_mutex_unlock(&run_lock);
  my_thread_exit(nullptr);

  return error; /* purecov: inspected */
}

int Recovery_module::update_recovery_process(bool did_members_left,
                                             bool is_leaving) {
  DBUG_TRACE;

  int error = 0;

  if (recovery_thd_state.is_running()) {
    /*
      If I left the Group... the group manager will only have me so recovery
      should stop.
      But if it was the plugin that chose to leave the group then it will stop
      by recovery in the process.
    */
    if (is_leaving && !recovery_aborted) {
      stop_recovery(!is_leaving); /* Do not wait for recovery thread
                                     termination if member is leaving */
    } else if (!recovery_aborted) {
      recovery_state_transfer.update_recovery_process(did_members_left);
    }
  }

  return error;
}

int Recovery_module::set_retrieved_cert_info(void *info) {
  DBUG_TRACE;

  View_change_log_event *view_change_event =
      static_cast<View_change_log_event *>(info);
  // Transmit the certification info into the pipeline
  Handler_certifier_information_action *cert_action =
      new Handler_certifier_information_action(
          view_change_event->get_certification_info());

  int error = applier_module->handle_pipeline_action(cert_action);
  delete cert_action;
  if (error) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_CERTIFICATION_REC_PROCESS);
    leave_group_on_recovery_failure();
    return 1;
    /* purecov: end */
  }

  recovery_state_transfer.end_state_transfer();

  return 0;
}

void Recovery_module::set_recovery_thread_context() {
  THD *thd = new THD;
  my_thread_init();
  thd->set_new_thread_id();
  thd->thread_stack = (char *)&thd;
  thd->store_globals();

  global_thd_manager_add_thd(thd);
  // Needed to start replication threads
  thd->security_context()->skip_grants();

  recovery_thd = thd;
}

void Recovery_module::clean_recovery_thread_context() {
  recovery_thd->release_resources();
  global_thd_manager_remove_thd(recovery_thd);
}

int Recovery_module::wait_for_applier_module_recovery() {
  DBUG_TRACE;

  size_t queue_size = 0;
  uint64 transactions_applied_during_recovery_delta = 0;
  bool applier_monitoring = true;
  Pipeline_stats_member_collector *pipeline_stats =
      applier_module->get_pipeline_stats_member_collector();

  while (!recovery_aborted && applier_monitoring) {
    queue_size = applier_module->get_message_queue_size();
    transactions_applied_during_recovery_delta =
        pipeline_stats->get_delta_transactions_applied_during_recovery();

    /*
      When the recovery completion policy is wait for transactions apply,
      the member will first wait until one of the conditions is fulfilled:
       1) the transactions to apply do fit on the flow control period
          configuration, that is, the transactions to apply can be applied
          on the next flow control iteration;
       2) no transactions are being queued or applied, the case of empty
          recovery. We need to also check if the applier is waiting for
          transactions, otherwise we may have a transactions applied delta
          equal to zero because the applier is processing a big transaction.
          If the applier is stopped, channel_is_applier_waiting() will
          return a negative value, the wait_for_gtid_execution() will detect
          the error and change the member state to ERROR.
      Then, the member will wait for the apply of the currently queued
      transactions on the group_replication_applier channel, before the
      member state changes to ONLINE.
    */
    if ((pipeline_stats->get_transactions_waiting_apply_during_recovery() <=
         transactions_applied_during_recovery_delta) ||
        (0 == queue_size && 0 == transactions_applied_during_recovery_delta &&
         0 != channel_is_applier_waiting("group_replication_applier"))) {
      /*
        Fetch current retrieved gtid set of group_replication_applier channel.
      */
      std::string applier_retrieved_gtids;
      Replication_thread_api applier_channel("group_replication_applier");
      if (applier_channel.get_retrieved_gtid_set(applier_retrieved_gtids)) {
        /* purecov: begin inspected */
        LogPluginErr(WARNING_LEVEL,
                     ER_GRP_RPL_GTID_SET_EXTRACT_ERROR_DURING_RECOVERY);
        return 1;
        /* purecov: end */
      }
      /*
        Wait for the View_change_log_event to be queued, otherwise the
        member can be declared ONLINE before the view is applied when
        there are no transactions to apply.
        If vcle is not getting logged
        i.e. m_until_condition == CHANNEL_UNTIL_APPLIER_AFTER_GTIDS, then
        applier retrieved gtid will always be empty, so this check should be
        skipped.
      */
      if (m_until_condition == CHANNEL_UNTIL_VIEW_ID &&
          applier_retrieved_gtids.empty()) {
        continue;
      }

      int error = 1;
      while (!recovery_aborted && error != 0) {
        /*
          Wait until the fetched gtid set is applied.
        */
        error =
            applier_channel.wait_for_gtid_execution(applier_retrieved_gtids, 1);

        /* purecov: begin inspected */
        if (error == -2)  // error when waiting
        {
          LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_UNABLE_TO_ENSURE_EXECUTION_REC);
          return 1;
        }
        /* purecov: end */
      }

      applier_monitoring = false;
    } else {
      my_sleep(100 * std::min(queue_size, static_cast<size_t>(5000)));
    }
  }

  if (applier_module->get_applier_status() == APPLIER_ERROR &&
      !recovery_aborted)
    return 1; /* purecov: inspected */

  return 0;
}

void Recovery_module::notify_group_recovery_end() {
  DBUG_TRACE;

  Recovery_message recovery_msg(Recovery_message::RECOVERY_END_MESSAGE,
                                local_member_info->get_uuid());
  enum_gcs_error msg_error = gcs_module->send_message(recovery_msg);
  if (msg_error != GCS_OK) {
    LogPluginErr(ERROR_LEVEL,
                 ER_GRP_RPL_WHILE_SENDING_MSG_REC); /* purecov: inspected */
  }
}

bool Recovery_module::is_own_event_channel(my_thread_id id) {
  DBUG_TRACE;
  return recovery_state_transfer.is_own_event_channel(id);
}

int Recovery_module::check_recovery_thread_status() {
  DBUG_TRACE;
  if (recovery_state_transfer.check_recovery_thread_status()) {
    return GROUP_REPLICATION_RECOVERY_CHANNEL_STILL_RUNNING;
  }

  return 0;
}

Recovery_module::enum_recovery_metadata_error
Recovery_module::wait_for_recovery_metadata_gtid_executed() {
  enum_recovery_metadata_error recovery_metadata_error_status{
      enum_recovery_metadata_error::RECOVERY_METADATA_RECEIVED_NO_ERROR};

  mysql_mutex_lock(&m_recovery_metadata_receive_lock);
  DBUG_EXECUTE_IF("gr_set_metadata_wait_time_10",
                  { m_max_metadata_wait_time = 10; });

  unsigned int wait_counter{0};
  while (!m_recovery_metadata_received && !recovery_aborted &&
         wait_counter <= m_max_metadata_wait_time) {
    struct timespec abstime;
    set_timespec(&abstime, 1);
    mysql_cond_timedwait(&m_recovery_metadata_receive_waiting_condition,
                         &m_recovery_metadata_receive_lock, &abstime);
    wait_counter++;
  }

  if (!m_recovery_metadata_received &&
      wait_counter > m_max_metadata_wait_time) {
    recovery_metadata_error_status =
        enum_recovery_metadata_error::RECOVERY_METADATA_RECEIVED_TIMEOUT_ERROR;
  }

  if (m_recovery_metadata_received_error || recovery_aborted) {
    recovery_metadata_error_status =
        enum_recovery_metadata_error::RECOVERY_METADATA_RECEIVED_ERROR;
  }

  if (recovery_aborted) {
    recovery_metadata_error_status =
        enum_recovery_metadata_error::RECOVERY_METADATA_RECOVERY_ABORTED_ERROR;
  }

  mysql_mutex_unlock(&m_recovery_metadata_receive_lock);
  return recovery_metadata_error_status;
}

void Recovery_module::awake_recovery_metadata_suspension(bool error) {
  mysql_mutex_lock(&m_recovery_metadata_receive_lock);
  m_recovery_metadata_received_error = error;
  m_recovery_metadata_received = true;
  mysql_cond_broadcast(&m_recovery_metadata_receive_waiting_condition);
  mysql_mutex_unlock(&m_recovery_metadata_receive_lock);
}

void Recovery_module::suspend_recovery_metadata() {
  mysql_mutex_lock(&m_recovery_metadata_receive_lock);
  m_recovery_metadata_received = false;
  m_recovery_metadata_received_error = false;
  mysql_mutex_unlock(&m_recovery_metadata_receive_lock);
}

bool Recovery_module::set_recovery_metadata_message(
    Recovery_metadata_message *recovery_metadata_message) {
  if (m_recovery_metadata_message != nullptr) {
    return true;
  }

  // m_recovery_metadata_message is set only on joiner
  m_recovery_metadata_message = recovery_metadata_message;
  return false;
}

void Recovery_module::delete_recovery_metadata_message() {
  if (m_recovery_metadata_message != nullptr) {
    delete m_recovery_metadata_message;
    m_recovery_metadata_message = nullptr;
  }
}

bool Recovery_module::is_vcle_enable() { return m_is_vcle_enable; }

void Recovery_module::set_vcle_enabled(bool is_vcle_enable) {
  m_is_vcle_enable = is_vcle_enable;
}
