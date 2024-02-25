/* Copyright (c) 2018, 2023, Oracle and/or its affiliates.

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

#include "plugin/group_replication/include/group_actions/primary_election_action.h"
#include "plugin/group_replication/include/plugin.h"
#include "plugin/group_replication/include/plugin_handlers/server_ongoing_transactions_handler.h"
#include "plugin/group_replication/include/plugin_messages/group_action_message.h"
#include "plugin/group_replication/include/services/system_variable/set_system_variable.h"
#include "template_utils.h"

Primary_election_action::Primary_election_action()
    : Primary_election_action(std::string(""), 0) {
  if (local_member_info && local_member_info->in_primary_mode()) {
    action_execution_mode = PRIMARY_ELECTION_ACTION_PRIMARY_SWITCH;
  } else {
    action_execution_mode = PRIMARY_ELECTION_ACTION_MODE_SWITCH;
  }
}

Primary_election_action::Primary_election_action(
    std::string primary_uuid_arg, my_thread_id thread_id,
    int32 transaction_wait_timeout_arg)
    : action_execution_mode(PRIMARY_ELECTION_ACTION_END),
      current_action_phase(PRIMARY_NO_PHASE),
      single_election_action_aborted(false),
      error_on_primary_election(false),
      action_killed(false),
      appointed_primary_uuid(primary_uuid_arg),
      appointed_primary_gcs_id(""),
      invoking_member_gcs_id(""),
      old_primary_uuid(""),
      is_primary(false),
      invoking_thread_id(thread_id),
      is_primary_election_invoked(false),
      is_transaction_queue_applied(false),
      m_transaction_wait_timeout(transaction_wait_timeout_arg) {
  mysql_mutex_init(key_GR_LOCK_primary_election_action_phase, &phase_lock,
                   MY_MUTEX_INIT_FAST);
  mysql_mutex_init(key_GR_LOCK_primary_election_action_notification,
                   &notification_lock, MY_MUTEX_INIT_FAST);
  mysql_cond_init(key_GR_COND_primary_election_action_notification,
                  &notification_cond);
  if (local_member_info && local_member_info->in_primary_mode()) {
    action_execution_mode = PRIMARY_ELECTION_ACTION_PRIMARY_SWITCH;
  } else {
    action_execution_mode = PRIMARY_ELECTION_ACTION_MODE_SWITCH;
  }
}

Primary_election_action::~Primary_election_action() {
  mysql_mutex_destroy(&phase_lock);
  mysql_mutex_destroy(&notification_lock);
  mysql_cond_destroy(&notification_cond);
  /**
    Assert has been used since object has not been destroyed at the exit point
    code. If this ever occurs, the scenario needs to be analyzed to understand
    why object was not deleted on the exit points of the primary election.
  */
  assert(transaction_monitor_thread == nullptr);
  /**
    delete transaction_monitor_thread is present at exit points when a change of
    primary is finishing. However if delete is not triggered at those exit
    points then object should be destroyed here. This code is added for safety.
  */
  stop_transaction_monitor_thread();
}

bool Primary_election_action::stop_transaction_monitor_thread() {
  bool ret = false;
  if (transaction_monitor_thread) {
    ret = transaction_monitor_thread->terminate();
    delete transaction_monitor_thread;
    transaction_monitor_thread = nullptr;
  }
  return ret;
}

void Primary_election_action::get_action_message(
    Group_action_message **message) {
  *message = new Group_action_message(appointed_primary_uuid,
                                      m_transaction_wait_timeout);
}

int Primary_election_action::process_action_message(
    Group_action_message &message, const std::string &message_origin) {
  execution_message_area.clear_info();

  appointed_primary_uuid.assign(message.get_primary_to_elect_uuid());
  invoking_member_gcs_id.clear();
  old_primary_uuid.clear();

  validation_handler.initialize_validation_structures();
  Primary_election_validation_handler::enum_primary_validation_result
      validation_result;

  if (!appointed_primary_uuid.empty()) {
    validation_result =
        validation_handler.validate_primary_uuid(appointed_primary_uuid);
    if (Primary_election_validation_handler::INVALID_PRIMARY ==
        validation_result) {
      /* purecov: begin inspected */
      execution_message_area.set_execution_message(
          Group_action_diagnostics::GROUP_ACTION_LOG_ERROR,
          "Requested member for primary election is no longer in the group.");
      validation_handler.terminates_validation_structures();
      return 1;
      /* purecov: end */
    }
    if (Primary_election_validation_handler::CURRENT_PRIMARY ==
        validation_result) {
      /* purecov: begin inspected */
      execution_message_area.set_execution_message(
          Group_action_diagnostics::GROUP_ACTION_LOG_ERROR,
          "Requested member for primary election is already the primary.");
      validation_handler.terminates_validation_structures();
      return 1;
      /* purecov: end */
    }

    Group_member_info *appointed_primary_info =
        group_member_mgr->get_group_member_info(appointed_primary_uuid);
    appointed_primary_gcs_id.assign(
        appointed_primary_info->get_gcs_member_id().get_member_id());
    delete appointed_primary_info;
  }

  std::string error_message;
  validation_result = validation_handler.validate_primary_version(
      appointed_primary_uuid, error_message);
  if (Primary_election_validation_handler::VALID_PRIMARY != validation_result) {
    execution_message_area.set_execution_message(
        Group_action_diagnostics::GROUP_ACTION_LOG_ERROR, error_message);
    validation_handler.terminates_validation_structures();
    return 1;
  }

  if (PRIMARY_ELECTION_ACTION_PRIMARY_SWITCH == action_execution_mode) {
    if (local_member_info->get_role() ==
            Group_member_info::MEMBER_ROLE_PRIMARY &&
        message.get_transaction_monitor_timeout() != -1) {
      /*
       Here Transaction_monitor_thread object is created but thread is not
       started.
       @note Transaction_monitor_thread thread start is delayed because
       validation of running channels has not been done yet.
       @note If we need to start the Transaction_monitor_thread thread now, we
       will have to check if there is any running channel on this member. It is
       not future safe, since validations may increase in future.
       @refer validate_election for duplicate post validation conditions
      */
      transaction_monitor_thread = new Transaction_monitor_thread(
          message.get_transaction_monitor_timeout());
    }

    Group_member_info *primary_info =
        group_member_mgr->get_primary_member_info();
    if (primary_info != nullptr) {
      invoking_member_gcs_id.assign(
          primary_info->get_gcs_member_id().get_member_id());
      is_primary = invoking_member_gcs_id ==
                   local_member_info->get_gcs_member_id().get_member_id();
      old_primary_uuid.assign(primary_info->get_uuid());
      delete primary_info;
    }
  }

  /*
    If there is no old primary to invoke the election then select
    1. The action invocation member
    2. If not there, then the first member in the group (after sort).
  */
  if (invoking_member_gcs_id.empty()) {
    Group_member_info_list *all_members_info =
        group_member_mgr->get_all_members();
    std::sort(all_members_info->begin(), all_members_info->end());

    for (Group_member_info *member : *all_members_info) {
      if (member->get_gcs_member_id().get_member_id() == message_origin) {
        invoking_member_gcs_id.assign(message_origin);
        break;
      }
    }
    if (invoking_member_gcs_id.empty()) {
      invoking_member_gcs_id.assign(
          all_members_info->front()
              ->get_gcs_member_id()
              .get_member_id()); /* purecov: inspected */
    }

    for (Group_member_info *member : *all_members_info) {
      delete member;
    }
    delete all_members_info;
  }

  m_execution_status = PRIMARY_ELECTION_INIT;
  is_transaction_queue_applied = false;
  change_action_phase(PRIMARY_VALIDATION_PHASE);
  group_events_observation_manager->register_group_event_observer(this);

  return 0;
}

Group_action::enum_action_execution_result
Primary_election_action::execute_action(
    bool, Plugin_stage_monitor_handler *stage_handler, Notification_context *) {
  bool mode_is_set = false;
  bool action_terminated = false;
  int error = 0;
  PSI_stage_key stage_key =
      PRIMARY_ELECTION_ACTION_PRIMARY_SWITCH == action_execution_mode
          ? info_GR_STAGE_primary_switch_checks.m_key
          : info_GR_STAGE_single_primary_mode_switch_checks.m_key;
  stage_handler->set_stage(stage_key, __FILE__, __LINE__, 1, 0);
  stage_handler->set_completed_work(0);

  Primary_election_validation_handler::enum_primary_validation_result
      validation_result;
  std::string valid_primary_uuid;
  std::string error_msg;
  if (validation_handler.prepare_election()) {
    /* purecov: begin inspected */
    error = 1;
    error_msg =
        " This operation ended in error as it was not possible to share "
        "information for the election process.";
    goto end;
    /* purecov: end */
  }
  validation_result = validation_handler.validate_election(
      appointed_primary_uuid, valid_primary_uuid, error_msg);
  validation_handler.terminates_validation_structures();
  if (Primary_election_validation_handler::VALID_PRIMARY != validation_result) {
    if (Primary_election_validation_handler::GROUP_SOLO_PRIMARY ==
        validation_result)
      appointed_primary_uuid.assign(valid_primary_uuid);
    else {
      execution_message_area.set_execution_message(
          Group_action_diagnostics::GROUP_ACTION_LOG_ERROR, error_msg);
      single_election_action_aborted = true;
      goto end;
    }
  }
  if (transaction_monitor_thread != nullptr) {
    if (transaction_monitor_thread->start()) {
      error = 1;
      error_msg =
          " This operation ended in error as it was not possible to stop the "
          "ongoing transactions.";
      goto end;
    }
  }

  DBUG_EXECUTE_IF("group_replication_block_primary_action_validation", {
    const char act[] = "now wait_for signal.primary_action_continue";
    assert(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
  });

  stage_handler->set_completed_work(1);

  change_action_phase(PRIMARY_SAFETY_CHECK_PHASE);

  if (PRIMARY_ELECTION_ACTION_PRIMARY_SWITCH == action_execution_mode) {
    set_enforce_update_everywhere_checks(true);
    group_member_mgr->update_enforce_everywhere_checks_flag(true);

    if (is_primary) {
      stage_handler->set_stage(
          info_GR_STAGE_primary_switch_pending_transactions.m_key, __FILE__,
          __LINE__, 999, 0);

      Server_ongoing_transactions_handler ongoing_transactions_handler;
      ongoing_transactions_handler.initialize_server_service(stage_handler);
      if (ongoing_transactions_handler
              .wait_for_current_transaction_load_execution(
                  &single_election_action_aborted, invoking_thread_id)) {
        /* purecov: begin inspected */
        error = 2;
        error_msg =
            " This operation ended in error as it was not possible to wait for "
            "the execution of server running transactions.";
        goto end;
        /* purecov: end */
      }
    } else {
      stage_handler->set_stage(
          info_GR_STAGE_primary_switch_step_completion.m_key, __FILE__,
          __LINE__, 1, 0);
    }
  }

  if (!single_election_action_aborted &&
      invoking_member_gcs_id ==
          local_member_info->get_gcs_member_id().get_member_id()) {
    if (PRIMARY_ELECTION_ACTION_PRIMARY_SWITCH == action_execution_mode)
      primary_election_handler->request_group_primary_election(
          appointed_primary_uuid, UNSAFE_OLD_PRIMARY);
    else
      primary_election_handler->request_group_primary_election(
          appointed_primary_uuid, SAFE_OLD_PRIMARY);
  }

  mysql_mutex_lock(&notification_lock);
  while (!is_primary_election_invoked && !single_election_action_aborted) {
    DBUG_PRINT("sleep", ("Waiting for the primary election to be invoked."));
    mysql_cond_wait(&notification_cond, &notification_lock);
  }
  mysql_mutex_unlock(&notification_lock);

  stage_key = PRIMARY_ELECTION_ACTION_PRIMARY_SWITCH == action_execution_mode
                  ? info_GR_STAGE_primary_switch_election.m_key
                  : info_GR_STAGE_single_primary_mode_switch_election.m_key;
  stage_handler->set_stage(stage_key, __FILE__, __LINE__, 2, 0);

  mysql_mutex_lock(&notification_lock);
  while (PRIMARY_ELECTION_INIT == m_execution_status &&
         !single_election_action_aborted) {
    DBUG_PRINT("sleep", ("Waiting for the primary to be elected."));
    mysql_cond_wait(&notification_cond, &notification_lock);
  }
  mysql_mutex_unlock(&notification_lock);

  stage_handler->set_completed_work(1);

  if (PRIMARY_ELECTION_END_ERROR == m_execution_status ||
      PRIMARY_ELECTION_INIT == m_execution_status) {
    goto end;
  }

  if (!single_election_action_aborted) {
    set_single_primary_mode_var(true);
    mode_is_set =
        (PRIMARY_ELECTION_ACTION_MODE_SWITCH == action_execution_mode);
  }

  mysql_mutex_lock(&notification_lock);
  while (!is_transaction_queue_applied && !single_election_action_aborted) {
    DBUG_PRINT("sleep",
               ("Waiting for transaction to be applied on the primary."));
    mysql_cond_wait(&notification_cond, &notification_lock);
  }
  mysql_mutex_unlock(&notification_lock);

  stage_handler->set_completed_work(2);

  /* We only need to update the increment values on mode switches*/
  if ((!single_election_action_aborted &&
       PRIMARY_ELECTION_ACTION_MODE_SWITCH == action_execution_mode) ||
      mode_is_set) {
    reset_auto_increment_handler_values(true);
  }

  if (!single_election_action_aborted &&
      PRIMARY_ELECTION_ACTION_MODE_SWITCH == action_execution_mode) {
    persist_variable_values();
    action_terminated = true;
  }

end:

  /* Even if the action was cancelled, if it already is in primary mode or if
     it was a primary switch reset it the flags anyway */
  if ((!single_election_action_aborted || mode_is_set) ||
      PRIMARY_ELECTION_ACTION_PRIMARY_SWITCH == action_execution_mode) {
    set_enforce_update_everywhere_checks(false);
    group_member_mgr->update_enforce_everywhere_checks_flag(false);
  }

  if (single_election_action_aborted && !mode_is_set &&
      PRIMARY_ELECTION_ACTION_MODE_SWITCH == action_execution_mode) {
    group_member_mgr->update_primary_member_flag(false);
  }

  group_events_observation_manager->unregister_group_event_observer(this);
  /**
    Observers have been de-registered so transaction_monitor_thread should have
    been destroyed by now in after_view_change or after_primary_election. If
    not, then destroy the object now(during UDF failure it has been observed
    intermittently object was not destroyed in after_view_change or
    after_primary_election).
  */
  stop_transaction_monitor_thread();

  error += error_on_primary_election;
  log_result_execution(error,
                       single_election_action_aborted && !action_terminated,
                       mode_is_set, error_msg);

  // Don't abort if it already finished
  if ((!single_election_action_aborted && !error) || action_terminated)
    return Group_action::GROUP_ACTION_RESULT_TERMINATED;
  if (error) {
    return Group_action::GROUP_ACTION_RESULT_ERROR; /* purecov: inspected */
  }
  if (action_killed) {
    return Group_action::GROUP_ACTION_RESULT_KILLED;
  }

  return Group_action::GROUP_ACTION_RESULT_ABORTED;
}

bool Primary_election_action::stop_action_execution(bool killed) {
  mysql_mutex_lock(&notification_lock);
  /**
    When a UDF group_replication_set_as_primary is killed using 'KILL
    connection_id' code reaches here.
  */
  stop_transaction_monitor_thread();

  action_killed = killed;
  single_election_action_aborted = true;
  mysql_cond_broadcast(&notification_cond);
  mysql_mutex_unlock(&notification_lock);
  return false;
}

void Primary_election_action::change_action_phase(
    enum_primary_election_phase phase) {
  mysql_mutex_lock(&phase_lock);
  // We only increment phases - concurrency issues could make it the other way
  if (phase > current_action_phase) current_action_phase = phase;
  mysql_mutex_unlock(&phase_lock);
}

PSI_stage_key Primary_election_action::get_action_stage_termination_key() {
  PSI_stage_key stage_key =
      PRIMARY_ELECTION_ACTION_PRIMARY_SWITCH == action_execution_mode
          ? info_GR_STAGE_primary_switch_completion.m_key
          : info_GR_STAGE_single_primary_mode_switch_completion.m_key;
  return stage_key;
}

Group_action_diagnostics *Primary_election_action::get_execution_info() {
  return &execution_message_area;
}

// The listeners for group events

int Primary_election_action::after_view_change(
    const std::vector<Gcs_member_identifier> &,
    const std::vector<Gcs_member_identifier> &leaving,
    const std::vector<Gcs_member_identifier> &, bool is_leaving,
    bool *skip_primary_election, enum_primary_election_mode *election_mode_out,
    std::string &proposed_primary) {
  if (is_leaving) {
    stop_transaction_monitor_thread(); /* purecov: inspected */
    return 0;
  }

  if (single_election_action_aborted) return 0;

  bool is_old_primary_leaving = false;
  bool is_appointed_primary_leaving = false;
  for (Gcs_member_identifier leaving_member : leaving) {
    if (leaving_member.get_member_id() == appointed_primary_gcs_id) {
      is_appointed_primary_leaving = true;
    }
    if (leaving_member.get_member_id() == invoking_member_gcs_id) {
      is_old_primary_leaving = true;
    }
  }

  if (is_old_primary_leaving) {
    old_primary_uuid.clear();
  }

  /*
    The primary to be elected left even before the election call was *received*.
    Here we choose to abort, in a logical moment, so all members make the same
    decision.
    However, the invoking member could already sent the message for the
    election. If that happens, the invocation process will terminate in the
    primary election handler but only because the appointed uuid is not in the
    group anymore. Check Primary_election_handler::execute_primary_election
  */
  if (is_appointed_primary_leaving &&
      current_action_phase < PRIMARY_ELECTION_PHASE) {
    mysql_mutex_lock(&notification_lock);
    execution_message_area.set_execution_message(
        Group_action_diagnostics::GROUP_ACTION_LOG_ERROR,
        "Primary assigned for election left the group,"
        " this operation will be aborted. "
        "No primary election was invoked under this operation.");
    single_election_action_aborted = true;
    mysql_cond_broadcast(&notification_cond);
    mysql_mutex_unlock(&notification_lock);
    return 0;
  }

  /**
    Either:
      -This is a change from SP and the the old primary died, so there is no
      invoking member, nor a member that is waiting on local transactions.
      -On changes from multi primary, and the invoking member left

    For all the old secondaries that remain this means:
    1. If election was already called, then we don't care
    2. If election was not yet called then, either
      2.1 we are still validating the appointed primary, so we just change what
      the invoking member is
      2.2 we have already validate it, so if this is the new invoker it must
      invoke the new primary election

    Note that this state change from validation to the next state is not in a
    group logical moment so different members can be at different stages.
    Still all that members care is the moment the election is called, so the
    result will be the same at all members. If the primary is not valid, the
    stage never changed from validation so member will invoke the election here.
    If valid, members will either update the invoking member or invoke the
    election according to the stage (hence the phase lock)

    We also make the failover primary election process to be skipped, because
    we want a specific primary to be elected.
  */
  if (is_old_primary_leaving && current_action_phase < PRIMARY_ELECTION_PHASE) {
    *skip_primary_election = true;

    Group_member_info_list *all_members_info =
        group_member_mgr->get_all_members();
    std::sort(all_members_info->begin(), all_members_info->end(),
              Group_member_info::comparator_group_member_uuid);
    Group_member_info *new_invoking_member = all_members_info->front();

    mysql_mutex_lock(&phase_lock);
    if (current_action_phase == PRIMARY_VALIDATION_PHASE) {
      invoking_member_gcs_id.assign(
          new_invoking_member->get_gcs_member_id().get_member_id());
    } else {
      /* purecov: begin inspected */
      assert(proposed_primary.empty());
      *skip_primary_election = false;
      if (PRIMARY_ELECTION_ACTION_PRIMARY_SWITCH == action_execution_mode) {
        proposed_primary.assign(appointed_primary_uuid);
        *election_mode_out = DEAD_OLD_PRIMARY;
      } else {
        proposed_primary.assign(appointed_primary_uuid);
        *election_mode_out = SAFE_OLD_PRIMARY;
      }
      /* purecov: end */
    }
    delete_container_pointers(*all_members_info);
    delete all_members_info;

    mysql_mutex_unlock(&phase_lock);
  }

  /**
    There is an election and the primary being elected dies.

    Case 1: We are moving to another primary, and it failed. The process will
    fail abort but we still try to invoke an election to go back to the old
    primary.

    Case 2: We are moving to single primary mode and the appointed primary died,
    then we move to another primary and emit a warning

    Note 1: We pass election params instead of invoking them because of the hook
    mechanism that is invoking this method and is also used in the invoked
    election methods.
  */
  if (current_action_phase == PRIMARY_ELECTION_PHASE) {
    Group_member_info *member_info =
        group_member_mgr->get_primary_member_info();
    if (member_info == nullptr || is_appointed_primary_leaving) {
      assert(appointed_primary_gcs_id.empty() || is_appointed_primary_leaving);
      *skip_primary_election = false;
      std::string new_primary("");
      if (PRIMARY_ELECTION_ACTION_PRIMARY_SWITCH == action_execution_mode) {
        execution_message_area.set_execution_message(
            Group_action_diagnostics::GROUP_ACTION_LOG_ERROR,
            "The appointed primary for election left the group,"
            " this operation will be aborted and if present the old primary"
            " member will be re-elected."
            " Check the group member list to see who is the primary.");
        /**
          Check if we can change to the old primary.
          If this variable is empty, it is not present.
        */
        if (!old_primary_uuid.empty()) {
          new_primary.assign(old_primary_uuid);
        }

        proposed_primary.assign(new_primary);
        *election_mode_out = DEAD_OLD_PRIMARY;

        mysql_mutex_lock(&notification_lock);
        single_election_action_aborted = true;
        mysql_cond_broadcast(&notification_cond);
        mysql_mutex_unlock(&notification_lock);
      } else {
        /*
          There was a failure when moving from multi master
          Question is: what is the state of the other members, are the members
          already on read mode, or are they still executing transactions?
          For that reason, we request a new election here with the original
          parameters.

          Note: since several primary members can fail in sequence, the same
          logic applies over and over.
        */
        execution_message_area.set_warning_message(
            "The appointed primary being elected exited the group. "
            "Check the group member list to see who is the primary.");
        proposed_primary.assign(new_primary);
        *election_mode_out = SAFE_OLD_PRIMARY;
      }
      appointed_primary_gcs_id.clear();
    }
    delete member_info;
  }

  /*
    The primary left but we are already in the end
    Just emit a warning.
  */
  if (current_action_phase > PRIMARY_ELECTION_PHASE) {
    if (is_appointed_primary_leaving) {
      execution_message_area.set_warning_message(
          "The appointed primary left the group as the operation is "
          "terminating. "
          "Check the group member list to see who is the primary.");
    }
  }

  return 0;
}

int Primary_election_action::after_primary_election(
    std::string elected_uuid,
    enum_primary_election_primary_change_status primary_change_status,
    enum_primary_election_mode primary_election_mode, int error) {
  // We are leaving the group but we can speed up the process
  if (error == PRIMARY_ELECTION_PROCESS_ERROR) {
    error_on_primary_election = true; /* purecov: inspected */
    stop_action_execution(false);     /* purecov: inspected */
  }
  if (UNSAFE_OLD_PRIMARY == primary_election_mode) {
    stop_transaction_monitor_thread();
  }
  // No candidates? Just abort
  if (error == PRIMARY_ELECTION_NO_CANDIDATES_ERROR) {
    /* purecov: begin inspected */
    mysql_mutex_lock(&notification_lock);
    single_election_action_aborted = true;
    mysql_cond_broadcast(&notification_cond);
    mysql_mutex_unlock(&notification_lock);
    /* purecov: end */
  }
  if (enum_primary_election_primary_change_status::PRIMARY_DID_CHANGE ==
          primary_change_status ||
      enum_primary_election_primary_change_status::
              PRIMARY_DID_NOT_CHANGE_PRIMARY_LEFT_FORCE_ELECTION_END ==
          primary_change_status) {
    mysql_mutex_lock(&notification_lock);

    m_execution_status = PRIMARY_ELECTION_END_ELECTION;
    // Set this also to true for election invoked on member leaves
    is_primary_election_invoked = true;

    /*
      Note that for all elections types besides DEAD_OLD_PRIMARY this observer
      is invoked in a group logical moment.
    */
    change_action_phase(PRIMARY_ELECTED_PHASE);

    mysql_cond_broadcast(&notification_cond);
    mysql_mutex_unlock(&notification_lock);
  }

  return 0;
}

int Primary_election_action::before_message_handling(
    const Plugin_gcs_message &message, const std::string &,
    bool *skip_message) {
  *skip_message = false;
  Plugin_gcs_message::enum_cargo_type message_type = message.get_cargo_type();

  if (Plugin_gcs_message::CT_SINGLE_PRIMARY_MESSAGE == message_type) {
    const Single_primary_message &single_primary_message =
        down_cast<const Single_primary_message &>(message);
    Single_primary_message::Single_primary_message_type
        single_primary_msg_type =
            single_primary_message.get_single_primary_message_type();

    if (Single_primary_message::SINGLE_PRIMARY_QUEUE_APPLIED_MESSAGE ==
        single_primary_msg_type) {
      mysql_mutex_lock(&notification_lock);
      is_transaction_queue_applied = true;
      mysql_cond_broadcast(&notification_cond);
      mysql_mutex_unlock(&notification_lock);
    }

    if (Single_primary_message::SINGLE_PRIMARY_PRIMARY_ELECTION ==
        single_primary_msg_type) {
      change_action_phase(PRIMARY_ELECTION_PHASE);
      mysql_mutex_lock(&notification_lock);
      is_primary_election_invoked = true;
      mysql_cond_broadcast(&notification_cond);
      mysql_mutex_unlock(&notification_lock);
    }
  }
  return 0;
}

bool Primary_election_action::persist_variable_values() {
  int error = 0;
  Set_system_variable set_system_variable;

  if ((error =
           set_system_variable
               .set_persist_only_group_replication_enforce_update_everywhere_checks(
                   false))) {
    goto end; /* purecov: inspected */
  }

  if ((error =
           set_system_variable
               .set_persist_only_group_replication_single_primary_mode(true))) {
    goto end; /* purecov: inspected */
  }

end:
  if (error) {
    execution_message_area.set_warning_message(
        "It was not possible to persist the configuration values for this "
        "mode. Check your server configuration for future server restarts "
        "and/or try to use SET PERSIST_ONLY.");
  }
  return error != 0;
}

void Primary_election_action::log_result_execution(bool error, bool aborted,
                                                   bool mode_changed,
                                                   std::string &error_msg) {
  if (error) {
    execution_message_area.set_execution_message(
        Group_action_diagnostics::GROUP_ACTION_LOG_ERROR,
        "A critical error occurred during the local execution of this "
        "action."); /* purecov: inspected */
    if (mode_changed) {
      execution_message_area.append_execution_message(
          " However the member is already configured to run in single "
          "primary mode, but the configuration was not "
          "persisted."); /* purecov: inspected */
    }
    if (!error_msg.empty())
      execution_message_area.append_execution_message(error_msg);
    return; /* purecov: inspected */
  }

  if (!aborted) {
    if (execution_message_area.has_warning()) {
      if (PRIMARY_ELECTION_ACTION_PRIMARY_SWITCH == action_execution_mode) {
        std::string warning_message =
            "Primary switch to server " + appointed_primary_uuid +
            " terminated with some warnings: " +
            execution_message_area
                .get_warning_message(); /* purecov: inspected */
        execution_message_area.set_execution_message(
            Group_action_diagnostics::GROUP_ACTION_LOG_WARNING,
            warning_message); /* purecov: inspected */
      } else {
        std::string warning_message =
            "Mode switched to single-primary with reported warnings: " +
            execution_message_area.get_warning_message();
        execution_message_area.set_execution_message(
            Group_action_diagnostics::GROUP_ACTION_LOG_WARNING,
            warning_message);
      }
    } else {
      if (PRIMARY_ELECTION_ACTION_PRIMARY_SWITCH == action_execution_mode) {
        std::string message =
            "Primary server switched to: " + appointed_primary_uuid;
        execution_message_area.set_execution_message(
            Group_action_diagnostics::GROUP_ACTION_LOG_INFO, message);
      } else
        execution_message_area.set_execution_message(
            Group_action_diagnostics::GROUP_ACTION_LOG_INFO,
            "Mode switched to single-primary successfully.");
    }
  } else {
    if (execution_message_area.get_execution_message().empty()) {
      if (!action_killed) {
        execution_message_area.set_execution_message(
            Group_action_diagnostics::GROUP_ACTION_LOG_ERROR,
            "This operation was locally aborted and for that reason "
            "terminated.");
      } else {
        execution_message_area.set_execution_message(
            Group_action_diagnostics::GROUP_ACTION_LOG_ERROR,
            "This operation was locally killed and for that reason "
            "terminated.");
      }
      if (mode_changed) {
        execution_message_area.append_execution_message(
            " However the member is already configured to run in single "
            "primary mode, but the configuration was not persisted.");
      }
    }
  }
}
