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

#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <time.h>

#include "my_dbug.h"
#include "my_systime.h"
#include "plugin/group_replication/include/member_info.h"
#include "plugin/group_replication/include/plugin.h"
#include "plugin/group_replication/include/plugin_log.h"
#include "plugin/group_replication/include/recovery.h"
#include "plugin/group_replication/include/recovery_channel_state_observer.h"
#include "plugin/group_replication/include/recovery_message.h"
#include "plugin/group_replication/include/services/notification/notification.h"

using std::list;
using std::string;
using std::vector;

/** The number of queued transactions below which we declare the member online */
static uint RECOVERY_TRANSACTION_THRESHOLD= 0;

/** The relay log name*/
static char recovery_channel_name[]= "group_replication_recovery";

static void *launch_handler_thread(void* arg)
{
  Recovery_module *handler= (Recovery_module*) arg;
  handler->recovery_thread_handle();
  return 0;
}

Recovery_module::
Recovery_module(Applier_module_interface *applier,
                Channel_observation_manager *channel_obsr_mngr,
                ulong components_stop_timeout)
  : applier_module(applier), recovery_state_transfer(recovery_channel_name,
    local_member_info->get_uuid(), channel_obsr_mngr),
    recovery_running(false), recovery_starting(false),
    recovery_completion_policy(RECOVERY_POLICY_WAIT_CERTIFIED),
    stop_wait_timeout(components_stop_timeout)
{
  mysql_mutex_init(key_GR_LOCK_recovery_module_run, &run_lock, MY_MUTEX_INIT_FAST);
  mysql_cond_init(key_GR_COND_recovery_module_run, &run_cond);
}

Recovery_module::~Recovery_module()
{
  mysql_mutex_destroy(&run_lock);
  mysql_cond_destroy(&run_cond);
}

int
Recovery_module::start_recovery(const string& group_name,
                                const string& rec_view_id)
{
  DBUG_ENTER("Recovery_module::start_recovery");

  mysql_mutex_lock(&run_lock);

  if (recovery_state_transfer.check_recovery_thread_status())
  {
    /* purecov: begin inspected */
    log_message(MY_ERROR_LEVEL,
                "A previous recovery session is still running. "
                "Please stop the group replication plugin and"
                " wait for it to stop.");
    DBUG_RETURN(1);
    /* purecov: end */
  }

  this->group_name= group_name;
  recovery_state_transfer.initialize(rec_view_id);

  //reset the recovery aborted status here to avoid concurrency
  recovery_aborted= false;

  recovery_starting= true;

  if (mysql_thread_create(key_GR_THD_recovery,
                          &recovery_pthd,
                          get_connection_attrib(),
                          launch_handler_thread,
                          (void*)this))
  {
    /* purecov: begin inspected */
    mysql_mutex_unlock(&run_lock);
    DBUG_RETURN(1);
    /* purecov: end */
  }

  while (!recovery_running && !recovery_aborted)
  {
    DBUG_PRINT("sleep",("Waiting for recovery thread to start"));
    mysql_cond_wait(&run_cond, &run_lock);
  }
  mysql_mutex_unlock(&run_lock);

  DBUG_RETURN(0);
}

int
Recovery_module::stop_recovery()
{
  DBUG_ENTER("Recovery_module::stop_recovery");

  mysql_mutex_lock(&run_lock);

  if (!recovery_running)
  {
    if (!recovery_starting)
    {
      mysql_mutex_unlock(&run_lock);
      DBUG_RETURN(0);
    }
  }

  recovery_aborted= true;

  while (recovery_running || recovery_starting)
  {
    DBUG_PRINT("loop", ("killing group replication recovery thread"));

    mysql_mutex_lock(&recovery_thd->LOCK_thd_data);

    recovery_thd->awake(THD::NOT_KILLED);
    mysql_mutex_unlock(&recovery_thd->LOCK_thd_data);

    //Break the wait for the applier suspension
    applier_module->interrupt_applier_suspension_wait();
    //Break the state transfer process
    recovery_state_transfer.abort_state_transfer();

    /*
      There is a small chance that thread might miss the first
      alarm. To protect against it, resend the signal until it reacts
    */
    struct timespec abstime;
    set_timespec(&abstime, 2);
#ifndef DBUG_OFF
    int error=
#endif
    mysql_cond_timedwait(&run_cond, &run_lock, &abstime);
    if (stop_wait_timeout >= 2)
    {
      stop_wait_timeout= stop_wait_timeout - 2;
    }
    /* purecov: begin inspected */
    else if (recovery_running) // quit waiting
    {
      mysql_mutex_unlock(&run_lock);
      DBUG_RETURN(1);
    }
    /* purecov: inspected */
    DBUG_ASSERT(error == ETIMEDOUT || error == 0);
  }

  DBUG_ASSERT(!recovery_running);

  mysql_mutex_unlock(&run_lock);

  DBUG_RETURN(0);
}

  /*
   If recovery failed, it's no use to continue in the group as the member cannot
   take an active part in it, so it must leave.
  */
void Recovery_module::leave_group_on_recovery_failure()
{
  Notification_context ctx;
  log_message(MY_ERROR_LEVEL, "Fatal error during the Recovery process of "
              "Group Replication. The server will leave the group.");
  //tell the update process that we are already stopping
  recovery_aborted= true;

  //If you can't leave at least force the Error state.
  group_member_mgr->update_member_status(local_member_info->get_uuid(),
                                         Group_member_info::MEMBER_ERROR,
                                         ctx);

  /* Single state update. Notify right away. */
  notify_and_reset_ctx(ctx);

  Gcs_operations::enum_leave_state state= gcs_module->leave();

  char **error_message= NULL;
  int error= channel_stop_all(CHANNEL_APPLIER_THREAD|CHANNEL_RECEIVER_THREAD,
                              stop_wait_timeout, error_message);
  if (error)
  {
    if (error_message != NULL && *error_message != NULL)
    {
      log_message(MY_ERROR_LEVEL,
                  "Error stopping all replication channels while server was"
                  " leaving the group. %s", *error_message);
      my_free(error_message);
    }
    else
    {
      log_message(MY_ERROR_LEVEL,
                  "Error stopping all replication channels while server was"
                  " leaving the group. Got error: %d. Please check the error"
                  " log for more details.", error);
    }
  }

  std::stringstream ss;
  plugin_log_level log_severity= MY_WARNING_LEVEL;
  switch (state)
  {
    case Gcs_operations::ERROR_WHEN_LEAVING:
      /* purecov: begin inspected */
      ss << "Unable to confirm whether the server has left the group or not. "
            "Check performance_schema.replication_group_members to check group membership information.";
      log_severity= MY_ERROR_LEVEL;
      break;
      /* purecov: end */
    case Gcs_operations::ALREADY_LEAVING:
      ss << "Skipping leave operation: concurrent attempt to leave the group is on-going.";
      break;
    case Gcs_operations::ALREADY_LEFT:
      /* purecov: begin inspected */
      ss << "Skipping leave operation: member already left the group.";
      break;
      /* purecov: end */
    case Gcs_operations::NOW_LEAVING:
      return;
  }
  log_message(log_severity, ss.str().c_str());
}

/*
  Recovery core method:

  * Step 0: Declare recovery running after extracting group information

  * Step 1: Wait for the applier to execute pending transactions and suspend.
    Even if the joiner is alone, it goes trough this phase so it is declared
    ONLINE only when it executed all pending local transactions.

  * Step 2: Declare the node ONLINE if alone.
    This is done solely based on the number of member the group had when
    recovery started. No further group changes affect this decision.

  * Step 3:  State transfer.
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

  * Step 4: Awake the applier and wait for the execution of cached transactions.

  * Step 5: Notify the group that we are now online if no error occurred.
    This is done even if the member is alone in the group.

  * Step 6: If an error occurred and recovery is impossible leave the group.
    We leave the group but the plugin is left running.

  * Step 7: Terminate the recovery thread.
*/
int
Recovery_module::recovery_thread_handle()
{
  DBUG_ENTER("Recovery_module::recovery_thread_handle");

  /* Step 0 */

  int error= 0;

  set_recovery_thread_context();

  //take this before the start method returns
  size_t number_of_members= group_member_mgr->get_number_of_members();
  recovery_state_transfer.initialize_group_info();

  mysql_mutex_lock(&run_lock);
  recovery_running= true;
  recovery_starting= false;
  mysql_cond_broadcast(&run_cond);
  mysql_mutex_unlock(&run_lock);

#ifndef _WIN32
  THD_STAGE_INFO(recovery_thd, stage_executing);
#endif

  /* Step 1 */

  //wait for the appliers suspension
  error= applier_module->wait_for_applier_complete_suspension(&recovery_aborted);

  //If the applier is already stopped then something went wrong and we are
  //already leaving the group
  if (error == APPLIER_THREAD_ABORTED)
  {
      /* purecov: begin inspected */
      error= 0;
      recovery_aborted= true;
      goto cleanup;
      /* purecov: end */
  }

  if (!recovery_aborted && error)
  {
    /* purecov: begin inspected */
    log_message(MY_ERROR_LEVEL,
                "Can't evaluate the group replication applier execution status. "
                "Group replication recovery will shutdown to avoid data "
                "corruption.");
    goto cleanup;
    /* purecov: end */
  }

#ifndef DBUG_OFF
  DBUG_EXECUTE_IF("recovery_thread_start_wait_num_of_members",
                  {
                    DBUG_ASSERT(number_of_members != 1);
                    DBUG_SET("d,recovery_thread_start_wait");
                  });
  DBUG_EXECUTE_IF("recovery_thread_start_wait",
                  {
                    const char act[]= "now signal signal.recovery_waiting wait_for signal.recovery_continue";
                    DBUG_ASSERT(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
                  });
#endif // DBUG_OFF

  /* Step 2 */

  if (number_of_members == 1)
  {
    if (!recovery_aborted)
    {
      log_message(MY_INFORMATION_LEVEL,
                  "Only one server alive."
                  " Declaring this server as online within the replication group");
    }
    goto single_member_online;
  }

  /* Step 3 */

  error= recovery_state_transfer.state_transfer(recovery_thd);

#ifndef DBUG_OFF
  DBUG_EXECUTE_IF("recovery_thread_wait_before_finish",
                  {
                    const char act[]= "now wait_for signal.recovery_end";
                    DBUG_ASSERT(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
                  });
#endif // DBUG_OFF

  if (error)
  {
    goto cleanup;
  }

single_member_online:

  /* Step 4 */

  /**
    If recovery fails or is aborted, it never makes sense to awake the applier,
    as that would lead to the certification and execution of transactions on
    the wrong context.
  */
  if (!recovery_aborted)
    applier_module->awake_applier_module();

  error= wait_for_applier_module_recovery();

cleanup:

  /* Step 5 */

  //if finished, declare the member online
  if (!recovery_aborted && !error)
  {
    notify_group_recovery_end();
  }

  /* Step 6 */

  /*
   If recovery failed, it's no use to continue in the group as the member cannot
   take an active part in it, so it must leave.
  */
  if (error)
  {
    leave_group_on_recovery_failure();
  }

#ifndef DBUG_OFF
  DBUG_EXECUTE_IF("recovery_thread_wait_before_cleanup",
                  {
                    const char act[]= "now wait_for signal.recovery_end_end";
                    debug_sync_set_action(current_thd, STRING_WITH_LEN(act));
                  });
#endif // DBUG_OFF

  /* Step 7 */

  clean_recovery_thread_context();

  mysql_mutex_lock(&run_lock);
  delete recovery_thd;

  recovery_aborted= true;  // to avoid the start missing signals
  recovery_running= false;
  mysql_cond_broadcast(&run_cond);
  mysql_mutex_unlock(&run_lock);

  Gcs_interface_factory::cleanup(Gcs_operations::get_gcs_engine());

  my_thread_end();
  my_thread_exit(0);

  DBUG_RETURN(error); /* purecov: inspected */
}

int
Recovery_module::update_recovery_process(bool did_members_left, bool is_leaving)
{
  DBUG_ENTER("Recovery_module::update_recovery_process");

  int error= 0;

  if (recovery_running)
  {
    /*
      If I left the Group... the group manager will only have me so recovery
      should stop.
      But if it was the plugin that chose to leave the group then it will stop
      by recovery in the process.
    */
    if (is_leaving && !recovery_aborted)
    {
      stop_recovery();
    }
    else if (!recovery_aborted)
    {
      recovery_state_transfer.update_recovery_process(did_members_left);
    }
  }

  DBUG_RETURN(error);
}

int
Recovery_module::set_retrieved_cert_info(void* info)
{
  DBUG_ENTER("Recovery_module::set_retrieved_cert_info");

  View_change_log_event* view_change_event= static_cast<View_change_log_event*>(info);
  // Transmit the certification info into the pipeline
  Handler_certifier_information_action *cert_action=
    new Handler_certifier_information_action(view_change_event->get_certification_info());

  int error= applier_module->handle_pipeline_action(cert_action);
  delete cert_action;
  if (error)
  {
    /* purecov: begin inspected */
    log_message(MY_ERROR_LEVEL, "Error when processing Certification "
                "information in the Recovery process");
    leave_group_on_recovery_failure();
    DBUG_RETURN(1);
    /* purecov: end */
  }

  recovery_state_transfer.end_state_transfer();

  DBUG_RETURN(0);
}

void
Recovery_module::set_recovery_thread_context()
{
  THD* thd= new THD;
  my_thread_init();
  thd->set_new_thread_id();
  thd->thread_stack= (char*) &thd;
  mysql_thread_set_psi_id(thd->thread_id());
  thd->store_globals();

  global_thd_manager_add_thd(thd);
  thd->security_context()->skip_grants();

  thd->slave_thread= 1;
  recovery_thd= thd;
}

void
Recovery_module::clean_recovery_thread_context()
{
  recovery_thd->release_resources();
  THD_CHECK_SENTRY(recovery_thd);
  global_thd_manager_remove_thd(recovery_thd);
}

int Recovery_module::wait_for_applier_module_recovery()
{
  DBUG_ENTER("Recovery_module::wait_for_applier_module_recovery");

  size_t queue_size= 0, queue_initial_size= queue_size= applier_module->get_message_queue_size();
  uint64 transactions_applied_during_recovery= 0;

  /*
    Wait for the number the transactions to be applied be greater than the
    initial size of the queue or the queue be empty, what happens first will
    finish the recovery.
  */

  bool applier_monitoring= true;
  while (!recovery_aborted && applier_monitoring)
  {
    transactions_applied_during_recovery= applier_module
      ->get_pipeline_stats_member_collector_transactions_applied_during_recovery();
    queue_size = applier_module->get_message_queue_size();

    if ((queue_initial_size - RECOVERY_TRANSACTION_THRESHOLD) < transactions_applied_during_recovery
        || queue_size <= RECOVERY_TRANSACTION_THRESHOLD)
    {
      int error= 1;
      while (recovery_completion_policy == RECOVERY_POLICY_WAIT_EXECUTED
             && !recovery_aborted && error != 0)
      {
        error= applier_module->wait_for_applier_event_execution(1, false);

        /* purecov: begin inspected */
        if (error == -2) //error when waiting
        {
          applier_monitoring= false;
          log_message(MY_ERROR_LEVEL,
                      "It is not possible to ensure the execution of group"
                      " transactions received during recovery.");
          DBUG_RETURN(1);
        }
        /* purecov: end */
      }
      applier_monitoring= false;
    }
    else
    {
      my_sleep(100 * std::min(queue_size, static_cast<size_t>(5000)));
    }
  }

  if (applier_module->get_applier_status() == APPLIER_ERROR &&
      !recovery_aborted)
    DBUG_RETURN(1); /* purecov: inspected */

  DBUG_RETURN(0);
}

void Recovery_module::notify_group_recovery_end()
{
  DBUG_ENTER("Recovery_module::notify_group_recovery_end");

  Recovery_message recovery_msg(Recovery_message::RECOVERY_END_MESSAGE,
                                local_member_info->get_uuid());
  enum_gcs_error msg_error= gcs_module->send_message(recovery_msg);
  if (msg_error != GCS_OK)
  {
    log_message(MY_ERROR_LEVEL,
                "Error while sending message for group replication recovery"); /* purecov: inspected */
  }

  DBUG_VOID_RETURN;
}

bool Recovery_module::is_own_event_channel(my_thread_id id)
{
  DBUG_ENTER("Recovery_module::is_own_event_channel");
  DBUG_RETURN(recovery_state_transfer.is_own_event_channel(id));
}
